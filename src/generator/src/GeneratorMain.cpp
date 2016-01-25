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

#include "Uuid.h"

#include <assert.h>

#include <spdlog.h>

#include "build-db-structure.h"
#include "install-db-structure.h"
#include "install-db-indices.h"

#include "SourcePackageWriter.h"

#include "sql/Database.h"

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

struct ContentObjectIdHashDigest
{
	std::int64_t id;
	kyla::SHA512Digest digest;
};

class FileChunker
{
public:
	struct ChunkInfo
	{
		std::string name;
		std::int64_t size;
	};

	FileChunker (const boost::filesystem::path& temporaryDirectory)
	: temporaryDirectory_ (temporaryDirectory)
	{
		compressor_ = kyla::CreateBlockCompressor (kyla::CompressionMode::Zip);
		readBuffer_.resize (fileChunkSize_);
		compressionBuffer_.resize (compressor_->GetCompressionBound (fileChunkSize_));
	}

	struct ChunkResult
	{
		std::vector<ChunkInfo> chunks;
		std::int64_t inputSize;
		std::int64_t compressedSize;
		int chunkCount;
		kyla::SHA512Digest inputDigest;
	};

	ChunkResult ChunkFile (const boost::filesystem::path& fullSourcePath)
	{
		// TODO handle read errors
		// TODO handle empty files
		boost::filesystem::ifstream input (fullSourcePath, std::ios::binary);

		if (!input) {
			spdlog::get ("log")->error () << "Could not open file: '"
									 << fullSourcePath.string ().c_str () << "'";
			exit (1);
		}

		std::vector<ChunkInfo> chunks;

		hasher_.Initialize ();
		int chunkCount = 0;
		std::int64_t contentObjectCompressedSize = 0;
		std::int64_t inputSize = 0;

		for (;;) {
			input.read (reinterpret_cast<char*> (readBuffer_.data ()), fileChunkSize_);
			const std::int64_t bytesRead = input.gcount ();
			inputSize += bytesRead;

			hasher_.Update (kyla::ArrayRef<kyla::byte> (readBuffer_.data (), bytesRead));

			const auto compressedSize = compressor_->Compress (
				kyla::ArrayRef<kyla::byte> (readBuffer_.data (), bytesRead),
				compressionBuffer_);

			const auto chunkName = kyla::Uuid::CreateRandom().ToString();

			const ChunkInfo chunkInfo {chunkName, compressedSize};
			chunks.push_back (chunkInfo);

			kyla::SourcePackageChunk pdc;
			::memset (&pdc, 0, sizeof (pdc));

			pdc.compressedSize = compressedSize;
			pdc.size = bytesRead;
			pdc.offset = fileChunkSize_ * chunkCount;
			pdc.compressionMode = kyla::CompressionMode::Zip;

			contentObjectCompressedSize += compressedSize;

			const auto chunkDigest = kyla::ComputeSHA512 (
				kyla::ArrayRef<kyla::byte> (compressionBuffer_.data (), compressedSize));
			::memcpy (pdc.sha512digest, chunkDigest.bytes, sizeof (pdc.sha512digest));

			boost::filesystem::ofstream chunkOutput (
				temporaryDirectory_ / chunkName, std::ios::binary);
			chunkOutput.write (reinterpret_cast<const char*> (&pdc), sizeof (pdc));
			chunkOutput.write (reinterpret_cast<char*> (compressionBuffer_.data ()),
				compressedSize);

			spdlog::get ("log")->trace () << "Wrote chunk '" << chunkName
				<< "' for file '" << fullSourcePath.string ().c_str () << "' (uncompressed: "
				<< bytesRead << ", compressed: " << compressedSize << ")";

			++chunkCount;

			// Compress data
			if (input.gcount () < fileChunkSize_) {
				break;
			}
		}

		ChunkResult result;
		result.chunkCount = chunkCount;
		result.chunks = chunks;
		result.inputDigest = hasher_.Finalize ();
		result.inputSize = inputSize;
		result.compressedSize = contentObjectCompressedSize;

		return result;
	}

private:
	std::unique_ptr<kyla::BlockCompressor>			compressor_;
	kyla::SHA512StreamHasher		hasher_;
	boost::filesystem::path			temporaryDirectory_;
	std::vector<kyla::byte>			readBuffer_;
	std::vector<kyla::byte>			compressionBuffer_;
	kyla::int64						fileChunkSize_ = 1 << 24;
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
std::unordered_map<std::string, ContentObjectIdHashDigest> PrepareFiles (
	const std::unordered_set<std::string>& sourcePaths,
	const boost::filesystem::path& sourceDirectory,
	const boost::filesystem::path& temporaryDirectory,
	kyla::Sql::Database& buildDatabase,
	kyla::Sql::Database& installationDatabase)
{
	auto buildTransaction = buildDatabase.BeginTransaction();
	auto installationTransaction = installationDatabase.BeginTransaction();

	std::unordered_map<std::string, ContentObjectIdHashDigest> pathToContentObject;

	// Read every unique source path in chunks, update hash, compress, compute
	// hash, write to temporary directory
	FileChunker chunker (temporaryDirectory);

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

		const auto chunkResult = chunker.ChunkFile (fullSourcePath);

		// Empty file, don't create a content object for it
		if (chunkResult.inputSize == 0) {
			continue;
		}

		totalSize += chunkResult.inputSize;

		spdlog::get ("log")->debug() << sourcePath << " -> " << ToString (chunkResult.inputDigest);

		selectContentObjectIdStatement.Bind (1, chunkResult.inputDigest);

		if (! selectContentObjectIdStatement.Step ()) {
			insertContentObjectStatement.BindArguments (
				chunkResult.inputSize,
				chunkResult.inputDigest,
				chunkResult.chunkCount);
			insertContentObjectStatement.Step ();
			insertContentObjectStatement.Reset ();

			const auto contentObjectId = installationDatabase.GetLastRowId ();

			pathToContentObject [sourcePath] = {contentObjectId, chunkResult.inputDigest};

			for (const auto chunk : chunkResult.chunks) {
				insertChunkStatement.Bind (1, contentObjectId);
				insertChunkStatement.Bind (2,
					absolute (temporaryDirectory / chunk.name).string ().c_str ());
				insertChunkStatement.Bind (3, chunk.size);
				insertChunkStatement.Step ();
				insertChunkStatement.Reset ();
			}

			totalCompressedSize += chunkResult.compressedSize;
		} else {
			// We have two files with the same hash
			pathToContentObject [sourcePath] = {
				selectContentObjectIdStatement.GetInt64 (0),
				chunkResult.inputDigest};
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
	std::int64_t		id;
	kyla::SHA512Digest	sha512digest;
};

////////////////////////////////////////////////////////////////////////////////
std::vector<SourcePackageInfo> WriteUserPackages (
	kyla::Sql::Database& buildDatabase,
	kyla::Sql::Database& installationDatabase,
	const std::unordered_map<std::string, ContentObjectIdHashDigest>& pathToContentObject,
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
		std::unordered_map<kyla::SHA512Digest, std::vector<std::string>, kyla::HashDigestHash, kyla::HashDigestEqual>
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
						<< "' references content object " << contentObjectId;

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

					contentObjectsAndChunksInPackage [contentObjectIdHash.digest]
						= std::move (chunksForContentObject);
					contentObjectsInPackage.insert (contentObjectIdHash.id);
				} else {
					spdlog::get ("log")->trace () << "'" << sourcePath
						<< "' references content object " << contentObjectId
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
	const kyla::Uuid& packageUuid)
{
	auto insertSourcePackageStatement = installationDatabase.Prepare (
		"INSERT INTO source_packages (Name, Filename, Uuid, Hash) VALUES (?, ?, ?, ?);");

	const std::string packageName = std::string ("Generated_")
		+ packageUuid.ToString ();

	insertSourcePackageStatement.Bind (1, packageName);

	// Dummy filename and hash, will be fixed up later
	insertSourcePackageStatement.Bind (2, packageName);

	insertSourcePackageStatement.Bind (3, kyla::ArrayRef<> (packageUuid));
	insertSourcePackageStatement.Bind (4,
		kyla::ArrayRef<> (packageName.c_str (), packageName.size ()));

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
	const std::unordered_map<std::string, ContentObjectIdHashDigest>& pathToContentObject,
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
						const auto packageId = kyla::Uuid::CreateRandom ();
						currentPackageId = CreateGeneratedPackage (
							installationDatabase, packageId);

						spw.Open (targetDirectory / packageNameTemplate (currentPackageId),
							packageId.GetData ());
					}

					// Insert the chunk right away, increment current size, if
					// package is full, finalize, and reset size
					const std::string chunkPath = selectChunksForContentObject.GetText (0);
					spw.Add (contentObjectIdHash.digest, chunkPath);

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
	const std::unordered_map<std::string, ContentObjectIdHashDigest> pathToContentObject,
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
		spdlog::get ("log")->trace () << "Build database: '" << (temporaryDirectory / "build.db").string ().c_str () << "'";
		buildDatabase = kyla::Sql::Database::Create (
					(temporaryDirectory / "build.db").string ().c_str ());
		buildDatabase.Execute (build_db_structure);
	}

