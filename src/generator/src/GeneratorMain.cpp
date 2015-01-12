#include "SourcePackage.h"
#include "Hash.h"

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <iostream>
#include <string>

#include <pugixml.hpp>
#include <sqlite3.h>

#include <unordered_map>
#include <memory>

struct FileEntry
{
	int packageId = -1;
	std::string	sourcePath;
	std::string targetPath;
};

std::vector<FileEntry>	GatherFiles (const pugi::xml_node& product)
{
	std::vector<FileEntry> result;

	std::unordered_map<std::string, std::size_t> packageIds;

	for (auto files : product.children ("Files")) {
		const auto packageIdAttr = files.attribute ("SourcePackage");

		int packageId = -1;
		if (packageIdAttr) {
			const char* packageIdName = packageIdAttr.value ();

			if (packageIds.find (packageIdName) == packageIds.end ()) {
				const auto nextId = packageIds.size ();
				packageIds [packageIdName] = nextId;
			}

			packageId = static_cast<int> (packageIds [packageIdName]);
		}

		for (auto file : files.children ("File")) {
			FileEntry fe;
			fe.packageId = packageId;
			fe.sourcePath = file.attribute ("Source").value ();

			auto targetAttr = file.attribute ("Target");
			if (targetAttr) {
				fe.targetPath = targetAttr.value ();
			} else {
				fe.targetPath = fe.sourcePath;
			}

			result.push_back (fe);
		}
	}

	return result;
}

struct GeneratorContext
{
public:
	GeneratorContext ()
	{
		sqlite3_open (":memory:", &database);
	}

	~GeneratorContext ()
	{
		sqlite3_close (database);
	}

	bool WritePackage (const std::string& filename) const
	{

	}

	pugi::xml_node	productNode;
	sqlite3*		database;
	std::string		sourceDirectory;
};

void SetupTables (sqlite3* db)
{

}

void CreateSourcePackages (GeneratorContext& gc)
{
	std::unordered_map<std::string, Hash> sourceFileToHash;

	std::unordered_map<std::string, Hash> outputFileToHash;
	std::unordered_map<Hash, SourcePackageWriter*, HashHash, HashEqual> hashToPackage;

	std::unordered_map<int, std::unique_ptr<SourcePackageWriter>> packages;

	/*
	We read each file, compute the SHA1, assign it to a source package (or the
	default package.)

	The output is a map of (file, hash) pairs (multiple files may point at the
	same hash, and file is output-relative) as well a list of (hash, package)
	pairs, which indicate which hash is stored in which package.
	*/

	// Create the file and entry tables

	// First, we iterate over all files, and put them right into their packages
	const auto fileEntries = GatherFiles (gc.productNode);
	for (const auto& fe : fileEntries) {

	}
}

int main (int argc, char* argv[])
{
    namespace po = boost::program_options;
    po::options_description generic ("Generic options");
    generic.add_options ()
        ("help,h", "Show help message");

    po::options_description desc ("Configuration");
    desc.add_options ()
		("source-directory", po::value<std::string> ()->default_value ("."))
		("output,o", "Output file name");

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

	pugi::xml_document doc;
	doc.load_file (inputFile.c_str ());

	boost::filesystem::path sourcePath (inputFile);
	const auto outputFile = sourcePath.stem ();

	GeneratorContext gc;

	CreateSourcePackages (gc);
	gc.WritePackage (outputFile.string () + ".nimdb");
}
