#include "SourcePackage.h"
#include "Hash.h"

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <iostream>
#include <fstream>
#include <string>

#include <pugixml.hpp>

#include <unordered_map>
#include <unordered_set>
#include <memory>

#include <zlib.h>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <assert.h>

#include <spdlog.h>

#include "build-db-structure.h"
#include "install-db-structure.h"
#include "install-db-indices.h"

#include "SourcePackageWriter.h"

#include "sql/Database.h"

#define SAFE_SQLITE_INTERNAL(expr, file, line) do { const int r_ = (expr); if (r_ != SQLITE_OK) { spdlog::get ("log")->error () << file << ":" << line << " " << sqlite3_errstr(r_); exit (1); } } while (0)
#define SAFE_SQLITE_INSERT_INTERNAL(expr, file, line) do { const int r_ = (expr); if (r_ != SQLITE_DONE) { spdlog::get ("log")->error () << file << ":" << line << " " << sqlite3_errstr(r_); exit (1); } } while (0)

#define SAFE_SQLITE(expr) SAFE_SQLITE_INTERNAL(expr, __FILE__, __LINE__)
#define SAFE_SQLITE_INSERT(expr) SAFE_SQLITE_INSERT_INTERNAL(expr, __FILE__, __LINE__)

std::unordered_set<std::string> GetUniqueSourcePaths (const pugi::xml_node& product)
{
	std::unordered_set<std::string> result;

	int inputFileCount = 0;
	for (const auto file : product.select_nodes ("//File")) {
		// TODO Also validate that these are actually file paths
		// TODO Validate that a source path is not empty
		// TODO Validate that the target path is not empty if present
		// TODO Validate that target paths don't collide

		result.insert (file.node ().attribute ("Source").value ());

		++inputFileCount;
	}

	spdlog::get ("log")->info () << "Processed " << inputFileCount
		<< " file entries, " << result.size () << " unique source paths found.";

	return result;
}

////////////////////////////////////////////////////////////////////////////////
void AssignFilesToFeaturesPackages (const pugi::xml_node& product,
	const std::unordered_map<std::string, std::int64_t>& sourcePackageIds,
	const std::unordered_map<std::string, std::int64_t>& featureIds,
	kyla::Sql::Database& buildDatabase)
{
	auto transaction = buildDatabase.BeginTransaction ();
	auto insertFilesStatement = buildDatabase.Prepare (
		"INSERT INTO files (SourcePath, TargetPath, FeatureId, SourcePackageId) VALUES (?, ?, ?, ?);");

	int inputFileCount = 0;

	for (const auto& feature : product.child ("Features").children ("Feature")) {
		std::int64_t fileFeatureId = -1;
		std::int64_t fileSourcePackageId = -1;

		fileFeatureId = featureIds.find (
				feature.attribute ("Id").value ())->second;

		insertFilesStatement.Bind (3, fileFeatureId);

		for (const auto& file : feature.children ("File")) {
			// Embed directly

			if (file.attribute ("SourcePackage")) {
				fileSourcePackageId = sourcePackageIds.find (
					file.attribute ("SourcePackage").value ())->second;
			}

			insertFilesStatement.Bind (1,
				file.attribute ("Source").value ());

			if (file.attribute ("Target")) {
				insertFilesStatement.Bind (2, file.attribute ("Target").value ());
			} else {
				insertFilesStatement.Bind (2, file.attribute ("Source").value ());
			}

			if (fileSourcePackageId == -1) {
				insertFilesStatement.Bind (4, kyla::Sql::Null ());
			} else {
				insertFilesStatement.Bind (4, fileSourcePackageId);
			}

			insertFilesStatement.Step ();
			insertFilesStatement.Reset ();

			++inputFileCount;
		}
	}

	spdlog::get ("log")->info () << "Found " << inputFileCount
		<< " files";

	transaction.Commit ();
}

struct ContentObjectIdHash
{
	std::int64_t id;
	kyla::Hash hash;
};

