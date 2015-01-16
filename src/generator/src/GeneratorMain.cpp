#include "SourcePackage.h"
#include "Hash.h"

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <iostream>
#include <fstream>
#include <string>

#include <pugixml.hpp>
#include <sqlite3.h>

#include <unordered_map>
#include <unordered_set>
#include <memory>

#include <boost/log/trivial.hpp>
#include <openssl/evp.h>
#include <zlib.h>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <assert.h>

#include "build-db-structure.h"
#include "install-db-structure.h"

#define SAFE_SQLITE_INTERNAL(expr, file, line) do { const int r_ = (expr); if (r_ != SQLITE_OK) { BOOST_LOG_TRIVIAL(error) << file << ":" << line << " " << sqlite3_errstr(r_); exit (1); } } while (0)
#define SAFE_SQLITE_INSERT_INTERNAL(expr, file, line) do { const int r_ = (expr); if (r_ != SQLITE_DONE) { BOOST_LOG_TRIVIAL(error) << file << ":" << line << " " << sqlite3_errstr(r_); exit (1); } } while (0)

#define SAFE_SQLITE(expr) SAFE_SQLITE_INTERNAL(expr, __FILE__, __LINE__)
#define SAFE_SQLITE_INSERT(expr) SAFE_SQLITE_INSERT_INTERNAL(expr, __FILE__, __LINE__)

std::unordered_set<std::string> GetUniqueSourcePaths (const pugi::xml_node& product)
{
	std::unordered_set<std::string> result;

	int inputFileCount = 0;
	for (auto files : product.children ("Files")) {
		for (auto file : files.children ("File")) {
			// TODO Also validate that these are actually file paths
			// TODO Validate that a source path is not empty
			// TODO Validate that the target path is not empty if present
			// TODO Validate that target paths don't collide

			result.insert (file.attribute ("Source").value ());

			++inputFileCount;
		}
	}

	BOOST_LOG_TRIVIAL(info) << "Processed " << inputFileCount
		<< " source paths, " << result.size () << " unique paths found.";

	return result;
}

////////////////////////////////////////////////////////////////////////////////
void AssignFilesToFeaturesPackages (const pugi::xml_node& product,
	const std::unordered_map<std::string, std::int64_t>& sourcePackageIds,
	const std::unordered_map<std::string, std::int64_t>& featureIds,
	sqlite3* buildDatabase)
{
	sqlite3_exec (buildDatabase, "BEGIN TRANSACTION;",
		nullptr, nullptr, nullptr);
	sqlite3_stmt* insertFilesStatement;
	SAFE_SQLITE (sqlite3_prepare_v2 (buildDatabase,
		"INSERT INTO files (SourcePath, TargetPath, FeatureId, SourcePackageId) VALUES (?, ?, ?, ?);",
		-1, &insertFilesStatement, nullptr));

	int inputFileCount = 0;
	for (auto files : product.children ("Files")) {
		std::int64_t featureId = -1;
		std::int64_t sourcePackageId = -1;

		if (files.attribute ("SourcePackage")) {
			// TODO Validate it's present
			sourcePackageId = sourcePackageIds.find (
				files.attribute ("SourcePackage").value ())->second;
		}

		if (files.attribute ("Feature")) {
			featureId = featureIds.find (
				files.attribute ("Feature").value ())->second;
		}

		for (auto file : files.children ("File")) {
			// TODO Validate Source= is present

			std::int64_t fileFeatureId = featureId;
			std::int64_t fileSourcePackageId = sourcePackageId;

			if (file.attribute ("SourcePackage")) {
				// TODO Validate it's present
				fileSourcePackageId = sourcePackageIds.find (
					file.attribute ("SourcePackage").value ())->second;
			}

			if (file.attribute ("Feature")) {
				fileFeatureId = featureIds.find (
					file.attribute ("Feature").value ())->second;
			}

			SAFE_SQLITE (sqlite3_bind_text (insertFilesStatement, 1,
				file.attribute ("Source").value (), -1, SQLITE_TRANSIENT));

			if (file.attribute ("Target")) {
				SAFE_SQLITE (sqlite3_bind_text (insertFilesStatement, 2,
					file.attribute ("Target").value (), -1, SQLITE_TRANSIENT));
			} else {
				SAFE_SQLITE (sqlite3_bind_text (insertFilesStatement, 2,
					file.attribute ("Source").value (), -1, SQLITE_TRANSIENT));
			}

			SAFE_SQLITE (sqlite3_bind_int64 (insertFilesStatement, 3, fileFeatureId));

			if (fileSourcePackageId != -1) {
				SAFE_SQLITE (sqlite3_bind_int64 (insertFilesStatement, 4, fileSourcePackageId));
			} else {
				SAFE_SQLITE (sqlite3_bind_null (insertFilesStatement, 4));
			}

			SAFE_SQLITE_INSERT (sqlite3_step (insertFilesStatement));
			SAFE_SQLITE (sqlite3_reset (insertFilesStatement));

			++inputFileCount;
		}
	}

	SAFE_SQLITE (sqlite3_finalize (insertFilesStatement));

	BOOST_LOG_TRIVIAL(info) << "Processed " << inputFileCount
		<< " files";

	sqlite3_exec (buildDatabase, "COMMIT TRANSACTION;",
		nullptr, nullptr, nullptr);
}

