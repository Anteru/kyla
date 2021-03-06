/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#include "KylaBuild.h"

#include "Hash.h"

#include <filesystem>
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
#include <stack>

#include "sql/Database.h"
#include "Exception.h"

#include "Compression.h"

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

class BuildDatabase
{
public:
	BuildDatabase (Sql::Database& db)
		: db_ (db)
		, fileInsertStatement_ (db.Prepare (
			"INSERT INTO fs_files (Path, ContentId, FeatureId) VALUES (?, ?, ?);"))
		, packageInsertStatement_ (db.Prepare ("INSERT INTO fs_packages (Filename) VALUES (?);"))
		, packageDeleteStatement_ (db.Prepare ("DELETE FROM fs_packages WHERE Id=?;"))
		, contentInsertStatement_ (db.Prepare ("INSERT INTO fs_contents (Hash, Size) VALUES (?, ?);"))
		, featureInsertStatement_ (db.Prepare ("INSERT INTO features (Uuid, Title, Description, ParentId) VALUES (?,?,?,?);"))
		, featureDependencyInsertStatement_ (db.Prepare (
			"INSERT INTO feature_dependencies (SourceId, TargetId, Relation) VALUES ( "
			"(SELECT Id FROM features WHERE Uuid=?), "
			"(SELECT Id FROM features WHERE Uuid=?), "
			"?);"))
		, chunkInsertQuery_ (db.Prepare (
			"INSERT INTO fs_chunks "
			"(ContentId, PackageId, PackageOffset, PackageSize, SourceOffset, SourceSize) "
			"VALUES (?, ?, ?, ?, ?, ?)"))
		, chunkHashesInsertQuery_ (db.Prepare (
		"INSERT INTO fs_chunk_hashes "
		"(ChunkId, Hash) "
		"VALUES (?, ?)"))
		, chunkCompressionInsertQuery_ (db.Prepare (
		"INSERT INTO fs_chunk_compression "
		"(ChunkId, Algorithm, InputSize, OutputSize) "
		"VALUES (?, ?, ?, ?)"))
			, chunkEncryptionInsertQuery_ (db.Prepare (
		"INSERT INTO fs_chunk_encryption "
		"(ChunkId, Algorithm, Data, InputSize, OutputSize) "
		"VALUES (?, ?, ?, ?, ?)"))
	{
	}

	int64 StoreFeature (const Uuid& uuid, const std::string& title,
		const std::string& description, const int64 parentId)
	{
		featureInsertStatement_.Bind (1, uuid);
		if (title.empty ()) {
			featureInsertStatement_.Bind (2, Sql::Null ());
		} else {
			featureInsertStatement_.Bind (2, title);
		}
		if (description.empty ()) {
			featureInsertStatement_.Bind (3, Sql::Null ());
		} else {
			featureInsertStatement_.Bind (3, description);
		}
		if (parentId < 0) {
			featureInsertStatement_.Bind (4, Sql::Null ());
		} else {
			featureInsertStatement_.Bind (4, parentId);
		}
		featureInsertStatement_.Step ();
		featureInsertStatement_.Reset ();

		return db_.GetLastRowId ();
	}

	void StoreFeatureDependency (const Uuid& source, const Uuid& target,
		const char* relation)
	{
		featureDependencyInsertStatement_.BindArguments (
			source, target, relation);
		featureDependencyInsertStatement_.Step ();
		featureDependencyInsertStatement_.Reset ();
	}

	int64 StorePackage (const char* filename)
	{
		packageInsertStatement_.BindArguments (filename);
		packageInsertStatement_.Step ();
		packageInsertStatement_.Reset ();

		return db_.GetLastRowId ();
	}

	void DeletePackage (const int64 id)
	{
		packageDeleteStatement_.BindArguments (id);
		packageDeleteStatement_.Step ();
		packageDeleteStatement_.Reset ();
	}

	int64 StoreContent (const SHA256Digest& hash, int64 size)
	{
		contentInsertStatement_.BindArguments (hash, size);
		contentInsertStatement_.Step ();
		contentInsertStatement_.Reset ();

		return db_.GetLastRowId ();
	}

	int64 StoreFile (const char* path, int64 contentId, int64 featureId)
	{
		fileInsertStatement_.BindArguments (path, contentId, featureId);
		fileInsertStatement_.Step ();
		fileInsertStatement_.Reset ();

		return db_.GetLastRowId ();
	}