/**
For every unique source file, create the file chunks and update the build
database accordingly.

After this function has run:
  - Every file in the input set has been hash'ed, compressed and chunked
  - All chunks are stored in the temporary directory
  - The buildDatabase has been populated (chunks)
  - The installationDatabase has been populated (content_objects)

Returns a list of file-path to content object ids, as stored in the database
*/
std::unordered_map<std::string, ContentObjectIdHash> PrepareFiles (
	const std::unordered_set<std::string>& sourcePaths,
	const boost::filesystem::path& sourceDirectory,
	const boost::filesystem::path& temporaryDirectory,
	kyla::Sql::Database& buildDatabase,
	kyla::Sql::Database& installationDatabase,
	const std::int64_t fileChunkSize = 1 << 24 /* 16 MiB */)
{
	auto buildTransaction = buildDatabase.BeginTransaction();
	auto installationTransaction = installationDatabase.BeginTransaction();

	std::unordered_map<std::string, ContentObjectIdHash> pathToContentObject;

	// Read every unique source path in chunks, update hash, compress, compute
	// hash, write to temporary directory
	std::vector<unsigned char> buffer (fileChunkSize);
	std::vector<unsigned char> compressed (compressBound(fileChunkSize));

	// Prepare hash
	kyla::StreamHasher fileHasher;
	kyla::StreamHasher chunkHasher;

	boost::uuids::random_generator uuidGen;

	auto insertContentObjectStatement = installationDatabase.Prepare (
		"INSERT INTO content_objects (Size, Hash, ChunkCount) VALUES (?, ?, ?)");

	auto insertChunkStatement = buildDatabase.Prepare (
		"INSERT INTO chunks (ContentObjectId, Path, Size) VALUES (?, ?, ?)");

	auto selectContentObjectIdStatement = installationDatabase.Prepare (
		"SELECT Id FROM content_objects WHERE Hash=?");

	spdlog::get ("log")->info () << "Processing " << sourcePaths.size () << " files";

	std::int64_t totalSize = 0, totalCompressedSize = 0;

	// TODO This can be parallelized
	for (const auto sourcePath : sourcePaths) {
		const auto fullSourcePath = sourceDirectory / sourcePath;

		// TODO handle read errors
		// TODO handle empty files
		boost::filesystem::ifstream input (fullSourcePath, std::ios::binary);

		struct ChunkInfo
		{
			std::string name;
			std::int64_t size;
		};

		std::vector<ChunkInfo> chunks;

		fileHasher.Initialize ();
		int chunkNumber = 0;
		std::int64_t contentObjectSize = 0;
		std::int64_t contentObjectCompressedSize = 0;

		for (;;) {
			input.read (reinterpret_cast<char*> (buffer.data ()), fileChunkSize);
			const std::int64_t chunkSize = input.gcount ();
			contentObjectSize += chunkSize;

			fileHasher.Update (buffer.data (), chunkSize);

			uLongf compressedSize = compressed.size ();
			// TODO handle compression failure
			compress2 (reinterpret_cast<Bytef*> (compressed.data ()),
				&compressedSize,
				buffer.data (), chunkSize, Z_BEST_COMPRESSION);

			const auto chunkName = kyla::ToString (uuidGen ().data);

			const ChunkInfo chunkInfo {chunkName,
				static_cast<std::int64_t> (compressedSize)};
			chunks.push_back (chunkInfo);

			kyla::SourcePackageChunk pdc;
			::memset (&pdc, 0, sizeof (pdc));

			pdc.compressedSize = compressedSize;
			pdc.size = chunkSize;
			pdc.offset = fileChunkSize * chunkNumber;
			pdc.compressionMode = kyla::CompressionMode::Zip;

			contentObjectCompressedSize += compressedSize;

			auto chunkHash = kyla::ComputeHash (compressed.data (), compressedSize);
			::memcpy (pdc.hash, chunkHash.hash, sizeof (pdc.hash));

			boost::filesystem::ofstream chunkOutput (
				temporaryDirectory / chunkName, std::ios::binary);
			chunkOutput.write (reinterpret_cast<const char*> (&pdc), sizeof (pdc));
			chunkOutput.write (reinterpret_cast<char*> (compressed.data ()),
				compressedSize);

			spdlog::get ("log")->trace () << "Wrote chunk '" << chunkName
				<< "' for file '" << sourcePath << "' (uncompressed: "
				<< chunkSize << ", compressed: " << compressedSize << ")";

			++chunkNumber;

			// Compress data
			if (input.gcount () < fileChunkSize) {
				break;
			}
		}

		totalSize += contentObjectSize;

		if (contentObjectSize == 0) {
			// Don't create a content object when the file is empty
			continue;
		}

		const auto fileHash = fileHasher.Finalize ();

		spdlog::get ("log")->debug() << sourcePath << " -> " << ToString (fileHash);

		selectContentObjectIdStatement.Bind (1, sizeof (fileHash.hash),
			fileHash.hash);

		if (! selectContentObjectIdStatement.Step ()) {
			insertContentObjectStatement.Bind (1, contentObjectSize);
			insertContentObjectStatement.Bind (2, sizeof (fileHash.hash),
				fileHash.hash);
			insertContentObjectStatement.Bind (3, chunkNumber);
			insertContentObjectStatement.Step ();
			insertContentObjectStatement.Reset ();

			const auto contentObjectId = installationDatabase.GetLastRowId ();

			pathToContentObject [sourcePath] = {contentObjectId, fileHash};

			for (const auto chunk : chunks) {
				insertChunkStatement.Bind (1, contentObjectId);
				insertChunkStatement.Bind (2,
					absolute (temporaryDirectory / chunk.name).c_str ());
				insertChunkStatement.Bind (3, chunk.size);
				insertChunkStatement.Step ();
				insertChunkStatement.Reset ();
			}

			totalCompressedSize += contentObjectCompressedSize;
		} else {
			// We have two files with the same hash
			pathToContentObject [sourcePath] = {
				selectContentObjectIdStatement.GetInt64 (0),
				fileHash};
		}

		selectContentObjectIdStatement.Reset ();
	}

	buildTransaction.Commit ();
	installationTransaction.Commit();

	auto selectContentObjectCountStatement = installationDatabase.Prepare (
		"SELECT COUNT() FROM content_objects;");
	selectContentObjectCountStatement.Step ();

	const auto contentObjectCount = selectContentObjectCountStatement.GetInt64 (0);

	spdlog::get ("log")->info () << "Created " << contentObjectCount
								 << " content objects";
	spdlog::get ("log")->info () << "Compressed from " << totalSize
		<< " bytes to " << totalCompressedSize << " bytes ("
		<< static_cast<double> (totalCompressedSize) / totalSize * 100.0 << "%)";

	return pathToContentObject;
}

