#include "SourcePackage.h"
#include "Hash.h"

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <iostream>
#include <fstream>
#include <string>

#include "FileIO.h"

#include <pugixml.hpp>

#include <unordered_map>
#include <unordered_set>
#include <memory>

#include "Uuid.h"

#include <assert.h>

#include "Log.h"

#include "build-db-structure.h"
#include "install-db-structure.h"
#include "install-db-indices.h"

#include "sql/Database.h"

using path = boost::filesystem::path;

struct BuildContext
{
	path sourceDirectory;
	path targetDirectory;

	std::unique_ptr<kyla::Log> log;
};

struct File
{
	path source;
	path target;

	kyla::SHA256Digest hash;
};

struct FileSet
{
	std::vector<File> files;

	std::string name;
	kyla::Uuid id;
};

///////////////////////////////////////////////////////////////////////////////
std::vector<FileSet> GetFileSets (const pugi::xml_document& doc,
	const BuildContext& ctx)
{
	std::vector<FileSet> result;
	
	int filesFound = 0;

	for (const auto& fileSetNode : doc.select_nodes ("//FileSet")) {
		FileSet fileSet;

		fileSet.id = kyla::Uuid::Parse (fileSetNode.node ().attribute ("Id").as_string ());
		fileSet.name = fileSetNode.node ().attribute ("name").as_string ();

		for (const auto& fileNode : fileSetNode.node ().children ("File")) {
			File file;
			file.source = fileNode.attribute ("Source").as_string ();

			if (fileNode.attribute ("Target")) {
				file.target = fileNode.attribute ("Target").as_string ();
			}

			fileSet.files.push_back (file);

			++filesFound;
		}

		result.emplace_back (std::move (fileSet));
	}

	ctx.log->Info () << "Found " << filesFound << " files";

	return result;
}

///////////////////////////////////////////////////////////////////////////////
void HashFiles (std::vector<FileSet>& fileSets,
	const BuildContext& ctx)
{
	for (auto& fileSet : fileSets) {
		ctx.log->Trace () << "Hashing file set " << ToString (fileSet.id) << " with "
			<< fileSet.files.size () << " files";

		for (auto& file : fileSet.files) {
			ctx.log->Trace () << "Hashing file '" << file.source.string () << "'";
			file.hash = kyla::ComputeSHA256 (ctx.sourceDirectory / file.source);
			ctx.log->Trace () << " ... " << ToString (file.hash);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
struct UniqueFile
{
	path sourceFile;
	kyla::SHA256Digest hash;

	std::vector<path> references;
};

std::vector<UniqueFile> FindUniqueFileContents (std::vector<FileSet>& fileSets,
	const BuildContext& ctx)
{
	std::unordered_map<kyla::SHA256Digest, std::vector<path>,
		kyla::HashDigestHash, kyla::HashDigestEqual> uniqueContents;

	for (const auto& fileSet : fileSets) {
		for (const auto& file : fileSet.files) {
			// This assumes the hashes are up-to-date
			uniqueContents [file.hash].push_back (file.source);
		}
	}

	ctx.log->Info () << "Found " << uniqueContents.size () << " unique files";

	std::vector<UniqueFile> result;
	result.reserve (uniqueContents.size ());

	for (const auto& kv : uniqueContents) {
		UniqueFile uf;

		uf.hash = kv.first;
		uf.sourceFile = kv.second.front ();
		
		if (kv.second.size () > 1) {
			ctx.log->Trace () << "File '" << uf.sourceFile.string () << "' has " <<
				(kv.second.size () - 1) << " duplicates";

			uf.references.assign (kv.second.begin () + 1, kv.second.end ());
		}

		result.push_back (uf);
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////////
int main (int argc, char* argv[])
{
	namespace po = boost::program_options;
	po::options_description generic ("Generic options");
	generic.add_options ()
		("help,h", "Show help message");

	po::options_description desc ("Configuration");
	desc.add_options ()
		("log-level", po::value<int> ()->default_value (2))
		("source-directory", po::value<std::string> ()->default_value ("."))
		("target-directory", po::value<std::string> ()->default_value ("."))
		;

	po::options_description hidden ("Hidden options");
	hidden.add_options ()
		("input-file", po::value<std::string> ());

	po::options_description cmdline_options;
	cmdline_options.add (generic).add (desc).add (hidden);

	po::options_description visible_options;
	visible_options.add (generic).add (desc);

	po::positional_options_description p;
	p.add("input-file", 1);

	po::variables_map vm;
	po::store (po::command_line_parser (argc, argv)
		.options (cmdline_options).positional (p).run (), vm);
	po::notify (vm);

	if (vm.count ("help")) {
		std::cout << visible_options << std::endl;
		return 0;
	}

	const auto inputFile = vm ["input-file"].as<std::string> ();

	BuildContext ctx;
	ctx.sourceDirectory = vm ["source-directory"].as<std::string> ();
	ctx.targetDirectory = vm ["target-directory"].as<std::string> ();

	ctx.log.reset (new kyla::Log ("Generator", "build.log", kyla::LogLevel::Trace));

	pugi::xml_document doc;
	doc.load_file (inputFile.c_str ());

	auto fileSets = GetFileSets (doc, ctx);

	HashFiles (fileSets, ctx);

	auto uniqueFiles = FindUniqueFileContents (fileSets, ctx);

	// Build content object database
}