	int64 StoreChunk (int64 contentId, int64 packageId, int64 packageOffset, int64 packageSize, int64 sourceOffset, int64 sourceSize)
	{
		chunkInsertQuery_.BindArguments (contentId, packageId, packageOffset, packageSize, sourceOffset, sourceSize);
		chunkInsertQuery_.Step ();
		chunkInsertQuery_.Reset ();

		return db_.GetLastRowId ();
	}

	int64 StoreChunkHash (int64 chunkId, const SHA256Digest& hash)
	{
		chunkHashesInsertQuery_.BindArguments (chunkId, hash);
		chunkHashesInsertQuery_.Step ();
		chunkHashesInsertQuery_.Reset ();

		return db_.GetLastRowId ();
	}

	int64 StoreChunkCompression (int64 chunkId, CompressionAlgorithm algorithm, int64 inputSize, int64 outputSize)
	{
		chunkCompressionInsertQuery_.BindArguments (chunkId, IdFromCompressionAlgorithm (algorithm), inputSize, outputSize);
		chunkCompressionInsertQuery_.Step ();
		chunkCompressionInsertQuery_.Reset ();

		return db_.GetLastRowId ();
	}

	int64 StoreChunkEncryption (int64 chunkId, const char* algorithm, const ArrayRef<>& data, int64 inputSize, int64 outputSize)
	{
		chunkEncryptionInsertQuery_.BindArguments (chunkId, algorithm, data, inputSize, outputSize);
		chunkEncryptionInsertQuery_.Step ();
		chunkEncryptionInsertQuery_.Reset ();

		return db_.GetLastRowId ();
	}
private:
	Sql::Database& db_;

	Sql::Statement fileInsertStatement_;
	Sql::Statement packageInsertStatement_;
	Sql::Statement packageDeleteStatement_;
	Sql::Statement contentInsertStatement_;
	Sql::Statement featureInsertStatement_;
	Sql::Statement featureDependencyInsertStatement_;
	Sql::Statement chunkInsertQuery_;
	Sql::Statement chunkHashesInsertQuery_;
	Sql::Statement chunkCompressionInsertQuery_;
	Sql::Statement chunkEncryptionInsertQuery_;
};

///////////////////////////////////////////////////////////////////////////////
struct BuildContext
{
	Path sourceDirectory;
	Path targetDirectory;

	BuildDatabase buildDatabase;
	BuildStatistics statistics;
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

		pendingLinks_.clear ();
	}
};

//////////////////////////////////////////////////////////////////////////////
struct Feature : public RepositoryObjectBase<RepositoryObjectType::Feature>
{
	Feature (const pugi::xml_node& featureNode, Feature* parent)
	: parent_ (parent)
	{
		uuid_ = Uuid::Parse (featureNode.attribute ("Id").as_string ());

		if (featureNode.attribute ("Title")) {
			title_ = featureNode.attribute ("Title").as_string ();
		}

		if (featureNode.attribute ("Description")) {
			description_ = featureNode.attribute ("Description").as_string ();
		}

		for (auto refNode : featureNode.children ("Reference")) {
			references_.push_back (Reference{
				Uuid::Parse (refNode.attribute ("Id").as_string ())
			});
		};

		for (auto depNode : featureNode.children ("Dependency")) {
			dependencies_.push_back (Reference{
				Uuid::Parse (depNode.attribute ("Id").as_string ())
			});
		}
	}

	void Store (BuildDatabase& db)
	{
		assert (persistentId_ == -1);
		if (parent_) {
			persistentId_ = db.StoreFeature (uuid_, title_, description_, parent_->GetPersistentId ());
		} else {
			persistentId_ = db.StoreFeature (uuid_, title_, description_, -1);
		}
	}

	void StoreDependencies (BuildDatabase& db)
	{
		// Can be only used after the feature has been stored
		assert (persistentId_ != -1);

		for (const auto& dependency : dependencies_) {
			db.StoreFeatureDependency (uuid_, dependency.id, "requires");
		}
	}

	int64 GetPersistentId () const
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
	int64 persistentId_ = -1;

	Feature* parent_ = nullptr;

	std::string title_;
	std::string description_;

	std::vector<Reference> references_;
	std::vector<Reference> dependencies_;
};


///////////////////////////////////////////////////////////////////////////////
struct Content
{
	Path sourceFile;
	SHA256Digest hash;
	std::size_t size;

	std::vector<Path> duplicates;

	void Store (BuildDatabase& db)
	{
		assert (persistentId_ == -1);
		persistentId_ = db.StoreContent (hash, size);
	}