////////////////////////////////////////////////////////////////////////////////
void InsertStorageMappingEntries (kyla::Sql::Database& installationDatabase,
	const std::int64_t sourcePackageId,
	const std::unordered_set<std::int64_t>& contentObjectIds)
{
	auto insertIntoStorageMappingStatement = installationDatabase.Prepare (
		"INSERT INTO storage_mapping (ContentObjectId, SourcePackageId) VALUES (?, ?)");

	for (const auto contentObjectId : contentObjectIds) {
		insertIntoStorageMappingStatement.BindArguments (
			contentObjectId, sourcePackageId);
		insertIntoStorageMappingStatement.Step ();
		insertIntoStorageMappingStatement.Reset();
	}
}

////////////////////////////////////////////////////////////////////////////////
struct SourcePackageNameTemplate
{
public:
	SourcePackageNameTemplate (const std::string& str)
	{
		auto start = str.find ("{id}");
		prefix_ = str.substr (0, start);
		suffix_ = str.substr (start + 4);
	}

	std::string operator () (const std::int64_t i) const
	{
		return prefix_ + std::to_string (i) + suffix_ + ".kypkg";
	}

private:
	std::string prefix_;
	std::string suffix_;
};

////////////////////////////////////////////////////////////////////////////////
SourcePackageNameTemplate GetSourcePackageNameTemplate (const pugi::xml_node& productNode)
{
	const char* packageNameTemplate = "pack{id}";

	if (productNode.child ("SourcePackages")) {
		auto sourcePackagesNode = productNode.child ("SourcePackages");

		if (sourcePackagesNode.attribute ("NameTemplate")) {
			packageNameTemplate = sourcePackagesNode.attribute ("NameTemplate").value ();
		}
	}

	return SourcePackageNameTemplate (packageNameTemplate);
}

struct SourcePackageInfo
{
	std::int64_t	id;
	kyla::Hash		hash;
};

