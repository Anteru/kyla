#include "Kyla.h"
#include "Hash.h"

#include <boost/filesystem.hpp>
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

#include "install-db-structure.h"

#include <map>

#include "sql/Database.h"

namespace {
struct BuildContext
{
	kyla::Path sourceDirectory;
	kyla::Path targetDirectory;

	std::unique_ptr<kyla::Log> log;
};

struct File
{
	kyla::Path source;
	kyla::Path target;

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
		fileSet.name = fileSetNode.node ().attribute ("Name").as_string ();

		for (const auto& fileNode : fileSetNode.node ().children ("File")) {
			File file;
			file.source = fileNode.attribute ("Source").as_string ();

			if (fileNode.attribute ("Target")) {
				file.target = fileNode.attribute ("Target").as_string ();
			} else {
				file.target = file.source;
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
struct UniqueContentObjects
{
	kyla::Path sourceFile;
	kyla::SHA256Digest hash;
	std::size_t size;

	std::vector<kyla::Path> duplicates;
};

///////////////////////////////////////////////////////////////////////////////
/**
Given a couple of file sets, we find unique files by hashing everything
and merging the results on the hash.
*/
std::vector<UniqueContentObjects> FindUniqueFileContents (std::vector<FileSet>& fileSets,
	const BuildContext& ctx)
{
	std::unordered_map<kyla::SHA256Digest, std::vector<kyla::Path>,
		kyla::HashDigestHash, kyla::HashDigestEqual> uniqueContents;

	for (const auto& fileSet : fileSets) {
		for (const auto& file : fileSet.files) {
			// This assumes the hashes are up-to-date, i.e. initialized
			uniqueContents [file.hash].push_back (file.source);
		}
	}

	ctx.log->Info () << "Found " << uniqueContents.size () << " unique files";

	std::vector<UniqueContentObjects> result;
	result.reserve (uniqueContents.size ());

	for (const auto& kv : uniqueContents) {
		UniqueContentObjects uf;

		uf.hash = kv.first;
		uf.sourceFile = ctx.sourceDirectory / kv.second.front ();

		uf.size = kyla::Stat (uf.sourceFile.string ().c_str ()).size;

		if (kv.second.size () > 1) {
			ctx.log->Trace () << "File '" << uf.sourceFile.string () << "' has " <<
				(kv.second.size () - 1) << " duplicates";
		}

		uf.duplicates.assign (kv.second.begin (), kv.second.end ());

		result.push_back (uf);
	}

	return result;
}

struct IRepositoryBuilder
{
	virtual ~IRepositoryBuilder ()
	{
	}

	virtual void Build (const BuildContext& ctx,
		const std::vector<FileSet>& fileSets,
		const std::vector<UniqueContentObjects>& uniqueFiles) = 0;
};

/**
A loose repository is little more than the files themselves, with hashes.
*/
struct LooseRepositoryBuilder final : public IRepositoryBuilder
{
	void Build (const BuildContext& ctx,
		const std::vector<FileSet>& fileSets,
		const std::vector<UniqueContentObjects>& uniqueFiles) override
	{
		boost::filesystem::create_directories (ctx.targetDirectory / ".ky");
		boost::filesystem::create_directories (ctx.targetDirectory / ".ky" / "objects");

		auto dbFile = ctx.targetDirectory / ".ky" / "repository.db";
		boost::filesystem::remove (dbFile);

		auto db = kyla::Sql::Database::Create (
			dbFile.string ().c_str ());

		db.Execute (install_db_structure); 
		db.Execute ("PRAGMA journal_mode=WAL;");
		db.Execute ("PRAGMA synchronous=NORMAL;");

		const auto fileToFileSetId = PopulateFileSets (db, fileSets);
		PopulateContentObjectsAndFiles (db, uniqueFiles, fileToFileSetId,
			ctx.targetDirectory / ".ky" / "objects");

		db.Execute ("PRAGMA journal_mode=DELETE;");
		// Necessary to get good index statistics
		db.Execute ("ANALYZE");

		db.Close ();
	}

private:
	std::map<kyla::Path, std::int64_t> PopulateFileSets (kyla::Sql::Database& db,
		const std::vector<FileSet>& fileSets)
	{
		auto fileSetsInsert = db.BeginTransaction ();
		auto fileSetsInsertQuery = db.Prepare (
			"INSERT INTO file_sets (Uuid, Name) VALUES (?, ?);");

		std::map<kyla::Path, std::int64_t> result;

		for (const auto& fileSet : fileSets) {
			fileSetsInsertQuery.BindArguments (
				fileSet.id, fileSet.name);

			fileSetsInsertQuery.Step ();
			fileSetsInsertQuery.Reset ();

			const auto fileSetId = db.GetLastRowId ();

			for (const auto& file : fileSet.files) {
				result [file.target] = fileSetId;
			}
		}

		fileSetsInsert.Commit ();

		return result;
	}

	void PopulateContentObjectsAndFiles (kyla::Sql::Database& db,
		const std::vector<UniqueContentObjects>& uniqueFiles,
		const std::map<kyla::Path, std::int64_t>& fileToFileSetId,
		const kyla::Path& contentObjectPath)
	{
		auto contentObjectInsert = db.BeginTransaction ();
		auto contentObjectInsertQuery = db.Prepare (
			"INSERT INTO content_objects (Hash, Size) VALUES (?, ?);");
		auto filesInsertQuery = db.Prepare (
			"INSERT INTO files (Path, ContentObjectId, FileSetId) VALUES (?, ?, ?);");

		// We can insert content objects directly - every unique file is one
		for (const auto& kv : uniqueFiles) {
			contentObjectInsertQuery.BindArguments (
				kv.hash,
				kv.size);
			contentObjectInsertQuery.Step ();
			contentObjectInsertQuery.Reset ();

			const auto contentObjectId = db.GetLastRowId ();

			for (const auto& reference : kv.duplicates) {
				const auto fileSetId = fileToFileSetId.find (reference)->second;

				filesInsertQuery.BindArguments (
					reference.string ().c_str (),
					contentObjectId,
					fileSetId);
				filesInsertQuery.Step ();
				filesInsertQuery.Reset ();
			}

			///@TODO(minor) Enable compression here
			// store the file itself
			boost::filesystem::copy_file (kv.sourceFile,
				contentObjectPath / ToString (kv.hash));
		}

		contentObjectInsert.Commit ();
	}
};
}

namespace kyla {
///////////////////////////////////////////////////////////////////////////////
void BuildRepository (const char* descriptorFile,
	const char* sourceDirectory, const char* targetDirectory)
{
	const auto inputFile = descriptorFile;

	BuildContext ctx;
	ctx.sourceDirectory = sourceDirectory;
	ctx.targetDirectory = targetDirectory;

	boost::filesystem::create_directories (ctx.targetDirectory);

	ctx.log.reset (new kyla::Log ("Generator", "build.log", kyla::LogLevel::Trace));

	pugi::xml_document doc;
	doc.load_file (inputFile);

	auto fileSets = GetFileSets (doc, ctx);

	HashFiles (fileSets, ctx);

	auto uniqueFiles = FindUniqueFileContents (fileSets, ctx);

	const auto packageTypeNode = doc.select_node ("//Package/Type");

	std::unique_ptr<IRepositoryBuilder> builder;

	if (packageTypeNode) {
		if (strcmp (packageTypeNode.node ().text ().as_string (), "Loose") == 0) {
			builder.reset (new LooseRepositoryBuilder);
		}
	}

	if (builder) {
		builder->Build (ctx, fileSets, uniqueFiles);
	}
}
}