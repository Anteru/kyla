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

///////////////////////////////////////////////////////////////////////////////
struct BuildStatistics
{
	int64 bytesStoredUncompressed = 0;
	int64 bytesStoredCompressed = 0;

	std::chrono::high_resolution_clock::duration compressionTime =
		std::chrono::high_resolution_clock::duration::zero ();
	std::chrono::high_resolution_clock::duration encryptionTime =
		std::chrono::high_resolution_clock::duration::zero ();
};

///////////////////////////////////////////////////////////////////////////////
struct BuildContext
{
	Path sourceDirectory;
	Path targetDirectory;

	Sql::Database& buildDatabase;
};

struct Reference
{
	Uuid id;
};

struct Feature;

enum class RepositoryObjectType
{
	Feature,
	Group,
	FileStorage_Package,
	FileStorage_File
};

struct RepositoryObject
{
	virtual ~RepositoryObject () = default;

	RepositoryObjectType GetType () const
	{
		return GetTypeImpl ();
	}

	void AddLink (RepositoryObject* target)
	{
		AddLinkImpl (target);
	}

	void OnLinkAdded (RepositoryObject* source)
	{
		OnLinkAddedImpl (source);
	}

private:
	virtual RepositoryObjectType GetTypeImpl () const = 0;
	virtual void AddLinkImpl (RepositoryObject* other) = 0;
	virtual void OnLinkAddedImpl (RepositoryObject* source) = 0;
};

template <RepositoryObjectType objectType>
struct RepositoryObjectBase : public RepositoryObject
{
private:
	RepositoryObjectType GetTypeImpl () const
	{
		return objectType;
	}

	void AddLinkImpl (RepositoryObject*)
	{
	}

	void OnLinkAddedImpl (RepositoryObject*)
	{
	}
};

struct Group : public RepositoryObjectBase<RepositoryObjectType::Group>
{
public:
	Group (const Uuid& uuid)
	: uuid_ (uuid)
	{
	}

	std::vector<RepositoryObject*>& GetChildren ()
	{
		return children_;
	}

	const std::vector<RepositoryObject*>& GetChildren () const
	{
		return children_;
	}

	void AddChild (RepositoryObject* child)
	{
		children_.emplace_back (child);
	}

	const Uuid& GetUuid () const
	{
		return uuid_;
	}
	
private:
	std::vector<RepositoryObject*> children_;
	Uuid uuid_;
};

class RepositoryObjectLinker
{
private:

	///////////////////////////////////////////////////////////////////////////////
	struct PendingLink
	{
		RepositoryObject* source;
		RepositoryObject* target;
	};