	int64 GetPersistentId () const
	{
		assert (persistentId_ != -1);
		return persistentId_;
	}

private:
	int64 persistentId_ = -1;
};

//////////////////////////////////////////////////////////////////////////////
struct File : public RepositoryObjectBase<RepositoryObjectType::FileStorage_File>
{
	Path source;
	Path target;

	int64 packageId = -1;
	int64 featureId = -1;

	void Store (BuildDatabase& db)
	{
		assert (persistentId_ == -1);
		persistentId_ = db.StoreFile (target.string ().c_str (),
			fileContents_->GetPersistentId (), featureId);
	}

	int64 GetPersistentId () const
	{
		assert (persistentId_ == -1);
		return persistentId_;
	}

	void SetFileContents (Content* contents)
	{
		assert (fileContents_ == nullptr);
		fileContents_ = contents;
	}

	const Content* GetFileContents () const
	{
		return fileContents_;
	}

private:
	int64 persistentId_ = -1;
	Content* fileContents_ = nullptr;

public:
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

//////////////////////////////////////////////////////////////////////////////
struct Package : public RepositoryObjectBase<RepositoryObjectType::FileStorage_Package>
{
	Package (const pugi::xml_node& node)
	{
		name = node.attribute ("Name").as_string ();
		name += ".kypkg";

		for (const auto& ref : node.children ("Reference")) {
			references_.push_back (
				{ Uuid::Parse (ref.attribute ("Id").as_string ()) }
			);
		}
	}

	Package (const std::string& name, std::vector<Reference>& references)
	: name (name + ".kypkg")
	, references_ (references)
	{
	}

	void Store (BuildDatabase& db)
	{
		persistentId_ = db.StorePackage (name.c_str ());
	}

	void Delete (BuildDatabase& db)
	{
		db.DeletePackage (persistentId_);
	}

	std::string name;

	int64 GetPersistentId () const
	{
		assert (persistentId_ != -1);
		return persistentId_;
	}

	const std::vector<Reference>& GetReferences () const
	{
		return references_;
	}

	const std::vector<File*>& GetReferencedFiles () const
	{
		return referencedFiles_;
	}

	void AddLinkImpl (RepositoryObject* target)
	{
		if (target->GetType () == RepositoryObjectType::FileStorage_File) {
			auto ptr = static_cast<File*> (target);

			// See PopulatePackages() for cases when we could double-link to the
			// same file
			if (ptr->packageId == -1) {
				referencedFiles_.push_back (ptr);
			} else {
				assert (ptr->packageId != persistentId_);
			}
		}
	}

	CompressionAlgorithm GetCompressionAlgorithm () const
	{
		return compressionAlgorithm_;
	}

	std::vector<const Content*> GetUniqueContents () const
	{
		std::vector<const Content*> uniqueFileContents;
		for (auto& file : GetReferencedFiles ()) {
			uniqueFileContents.push_back (file->GetFileContents ());
		}

		std::sort (uniqueFileContents.begin (), uniqueFileContents.end ());

		uniqueFileContents.erase (
			std::unique (uniqueFileContents.begin (), uniqueFileContents.end ()),
			uniqueFileContents.end ());

		// We ensure deterministic order
		std::sort (uniqueFileContents.begin (), uniqueFileContents.end (),
			[](const Content* a, const Content* b) -> bool {
			return ::memcmp (a->hash.bytes, b->hash.bytes, sizeof (a->hash.bytes)) < 0;
		});

		return uniqueFileContents;
	}

private:
	std::vector<Reference> references_;
	CompressionAlgorithm compressionAlgorithm_ = CompressionAlgorithm::Brotli;
	int64 persistentId_ = -1;
	std::vector<File*> referencedFiles_;
};

//////////////////////////////////////////////////////////////////////////////
void File::OnLinkAddedImpl (RepositoryObject* source)
{
	switch (source->GetType ()) {
	case RepositoryObjectType::Feature:
		featureId = static_cast<Feature*> (source)->GetPersistentId ();
		break;

	case RepositoryObjectType::FileStorage_Package:
		// See PopulatePackages() for cases when we could double-link to the
		// same package
		if (packageId == -1) {
			packageId = static_cast<Package*> (source)->GetPersistentId ();
		} else {
			assert (packageId != static_cast<Package*>(source)->GetPersistentId ());
		}

		break;

	default:
		break;
	}
}

//////////////////////////////////////////////////////////////////////////////
struct XmlTreeWalker
{
	virtual ~XmlTreeWalker () = default;

