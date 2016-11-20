/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#include "KylaBuild.h"

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

#include <boost/format.hpp>
#include <chrono>

#include <openssl/evp.h>
#include <openssl/rand.h>

namespace {
using namespace kyla;

struct BuildStatistics
{
	int64 bytesStoredUncompressed = 0;
	int64 bytesStoredCompressed = 0;

	std::chrono::high_resolution_clock::duration compressionTime =
		std::chrono::high_resolution_clock::duration::zero ();
	std::chrono::high_resolution_clock::duration encryptionTime =
		std::chrono::high_resolution_clock::duration::zero ();
};

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
	SourcePackage (const std::string& name)
	: name (name)
	{
	}

	SourcePackage () = default;

	std::string name;

	CompressionAlgorithm compressionAlgorithm = CompressionAlgorithm::Brotli;

	std::vector<FileSet> fileSets;
	std::vector<ContentObject> contentObjects;
};

///////////////////////////////////////////////////////////////////////////////
std::unordered_map<std::string, SourcePackage> GetSourcePackages (const pugi::xml_document& doc,
	const BuildContext& ctx)
{
	std::unordered_map<std::string, SourcePackage> result;
	std::unordered_set<std::string> sourcePackageIds;

	for (const auto& sourcePackageNode : doc.select_nodes ("//SourcePackage")) {
		SourcePackage sourcePackage;

		sourcePackage.name = sourcePackageNode.node ().attribute ("Name").as_string ();

		if (sourcePackageIds.find (sourcePackage.name) != sourcePackageIds.end ()) {
			throw RuntimeException (
				str (boost::format ("Source package '%1%' already exists") % sourcePackage.name),
				KYLA_FILE_LINE);
		} else {
			sourcePackageIds.insert (sourcePackage.name);
		}

		if (sourcePackageNode.node ().attribute ("Compression")) {
			sourcePackage.compressionAlgorithm = CompressionAlgorithmFromId (
				sourcePackageNode.node ().attribute ("Compression").as_string ());
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

		// Default package name is main
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

struct RepositoryBuilder
{
	virtual ~RepositoryBuilder ()
	{
	}

	virtual void Configure (const pugi::xml_document& repositoryDefinition) = 0;

	virtual void Build (const BuildContext& ctx,
		const std::unordered_map<std::string, SourcePackage>& packages,
		BuildStatistics* statistics = nullptr) = 0;
};

/**
A loose repository is little more than the files themselves, with hashes.
*/
struct LooseRepositoryBuilder final : public RepositoryBuilder
{
	void Configure (const pugi::xml_document&) override
	{
	}

	void Build (const BuildContext& ctx,
		const std::unordered_map<std::string, SourcePackage>& packages,
		BuildStatistics*) override
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
	/**
	Store all file sets, and return a mapping of every file to its file set id.
	*/
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

	/**
	Write the content objects and files table.
	*/
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
struct PackedRepositoryBuilder final : public RepositoryBuilder
{
	using HashIntMap = std::unordered_map<SHA256Digest, int64, HashDigestHash, HashDigestEqual>;

	void Configure (const pugi::xml_document& repositoryDefinition) override
	{
		auto chunkSizeNode = repositoryDefinition.select_node ("//Package/ChunkSize");

		if (chunkSizeNode) {
			chunkSize_ = chunkSizeNode.node ().text ().as_llong ();

			// Some compressors expect integer-sized chunks, so we clamp to
			// 2^31-1 here - this is a safety measure, as 2 GiB sized chunks
			// should never be used
			chunkSize_ = std::min (chunkSize_, static_cast<int64> ((1ll << 31) - 1));

			assert (chunkSize_ >= 1);
		}

		auto encryptionNode = repositoryDefinition.select_node ("//Package/Encryption");

		if (encryptionNode) {
			encryptionKey_ = encryptionNode.node ().select_node ("Key").node ().text ().as_string ();
		}
	}

	void Build (const BuildContext& ctx,
		const std::unordered_map<std::string, SourcePackage>& packages,
		BuildStatistics* statistics) override
	{
		buildStatistics_ = BuildStatistics ();

		auto dbFile = ctx.targetDirectory / "repository.db";
		boost::filesystem::remove (dbFile);

		auto db = Sql::Database::Create (
			dbFile.string ().c_str ());

		db.Execute (install_db_structure);
		db.Execute ("PRAGMA journal_mode=WAL;");
		db.Execute ("PRAGMA synchronous=NORMAL;");

		{
			auto featuresInsertQuery = db.Prepare (
				"INSERT INTO features (Name) VALUES (?);");

			std::vector<const char*> features = {
				"compression"
			};

			if (! encryptionKey_.empty ()) {
				features.push_back ("encryption");
			}

			for (const auto& feature : features) {
				featuresInsertQuery.BindArguments (feature);
				featuresInsertQuery.Step ();
				featuresInsertQuery.Reset ();
			}
		}

		auto uniqueObjects = PopulateUniqueContentObjects (db, packages);

		for (const auto& sourcePackage : packages) {
			if (sourcePackage.second.contentObjects.empty ()) {
				continue;
			}

			const auto fileToFileSetId = PopulateFileSets (db,
				sourcePackage.second.fileSets);

			WritePackage (db, sourcePackage.second,
				fileToFileSetId, uniqueObjects,
				ctx.targetDirectory, encryptionKey_);
		}

		db.Execute ("PRAGMA journal_mode=DELETE;");
		// Necessary to get good index statistics
		db.Execute ("ANALYZE");
		db.Execute ("VACUUM");

		db.Close ();

		if (statistics) {
			*statistics = buildStatistics_;
		}
	}

private:
	/**
	Store unique content objects from all source packages and return a mapping
	of the content object to its id.
	*/
	HashIntMap PopulateUniqueContentObjects (Sql::Database& db,
		const std::unordered_map<std::string, SourcePackage>& packages)
	{
		auto contentObjectInsert = db.BeginTransaction ();
		auto contentObjectInsertQuery = db.Prepare (
			"INSERT INTO content_objects (Hash, Size) VALUES (?, ?);");

		HashIntMap uniqueObjects;

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

		contentObjectInsert.Commit ();

		return uniqueObjects;
	}

	/**
	Store all file sets, and return a mapping of every file to its file set id.
	*/
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
			// Major           ^^^^
			// Minor               ^^^^
			// Patch                   ^^^^^^^^
		}
	};

	struct TransformationResult
	{
		int64 inputBytes = 0;
		int64 outputBytes = 0;
		std::chrono::nanoseconds duration = std::chrono::nanoseconds{ 0 };
	};

	static TransformationResult TransformCompress (std::vector<byte>& input,
		std::vector<byte>& output, BlockCompressor* compressor)
	{
		auto compressionStartTime = std::chrono::high_resolution_clock::now ();

		TransformationResult result;
		result.inputBytes = static_cast<int64> (input.size ());

		output.resize (
			compressor->GetCompressionBound (result.inputBytes));

		const auto compressedSize = compressor->Compress (
			input, output);

		output.resize (compressedSize);
		result.outputBytes = compressedSize;

		result.duration =
			(std::chrono::high_resolution_clock::now () - compressionStartTime);

		return result;
	}

	static TransformationResult TransformEncrypt (std::vector<byte>& input,
		std::vector<byte>& output, const std::string& encryptionKey,
		std::array<byte, 24>& encryptionData,
		EVP_CIPHER_CTX* encryptionContext)
	{
		auto encryptionStartTime = std::chrono::high_resolution_clock::now ();
		TransformationResult result;
		result.inputBytes = static_cast<int64> (input.size ());
		unsigned char salt [8] = {};
		unsigned char key [64] = {};
		unsigned char iv [16] = {};

		RAND_bytes (salt, sizeof (salt));
		RAND_bytes (iv, sizeof (iv));

		PKCS5_PBKDF2_HMAC_SHA1 (encryptionKey.data (),
			static_cast<int> (encryptionKey.size ()),
			salt, sizeof (salt), 4096, 64, key);

		::memcpy (encryptionData.data (), salt, sizeof (salt));
		::memcpy (encryptionData.data () + sizeof (salt), iv, sizeof (iv));

		EVP_EncryptInit_ex (encryptionContext, EVP_aes_256_cbc (), nullptr,
			key, iv);

		// We need to have storage for 2 AES blocks at the end
		output.resize (input.size () + 32);

		int bytesEncrypted = 0;
		int outputLength = static_cast<int> (output.size ());
		EVP_EncryptUpdate (encryptionContext, output.data (),
			&outputLength, input.data (), static_cast<int> (input.size ()));
		bytesEncrypted += outputLength;
		EVP_EncryptFinal_ex (encryptionContext, output.data () + bytesEncrypted,
			&outputLength);
		bytesEncrypted += outputLength;
		output.resize (bytesEncrypted);
		result.outputBytes = bytesEncrypted;

		result.duration =
			(std::chrono::high_resolution_clock::now () - encryptionStartTime);

		return result;
	}

	void WritePackage (Sql::Database& db,
		const SourcePackage& sourcePackage,
		const std::map<Path, int64>& fileToFileSetId,
		const HashIntMap& uniqueContentObjects,
		const Path& packagePath,
		const std::string& encryptionKey)
	{
		auto contentObjectInsert = db.BeginTransaction ();
		auto filesInsertQuery = db.Prepare (
			"INSERT INTO files (Path, ContentObjectId, FileSetId) VALUES (?, ?, ?);");
		auto packageInsertQuery = db.Prepare (
			"INSERT INTO source_packages (Name, Filename, Uuid) VALUES (?, ?, ?)");
		auto storageMappingInsertQuery = db.Prepare (
			"INSERT INTO storage_mapping "
			"(ContentObjectId, SourcePackageId, PackageOffset, PackageSize, SourceOffset, SourceSize) "
			"VALUES (?, ?, ?, ?, ?, ?)");

		auto storageHashesInsertQuery = db.Prepare (
			"INSERT INTO storage_hashes "
			"(StorageMappingId, Hash) "
			"VALUES (?, ?)"
		);
		auto storageCompressionInsertQuery = db.Prepare (
			"INSERT INTO storage_compression "
			"(StorageMappingId, Algorithm, InputSize, OutputSize) "
			"VALUES (?, ?, ?, ?)"
		);
		auto storageEncryptionInsertQuery = db.Prepare (
			"INSERT INTO storage_encryption "
			"(StorageMappingId, Algorithm, Data, InputSize, OutputSize) "
			"VALUES (?, ?, ?, ?, ?)"
		);

		///@TODO(minor) Support splitting packages for media limits
		auto package = CreateFile (packagePath / (sourcePackage.name + ".kypkg"));

		PackageHeader packageHeader;
		PackageHeader::Initialize (packageHeader);

		package->Write (ArrayRef<PackageHeader> (packageHeader));

		packageInsertQuery.BindArguments (sourcePackage.name, (sourcePackage.name + ".kypkg"),
			Uuid::CreateRandom ());
		packageInsertQuery.Step ();
		packageInsertQuery.Reset ();

		const auto packageId = db.GetLastRowId ();

		auto compressor = CreateBlockCompressor (sourcePackage.compressionAlgorithm);
		auto compressorId = IdFromCompressionAlgorithm (sourcePackage.compressionAlgorithm);

		EVP_CIPHER_CTX* encryptionContext = nullptr;
		if (!encryptionKey.empty ()) {
			encryptionContext = EVP_CIPHER_CTX_new ();
		}

		std::vector<byte> readBuffer, writeBuffer;

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

			///@TODO(minor) Support per-file compression algorithms

			auto inputFile = OpenFile (kv.sourceFile, FileOpenMode::Read);
			const auto inputFileSize = inputFile->GetSize ();

			if (inputFileSize == 0) {
				// If it's a null-byte file, we still store a storage mapping
				const auto startOffset = package->Tell ();

				storageMappingInsertQuery.BindArguments (contentObjectId,
					packageId,
					startOffset, 0 /* = size */,
					0 /* = output offset */,
					0 /* = uncompressed size */);
				storageMappingInsertQuery.Step ();
				storageMappingInsertQuery.Reset ();
			} else {
				readBuffer.resize (std::min (
					chunkSize_, inputFileSize));

				int64 bytesRead = -1;
				int64 readOffset = 0;
				while ((bytesRead = inputFile->Read (readBuffer)) > 0) {
					readBuffer.resize (bytesRead);

					TransformationResult compressionResult;
					compressionResult = TransformCompress (readBuffer,
							writeBuffer, compressor.get ());

					buildStatistics_.bytesStoredUncompressed +=
						compressionResult.inputBytes;
					buildStatistics_.bytesStoredCompressed +=
						compressionResult.outputBytes;

					const auto compressedChunkHash = ComputeSHA256 (writeBuffer);

					TransformationResult encryptionResult;
					std::array<byte, 24> encryptionData;
					if (!encryptionKey.empty ()) {
						std::swap (readBuffer, writeBuffer);

						encryptionResult = TransformEncrypt (readBuffer,
							writeBuffer, encryptionKey,
							encryptionData, encryptionContext);

						buildStatistics_.encryptionTime +=
							encryptionResult.duration;
					}

					const auto startOffset = package->Tell ();
					package->Write (writeBuffer);
					const auto endOffset = package->Tell ();

					storageMappingInsertQuery.BindArguments (contentObjectId, packageId,
						startOffset, endOffset - startOffset,
						readOffset,
						bytesRead);
					storageMappingInsertQuery.Step ();
					storageMappingInsertQuery.Reset ();

					auto storageMappingId = db.GetLastRowId ();

					// Store the hash
					storageHashesInsertQuery.BindArguments (
						storageMappingId, compressedChunkHash
					);
					storageHashesInsertQuery.Step ();
					storageHashesInsertQuery.Reset ();

					// Store the compression data if not uncompressed
					if (sourcePackage.compressionAlgorithm != CompressionAlgorithm::Uncompressed) {
						storageCompressionInsertQuery.BindArguments (
							storageMappingId,
							compressorId,
							compressionResult.inputBytes,
							compressionResult.outputBytes
						);
						storageCompressionInsertQuery.Step ();
						storageCompressionInsertQuery.Reset ();
					}

					// Store encryption data
					if (! encryptionKey.empty ()) {
						storageEncryptionInsertQuery.BindArguments (
							storageMappingId,
							"AES256",
							encryptionData,
							encryptionResult.inputBytes,
							encryptionResult.outputBytes
						);
						storageEncryptionInsertQuery.Step ();
						storageEncryptionInsertQuery.Reset ();
					}

					readOffset += bytesRead;
				}
			}
		}

		contentObjectInsert.Commit ();

		if (encryptionContext) {
			EVP_CIPHER_CTX_free (encryptionContext);
		}
	}

	int64 chunkSize_ = 4 << 20; // 4 MiB chunks is the default

	BuildStatistics buildStatistics_;
	std::string encryptionKey_;
};
}