	std::vector<PendingLink> pendingLinks_;

public:
	///////////////////////////////////////////////////////////////////////////////
	void Prepare (RepositoryObject* source, RepositoryObject* target)
	{
		if (target->GetType () == RepositoryObjectType::Group) {
			for (auto& child : static_cast<Group*> (target)->GetChildren ()) {
				Prepare (source, child);
			}
		} else {
			pendingLinks_.push_back ({ source, target });
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	void Link ()
	{
		for (auto& link : pendingLinks_) {
			link.source->AddLink (link.target);
			link.target->OnLinkAdded (link.source);
		}
	}
};

//////////////////////////////////////////////////////////////////////////////
struct RepositoryBuilder
{
	virtual ~RepositoryBuilder ()
	{
	}

	virtual void Configure (const pugi::xml_document& repositoryDefinition) = 0;

	virtual void Build (const BuildContext& ctx,
		BuildStatistics* statistics = nullptr) = 0;
};

struct Feature : public RepositoryObjectBase<RepositoryObjectType::Feature>
{
	Feature (pugi::xml_node& featureNode, BuildContext& ctx)
	{
		uuid_ = Uuid::Parse (featureNode.attribute ("Id").as_string ());

		for (auto refNode : featureNode.children ("Reference")) {
			references_.push_back (Reference{ 
				Uuid::Parse (refNode.attribute ("Id").as_string ()) 
			});
		};

		Persist (ctx.buildDatabase);
	}

	void Persist (Sql::Database& db)
	{
		auto statement = db.Prepare ("INSERT INTO features (Uuid) VALUES (?);");
		statement.BindArguments (uuid_);
		statement.Step ();
		statement.Reset ();
	}

	int GetPersistentId () const
	{
		assert (persistentId_ != -1);
		return persistentId_;
	}

	const Uuid& GetUuid () const
	{
		return uuid_;
	}

	const std::vector<Reference>& GetReferences () const
	{
		return references_;
	}

private:
	Uuid uuid_;
	int persistentId_ = -1;

	std::vector<Reference> references_;
};


///////////////////////////////////////////////////////////////////////////////
struct FileContents
{
	Path sourceFile;
	SHA256Digest hash;
	std::size_t size;

	std::vector<Path> duplicates;
};

///////////////////////////////////////////////////////////////////////////////
struct File : public RepositoryObjectBase<RepositoryObjectType::FileStorage_File>
{
	Path source;
	Path target;

	FileContents* contents = nullptr;

	int64 packageId = -1;
	int64 featureId = -1;

	File (const pugi::xml_node& node)
	{
		///@TODO(minor) Check if attributes are present

		source = node.attribute ("Source").as_string ();

		if (node.attribute ("Target")) {
			target = node.attribute ("Target").as_string ();
		} else {
			target = source;
		}
	}

private:
	void OnLinkAddedImpl (RepositoryObject* source);
};

struct Package : public RepositoryObjectBase<RepositoryObjectType::FileStorage_Package>
{
	Package (const pugi::xml_node& node,
		Sql::Database& db)
	{
		name = node.attribute ("Name").as_string ();

		for (const auto& ref : node.children ("Reference")) {
			references_.push_back (
				Reference{
				Uuid::Parse (ref.attribute ("Id").as_string ())
			}
			);
		}

		Persist (db);
	}

	Package (const std::string& name, std::vector<Reference>& references,
		Sql::Database& db)
		: name (name)
		, references_ (references)
	{
		Persist (db);
	}

	void Persist (Sql::Database& db)
	{
		auto statement = db.Prepare ("INSERT INTO fs_packages (Filename) VALUES (?);");
		statement.BindArguments (name);
		statement.Step ();
		persistentId_ = db.GetLastRowId ();
	}

	std::string name;
	int64 persistentId_ = -1;

	CompressionAlgorithm compressionAlgorithm = CompressionAlgorithm::Brotli;
	std::vector<Reference> references_;

	std::vector<File*> referencedFiles;

	int64 GetPersistentId () const
	{
		assert (persistentId_ != -1);
		return persistentId_;
	}

	const std::vector<Reference>& GetReferences () const
	{
		return references_;
	}

	void AddLinkImpl (RepositoryObject* target)
	{
		if (target->GetType () == RepositoryObjectType::FileStorage_File) {
			referencedFiles.push_back (static_cast<File*> (target));
		}
	}
};

//////////////////////////////////////////////////////////////////////////////
void File::OnLinkAddedImpl (RepositoryObject* source)
{
	switch (source->GetType ()) {
	case RepositoryObjectType::Feature:
		featureId = static_cast<Feature*> (source)->GetPersistentId ();
		break;

	case RepositoryObjectType::FileStorage_Package:
		packageId = static_cast<Package*> (source)->GetPersistentId ();
		break;
	}
}

struct FileStorage
{
private:
	template <typename T>
	using UniquePtrVector = std::vector<std::unique_ptr<T>>;
	using RepositoryObjectMap = std::unordered_map<Uuid, RepositoryObject*,
		ArrayRefHash, ArrayRefEqual>;
	RepositoryObjectMap repositoryObjects_;

	UniquePtrVector<File> files_;
	UniquePtrVector<Group> groups_;
	UniquePtrVector<Package> packages_;

	using FileContentMap =
		std::unordered_map<SHA256Digest, std::unique_ptr<FileContents>,
		ArrayRefHash, ArrayRefEqual>;

	FileContentMap fileContentMap_;

	struct FileTreeWalker : public pugi::xml_tree_walker
	{
		FileTreeWalker (std::vector<std::unique_ptr<File>>& files,
			std::vector<std::unique_ptr<Group>>& groups,
			RepositoryObjectMap& repositoryObjects)
			: files_ (files)
			, groups_ (groups)
			, repositoryObjects_ (repositoryObjects)
		{
		}

		bool begin (pugi::xml_node& node)
		{
			if (strcmp (node.name (), "Group") == 0) {
				auto uuid = Uuid::Parse (node.attribute ("Id").as_string ());
				groups_.emplace_back (new Group{uuid});
				auto ptr = groups_.back ().get ();
				currentGroup_.push (ptr);
				repositoryObjects_[uuid] = ptr;
			}

			return true;
		}

		bool for_each (pugi::xml_node& node)
		{
			if (strcmp (node.name (), "File") == 0) {
				files_.emplace_back (new File{ node });
				auto ptr = files_.back ().get ();

				if (node.attribute ("Id")) {
					auto uuid = Uuid::Parse (node.attribute ("Id").as_string ());
					repositoryObjects_[uuid] = ptr;
				}

				if (! currentGroup_.empty ()) {
					currentGroup_.top ()->AddChild (ptr);
				}
			}

			return true;
		}

		bool end (pugi::xml_node& node)
		{
			if (strcmp (node.name (), "Group") == 0) {
				currentGroup_.pop ();
			}

			return true;
		}

	private:
		std::vector<std::unique_ptr<File>>& files_;
		std::vector<std::unique_ptr<Group>>& groups_;
		RepositoryObjectMap& repositoryObjects_;

		Uuid currentId_;
		std::stack<Group*> currentGroup_;
	};

public:
	FileStorage (pugi::xml_node& filesNode, BuildContext& ctx)
	{
		PopulateFiles (filesNode, ctx);
		PopulatePackages (filesNode, ctx);
	}

	const RepositoryObjectMap& GetRepositoryObjects () const
	{
		return repositoryObjects_;
	}

	void Persist (BuildContext& ctx)
	{
		// Write packages
	}

private:
	void PopulateFiles (pugi::xml_node& filesNode, BuildContext& ctx)
	{
		// Traverse all groups and individual files, and add everything
		// with an ID to repositoryObjects_
		FileTreeWalker ftw{ files_, groups_, repositoryObjects_ };
		filesNode.traverse (ftw);

		// Now we have all files populated, so we hash the contents to
		// update the file->fileContents field

		HashFileContents (ctx.sourceDirectory);
	}

	void PopulatePackages (pugi::xml_node& filesNode, BuildContext& ctx)
	{
		// Packages can only reference files and groups inside the file storage
		// We track all targets here, remove all which are assigned to a package,
		// and put the remainder into a "main" package which is the default/catch-all
		std::unordered_set<Uuid, ArrayRefHash, ArrayRefEqual> unassignedObjects;
		for (auto& kv : repositoryObjects_) {
			unassignedObjects.insert (kv.first);
		}

		auto packagesNode = filesNode.child ("Packages");
		RepositoryObjectLinker linker;

		if (packagesNode) {
			for (auto packageNode : packagesNode.children ("Package")) {
				packages_.emplace_back (new Package{ packageNode, ctx.buildDatabase });
				auto ptr = packages_.back ().get ();

				for (auto& reference : ptr->GetReferences ()) {
					auto it = repositoryObjects_.find (reference.id);

					if (it == repositoryObjects_.end ()) {
						///@TODO(minor) Handle error
					} else {
						linker.Prepare (ptr, it->second);
						unassignedObjects.erase (reference.id);
					}
				}
			}
		}

		std::vector<Reference> mainPackageReferences;
		for (const auto& uuid : unassignedObjects) {
			mainPackageReferences.push_back (Reference{ uuid });
		}

		packages_.emplace_back (new Package{ "main", mainPackageReferences,
			ctx.buildDatabase });
		auto mainPackage = packages_.back ().get ();

		for (auto& object : unassignedObjects) {
			// This find is guaranteed to succeed, as unassignedObjects
			// contains the keys of repositoryObjects_ minus the assigned ones
			linker.Prepare (mainPackage, repositoryObjects_.find (object)->second);
		}

		linker.Link ();
	}

	void HashFileContents (const Path& sourcePath)
	{
		for (const auto& file : files_) {
			const auto filePath = sourcePath / file->source;
			const auto hash = ComputeSHA256 (filePath);

			auto it = fileContentMap_.find (hash);
			if (it == fileContentMap_.end ()) {
				std::unique_ptr<FileContents> fileContents{ new FileContents };
				fileContents->hash = hash;

				auto fileStats = Stat (filePath);
				fileContents->size = fileStats.size;
				fileContents->sourceFile = filePath;
				
				fileContentMap_[hash] = std::move (fileContents);
			} else {
				it->second->duplicates.push_back (filePath);
			}
		}
	}
};

class Repository
{
public:
	void CreateFeatures (pugi::xml_node& root, BuildContext& ctx)
	{
		auto featuresNode = root.select_node ("/Repository/Features");

		if (!featuresNode) {
			///@TODO(minor) Handle error
		} else {
			for (auto& feature : featuresNode.node ().children ("Feature")) {
				features_.emplace_back (std::move (new Feature{ feature, ctx }));
				auto ptr = features_.back ().get ();

				repositoryObjects_[ptr->GetUuid ()] = ptr;
			}
		}
	}

	void CreateFileStorage (pugi::xml_node& root, BuildContext& ctx)
	{
		auto filesNode = root.select_node ("/Repository/Files");

		fileStorage_.reset (new FileStorage{
			filesNode.node (), ctx
		});

		for (auto& kv : fileStorage_->GetRepositoryObjects ()) {
			repositoryObjects_.insert (kv);
		}
	}

	void LinkFeatures ()
	{
		RepositoryObjectLinker linker;

		for (auto& feature : features_) {
			for (auto& reference : feature->GetReferences ()) {
				auto it = repositoryObjects_.find (reference.id);

				if (it == repositoryObjects_.end ()) {
					///@TODO(minor) Handle error
				} else {
					linker.Prepare (feature.get (), it->second);
				}
			}
		}

		linker.Link ();
	}

	void PersistFileStorage (BuildContext& ctx)
	{
		fileStorage_->Persist (ctx);
	}

private:
	std::unordered_map<Uuid, RepositoryObject*,
		ArrayRefHash, ArrayRefEqual> repositoryObjects_;

	std::vector<std::unique_ptr<Feature> > features_;
	std::unique_ptr<FileStorage> fileStorage_;
};

/**
Store all files into one or more source packages. A source package can be
compressed as well.
*/
struct PackedRepositoryBuilder final : public RepositoryBuilder
{
	using HashIntMap = std::unordered_map<SHA256Digest, int64, ArrayRefHash, ArrayRefEqual>;

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
		BuildStatistics* statistics) override
	{
		buildStatistics_ = BuildStatistics ();

		if (statistics) {
			*statistics = buildStatistics_;
		}
	}

private:
	// The file starts with a header followed by all content objects.
	// The database is stored separately
	struct PackageHeader
	{
		char id[8];
		std::uint64_t version;
		char reserved[48];

		static void Initialize (PackageHeader& header)
		{
			memset (&header, 0, sizeof (header));

			memcpy (header.id, "KYLAPKG", 8);
			header.version = 0x0002000000000000ULL;
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
		unsigned char salt[8] = {};
		unsigned char key[64] = {};
		unsigned char iv[16] = {};

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
		const Package& package,
		const std::map<Path, int64>& fileToFileSetId,
		const HashIntMap& uniqueContentObjects,
		const Path& packagePath,
		const std::string& encryptionKey)
	{
#if 0
		auto contentObjectInsert = db.BeginTransaction ();
		auto filesInsertQuery = db.Prepare (
			"INSERT INTO files (Path, ContentObjectId, FileSetId) VALUES (?, ?, ?);");
		auto packageInsertQuery = db.Prepare (
			"INSERT INTO source_packages (Filename, Uuid) VALUES (?, ?)");
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
		auto packageFile = CreateFile (packagePath / (package.name + ".kypkg"));

		PackageHeader packageHeader;
		PackageHeader::Initialize (packageHeader);

		packageFile->Write (ArrayRef<PackageHeader> (packageHeader));

		packageInsertQuery.BindArguments (package.name + ".kypkg",
			Uuid::CreateRandom ());
		packageInsertQuery.Step ();
		packageInsertQuery.Reset ();

		const auto packageId = db.GetLastRowId ();

		auto compressor = CreateBlockCompressor (package.compressionAlgorithm);
		auto compressorId = IdFromCompressionAlgorithm (package.compressionAlgorithm);

		EVP_CIPHER_CTX* encryptionContext = nullptr;
		if (!encryptionKey.empty ()) {
			encryptionContext = EVP_CIPHER_CTX_new ();
		}

		std::vector<byte> readBuffer, writeBuffer;

		// We can insert content objects directly - every unique file is one
		for (const auto& kv : package.fileContents) {
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
				const auto startOffset = packageFile->Tell ();

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

					const auto startOffset = packageFile->Tell ();
					packageFile->Write (writeBuffer);
					const auto endOffset = packageFile->Tell ();

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
					if (package.compressionAlgorithm != CompressionAlgorithm::Uncompressed) {
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
					if (!encryptionKey.empty ()) {
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
#endif
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

	boost::filesystem::create_directories (settings->targetDirectory);

	auto dbFile = Path{ settings->targetDirectory } / "repository.db";
	boost::filesystem::remove (dbFile);

	auto db = Sql::Database::Create (
		dbFile.string ().c_str ());

	db.Execute (install_db_structure);
	db.Execute ("PRAGMA journal_mode=WAL;");
	db.Execute ("PRAGMA synchronous=NORMAL;");

	BuildContext ctx {
		settings->sourceDirectory, settings->targetDirectory, db
	};

	pugi::xml_document doc;
	if (!doc.load_file (inputFile)) {
		throw RuntimeException ("Could not parse input file.",
			KYLA_FILE_LINE);
	}

	Repository repository;
	repository.CreateFeatures (doc, ctx);
	
	const auto hashStartTime = std::chrono::high_resolution_clock::now ();
	///@TODO(minor) Hash files
	repository.CreateFileStorage (doc, ctx);
	const auto hashTime = std::chrono::high_resolution_clock::now () -
		hashStartTime;

	repository.LinkFeatures ();

	std::unique_ptr<RepositoryBuilder> builder;

	builder.reset (new PackedRepositoryBuilder);
	builder->Configure (doc);

	BuildStatistics statistics;
	builder->Build (ctx, &statistics);

	db.Execute ("PRAGMA journal_mode=DELETE;");
	// Necessary to get good index statistics
	db.Execute ("ANALYZE");
	db.Execute ("VACUUM");

	db.Close ();

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