////////////////////////////////////////////////////////////////////////////////
std::vector<SourcePackageInfo> WriteUserPackages (
	kyla::Sql::Database& buildDatabase,
	kyla::Sql::Database& installationDatabase,
	const std::unordered_map<std::string, ContentObjectIdHash>& pathToContentObject,
	const SourcePackageNameTemplate& packageNameTemplate,
	const boost::filesystem::path& targetDirectory)
{
	std::vector<SourcePackageInfo> result;

	auto insertIntoFilesStatement = installationDatabase.Prepare (
		"INSERT INTO files (Path, ContentObjectId, FeatureId) VALUES (?, ?, ?);");

	// First, we assemble all user-requested source packages
	auto selectSourcePackageIdsStatement = installationDatabase.Prepare (
		"SELECT Id, Name, Uuid FROM source_packages;");

	while (selectSourcePackageIdsStatement.Step ()) {
		const std::int64_t sourcePackageId =
			selectSourcePackageIdsStatement.GetInt64 (0);

		spdlog::get ("log")->trace () << "Processing source package " << sourcePackageId;

		// Find all files in this package, and then all chunks
		auto selectFilesForPackageStatement = buildDatabase.Prepare (
			"SELECT SourcePath, TargetPath, FeatureId FROM files WHERE SourcePackageId=?;");
		selectFilesForPackageStatement.Bind (1, sourcePackageId);

		// contentObject (Hash) -> list of chunks that will go into this package
		std::unordered_map<kyla::Hash, std::vector<std::string>, kyla::HashHash, kyla::HashEqual>
				contentObjectsAndChunksInPackage;

		// We need to handle duplicates in case multiple files reference the
		// same content object, so we use a set here instead of a vector
		std::unordered_set<std::int64_t> contentObjectsInPackage;

		// For each file that goes into this package
		while (selectFilesForPackageStatement.Step ()) {
			const std::string sourcePath = selectFilesForPackageStatement.GetText (0);
			std::int64_t contentObjectId = -1;

			// May be NULL in case it's an empty file
			if (pathToContentObject.find (sourcePath) != pathToContentObject.end ()) {
				const auto contentObjectIdHash = pathToContentObject.find (sourcePath)->second;

				contentObjectId = contentObjectIdHash.id;

				// If this content object has already been added, don't readd
				// twice. This may happen if several files reference the same,
				// non-zero content object
				if (contentObjectsInPackage.find (contentObjectId) == contentObjectsInPackage.end ()) {
					spdlog::get ("log")->trace () << "'" << sourcePath
						<< "' references " << contentObjectId;

					// Write the chunks into this package
					// Order by rowid, so we get a sequential write into the output
					// file
					auto selectChunksForContentObject = buildDatabase.Prepare (
						"SELECT Path FROM chunks WHERE ContentObjectId=? ORDER BY rowid ASC");
					selectChunksForContentObject.Bind (1, contentObjectId);

					std::vector<std::string> chunksForContentObject;

					while (selectChunksForContentObject.Step ()) {
						chunksForContentObject.emplace_back (
							selectChunksForContentObject.GetText (0));
					}

					selectChunksForContentObject.Reset ();

					contentObjectsAndChunksInPackage [contentObjectIdHash.hash]
						= std::move (chunksForContentObject);
					contentObjectsInPackage.insert (contentObjectIdHash.id);
				} else {
					spdlog::get ("log")->trace () << "'" << sourcePath
						<< "' references " << contentObjectId
						<< " (already in package, skipped)";
				}
			}

			// We can now write the file entry
			insertIntoFilesStatement.Bind (1,
				selectFilesForPackageStatement.GetText (1));

			if (contentObjectId != -1) {
				insertIntoFilesStatement.Bind (2, contentObjectId);
			} else {
				insertIntoFilesStatement.Bind (2, kyla::Sql::Null ());
			}

			insertIntoFilesStatement.Bind (3,
				selectFilesForPackageStatement.GetInt64 (2));
			insertIntoFilesStatement.Step ();
			insertIntoFilesStatement.Reset ();
		}

		selectFilesForPackageStatement.Reset ();

		InsertStorageMappingEntries (installationDatabase,
			sourcePackageId, contentObjectsInPackage);

		// Write the package
		const auto packageUuid = selectSourcePackageIdsStatement.GetBlob (2);
		kyla::SourcePackageWriter spw;
		spw.Open (targetDirectory / (packageNameTemplate (sourcePackageId)),
			packageUuid);

		for (const auto kv : contentObjectsAndChunksInPackage) {
			for (const auto chunk : kv.second) {
				spw.Add (kv.first, chunk);
			}
		}

		const auto sourcePackageHash = spw.Finalize ();

		const SourcePackageInfo sourcePackageInfo = {
			sourcePackageId,
			sourcePackageHash
		};

		result.push_back(sourcePackageInfo);
		spdlog::get ("log")->info () << "Created package "
			<< selectSourcePackageIdsStatement.GetText (1);
	}

	return result;
}