namespace kyla {
///////////////////////////////////////////////////////////////////////////////
void BuildRepository (const KylaBuildSettings* settings)
{
	const auto inputFile = settings->descriptorFile;

	BuildContext ctx;
	ctx.sourceDirectory = settings->sourceDirectory;
	ctx.targetDirectory = settings->targetDirectory;

	boost::filesystem::create_directories (ctx.targetDirectory);

	pugi::xml_document doc;
	if (!doc.load_file (inputFile)) {
		throw RuntimeException ("Could not parse input file.",
			KYLA_FILE_LINE);
	}

	auto sourcePackages = GetSourcePackages (doc, ctx);
	AssignFileSetsToPackages (doc, ctx, sourcePackages);

	const auto hashStartTime = std::chrono::high_resolution_clock::now ();
	for (auto& sourcePackage : sourcePackages) {
		HashFiles (sourcePackage.second.fileSets, ctx);
	}
	const auto hashTime = std::chrono::high_resolution_clock::now () -
		hashStartTime;

	for (auto& sourcePackage : sourcePackages) {
		sourcePackage.second.contentObjects = FindContentObjects (
			sourcePackage.second.fileSets, ctx);
	}

	const auto packageTypeNode = doc.select_node ("//Package/Type");

	std::unique_ptr<RepositoryBuilder> builder;

	if (packageTypeNode) {
		const auto packageType = packageTypeNode.node ().text ().as_string ();
		if (strcmp (packageType, "Loose") == 0) {
			builder.reset (new LooseRepositoryBuilder);
		} else if (strcmp (packageType, "Packed") == 0) {
			builder.reset (new PackedRepositoryBuilder);
		} else {
			throw RuntimeException ("Unsupported package type",
				KYLA_FILE_LINE);
		}
	} else {
		builder.reset (new PackedRepositoryBuilder);
	}

	builder->Configure (doc);

	BuildStatistics statistics;
	builder->Build (ctx, sourcePackages, &statistics);

	if (settings->buildStatistics) {
		settings->buildStatistics->compressedContentSize = statistics.bytesStoredCompressed;
		settings->buildStatistics->uncompressedContentSize = statistics.bytesStoredUncompressed;
		settings->buildStatistics->compressionRatio =
			static_cast<float> (statistics.bytesStoredUncompressed) /
			static_cast<float> (statistics.bytesStoredCompressed);
		settings->buildStatistics->compressionTimeSeconds =
			static_cast<double> (statistics.compressionTime.count ())
			/ 1000000000.0;
		settings->buildStatistics->hashTimeSeconds =
			static_cast<double> (hashTime.count ())
			/ 1000000000.0;
		settings->buildStatistics->encryptionTimeSeconds =
			static_cast<double> (statistics.encryptionTime.count ())
			/ 1000000000.0;
	}
}
}

///////////////////////////////////////////////////////////////////////////////
KYLA_EXPORT int kylaBuildRepository (
	const KylaBuildSettings* settings)
{
	try {
		if (settings == nullptr) {
			return kylaResult_ErrorInvalidArgument;
		}

		if (settings->descriptorFile == nullptr) {
			return kylaResult_ErrorInvalidArgument;
		}

		if (settings->sourceDirectory == nullptr) {
			return kylaResult_ErrorInvalidArgument;
		}

		if (settings->targetDirectory == nullptr) {
			return kylaResult_ErrorInvalidArgument;
		}

		BuildRepository (settings);

		return kylaResult_Ok;

	} catch (const std::exception&) {
		return kylaResult_Error;
	}
}