struct ContentObjectIdHash
{
	std::int64_t id;
	Hash hash;
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
	sqlite3* buildDatabase,
	sqlite3* installationDatabase,
	const std::int64_t fileChunkSize = 1 << 24 /* 16 MiB */)
{
	sqlite3_exec (buildDatabase, "BEGIN TRANSACTION;",
		nullptr, nullptr, nullptr);
	sqlite3_exec (installationDatabase, "BEGIN TRANSACTION;",
		nullptr, nullptr, nullptr);

	std::unordered_map<std::string, ContentObjectIdHash> pathToContentObject;

	// Read every unique source path in chunks, update hash, compress, compute
	// hash, write to temporary directory
	std::vector<unsigned char> buffer (fileChunkSize);
	std::vector<unsigned char> compressed (compressBound(fileChunkSize));

	// Prepare hash
	EVP_MD_CTX* fileCtx = EVP_MD_CTX_create ();
	EVP_MD_CTX* chunkCtx = EVP_MD_CTX_create ();

	boost::uuids::random_generator uuidGen;

	sqlite3_stmt* insertContentObjectStatement;
	sqlite3_prepare_v2 (installationDatabase,
		"INSERT INTO content_objects (Size, Hash, ChunkCount) VALUES (?, ?, ?)",
		-1, &insertContentObjectStatement, nullptr);

	sqlite3_stmt* insertChunkStatement;
	sqlite3_prepare_v2 (buildDatabase,
		"INSERT INTO chunks (ContentObjectId, Path, Size) VALUES (?, ?, ?)",
		-1, &insertChunkStatement, nullptr);

	sqlite3_stmt* selectContentObjectIdStatement = nullptr;
	SAFE_SQLITE (sqlite3_prepare_v2 (installationDatabase,
		"SELECT Id FROM content_objects WHERE Hash=?", -1,
		&selectContentObjectIdStatement, nullptr));

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

		EVP_DigestInit_ex (fileCtx, EVP_sha512 (), nullptr);
		int chunkNumber = 0;
		std::int64_t contentObjectSize = 0;

		for (;;) {
			input.read (reinterpret_cast<char*> (buffer.data ()), fileChunkSize);
			const std::int64_t chunkSize = input.gcount ();
			contentObjectSize += chunkSize;

			EVP_DigestUpdate(fileCtx, buffer.data (), chunkSize);

			uLongf compressedSize = compressed.size ();
			// TODO handle compression failure
			compress2 (reinterpret_cast<Bytef*> (compressed.data ()),
				&compressedSize,
				buffer.data (), chunkSize, Z_BEST_COMPRESSION);

			const auto chunkName = ToString (uuidGen ().data);

			const ChunkInfo chunkInfo {chunkName,
				static_cast<std::int64_t> (compressedSize)};
			chunks.push_back (chunkInfo);

			PackageDataChunk pdc;
			::memset (&pdc, 0, sizeof (pdc));

			pdc.compressedSize = compressedSize;
			pdc.size = chunkSize;
			pdc.offset = fileChunkSize * chunkNumber;
			pdc.compressionMode = CompressionMode_Zip;

			EVP_DigestInit_ex (chunkCtx, EVP_sha512 (), nullptr);
			EVP_DigestUpdate(chunkCtx, compressed.data (), compressedSize);
			EVP_DigestFinal_ex (chunkCtx, pdc.hash, nullptr);

			boost::filesystem::ofstream chunkOutput (
				temporaryDirectory / chunkName, std::ios::binary);
			chunkOutput.write (reinterpret_cast<const char*> (&pdc), sizeof (pdc));
			chunkOutput.write (reinterpret_cast<char*> (compressed.data ()),
				compressedSize);

			BOOST_LOG_TRIVIAL(debug) << "Wrote chunk '" << chunkName
				<< "' for file '" << sourcePath << "' (uncompressed: "
				<< chunkSize << ", compressed: " << compressedSize << ")";

			++chunkNumber;

			// Compress data
			if (input.gcount () < fileChunkSize) {
				break;
			}
		}

		Hash fileHash;

		if (contentObjectSize == 0) {
			// Don't create a content object when the file is empty
			continue;
		}

		EVP_DigestFinal_ex (fileCtx, fileHash.hash, nullptr);

		BOOST_LOG_TRIVIAL(debug) << sourcePath << " -> " << ToString (fileHash.hash);

		sqlite3_bind_blob (selectContentObjectIdStatement, 1,
			fileHash.hash, sizeof (fileHash.hash), nullptr);

		if (sqlite3_step (selectContentObjectIdStatement) != SQLITE_ROW) {
			sqlite3_bind_int64 (insertContentObjectStatement, 1, contentObjectSize);
			sqlite3_bind_blob (insertContentObjectStatement, 2,
				fileHash.hash, sizeof (fileHash.hash), nullptr);
			sqlite3_bind_int64 (insertContentObjectStatement, 3, chunkNumber);
			SAFE_SQLITE_INSERT (sqlite3_step (insertContentObjectStatement));
			SAFE_SQLITE (sqlite3_reset (insertContentObjectStatement));

			const auto contentObjectId = sqlite3_last_insert_rowid (installationDatabase);

			pathToContentObject [sourcePath] = {contentObjectId, fileHash};

			for (const auto chunk : chunks) {
				SAFE_SQLITE (sqlite3_bind_int64 (insertChunkStatement, 1, contentObjectId));
				SAFE_SQLITE (sqlite3_bind_text (insertChunkStatement, 2,
					absolute (temporaryDirectory / chunk.name).c_str (), -1, SQLITE_TRANSIENT));
				SAFE_SQLITE (sqlite3_bind_int64 (insertChunkStatement, 3, chunk.size));
				SAFE_SQLITE_INSERT (sqlite3_step (insertChunkStatement));
				SAFE_SQLITE (sqlite3_reset (insertChunkStatement));
			}
		} else {
			// We have two files with the same hash
			pathToContentObject [sourcePath] = {
				sqlite3_column_int64 (selectContentObjectIdStatement, 0),
				fileHash};
		}

		SAFE_SQLITE (sqlite3_reset (selectContentObjectIdStatement));
	}

	sqlite3_finalize (insertContentObjectStatement);
	sqlite3_finalize (insertChunkStatement);
	sqlite3_finalize (selectContentObjectIdStatement);

	EVP_MD_CTX_destroy (fileCtx);
	EVP_MD_CTX_destroy (chunkCtx);

	SAFE_SQLITE (sqlite3_exec (buildDatabase, "COMMIT TRANSACTION;",
		nullptr, nullptr, nullptr));
	SAFE_SQLITE (sqlite3_exec (installationDatabase, "COMMIT TRANSACTION;",
		nullptr, nullptr, nullptr));

	return pathToContentObject;
}

