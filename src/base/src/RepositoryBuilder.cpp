/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
[LICENSE END]
*/

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
#include "Exception.h"

#include "Compression.h"

namespace {
using namespace kyla;

struct BuildContext
{
	Path sourceDirectory;
	Path targetDirectory;
};

struct File
{
	Path source;
	Path target;

	SHA256Digest hash;
};

struct FileSet
{
	std::vector<File> files;

	std::string name;
	Uuid id;
};

///////////////////////////////////////////////////////////////////////////////
struct ContentObject
{
	Path sourceFile;
	SHA256Digest hash;
	std::size_t size;

	std::vector<Path> duplicates;
};

struct SourcePackage
{
	std::string name;

	std::vector<FileSet> fileSets;
	std::vector<ContentObject> contentObjects;
};

///////////////////////////////////////////////////////////////////////////////
std::unordered_map<std::string, SourcePackage> GetSourcePackages (const pugi::xml_document& doc,
	const BuildContext& ctx)
{
	std::unordered_map<std::string, SourcePackage> result;

	bool mainFound = false;

	std::unordered_set<std::string> sourcePackageIds;

	for (const auto& sourcePackageNode : doc.select_nodes ("//SourcePackage")) {
		SourcePackage sourcePackage;

		sourcePackage.name = sourcePackageNode.node ().attribute ("Name").as_string ();

		if (sourcePackageIds.find (sourcePackage.name) != sourcePackageIds.end ()) {
			throw RuntimeException ("Source package already existing",
				KYLA_FILE_LINE);
		} else {
			sourcePackageIds.insert (sourcePackage.name);
		}

		result [sourcePackageNode.node ().attribute ("Id").as_string ()]
			= sourcePackage;
	}

	if (sourcePackageIds.find ("main") == sourcePackageIds.end ()) {
		// Add the default (== main) package
		result ["main"] = SourcePackage{"main"};
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////////
void AssignFileSetsToPackages (const pugi::xml_document& doc,
	const BuildContext& ctx,
	std::unordered_map<std::string, SourcePackage>& sourcePackages)
{
	int filesFound = 0;

	for (const auto& fileSetNode : doc.select_nodes ("//FileSet")) {
		FileSet fileSet;

		fileSet.id = Uuid::Parse (fileSetNode.node ().attribute ("Id").as_string ());
		fileSet.name = fileSetNode.node ().attribute ("Name").as_string ();

		// Loose package doesn't use this, so don't link them there
		std::string sourcePackageId = "main";
		if (fileSetNode.node ().attribute ("SourcePackageId")) {
			sourcePackageId = fileSetNode.node ().attribute ("SourcePackageId").as_string ();
		}

		auto& package = sourcePackages.find (sourcePackageId)->second;
		
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

		package.fileSets.emplace_back (std::move (fileSet));
	}
}

///////////////////////////////////////////////////////////////////////////////
void HashFiles (std::vector<FileSet>& fileSets,
	const BuildContext& ctx)
{
	for (auto& fileSet : fileSets) {
		for (auto& file : fileSet.files) {
			file.hash = ComputeSHA256 (ctx.sourceDirectory / file.source);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
/**
Given a couple of file sets, we find unique files by hashing everything
and merging the results on the hash.
*/
std::vector<ContentObject> FindContentObjects (std::vector<FileSet>& fileSets,
	const BuildContext& ctx)
{
	std::unordered_map<SHA256Digest, std::vector<std::pair<Path, Path>>,
		HashDigestHash, HashDigestEqual> uniqueContents;

	for (const auto& fileSet : fileSets) {
		for (const auto& file : fileSet.files) {
			// This assumes the hashes are up-to-date, i.e. initialized
			uniqueContents [file.hash].push_back (std::make_pair (file.source, file.target));
		}
	}

	std::vector<ContentObject> result;
	result.reserve (uniqueContents.size ());

	for (const auto& kv : uniqueContents) {
		ContentObject uf;

		uf.hash = kv.first;
		uf.sourceFile = ctx.sourceDirectory / kv.second.front ().first;

		uf.size = Stat (uf.sourceFile.string ().c_str ()).size;

		for (const auto& sourceTargetPair : kv.second){
			uf.duplicates.push_back (sourceTargetPair.second);
		}

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
		const std::unordered_map<std::string, SourcePackage>& packages) = 0;
};

/**
A loose repository is little more than the files themselves, with hashes.
*/
struct LooseRepositoryBuilder final : public IRepositoryBuilder
{
	void Build (const BuildContext& ctx,
		const std::unordered_map<std::string, SourcePackage>& packages) override
	{
		boost::filesystem::create_directories (ctx.targetDirectory / ".ky");
		boost::filesystem::create_directories (ctx.targetDirectory / ".ky" / "objects");

		auto dbFile = ctx.targetDirectory / ".ky" / "repository.db";
		boost::filesystem::remove (dbFile);

		auto db = Sql::Database::Create (
			dbFile.string ().c_str ());

		db.Execute (install_db_structure);
		db.Execute ("PRAGMA journal_mode=WAL;");
		db.Execute ("PRAGMA synchronous=NORMAL;");

		for (const auto& sourcePackage : packages) {
			const auto fileToFileSetId = PopulateFileSets (db, sourcePackage.second.fileSets);
			PopulateContentObjectsAndFiles (db, sourcePackage.second.contentObjects, fileToFileSetId,
				ctx.targetDirectory / ".ky" / "objects");
		}
		db.Execute ("PRAGMA journal_mode=DELETE;");
		// Necessary to get good index statistics
		db.Execute ("ANALYZE");

		db.Close ();
	}

private:
	std::map<Path, std::int64_t> PopulateFileSets (Sql::Database& db,
		const std::vector<FileSet>& fileSets)
	{
		auto fileSetsInsert = db.BeginTransaction ();
		auto fileSetsInsertQuery = db.Prepare (
			"INSERT INTO file_sets (Uuid, Name) VALUES (?, ?);");

		std::map<Path, std::int64_t> result;

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

	void PopulateContentObjectsAndFiles (Sql::Database& db,
		const std::vector<ContentObject>& uniqueFiles,
		const std::map<Path, std::int64_t>& fileToFileSetId,
		const Path& contentObjectPath)
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

/**
Store all files into one or more source packages. A source package can be
compressed as well.
*/
struct PackedRepositoryBuilder final : public IRepositoryBuilder
{
	void Build (const BuildContext& ctx,
		const std::unordered_map<std::string, SourcePackage>& packages) override
	{
		auto dbFile = ctx.targetDirectory / "repository.db";
		boost::filesystem::remove (dbFile);

		auto db = Sql::Database::Create (
			dbFile.string ().c_str ());

		db.Execute (install_db_structure);
		db.Execute ("PRAGMA journal_mode=WAL;");
		db.Execute ("PRAGMA synchronous=NORMAL;");
		
		auto uniqueObjects = PopulateUniqueContentObjects (db, packages);

		for (const auto& sourcePackage : packages) {
			if (sourcePackage.second.contentObjects.empty ()) {
				continue;
			}

			const auto fileToFileSetId = PopulateFileSets (db,
				sourcePackage.second.fileSets); 
			
			WritePackage (db, sourcePackage.second,
				fileToFileSetId, uniqueObjects,
				ctx.targetDirectory);
		}

		db.Execute ("PRAGMA journal_mode=DELETE;");
		// Necessary to get good index statistics
		db.Execute ("ANALYZE");

		db.Close ();
	}

private:
	std::unordered_map<SHA256Digest, int64, HashDigestHash, HashDigestEqual> PopulateUniqueContentObjects (Sql::Database& db,
		const std::unordered_map<std::string, SourcePackage>& packages)
	{
		auto contentObjectInsertQuery = db.Prepare (
			"INSERT INTO content_objects (Hash, Size) VALUES (?, ?);");

		std::unordered_map<SHA256Digest, int64, HashDigestHash, HashDigestEqual> uniqueObjects;

		for (const auto& package : packages) {
			for (const auto& contentObject : package.second.contentObjects) {
				if (uniqueObjects.find (contentObject.hash) != uniqueObjects.end ()) {
					continue;
				}
				
				contentObjectInsertQuery.BindArguments (
					contentObject.hash,
					contentObject.size);
				contentObjectInsertQuery.Step ();
				contentObjectInsertQuery.Reset ();

				uniqueObjects [contentObject.hash] = db.GetLastRowId ();
			}
		}

		return uniqueObjects;
	}

	std::map<Path, std::int64_t> PopulateFileSets (Sql::Database& db,
		const std::vector<FileSet>& fileSets)
	{
		auto fileSetsInsert = db.BeginTransaction ();
		auto fileSetsInsertQuery = db.Prepare (
			"INSERT INTO file_sets (Uuid, Name) VALUES (?, ?);");

		std::map<Path, std::int64_t> result;

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

	// The file starts with a header followed by all content objects.
	// The database is stored separately
	struct PackageHeader
	{
		char id [8];
		std::uint64_t version;
		char reserved [48];

		static void Initialize (PackageHeader& header)
		{
			memset (&header, 0, sizeof (header));

			memcpy (header.id, "KYLAPKG", 8);
			header.version = 0x0001000000000000ULL;
		}
	};

	void WritePackage (Sql::Database& db,
		const SourcePackage& sourcePackage,
		const std::map<Path, int64>& fileToFileSetId,
		const std::unordered_map<SHA256Digest, int64, HashDigestHash, HashDigestEqual>& uniqueContentObjects,
		const Path& packagePath)
	{
		auto contentObjectInsert = db.BeginTransaction ();
		auto filesInsertQuery = db.Prepare (
			"INSERT INTO files (Path, ContentObjectId, FileSetId) VALUES (?, ?, ?);");
		auto packageInsertQuery = db.Prepare (
			"INSERT INTO source_packages (Name, Filename, Uuid) VALUES (?, ?, ?)");
		auto storageMappingInsertQuery = db.Prepare (
			"INSERT INTO storage_mapping "
			"(ContentObjectId, SourcePackageId, PackageOffset, PackageSize, SourceOffset, SourceSize, Compression) "
			"VALUES (?, ?, ?, ?, ?, ?, ?)");

		auto package = CreateFile (packagePath / (sourcePackage.name + ".kypkg"));

		PackageHeader packageHeader;
		PackageHeader::Initialize (packageHeader);

		package->Write (ArrayRef<PackageHeader> (packageHeader));

		packageInsertQuery.BindArguments (sourcePackage.name, (sourcePackage.name + ".kypkg"),
			Uuid::CreateRandom ());
		packageInsertQuery.Step ();
		packageInsertQuery.Reset ();

		const auto packageId = db.GetLastRowId ();

		// For now we support only a single package

		auto compressor = CreateBlockCompressor (CompressionAlgorithm::Zip);
		auto compressorId = IdFromCompressionAlgorithm (CompressionAlgorithm::Zip);

		std::vector<byte> compressionInputBuffer, compressionOutputBuffer;

		// We can insert content objects directly - every unique file is one
		for (const auto& kv : sourcePackage.contentObjects) {
			const auto contentObjectId = uniqueContentObjects.find (kv.hash)->second;

			for (const auto& reference : kv.duplicates) {
				const auto fileSetId = fileToFileSetId.find (reference)->second;

				filesInsertQuery.BindArguments (
					reference.string ().c_str (),
					contentObjectId,
					fileSetId);
				filesInsertQuery.Step ();
				filesInsertQuery.Reset ();
			}

			///@TODO(minor) Chunk the input file here based on uncompressed size
			///@TODO(minor) Ensure chunk size is < 2 GiB
			///@TODO(minor) Check this handles 0-byte files
			auto startOffset = package->Tell ();
			
			auto inputFile = OpenFile (kv.sourceFile, FileOpenMode::Read);
			
			// Read complete file, compress, write compressed file
			compressionInputBuffer.resize (inputFile->GetSize ());
			inputFile->Read (compressionInputBuffer);
			compressionOutputBuffer.resize (compressor->GetCompressionBound (inputFile->GetSize ()));
			auto compressedSize = compressor->Compress (compressionInputBuffer, compressionOutputBuffer);
			package->Write (ArrayRef<byte> (compressionOutputBuffer.data (), compressedSize));
			auto endOffset = package->Tell ();

			storageMappingInsertQuery.BindArguments (contentObjectId, packageId,
				startOffset, endOffset - startOffset, 0 /* offset inside the content object */,
				inputFile->GetSize () /* source size */,
				compressorId);
			storageMappingInsertQuery.Step ();
			storageMappingInsertQuery.Reset ();
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

	pugi::xml_document doc;
	if (!doc.load_file (inputFile)) {
		throw RuntimeException ("Could not parse input file.",
			KYLA_FILE_LINE);
	}

	auto sourcePackages = GetSourcePackages (doc, ctx);
	AssignFileSetsToPackages (doc, ctx, sourcePackages);

	for (auto& sourcePackage : sourcePackages) {
		HashFiles (sourcePackage.second.fileSets, ctx);
	}

	for (auto& sourcePackage : sourcePackages) {
		sourcePackage.second.contentObjects = FindContentObjects (
			sourcePackage.second.fileSets, ctx);
	}

	const auto packageTypeNode = doc.select_node ("//Package/Type");

	std::unique_ptr<IRepositoryBuilder> builder;

	if (packageTypeNode) {
		if (strcmp (packageTypeNode.node ().text ().as_string (), "Loose") == 0) {
			builder.reset (new LooseRepositoryBuilder);
		} else if (strcmp (packageTypeNode.node ().text ().as_string (), "Packed") == 0) {
			builder.reset (new PackedRepositoryBuilder);
		}
	}

	if (builder) {
		builder->Build (ctx, sourcePackages);
	} else {
		throw RuntimeException ("Package type not specified.",
			KYLA_FILE_LINE);
	}
}
}