////////////////////////////////////////////////////////////////////////////////
std::int64_t CreateGeneratedPackage (kyla::Sql::Database& installationDatabase,
	boost::uuids::uuid& packageUuid)
{
	auto insertSourcePackageStatement = installationDatabase.Prepare (
		"INSERT INTO source_packages (Name, Filename, Uuid, Hash) VALUES (?, ?, ?, ?);");

	const std::string packageName = std::string ("Generated_")
		+ kyla::ToString (packageUuid.data);

	insertSourcePackageStatement.Bind (1, packageName.c_str ());

	// Dummy filename and hash, will be fixed up later
	insertSourcePackageStatement.Bind (2,
		packageName.c_str ());

	insertSourcePackageStatement.Bind (3, 16, packageUuid.data);
	insertSourcePackageStatement.Bind (4, packageName.size (),
		packageName.c_str ());

	insertSourcePackageStatement.Step ();

	const auto currentPackageId = installationDatabase.GetLastRowId ();

	insertSourcePackageStatement.Reset ();

	spdlog::get ("log")->info () << "Created package " << packageName;

	return currentPackageId;
}

////////////////////////////////////////////////////////////////////////////////
std::vector<SourcePackageInfo> WriteGeneratedPackages (
	kyla::Sql::Database& buildDatabase,
	kyla::Sql::Database& installationDatabase,
	const std::unordered_map<std::string, ContentObjectIdHash>& pathToContentObject,
	const boost::filesystem::path& targetDirectory,
	const SourcePackageNameTemplate& packageNameTemplate,
	const std::int64_t targetPackageSize)
{
	std::vector<SourcePackageInfo> result;

	auto insertIntoFilesStatement = installationDatabase.Prepare (
		"INSERT INTO files (Path, ContentObjectId, FeatureId) VALUES (?, ?, ?);");

	// Here we handle all files sent to the "default" package
	// This is very similar to the loop above, but we will create packages on
	// demand once the package size becomes too large
	auto selectFilesForDefaultPackageStatement = buildDatabase.Prepare (
		"SELECT SourcePath, TargetPath, FeatureId FROM files WHERE SourcePackageId IS NULL;");

	// Order by rowid, so we get a sequential write into the output
	// file
	auto selectChunksForContentObject = buildDatabase.Prepare(
		"SELECT Path, Size FROM chunks WHERE ContentObjectId=? ORDER BY rowid ASC");

	boost::uuids::random_generator uuidGen;

	kyla::SourcePackageWriter spw;
	std::int64_t dataInCurrentPackage = 0;
	std::int64_t currentPackageId = -1;

	// Same reasong as in WriteUserPackages, we need to handle files which
	// reference the same content object
	std::unordered_set<std::int64_t> contentObjectsInCurrentPackage;
	while (selectFilesForDefaultPackageStatement.Step ()) {
		const std::string sourcePath =
			selectFilesForDefaultPackageStatement.GetText (0);

		std::int64_t contentObjectId = -1;
		// May be NULL in case it's an empty file
		if (pathToContentObject.find (sourcePath) != pathToContentObject.end ()) {
			const auto contentObjectIdHash = pathToContentObject.find (sourcePath)->second;
			contentObjectId = contentObjectIdHash.id;

			if (contentObjectsInCurrentPackage.find (contentObjectId) == contentObjectsInCurrentPackage.end ()) {
				// We have found a new content object
				spdlog::get ("log")->trace () << "'" << sourcePath << "' references " << contentObjectId;

				selectChunksForContentObject.Bind (1, contentObjectId);

				while (selectChunksForContentObject.Step ()) {
					// Open current package if needed, start inserting
					if (! spw.IsOpen ()) {
						// Add a new source package
						auto packageId = uuidGen ();
						currentPackageId = CreateGeneratedPackage (
							installationDatabase, packageId);

						spw.Open (targetDirectory / packageNameTemplate (currentPackageId),
							packageId.data);
					}

					// Insert the chunk right away, increment current size, if
					// package is full, finalize, and reset size
					const std::string chunkPath = selectChunksForContentObject.GetText (0);
					spw.Add (contentObjectIdHash.hash, chunkPath);

					dataInCurrentPackage +=
						selectChunksForContentObject.GetInt64 (1);
					contentObjectsInCurrentPackage.insert (contentObjectIdHash.id);

					if (dataInCurrentPackage >= targetPackageSize) {
						dataInCurrentPackage = 0;

						InsertStorageMappingEntries (installationDatabase,
							currentPackageId, contentObjectsInCurrentPackage);
						contentObjectsInCurrentPackage.clear ();

						const auto sourcePackageHash = spw.Finalize ();
						const SourcePackageInfo sourcePackageInfo = {
							currentPackageId, sourcePackageHash};
						result.push_back (sourcePackageInfo);
					}
				}

				selectChunksForContentObject.Reset ();
			} else {
				spdlog::get ("log")->trace () << "'" << sourcePath
					<< "' references " << contentObjectId
					<< " (already in package)";
			}
		}

		// We can now write the file entry
		insertIntoFilesStatement.Bind (1,
			selectFilesForDefaultPackageStatement.GetText (1));

		if (contentObjectId != -1) {
			insertIntoFilesStatement.Bind (2, contentObjectId);
		} else {
			insertIntoFilesStatement.Bind (2, kyla::Sql::Null ());
		}

		insertIntoFilesStatement.Bind (3,
			selectFilesForDefaultPackageStatement.GetInt64 (2));
		insertIntoFilesStatement.Step ();
		insertIntoFilesStatement.Reset ();
	}

	// Flush if needed
	if (dataInCurrentPackage > 0) {
		InsertStorageMappingEntries (installationDatabase,
			currentPackageId, contentObjectsInCurrentPackage);

		const auto sourcePackageHash = spw.Finalize ();
		const SourcePackageInfo sourcePackageInfo = {
			currentPackageId, sourcePackageHash};
		result.push_back (sourcePackageInfo);
	}

	return result;
}