////////////////////////////////////////////////////////////////////////////////
void InsertStorageMappingEntries (sqlite3* installationDatabase,
	const std::int64_t sourcePackageId,
	const std::unordered_set<std::int64_t>& contentObjectIds)
{
	sqlite3_stmt* insertIntoStorageMappingStatement;
	sqlite3_prepare_v2 (installationDatabase,
		"INSERT INTO storage_mapping (ContentObjectId, SourcePackageId) VALUES (?, ?)",
		-1, &insertIntoStorageMappingStatement, nullptr);

	for (const auto contentObjectId : contentObjectIds) {
		sqlite3_bind_int64 (insertIntoStorageMappingStatement, 1,
			contentObjectId);
		sqlite3_bind_int64 (insertIntoStorageMappingStatement, 2,
			sourcePackageId);
		SAFE_SQLITE_INSERT (sqlite3_step (insertIntoStorageMappingStatement));
		sqlite3_reset (insertIntoStorageMappingStatement);
	}

	sqlite3_finalize (insertIntoStorageMappingStatement);
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
		return prefix_ + std::to_string (i) + suffix_ + ".nimpkg";
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
	Hash			hash;
};

////////////////////////////////////////////////////////////////////////////////
std::vector<SourcePackageInfo> WriteUserPackages (
	sqlite3* buildDatabase,
	sqlite3* installationDatabase,
	const std::unordered_map<std::string, ContentObjectIdHash>& pathToContentObject,
	const SourcePackageNameTemplate& packageNameTemplate,
	const boost::filesystem::path& targetDirectory)
{
	std::vector<SourcePackageInfo> result;

	sqlite3_stmt* insertIntoFilesStatement = nullptr;
	SAFE_SQLITE (sqlite3_prepare_v2 (installationDatabase,
		"INSERT INTO files (Path, ContentObjectId, FeatureId) VALUES (?, ?, ?);",
		-1, &insertIntoFilesStatement, nullptr));
	assert (insertIntoFilesStatement);

	// First, we assemble all user-requested source packages
	sqlite3_stmt* selectSourcePackageIdsStatement = nullptr;
	SAFE_SQLITE (sqlite3_prepare_v2 (installationDatabase,
		"SELECT Id, Name FROM source_packages;", -1,
		&selectSourcePackageIdsStatement, nullptr));
	assert (selectSourcePackageIdsStatement);

	while (sqlite3_step (selectSourcePackageIdsStatement) == SQLITE_ROW) {
		const std::int64_t sourcePackageId = sqlite3_column_int64 (
			selectSourcePackageIdsStatement, 0);

		BOOST_LOG_TRIVIAL(trace) << "Processing source package " << sourcePackageId;

		// Find all files in this package, and then all chunks
		sqlite3_stmt* selectFilesForPackageStatement = nullptr;
		sqlite3_prepare_v2 (buildDatabase,
			"SELECT SourcePath, TargetPath, FeatureId FROM files WHERE SourcePackageId=?;",
			-1, &selectFilesForPackageStatement, nullptr);
		assert (selectFilesForPackageStatement);
		sqlite3_bind_int64 (selectFilesForPackageStatement, 1, sourcePackageId);

		// contentObject (Hash) -> list of chunks that will go into this package
		std::unordered_map<Hash, std::vector<std::string>, HashHash, HashEqual>
				contentObjectsAndChunksInPackage;

		// We need to handle duplicates in case multiple files reference the
		// same content object, so we use a set here instead of a vector
		std::unordered_set<std::int64_t> contentObjectsInPackage;

		// For each file that goes into this package
		while (sqlite3_step (selectFilesForPackageStatement) == SQLITE_ROW) {
			const std::string sourcePath =
				reinterpret_cast<const char*> (sqlite3_column_text (selectFilesForPackageStatement, 0));
			std::int64_t contentObjectId = -1;

			// May be NULL in case it's an empty file
			if (pathToContentObject.find (sourcePath) != pathToContentObject.end ()) {
				const auto contentObjectIdHash = pathToContentObject.find (sourcePath)->second;

				contentObjectId = contentObjectIdHash.id;

				// If this content object has already been added, don't readd
				// twice. This may happen if several files reference the same,
				// non-zero content object
				if (contentObjectsInPackage.find (contentObjectId) == contentObjectsInPackage.end ()) {
					BOOST_LOG_TRIVIAL(trace) << "'" << sourcePath << "' references " << contentObjectId;

					// Write the chunks into this package
					sqlite3_stmt* selectChunksForContentObject;
					// Order by rowid, so we get a sequential write into the output
					// file
					sqlite3_prepare_v2 (buildDatabase,
						"SELECT Path FROM chunks WHERE ContentObjectId=? ORDER BY rowid ASC",
						-1, &selectChunksForContentObject, nullptr);
					sqlite3_bind_int64 (selectChunksForContentObject, 1, contentObjectId);

					std::vector<std::string> chunksForContentObject;

					while (sqlite3_step (selectChunksForContentObject) == SQLITE_ROW) {
						chunksForContentObject.emplace_back (
							reinterpret_cast<const char*> (
								sqlite3_column_text (selectChunksForContentObject, 0)));
					}

					sqlite3_reset (selectChunksForContentObject);
					sqlite3_finalize (selectChunksForContentObject);

					contentObjectsAndChunksInPackage [contentObjectIdHash.hash]
						= std::move (chunksForContentObject);
					contentObjectsInPackage.insert (contentObjectIdHash.id);
				} else {
					BOOST_LOG_TRIVIAL(trace) << "'" << sourcePath << "' references " << contentObjectId << " (already in package, skipped)";
				}
			}

			// We can now write the file entry
			sqlite3_bind_text (insertIntoFilesStatement, 1,
				reinterpret_cast<const char*> (
					sqlite3_column_text (selectFilesForPackageStatement, 1)),
				-1, SQLITE_TRANSIENT);

			if (contentObjectId != -1) {
				SAFE_SQLITE (sqlite3_bind_int64 (insertIntoFilesStatement, 2,
					contentObjectId));
			} else {
				SAFE_SQLITE (sqlite3_bind_null (insertIntoFilesStatement, 2));
			}

			SAFE_SQLITE (sqlite3_bind_int64 (insertIntoFilesStatement, 3,
				sqlite3_column_int64 (selectFilesForPackageStatement, 2)));
			SAFE_SQLITE_INSERT (sqlite3_step (insertIntoFilesStatement));
			SAFE_SQLITE (sqlite3_reset (insertIntoFilesStatement));
		}

		sqlite3_reset (selectFilesForPackageStatement);
		sqlite3_finalize (selectFilesForPackageStatement);

		InsertStorageMappingEntries (installationDatabase,
			sourcePackageId, contentObjectsInPackage);

		// Write the package
		SourcePackageWriter spw;
		spw.Open (targetDirectory / (packageNameTemplate (sourcePackageId)));

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
		BOOST_LOG_TRIVIAL(info) << "Created package "
			<< reinterpret_cast<const char*> (sqlite3_column_text (
				selectSourcePackageIdsStatement, 1));
	}

	sqlite3_finalize (selectSourcePackageIdsStatement);
	sqlite3_finalize (insertIntoFilesStatement);

	return result;
}