	~GeneratorContext ()
	{
	}

	void WriteInstallationDatabase (const boost::filesystem::path& outputFile) const
	{
		installationDatabase.SaveCopyTo (outputFile.string ().c_str ());

		spdlog::get ("log")->info () << "Wrote installation database "
			<< outputFile.string ();
	}

	pugi::xml_node	productNode;
	kyla::Sql::Database		installationDatabase;
	kyla::Sql::Database		buildDatabase;
	boost::filesystem::path sourceDirectory;
	boost::filesystem::path	temporaryDirectory;
	boost::filesystem::path targetDirectory;
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
std::unordered_map<std::string, std::int64_t> CreateSourcePackages (GeneratorContext& gc)
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

		const auto packageUuid = kyla::Uuid::CreateRandom ();
		insertSourcePackageStatement.Bind (2, packageUuid.ToString ());
		insertSourcePackageStatement.Bind (3, kyla::ArrayRef<> (packageUuid));
		insertSourcePackageStatement.Bind (4, kyla::ArrayRef<> (packageUuid));

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

		updateSourcePackageFilenameStatement.BindArguments (
			packageName, info.sha512digest);

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
		("log-level", po::value<int> ()->default_value (2))
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
	log->set_level (static_cast<spdlog::level::level_enum> (vm ["log-level"].as<int> ()));

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