////////////////////////////////////////////////////////////////////////////////
std::vector<SourcePackageInfo> WritePackages (
	kyla::Sql::Database& buildDatabase,
	kyla::Sql::Database& installationDatabase,
	const boost::filesystem::path& targetDirectory,
	const pugi::xml_node& productNode,
	const std::unordered_map<std::string, ContentObjectIdHash> pathToContentObject,
	const std::int64_t targetPackageSize = 1ll << 30 /* 1 GiB */)
{
	auto transaction = installationDatabase.BeginTransaction();

	std::vector<SourcePackageInfo> result;

	spdlog::get ("log")->trace () << "Writing packages";
	const auto packageNameTemplate = GetSourcePackageNameTemplate (productNode);

	const auto userPackages = WriteUserPackages(buildDatabase,
		installationDatabase, pathToContentObject,
		packageNameTemplate, targetDirectory);

	result.insert (result.end (), userPackages.begin (), userPackages.end ());

	const auto defaultPackages = WriteGeneratedPackages (buildDatabase,
		installationDatabase, pathToContentObject, targetDirectory,
		packageNameTemplate, targetPackageSize);

	result.insert (result.end (),
		defaultPackages.begin (), defaultPackages.end ());

	transaction.Commit ();

	return result;
}

struct GeneratorContext
{
public:
	GeneratorContext ()
	{
		installationDatabase = kyla::Sql::Database::Create ();

		installationDatabase.Execute (install_db_structure);
	}

	void SetupBuildDatabase (const boost::filesystem::path& temporaryDirectory)
	{
		spdlog::get ("log")->trace () << "Build database: '" << (temporaryDirectory / "build.sqlite").c_str () << "'";
		buildDatabase = kyla::Sql::Database::Create (
					(temporaryDirectory / "build.sqlite").c_str ());
		buildDatabase.Execute (build_db_structure);
	}

	~GeneratorContext ()
	{
	}

	void WriteInstallationDatabase (const boost::filesystem::path& outputFile) const
	{
		installationDatabase.SaveCopyTo (outputFile.c_str ());

		spdlog::get ("log")->info () << "Wrote installation database "
			<< outputFile.string ();
	}