std::int64_t CreateGeneratedPackage (sqlite3* installationDatabase,
	boost::uuids::random_generator& uuidGen)
{
	sqlite3_stmt* insertSourcePackageStatement;
	sqlite3_prepare_v2 (installationDatabase,
		"INSERT INTO source_packages (Name, Filename, Hash) VALUES (?, ?, ?);", -1,
		&insertSourcePackageStatement, nullptr);

	const std::string packageName = std::string ("Generated_")
		+ ToString (uuidGen ().data);

	sqlite3_bind_text (insertSourcePackageStatement, 1,
		packageName.c_str (), -1, SQLITE_TRANSIENT);

	// Dummy filename and hash, will be fixed up later
	sqlite3_bind_text (insertSourcePackageStatement, 2,
		packageName.c_str (), -1, SQLITE_TRANSIENT);
	sqlite3_bind_blob (insertSourcePackageStatement, 3,
		packageName.c_str (), packageName.size (), SQLITE_TRANSIENT);

	SAFE_SQLITE_INSERT (sqlite3_step (insertSourcePackageStatement));

	const auto currentPackageId = sqlite3_last_insert_rowid (installationDatabase);

	sqlite3_reset (insertSourcePackageStatement);
	sqlite3_finalize (insertSourcePackageStatement);

	BOOST_LOG_TRIVIAL(info) << "Created package " << packageName;

	return currentPackageId;
}