	virtual bool OnEnter (const pugi::xml_node&) = 0;
	virtual bool OnNode (const pugi::xml_node&) = 0;
	virtual bool OnLeave (const pugi::xml_node&) = 0;
};

//////////////////////////////////////////////////////////////////////////////
void Traverse (const pugi::xml_node& node, XmlTreeWalker& walker)
{
	if (!walker.OnEnter (node)) {
		return;
	}

	if (walker.OnNode (node)) {
		for (auto& child : node.children ()) {
			Traverse (child, walker);
		}
	}

	walker.OnLeave (node);
}

//////////////////////////////////////////////////////////////////////////////
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

	int64 chunkSize_ = 4 << 20; // 4 MiB chunks is the default
	std::string encryptionKey_;

	using FileContentMap =
		std::unordered_map<SHA256Digest, std::unique_ptr<Content>,
		ArrayRefHash, ArrayRefEqual>;

	FileContentMap fileContentMap_;

	struct FileTreeWalker : public XmlTreeWalker
	{
		FileTreeWalker (std::vector<std::unique_ptr<File>>& files,
			std::vector<std::unique_ptr<Group>>& groups,
			RepositoryObjectMap& repositoryObjects)
			: files_ (files)
			, groups_ (groups)
			, repositoryObjects_ (repositoryObjects)
		{
		}

		bool OnEnter (const pugi::xml_node& node)
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

		bool OnNode (const pugi::xml_node& node)
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

		bool OnLeave (const pugi::xml_node& node)
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

		std::stack<Group*> currentGroup_;
	};

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

public:
	void WritePackage (BuildDatabase& db,
		const Package& package,
		const Path& packagePath,
		const std::string& encryptionKey,
		BuildStatistics& statistics)
	{
		///@TODO(minor) Support splitting packages for media limits
		auto packageFile = CreateFile (packagePath / (package.name));

		PackageHeader packageHeader;
		PackageHeader::Initialize (packageHeader);

		packageFile->Write (ArrayRef<PackageHeader> (packageHeader));
		const auto packageId = package.GetPersistentId ();

		auto compressor = CreateBlockCompressor (package.GetCompressionAlgorithm ());

		EVP_CIPHER_CTX* encryptionContext = nullptr;
		if (!encryptionKey.empty ()) {
			encryptionContext = EVP_CIPHER_CTX_new ();
		}

		std::vector<byte> readBuffer, writeBuffer;

		for (const auto& content : package.GetUniqueContents ()) {
			const auto contentId = content->GetPersistentId ();
			///@TODO(minor) Support per-file compression algorithms

			auto inputFile = OpenFile (content->sourceFile, FileAccess::Read);
			const auto inputFileSize = inputFile->GetSize ();

			assert (inputFileSize == content->size);

			if (inputFileSize == 0) {
				// If it's a null-byte file, we still store a storage mapping
				const auto startOffset = packageFile->Tell ();

				db.StoreChunk (
					contentId,
					packageId,
					startOffset, 0 /* = size */,
					0 /* = output offset */,
					0 /* = uncompressed size */);
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

					statistics.bytesStoredUncompressed +=
						compressionResult.inputBytes;
					statistics.bytesStoredCompressed +=
						compressionResult.outputBytes;
					statistics.compressionTime += compressionResult.duration;

					const auto compressedChunkHash = ComputeSHA256 (writeBuffer);

					TransformationResult encryptionResult;
					std::array<byte, 24> encryptionData;
					if (!encryptionKey.empty ()) {
						std::swap (readBuffer, writeBuffer);

						encryptionResult = TransformEncrypt (readBuffer,
							writeBuffer, encryptionKey,
							encryptionData, encryptionContext);

						statistics.encryptionTime +=
							encryptionResult.duration;
					}

					const auto startOffset = packageFile->Tell ();
					packageFile->Write (writeBuffer);
					const auto endOffset = packageFile->Tell ();

					const auto storageMappingId = db.StoreChunk (
						contentId, packageId,
						startOffset, endOffset - startOffset,
						readOffset,
						bytesRead);

					// Store the hash
					db.StoreChunkHash (
						storageMappingId, compressedChunkHash
					);

					// Store the compression data if not uncompressed
					if (package.GetCompressionAlgorithm () != CompressionAlgorithm::Uncompressed) {
						db.StoreChunkCompression (
							storageMappingId,
							package.GetCompressionAlgorithm (),
							compressionResult.inputBytes,
							compressionResult.outputBytes
						);
					}

					// Store encryption data
					if (!encryptionKey.empty ()) {
						db.StoreChunkEncryption (
							storageMappingId,
							"AES256",
							encryptionData,
							encryptionResult.inputBytes,
							encryptionResult.outputBytes
						);
					}

					readOffset += bytesRead;
				}
			}
		}