	pugi::xml_node	productNode;
	kyla::Sql::Database		installationDatabase;
	kyla::Sql::Database		buildDatabase;
	boost::filesystem::path sourceDirectory;
	boost::filesystem::path	temporaryDirectory;
	boost::filesystem::path targetDirectory;
	boost::uuids::random_generator uuidGenerator;
};

/*
Returns a mapping of feature id string to the feature id in the database.
*/
std::unordered_map<std::string, std::int64_t> CreateFeatures (GeneratorContext& gc)
{
	auto transaction = gc.installationDatabase.BeginTransaction ();

	// TODO Validate that there is a feature element
	// TODO Validate that at least one feature is present
	// TODO Validate the feature ID is not empty
	// TODO Validate all feature IDs are unique
	spdlog::get ("log")->trace () << "Populating feature table";

	std::unordered_map<std::string, std::int64_t> result;

	auto insertFeatureStatement = gc.installationDatabase.Prepare (
		"INSERT INTO features (Name, UIName, UIDescription) VALUES (?, ?, ?)");

	int featureCount = 0;
	for (const auto feature : gc.productNode.child ("Features").children ()) {
		const auto featureId = feature.attribute ("Id").value ();
		insertFeatureStatement.Bind (1, featureId);

		const auto featureName = feature.attribute ("Name").value ();
		insertFeatureStatement.Bind (2, featureName);

		if (feature.attribute ("Description")) {
			insertFeatureStatement.Bind (3,
				feature.attribute ("Description").value ());
		} else {
			insertFeatureStatement.Bind (3, kyla::Sql::Null ());
		}

		insertFeatureStatement.Step ();
		insertFeatureStatement.Reset ();

		const auto lastRowId = gc.installationDatabase.GetLastRowId ();
		result [featureId] = lastRowId;

		spdlog::get ("log")->trace () << "Feature '" << featureId
			<< "' assigned to Id " << lastRowId;
		++featureCount;
	}

	spdlog::get ("log")->info () << "Created " << featureCount << " feature(s)";

	transaction.Commit ();

	return result;
}

/*
Returns a mapping of source package id string to the source package id in the database.
*/
std::unordered_map<std::string, std::int64_t> CreateSourcePackages (GeneratorContext& gc,
	boost::uuids::random_generator& uuidGen)
{
	auto transaction = gc.installationDatabase.BeginTransaction();

	// TODO Validate the source package ID is not empty
	// TODO Validate all source package IDs are unique
	spdlog::get ("log")->trace () << "Populating source package table";

	std::unordered_map<std::string, std::int64_t> result;

	auto insertSourcePackageStatement = gc.installationDatabase.Prepare (
		"INSERT INTO source_packages (Name, Filename, Uuid, Hash) VALUES (?, ?, ?, ?)");

	int sourcePackageCount = 0;
	for (const auto sourcePackage : gc.productNode.child ("SourcePackages").children ()) {
		const auto sourcePackageId = sourcePackage.attribute ("Id").value ();
		insertSourcePackageStatement.Bind (1, sourcePackageId);

		const auto packageUuid = uuidGen ();
		insertSourcePackageStatement.Bind (2,
			kyla::ToString (packageUuid.data).c_str ());
		insertSourcePackageStatement.Bind (3, packageUuid.size (),
			packageUuid.data);
		insertSourcePackageStatement.Bind (4, packageUuid.size (),
			packageUuid.data);

		insertSourcePackageStatement.Step ();
		insertSourcePackageStatement.Reset ();

		const auto lastRowId = gc.installationDatabase.GetLastRowId ();
		result [sourcePackageId] = lastRowId;

		spdlog::get ("log")->trace () << "Source package '" << sourcePackageId
			<< "' assigned to Id " << lastRowId;
		++sourcePackageCount;
	}

	spdlog::get ("log")->info () << "Created " << sourcePackageCount << " source package(s)";

	transaction.Commit ();
	return result;
}

/*
Write global properties
*/
void WriteProperties (kyla::Sql::Database& installationDatabase,
	const pugi::xml_node& productNode)
{
	auto transaction = installationDatabase.BeginTransaction ();

	spdlog::get ("log")->trace () << "Populating properties table";

	auto insertPropertiesStatement = installationDatabase.Prepare (
		"INSERT INTO properties (Name, Value) VALUES (?, ?)");

	insertPropertiesStatement.BindArguments (
		"ProductName", productNode.attribute ("Name").value ());

	insertPropertiesStatement.Step ();
	insertPropertiesStatement.Reset ();

	insertPropertiesStatement.BindArguments ("ProductVersion",
		productNode.attribute ("Version").value ());

	insertPropertiesStatement.Step ();
	insertPropertiesStatement.Reset ();

	transaction.Commit ();
}