////////////////////////////////////////////////////////////////////////////////
std::vector<SourcePackageInfo> WriteGeneratedPackages (
	sqlite3* buildDatabase,
	sqlite3* installationDatabase,
	const std::unordered_map<std::string, ContentObjectIdHash>& pathToContentObject,
	const boost::filesystem::path& targetDirectory,
	const SourcePackageNameTemplate& packageNameTemplate,
	const std::int64_t targetPackageSize)
{
	std::vector<SourcePackageInfo> result;

	sqlite3_stmt* insertIntoFilesStatement = nullptr;
	SAFE_SQLITE (sqlite3_prepare_v2 (installationDatabase,
		"INSERT INTO files (Path, ContentObjectId, FeatureId) VALUES (?, ?, ?);",
		-1, &insertIntoFilesStatement, nullptr));
	assert (insertIntoFilesStatement);

	// Here we handle all files sent to the "default" package
	// This is very similar to the loop above, but we will create packages on
	// demand once the package size becomes too large
	sqlite3_stmt* selectFilesForDefaultPackageStatement;
	sqlite3_prepare_v2 (buildDatabase,
		"SELECT SourcePath, TargetPath, FeatureId FROM files WHERE SourcePackageId IS NULL;",
		-1, &selectFilesForDefaultPackageStatement, nullptr);

	boost::uuids::random_generator uuidGen;

	SourcePackageWriter spw;
	std::int64_t dataInCurrentPackage = 0;
	std::int64_t currentPackageId = -1;

	// Same reasong as in WriteUserPackages, we need to handle files which
	// reference the same content object
	std::unordered_set<std::int64_t> contentObjectsInCurrentPackage;
	while (sqlite3_step (selectFilesForDefaultPackageStatement) == SQLITE_ROW) {
		const std::string sourcePath =
			reinterpret_cast<const char*> (sqlite3_column_text (selectFilesForDefaultPackageStatement, 0));

		std::int64_t contentObjectId = -1;
		// May be NULL in case it's an empty file
		if (pathToContentObject.find (sourcePath) != pathToContentObject.end ()) {
			const auto contentObjectIdHash = pathToContentObject.find (sourcePath)->second;
			contentObjectId = contentObjectIdHash.id;

			if (contentObjectsInCurrentPackage.find (contentObjectId) == contentObjectsInCurrentPackage.end ()) {
				// We have found a new content object
				BOOST_LOG_TRIVIAL(trace) << "'" << sourcePath << "' references " << contentObjectId;

				// Write the chunks into this package
				sqlite3_stmt* selectChunksForContentObject = nullptr;
				// Order by rowid, so we get a sequential write into the output
				// file
				sqlite3_prepare_v2 (buildDatabase,
					"SELECT Path, Size FROM chunks WHERE ContentObjectId=? ORDER BY rowid ASC",
					-1, &selectChunksForContentObject, nullptr);
				assert (selectChunksForContentObject);
				sqlite3_bind_int64 (selectChunksForContentObject, 1, contentObjectId);

				while (sqlite3_step (selectChunksForContentObject) == SQLITE_ROW) {
					// Open current package if needed, start inserting
					if (! spw.IsOpen ()) {
						// Add a new source package
						currentPackageId = CreateGeneratedPackage (
							installationDatabase, uuidGen);

						spw.Open (targetDirectory / packageNameTemplate (currentPackageId));
					}

					// Insert the chunk right away, increment current size, if
					// package is full, finalize, and reset size
					const std::string chunkPath = reinterpret_cast<const char*> (
						sqlite3_column_text (selectChunksForContentObject, 0));
					spw.Add (contentObjectIdHash.hash, chunkPath);

					dataInCurrentPackage += sqlite3_column_int64 (
						selectChunksForContentObject, 1);
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

				sqlite3_reset (selectChunksForContentObject);
				sqlite3_finalize (selectChunksForContentObject);
			} else {
				BOOST_LOG_TRIVIAL(trace) << "'" << sourcePath << "' references " << contentObjectId << " (already in package)";
			}
		}

		// We can now write the file entry
		sqlite3_bind_text (insertIntoFilesStatement, 1,
			reinterpret_cast<const char*> (
				sqlite3_column_text (selectFilesForDefaultPackageStatement, 1)),
			-1, SQLITE_TRANSIENT);

		if (contentObjectId != -1) {
			sqlite3_bind_int64 (insertIntoFilesStatement, 2,
				contentObjectId);
		} else {
			sqlite3_bind_null (insertIntoFilesStatement, 2);
		}

		sqlite3_bind_int64 (insertIntoFilesStatement, 3,
			sqlite3_column_int64 (selectFilesForDefaultPackageStatement, 2));
		SAFE_SQLITE_INSERT (sqlite3_step (insertIntoFilesStatement));
		sqlite3_reset (insertIntoFilesStatement);
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

	sqlite3_finalize (selectFilesForDefaultPackageStatement);
	sqlite3_finalize (insertIntoFilesStatement);

	return result;
}

////////////////////////////////////////////////////////////////////////////////
std::vector<SourcePackageInfo> WritePackages (
	sqlite3* buildDatabase,
	sqlite3* installationDatabase,
	const boost::filesystem::path& targetDirectory,
	const pugi::xml_node& productNode,
	const std::unordered_map<std::string, ContentObjectIdHash> pathToContentObject,
	const std::int64_t targetPackageSize = 1ll << 30 /* 1 GiB */)
{
	sqlite3_exec (installationDatabase, "BEGIN TRANSACTION;",
		nullptr, nullptr, nullptr);

	std::vector<SourcePackageInfo> result;

	BOOST_LOG_TRIVIAL(debug) << "Writing packages";
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

	sqlite3_exec (installationDatabase, "COMMIT TRANSACTION;",
		nullptr, nullptr, nullptr);

	return result;
}

struct GeneratorContext
{
public:
	GeneratorContext ()
	{
		sqlite3_open (":memory:", &installationDatabase);

		sqlite3_exec (installationDatabase,
			"PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
		sqlite3_exec (installationDatabase, install_db_structure,
			nullptr, nullptr, nullptr);
	}

	void SetupBuildDatabase (const boost::filesystem::path& temporaryDirectory)
	{
		BOOST_LOG_TRIVIAL(debug) << "Build database: '" << (temporaryDirectory / "build.sqlite").c_str () << "'";
		SAFE_SQLITE (sqlite3_open ((temporaryDirectory / "build.sqlite").c_str (),
			&buildDatabase));
		SAFE_SQLITE (sqlite3_exec (buildDatabase, build_db_structure,
			nullptr, nullptr, nullptr));
	}

	~GeneratorContext ()
	{
		sqlite3_close (buildDatabase);
		sqlite3_close (installationDatabase);
	}

	bool WriteInstallationDatabase (const boost::filesystem::path& outputFile) const
	{
		sqlite3* targetDb;
		sqlite3_open (outputFile.c_str (), &targetDb);
		auto backup = sqlite3_backup_init(targetDb, "main",
			installationDatabase, "main");
		sqlite3_backup_step (backup, -1);
		sqlite3_backup_finish (backup);
	}

	pugi::xml_node	productNode;
	sqlite3*		installationDatabase;
	sqlite3*		buildDatabase;
	boost::filesystem::path sourceDirectory;
	boost::filesystem::path	temporaryDirectory;
	boost::filesystem::path targetDirectory;
};

/*
Returns a mapping of feature id string to the feature id in the database.
*/
std::unordered_map<std::string, std::int64_t> CreateFeatures (GeneratorContext& gc)
{
	sqlite3_exec (gc.installationDatabase, "BEGIN TRANSACTION;",
		nullptr, nullptr, nullptr);

	// TODO Validate that there is a feature element
	// TODO Validate that at least one feature is present
	// TODO Validate the feature ID is not empty
	// TODO Validate all feature IDs are unique
	BOOST_LOG_TRIVIAL(info) << "Populating feature table";

	std::unordered_map<std::string, std::int64_t> result;

	sqlite3_stmt* insertFeatureStatement;
	sqlite3_prepare_v2 (gc.installationDatabase,
		"INSERT INTO features (Name, UIName, UIDescription) VALUES (?, ?, ?)", -1, &insertFeatureStatement,
		nullptr);

	int featureCount = 0;
	for (const auto feature : gc.productNode.child ("Features").children ()) {
		const auto featureId = feature.attribute ("Id").value ();
		sqlite3_bind_text (insertFeatureStatement, 1, featureId,
			-1, SQLITE_TRANSIENT);

		const auto featureName = feature.attribute ("Name").value ();
		sqlite3_bind_text (insertFeatureStatement, 2, featureName,
			-1, SQLITE_TRANSIENT);

		if (feature.attribute ("Description")) {
			sqlite3_bind_text (insertFeatureStatement, 3,
				feature.attribute ("Description").value (), -1, SQLITE_TRANSIENT);
		} else {
			sqlite3_bind_null (insertFeatureStatement, 3);
		}

		SAFE_SQLITE_INSERT (sqlite3_step (insertFeatureStatement));
		sqlite3_reset (insertFeatureStatement);

		const auto lastRowId = sqlite3_last_insert_rowid (gc.installationDatabase);
		result [featureId] = lastRowId;

		BOOST_LOG_TRIVIAL(debug) << "Feature '" << featureId
			<< "' assigned to Id " << lastRowId;
		++featureCount;
	}

	sqlite3_finalize (insertFeatureStatement);

	BOOST_LOG_TRIVIAL(info) << "Created " << featureCount << " feature(s)";

	sqlite3_exec (gc.installationDatabase, "COMMIT TRANSACTION;",
		nullptr, nullptr, nullptr);

	return result;
}

/*
Returns a mapping of source package id string to the source package id in the database.
*/
std::unordered_map<std::string, std::int64_t> CreateSourcePackages (GeneratorContext& gc)
{
	sqlite3_exec (gc.installationDatabase, "BEGIN TRANSACTION;",
		nullptr, nullptr, nullptr);

	// TODO Validate the source package ID is not empty
	// TODO Validate all source package IDs are unique
	BOOST_LOG_TRIVIAL(info) << "Populating source package table";

	std::unordered_map<std::string, std::int64_t> result;

	sqlite3_stmt* insertSourcePackageStatement;
	sqlite3_prepare_v2 (gc.installationDatabase,
		"INSERT INTO source_packages (Name, Filename, Hash) VALUES (?, ?, ?)", -1, &insertSourcePackageStatement,
		nullptr);

	// We must use dummy file names/hashes, and fix this later up
	// once we have all packages in place
	boost::uuids::random_generator uuidGen;

	int sourcePackageCount = 0;
	for (const auto sourcePackage : gc.productNode.child ("SourcePackages").children ()) {
		const auto sourcePackageId = sourcePackage.attribute ("Id").value ();
		sqlite3_bind_text (insertSourcePackageStatement, 1,
			sourcePackageId, -1, SQLITE_TRANSIENT);

		const auto randomId = ToString (uuidGen().data);
		sqlite3_bind_text (insertSourcePackageStatement, 2,
			randomId.c_str (), -1, SQLITE_TRANSIENT);
		sqlite3_bind_blob (insertSourcePackageStatement, 3,
			randomId.c_str (), randomId.size (), SQLITE_TRANSIENT);

		SAFE_SQLITE_INSERT (sqlite3_step (insertSourcePackageStatement));
		sqlite3_reset (insertSourcePackageStatement);

		const auto lastRowId = sqlite3_last_insert_rowid (gc.installationDatabase);
		result [sourcePackageId] = lastRowId;

		BOOST_LOG_TRIVIAL(debug) << "Source package '" << sourcePackageId
			<< "' assigned to Id " << lastRowId;
		++sourcePackageCount;
	}

	sqlite3_finalize (insertSourcePackageStatement);

	BOOST_LOG_TRIVIAL(info) << "Created " << sourcePackageCount << " source package(s)";

	sqlite3_exec (gc.installationDatabase, "COMMIT TRANSACTION;",
		nullptr, nullptr, nullptr);
	return result;
}

////////////////////////////////////////////////////////////////////////////////
void FinalizeSourcePackageNames (const SourcePackageNameTemplate& nameTemplate,
	const std::vector<SourcePackageInfo>& infos,
	sqlite3* installationDatabase)
{
	sqlite3_exec (installationDatabase, "BEGIN TRANSACTION;",
		nullptr, nullptr, nullptr);

	sqlite3_stmt* updateSourcePackageFilenameStatement;
	sqlite3_prepare_v2 (installationDatabase,
		"UPDATE source_packages SET Filename=?,Hash=? WHERE Id=?;", -1,
		&updateSourcePackageFilenameStatement, nullptr);

	for (const auto& info : infos) {
		const auto sourcePackageId = info.id;

		sqlite3_bind_int64 (updateSourcePackageFilenameStatement, 3,
			sourcePackageId);
		const auto packageName = nameTemplate (sourcePackageId);
		BOOST_LOG_TRIVIAL(debug) << "Package " << sourcePackageId << " stored in file " << packageName;
		sqlite3_bind_text (updateSourcePackageFilenameStatement, 1,
			packageName.c_str (), -1, SQLITE_TRANSIENT);
		sqlite3_bind_blob (updateSourcePackageFilenameStatement, 2,
			info.hash.hash, sizeof (info.hash.hash), SQLITE_TRANSIENT);
		SAFE_SQLITE_INSERT (sqlite3_step (updateSourcePackageFilenameStatement));
		sqlite3_reset (updateSourcePackageFilenameStatement);
	}

	sqlite3_finalize (updateSourcePackageFilenameStatement);

	sqlite3_exec (installationDatabase, "COMMIT TRANSACTION;",
		nullptr, nullptr, nullptr);
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
			 std::string ("nimtmp-") + boost::filesystem::unique_path ().string ()))
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
	BOOST_LOG_TRIVIAL(debug) << "Temporary directory: " << gc.temporaryDirectory;
	BOOST_LOG_TRIVIAL(debug) << "Target directory: " << gc.targetDirectory;

	gc.SetupBuildDatabase (gc.temporaryDirectory);

	const auto featureIds = CreateFeatures (gc);
	const auto sourcePackageIds = CreateSourcePackages (gc);

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

	gc.WriteInstallationDatabase (gc.targetDirectory / (outputFile.string () + ".nimdb"));

	boost::filesystem::remove_all (gc.temporaryDirectory);
}