		if (encryptionContext) {
			EVP_CIPHER_CTX_free (encryptionContext);
		}
	}

public:
	FileStorage (const pugi::xml_node& filesNode, BuildContext& ctx)
	{
		PopulateFiles (filesNode, ctx);
		PopulatePackages (filesNode, ctx);

		auto encryptionNode = filesNode.select_node ("/Packages/Encryption");

		if (encryptionNode) {
			auto keyNode = encryptionNode.node ().child ("Key");
			encryptionKey_ = keyNode.value ();
		}
	}

	const RepositoryObjectMap& GetRepositoryObjects () const
	{
		return repositoryObjects_;
	}

	void Persist (BuildContext& ctx)
	{
		for (auto& file : files_) {
			file->Store (ctx.buildDatabase);
		}

		for (auto& package : packages_) {
			WritePackage (ctx.buildDatabase, *package, ctx.targetDirectory, 
				encryptionKey_, ctx.statistics);
		}
	}

private:
	void PopulateFiles (const pugi::xml_node& filesNode, BuildContext& ctx)
	{
		// Traverse all groups and individual files, and add everything
		// with an ID to repositoryObjects_
		FileTreeWalker ftw{ files_, groups_, repositoryObjects_ };
		Traverse (filesNode, ftw);

		// Now we have all files populated, so we hash the contents to
		// update the file->fileContents field

		CreateFileContents (ctx);
	}

	void PopulatePackages (const pugi::xml_node& filesNode, BuildContext& ctx)
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
				packages_.emplace_back (new Package{ packageNode });
				auto ptr = packages_.back ().get ();

				ptr->Store (ctx.buildDatabase);

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

		linker.Link ();

		// Remaining objects go into the default "main" package
		// Notice that it's valid to have a file with an id inside a group with an
		// id. This will result in both the file, and the group ending up in the
		// unassignedObjects. In this case, the file/package linking will ensure
		// that the package doesn't link against the same file twice
		if (!unassignedObjects.empty ()) {
			std::vector<Reference> mainPackageReferences;
			for (const auto& uuid : unassignedObjects) {
				mainPackageReferences.push_back (Reference{ uuid });
			}

			packages_.emplace_back (new Package{ "main", mainPackageReferences });
			auto mainPackage = packages_.back ().get ();
			mainPackage->Store (ctx.buildDatabase);

			for (auto& object : unassignedObjects) {
				// This find is guaranteed to succeed, as unassignedObjects
				// contains the keys of repositoryObjects_ minus the assigned ones
				linker.Prepare (mainPackage, repositoryObjects_.find (object)->second);
			}
		}

		linker.Link ();

		// The main package could be empty now - if so, remove it again
		auto mainPackage = packages_.back ().get ();
		if (mainPackage->GetReferencedFiles ().empty ()) {
			mainPackage->Delete (ctx.buildDatabase);
			packages_.pop_back ();
		}
	}

	void CreateFileContents (BuildContext& ctx)
	{
		static const int BufferSize = 16 << 20; /* 16 MiB */
		std::unique_ptr<byte[]> buffer{ new byte[BufferSize] };

		for (auto& file : files_) {
			const auto filePath = file->source.is_absolute () ? file->source : ctx.sourceDirectory / file->source;
			const auto hash = ComputeSHA256 (filePath,
				MutableArrayRef<byte> {buffer.get (), BufferSize});

			auto it = fileContentMap_.find (hash);
			if (it == fileContentMap_.end ()) {
				std::unique_ptr<Content> fileContents{ new Content };
				fileContents->hash = hash;

				auto fileStats = Stat (filePath);
				fileContents->size = fileStats.size;
				fileContents->sourceFile = filePath;

				fileContents->Store (ctx.buildDatabase);
				file->SetFileContents (fileContents.get ());

				fileContentMap_[hash] = std::move (fileContents);
			} else {
				file->SetFileContents (it->second.get ());
				it->second->duplicates.push_back (filePath);
			}
		}
	}
};