////////////////////////////////////////////////////////////////////////////////
void FinalizeSourcePackageNames (const SourcePackageNameTemplate& nameTemplate,
	const std::vector<SourcePackageInfo>& infos,
	kyla::Sql::Database& installationDatabase)
{
	auto transaction = installationDatabase.BeginTransaction ();

	auto updateSourcePackageFilenameStatement = installationDatabase.Prepare(
		"UPDATE source_packages SET Filename=?,Hash=? WHERE Id=?;");

	for (const auto& info : infos) {
		const auto sourcePackageId = info.id;

		updateSourcePackageFilenameStatement.Bind (3, sourcePackageId);
		const auto packageName = nameTemplate (sourcePackageId);
		spdlog::get ("log")->debug () << "Package "
			<< sourcePackageId << " stored in file " << packageName;

		updateSourcePackageFilenameStatement.Bind (1, packageName.c_str ());
		updateSourcePackageFilenameStatement.Bind (2, sizeof (info.hash.hash),
			info.hash.hash);

		updateSourcePackageFilenameStatement.Step ();
		updateSourcePackageFilenameStatement.Reset ();
	}

	transaction.Commit ();
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
		("target-directory", po::value<std::string> ()->default_value ("."))
		("temp-directory", po::value<std::string> ()->default_value (
			 std::string ("kytmp-") + boost::filesystem::unique_path ().string ()))
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

	auto log = spdlog::stdout_logger_mt ("log");
	log->set_level (spdlog::level::trace);

	const auto inputFile = vm ["input-file"].as<std::string> ();

	pugi::xml_document doc;
	doc.load_file (inputFile.c_str ());

	boost::filesystem::path sourcePath (inputFile);
	const auto outputFile = sourcePath.stem ();

	GeneratorContext gc;
	gc.productNode = doc.document_element ().child ("Product");
	gc.sourceDirectory = absolute (boost::filesystem::path (
		vm ["source-directory"].as<std::string> ()));
	gc.temporaryDirectory = absolute (boost::filesystem::path (
		vm ["temp-directory"].as<std::string> ()));
	gc.targetDirectory = absolute (boost::filesystem::path (
		vm ["target-directory"].as<std::string> ()));

	boost::filesystem::create_directories (gc.temporaryDirectory);
	log->debug () << "Temporary directory: " << gc.temporaryDirectory.string ();
	log->debug () << "Target directory: " << gc.targetDirectory.string ();

	gc.SetupBuildDatabase (gc.temporaryDirectory);

	const auto featureIds = CreateFeatures (gc);
	const auto sourcePackageIds = CreateSourcePackages (gc, gc.uuidGenerator);

	AssignFilesToFeaturesPackages (gc.productNode, sourcePackageIds,
		featureIds, gc.buildDatabase);

	const auto uniqueFiles = GetUniqueSourcePaths (gc.productNode);
	auto pathToContentObject = PrepareFiles (uniqueFiles,
		gc.sourceDirectory, gc.temporaryDirectory,
		gc.buildDatabase, gc.installationDatabase);

	const auto packageInfos = WritePackages (gc.buildDatabase,
		gc.installationDatabase,
		gc.targetDirectory, gc.productNode, pathToContentObject);

	FinalizeSourcePackageNames (
		GetSourcePackageNameTemplate (gc.productNode),
		packageInfos,
		gc.installationDatabase);

	WriteProperties (gc.installationDatabase, gc.productNode);

	// Create indices
	gc.installationDatabase.Execute (
		install_db_indices);

	// Set the reference counts
	gc.installationDatabase.Execute (
		"UPDATE content_objects SET ReferenceCount = (SELECT COUNT() FROM files WHERE files.ContentObjectId=content_objects.Id);");

	// Help the query optimizer
	gc.installationDatabase.Execute (
		"ANALYZE;");

	gc.WriteInstallationDatabase (gc.targetDirectory / (outputFile.string () + ".kydb"));

	// Assemble the install package
	// Add the db compressed
	// Add all source packages (uncompressed)

	boost::filesystem::remove_all (gc.temporaryDirectory);
}