class Repository
{
public:
	void CreateFeatures (const pugi::xml_node& root, BuildContext& ctx)
	{
		auto featuresNode = root.select_node ("/Repository/Features");

		struct FeatureTreeWalker : public XmlTreeWalker
		{
			FeatureTreeWalker (std::vector<std::unique_ptr<Feature>>& features)
			: features_ (features)
			{
				featureStack_.push (nullptr);
			}

			bool OnEnter (const pugi::xml_node& node)
			{
				return true;
			}

			bool OnNode (const pugi::xml_node& node)
			{
				if (strcmp (node.name (), "Feature") == 0) {
					features_.emplace_back (std::move (new Feature{ node,
						featureStack_.top ()}));
					featureStack_.push (features_.back ().get ());
				}

				return true;
			}

			bool OnLeave (const pugi::xml_node& node)
			{
				if (strcmp (node.name (), "Feature") == 0) {
					featureStack_.pop ();
				}

				return true;
			}

		private:
			std::stack<Feature*> featureStack_;
			std::vector<std::unique_ptr<Feature>>& features_;
		};

		if (!featuresNode) {
			///@TODO(minor) Handle error
		} else {
			FeatureTreeWalker ftw{ features_ };
			Traverse (featuresNode.node (), ftw);
		}

		// Persist all features. As we added them in depth-first order, parents
		// will always get persistet before children, so no special ordering is
		// required here
		for (auto& feature : features_) {
			feature->Store (ctx.buildDatabase);
			repositoryObjects_ [feature->GetUuid ()] = feature.get ();
		}

		// Store the dependencies now that every feature has been stored
		for (auto& feature : features_) {
			feature->StoreDependencies (ctx.buildDatabase);
		}
	}

	void CreateFileStorage (const pugi::xml_node& root, BuildContext& ctx)
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
	using RepositoryObjectMap = std::unordered_map<Uuid, RepositoryObject*,
		ArrayRefHash, ArrayRefEqual>;
	
	RepositoryObjectMap repositoryObjects_;

	std::vector<std::unique_ptr<Feature> > features_;
	std::unique_ptr<FileStorage> fileStorage_;
};
}

namespace kyla {
///////////////////////////////////////////////////////////////////////////////
void BuildRepository (const KylaBuildSettings* settings)
{
	const auto inputFile = settings->descriptorFile;

	std::filesystem::create_directories (settings->targetDirectory);

	auto dbFile = Path{ settings->targetDirectory } / "repository.db";
	std::filesystem::remove (dbFile);

	auto db = Sql::Database::Create (
		dbFile.string ().c_str ());

	db.Execute (install_db_structure);
	db.Execute ("PRAGMA journal_mode=MEMORY;");
	db.Execute ("PRAGMA synchronous=OFF;");

	pugi::xml_document doc;
	if (!doc.load_file (inputFile)) {
		throw RuntimeException ("Could not parse input file.",
			KYLA_FILE_LINE);
	}

	Repository repository;
	std::unique_ptr<BuildContext> ctx (new BuildContext {
		settings->sourceDirectory,
		settings->targetDirectory,
		db
	});
	repository.CreateFeatures (doc, *ctx);

	const auto hashStartTime = std::chrono::high_resolution_clock::now ();
	repository.CreateFileStorage (doc, *ctx);
	const auto hashTime = std::chrono::high_resolution_clock::now () -
		hashStartTime;

	repository.LinkFeatures ();
	repository.PersistFileStorage (*ctx);

	if (settings->buildStatistics) {
		settings->buildStatistics->compressedContentSize = ctx->statistics.bytesStoredCompressed;
		settings->buildStatistics->uncompressedContentSize = ctx->statistics.bytesStoredUncompressed;
		settings->buildStatistics->compressionRatio =
			static_cast<float> (ctx->statistics.bytesStoredUncompressed) /
			static_cast<float> (ctx->statistics.bytesStoredCompressed);
		settings->buildStatistics->compressionTimeSeconds =
			static_cast<double> (ctx->statistics.compressionTime.count ())
			/ 1000000000.0;
		settings->buildStatistics->hashTimeSeconds =
			static_cast<double> (hashTime.count ())
			/ 1000000000.0;
		settings->buildStatistics->encryptionTimeSeconds =
			static_cast<double> (ctx->statistics.encryptionTime.count ())
			/ 1000000000.0;
	}

	ctx.reset ();

	db.Execute ("PRAGMA journal_mode=DELETE;");
	db.Execute ("PRAGMA synchronous=FULL;");
	db.Execute ("PRAGMA optimize;");
	db.Execute ("VACUUM;");

	db.Close ();
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
