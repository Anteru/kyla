/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#include "PackedRepositoryBase.h"

#include "sql/Database.h"
#include "Exception.h"
#include "FileIO.h"
#include "Hash.h"
#include "Log.h"

#include "Compression.h"

#include <boost/format.hpp>

#include "install-db-structure.h"

#include <unordered_map>
#include <set>

#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <openssl/evp.h>

namespace kyla {
namespace {
struct AES256IvSalt
{
	std::array<byte, 16> iv;
	std::array<byte, 8> salt;
};

///////////////////////////////////////////////////////////////////////////////
AES256IvSalt UnpackAES256IvSalt (const void* data)
{
	AES256IvSalt result;

	::memcpy (result.salt.data (), data, 8);
	::memcpy (result.iv.data (), static_cast<const byte*> (data) + 8, 16);

	return result;
}
}

struct PackedRepositoryBase::Decryptor
{
	Decryptor (const std::string& key)
	: key_ (key)
	{
		decryptionContext_ = EVP_CIPHER_CTX_new ();
		decryptionCypher_ = EVP_aes_256_cbc ();
	}

	~Decryptor ()
	{
		EVP_CIPHER_CTX_free (decryptionContext_);
	}

	void Decrypt (std::vector<byte>& input, std::vector<byte>& output,
		const AES256IvSalt& ivSalt)
	{
		byte key [64] = { 0 };
		PKCS5_PBKDF2_HMAC_SHA1 (key_.data (),
			static_cast<int> (key_.size ()),
			ivSalt.salt.data (), static_cast<int> (ivSalt.salt.size ()),
			4096, 64, key);

		// Extra memory for the decryption padding handling
		output.resize (output.size () + 32);

		EVP_DecryptInit_ex (decryptionContext_, decryptionCypher_,
			nullptr, key, ivSalt.iv.data ());

		int bytesWritten = 0;
		int outputLength = static_cast<int> (output.size ());
		EVP_DecryptUpdate (decryptionContext_, output.data (),
			&outputLength,
			input.data (), static_cast<int> (input.size ()));
		bytesWritten += outputLength;
		EVP_DecryptFinal_ex (decryptionContext_,
			output.data () + bytesWritten, &outputLength);
		bytesWritten += outputLength;
		output.resize (bytesWritten);
	}

	EVP_CIPHER_CTX*	decryptionContext_ = nullptr;
	const EVP_CIPHER* decryptionCypher_ = nullptr;
	std::string key_;
};

/**
@class PackedRepositoryBase
@brief Base class for packed repositories

This class provides the basic implementation for a packed repository, that is,
a repository which stores the data in one or more source packages indexed using
the storage_mapping and fs_packages tables.

The storage access itself is abstracted into the PackageFile class. This class
is used instead of the generic File class as a package file only supports
reading and may not be mapped.
*/

///////////////////////////////////////////////////////////////////////////////
PackedRepositoryBase::PackedRepositoryBase ()
{
}

///////////////////////////////////////////////////////////////////////////////
PackedRepositoryBase::~PackedRepositoryBase ()
{
}

///////////////////////////////////////////////////////////////////////////////
void PackedRepositoryBase::SetDecryptionKeyImpl (const std::string& key)
{
	decryptor_.reset (new Decryptor{ key });
	key_ = key;
}

namespace {
struct RequestData
{
	PackedRepositoryBase::PackageFile* packageFile = nullptr;
	
	// Inside the package file
	int64 packageOffset = -1;
	int64 packageSize = -1;

	int64 sourceSize = -1;
	int64 sourceOffset = -1;

	int64 totalSize = -1;

	bool hasChunkHash = false;
	SHA256Digest chunkHash;

	CompressionAlgorithm compressionAlgorithm = CompressionAlgorithm::Uncompressed;
	int64 compressionInputSize = 0;
	int64 compressionOutputSize = 0;

	PackedRepositoryBase::Decryptor* decryptor = nullptr;
	AES256IvSalt ivSalt;
	int64 encryptionInputSize = 0;
	int64 encryptionOutputSize = 0;

	SHA256Digest contentHash;

	Repository::GetContentObjectCallback callback;
};

struct ReadRequest
{
	std::unique_ptr<RequestData> requestData;

	ReadRequest () = default;

	ReadRequest (ReadRequest&& other)
		: requestData (std::move (other.requestData))
	{
	}

	ReadRequest& operator= (ReadRequest&& other)
	{
		requestData = std::move (other.requestData);
		return *this;
	}
};

struct ProcessRequest
{
	std::unique_ptr<RequestData> requestData;
	std::vector<byte> inputBuffer;

	ProcessRequest () = default;

	ProcessRequest (ProcessRequest&& other)
		: requestData (std::move (other.requestData))
		, inputBuffer (std::move (other.inputBuffer))
	{
	}

	ProcessRequest& operator==(ProcessRequest&& other)
	{
		requestData = std::move (other.requestData);
		inputBuffer = std::move (other.inputBuffer);
		return *this;
	}
};

struct OutputRequest
{
	std::unique_ptr<RequestData> requestData;
	std::vector<byte> data;

	OutputRequest () = default;

	OutputRequest (OutputRequest&& other)
		: requestData (std::move (other.requestData))
		, data (std::move (other.data))
	{
	}

	OutputRequest& operator== (OutputRequest&& other)
	{
		requestData = std::move (other.requestData);
		data = std::move (other.data);
		return *this;
	}
};

template <typename T>
class ProducerConsumerQueue
{
public:
	void Insert (T&& t)
	{
		std::unique_lock<std::mutex> lock{ mutex_ };

		queue_.emplace_back (std::move (t));
		
		lock.unlock ();
		conditionVariable_.notify_one ();
	}

	T Get ()
	{
		std::unique_lock<std::mutex> lock{ mutex_ };

		while (queue_.empty ()) {
			conditionVariable_.wait (lock);
		}

		auto t = std::move (queue_.front ());
		queue_.pop_front ();
		
		lock.unlock ();

		return std::move (t);
	}

private:
	std::mutex mutex_;
	std::condition_variable conditionVariable_;
	std::deque<T> queue_;
};

class ReadThread
{
public:
	ReadThread (std::vector<ReadRequest>&& readRequests,
		ProducerConsumerQueue<ProcessRequest>& processRequestQueue)
		: queue_ (processRequestQueue)
		, readRequests_ (std::move (readRequests))
	{
	}

	void Run ()
	{
		std::thread readThread{ [&] () -> void {
			for (auto& readRequest : readRequests_) {
				auto& rd = readRequest.requestData;

				ProcessRequest processRequest;
				processRequest.inputBuffer.resize (rd->packageSize);

				rd->packageFile->Read (rd->packageOffset, processRequest.inputBuffer);
				processRequest.requestData = std::move (readRequest.requestData);

				queue_.Insert (std::move (processRequest));
			}

			queue_.Insert (std::move (ProcessRequest{}));
		}
		};

		thread_ = std::move (readThread);
	}

	void Join ()
	{
		thread_.join ();
	}

private:
	ProducerConsumerQueue<ProcessRequest>& queue_;
	std::vector<ReadRequest> readRequests_;
	std::thread thread_;
};

class ProcessThread
{
public:
	ProcessThread (ProducerConsumerQueue<ProcessRequest>& processRequestQueue,
		ProducerConsumerQueue<OutputRequest>& outputRequestQueue)
	: inputQueue_ (processRequestQueue)
	, outputQueue_ (outputRequestQueue)
	{
	}

	void Run ()
	{
		std::thread processThread{ [&] () -> void {
			for (;;) {
				auto processRequest = inputQueue_.Get ();

				if (!processRequest.requestData) {
					break;
				}

				OutputRequest outputRequest;

				auto& rd = processRequest.requestData;

				auto& inputBuffer = processRequest.inputBuffer;
				std::vector<byte> outputBuffer;

				// Encryption
				if (rd->decryptor) {
					outputBuffer.resize (rd->encryptionOutputSize);
					rd->decryptor->Decrypt (inputBuffer, outputBuffer,
						rd->ivSalt);
					std::swap (inputBuffer, outputBuffer);
				}

				// Hash check
				if (rd->hasChunkHash) {
					if (ComputeSHA256 (inputBuffer) != rd->chunkHash) {
						throw RuntimeException ("PackedRepository",
							str (boost::format ("Source data for chunk '%1%' is corrupted") %
								ToString (rd->chunkHash)),
							KYLA_FILE_LINE);
					}
				}

				// Decompression
				if (rd->compressionAlgorithm != CompressionAlgorithm::Uncompressed) {
					auto decompressor = CreateBlockCompressor (rd->compressionAlgorithm);

					assert (rd->compressionInputSize == static_cast<int64> (inputBuffer.size ()));

					outputBuffer.resize (rd->compressionOutputSize);

					decompressor->Decompress (inputBuffer, outputBuffer);
				} else {
					std::swap (inputBuffer, outputBuffer);
				}

				outputRequest.data = std::move (outputBuffer);
				outputRequest.requestData = std::move (processRequest.requestData);

				outputQueue_.Insert (std::move (outputRequest));
			}

			outputQueue_.Insert (OutputRequest{});
		}
		};

		thread_ = std::move (processThread);
	}

	void Join ()
	{
		thread_.join ();
	}

private:
	ProducerConsumerQueue<ProcessRequest>& inputQueue_;
	ProducerConsumerQueue<OutputRequest>& outputQueue_;
	std::thread thread_;
};

class OutputThread
{
public:
	OutputThread (ProducerConsumerQueue<OutputRequest>& outputRequestQueue)
		: queue_ (outputRequestQueue)
	{
	}

	void Run ()
	{
		std::thread outputThread{ [&] () -> void {
			for (;;) {
				auto outputRequest = queue_.Get ();

				if (!outputRequest.requestData) {
					break;
				}

				auto& rd = outputRequest.requestData;

				outputRequest.requestData->callback (rd->contentHash, outputRequest.data,
					rd->sourceOffset, rd->totalSize);
			}
		}
		};

		thread_ = std::move (outputThread);
	}

	void Join ()
	{
		thread_.join ();
	}

private:
	ProducerConsumerQueue<OutputRequest>& queue_;
	std::thread thread_;
};
}

///////////////////////////////////////////////////////////////////////////////
void PackedRepositoryBase::GetContentObjectsImpl (const ArrayRef<SHA256Digest>& requestedObjects,
	const Repository::GetContentObjectCallback& getCallback)
{
	auto& db = GetDatabase ();

	// We need to join the requested objects on our existing data, so
	// store them in a temporary table
	auto requestedContentObjects = db.CreateTemporaryTable (
		"requested_fs_contents", "Hash BLOB NOT NULL UNIQUE");

	auto tempObjectInsert = db.Prepare ("INSERT INTO requested_fs_contents "
		"(Hash) VALUES (?)");

	for (const auto& obj : requestedObjects) {
		tempObjectInsert.BindArguments (obj);
		tempObjectInsert.Step ();
		tempObjectInsert.Reset ();
	}

	// Find all source packages we need to handle
	auto findSourcePackagesQuery = db.Prepare (
		"SELECT DISTINCT "
		"   fs_packages.Filename AS Filename, "
		"   fs_packages.Id AS Id "
		"FROM fs_chunks "
		"    INNER JOIN fs_contents ON fs_chunks.ContentId = fs_contents.Id "
		"    INNER JOIN fs_packages ON fs_chunks.PackageId = fs_packages.Id "
		"WHERE fs_contents.Hash IN (SELECT Hash FROM requested_fs_contents) "
	);

	// Finds the content objects we need in a particular source package, and
	// sorts them by the in-package offset
	auto contentObjectsInPackageQuery = db.Prepare ("SELECT "
		"	PackageOffset,  "			// = 0
		"	PackageSize, "				// = 1
		"	SourceOffset,  "			// = 2
		"	ContentHash, "				// = 3
		"	TotalSize, "				// = 4
		"	SourceSize, "				// = 5
		"	CompressionAlgorithm, "		// = 6
		"	CompressionInputSize, "		// = 7
		"	CompressionOutputSize, "	// = 8
		"	EncryptionAlgorithm, "		// = 9
		"	EncryptionData, "			// = 10
		"	EncryptionInputSize, "		// = 11
		"	EncryptionOutputSize, "		// = 12
		"	StorageHash "				// = 13
		"FROM fs_content_view "
		"WHERE ContentHash IN (SELECT Hash FROM requested_fs_contents) "
		"    AND PackageId = ? ");

	while (findSourcePackagesQuery.Step ()) {
		ProducerConsumerQueue<ProcessRequest> processRequestQueue;
		ProducerConsumerQueue<OutputRequest> outputRequestQueue;
		std::vector<ReadRequest> readRequests;

		const auto filename = findSourcePackagesQuery.GetText (0);
		const auto id = findSourcePackagesQuery.GetInt64 (1);

		auto packageFile = OpenPackage (filename);

		contentObjectsInPackageQuery.BindArguments (id);

		while (contentObjectsInPackageQuery.Step ()) {
			std::unique_ptr<RequestData> requestData{ new RequestData };

			requestData->packageOffset = contentObjectsInPackageQuery.GetInt64 (0);
			requestData->packageSize = contentObjectsInPackageQuery.GetInt64 (1);
			requestData->sourceOffset = contentObjectsInPackageQuery.GetInt64 (2);
			contentObjectsInPackageQuery.GetBlob (3, requestData->contentHash);
			requestData->totalSize = contentObjectsInPackageQuery.GetInt64 (4);
			requestData->sourceSize = contentObjectsInPackageQuery.GetInt64 (5);

			requestData->callback = getCallback;

			// Encryption handling
			if (contentObjectsInPackageQuery.GetText (10)) {
				if (!decryptor_) {
					throw RuntimeException ("PackedRepository",
						"Repository is encrypted but no key has been set",
						KYLA_FILE_LINE);
				}

				requestData->decryptor = decryptor_.get ();
				requestData->encryptionOutputSize = contentObjectsInPackageQuery.GetInt64 (12);
				requestData->ivSalt = UnpackAES256IvSalt (contentObjectsInPackageQuery.GetBlob (10));
			}

			// Hash handling
			if (contentObjectsInPackageQuery.GetColumnType (13) != Sql::Type::Null) {
				requestData->hasChunkHash = true;
				contentObjectsInPackageQuery.GetBlob (13, requestData->chunkHash);
			} else {
				assert (requestData->sourceSize == 0);
			}

			// Compression handling
			if (contentObjectsInPackageQuery.GetText (6)) {
				requestData->compressionAlgorithm = CompressionAlgorithmFromId (contentObjectsInPackageQuery.GetText (6));
				requestData->compressionOutputSize = contentObjectsInPackageQuery.GetInt64 (7);
				requestData->compressionInputSize = contentObjectsInPackageQuery.GetInt64 (8);
			}

			requestData->packageFile = packageFile.get ();

			ReadRequest readRequest;
			readRequest.requestData = std::move (requestData);

			readRequests.emplace_back (std::move (readRequest));
		}

		ReadThread readThread{ std::move (readRequests), processRequestQueue };
		ProcessThread processThread{ processRequestQueue, outputRequestQueue };
		OutputThread outputThread{ outputRequestQueue };

		readThread.Run ();
		processThread.Run ();
		outputThread.Run ();

		readThread.Join ();
		processThread.Join ();
		outputThread.Join ();

		contentObjectsInPackageQuery.Reset ();
	}
}

///////////////////////////////////////////////////////////////////////////////
void PackedRepositoryBase::RepairImpl (Repository& source,
	ExecutionContext& context,
	RepairCallback repairCallback,
	bool restore)
{
	// A packed repository can't restore files
	assert (restore == false);

	auto& db = GetDatabase ();

	// Queries as above
	auto findSourcePackagesQuery = db.Prepare (
		"SELECT DISTINCT "
		"   fs_packages.Filename AS Filename, "
		"   fs_packages.Id AS Id "
		"FROM storage_mapping "
		"    INNER JOIN fs_contents ON fs_chunks.ContentId = fs_contents.Id "
		"    INNER JOIN fs_packages ON fs_chunks.PackageId = fs_packages.Id"
	);

	auto contentObjectsInPackageQuery = db.Prepare (
		"SELECT "
		"	PackageOffset,  "				// = 0
		"	PackageSize, "					// = 1
		"	SourceOffset,  "				// = 2
		"	SourceSize, "					// = 3
		"	EncryptionAlgorithm, "			// = 4
		"	EncryptionData, "				// = 5
		"	EncryptionInputSize, "			// = 6
		"	EncryptionOutputSize, "			// = 7
		"	StorageHash, "					// = 8
		"	ContentHash "					// = 9
		"FROM fs_content_view "
		"WHERE PackageId = ? ");

	std::vector<byte> compressionOutputBuffer;
	std::vector<byte> readBuffer, writeBuffer;

	while (findSourcePackagesQuery.Step ()) {
		auto packageFile = OpenPackage (findSourcePackagesQuery.GetText (0));

		contentObjectsInPackageQuery.BindArguments (
			findSourcePackagesQuery.GetInt64 (0));

		std::string currentCompressorId;
		std::unique_ptr<BlockCompressor> compressor;

		while (contentObjectsInPackageQuery.Step ()) {
			const auto packageOffset = contentObjectsInPackageQuery.GetInt64 (0);
			const auto packageSize = contentObjectsInPackageQuery.GetInt64 (1);
			const auto sourceSize = contentObjectsInPackageQuery.GetInt64 (3);

			readBuffer.resize (packageSize);
			packageFile->Read (packageOffset, readBuffer);

			// Decrypt if needed
			if (contentObjectsInPackageQuery.GetText (4)) {
				if (!decryptor_) {
					throw RuntimeException ("PackedRepository",
						"Repository is encrypted but no key has been set",
						KYLA_FILE_LINE);
				}

				//@TODO(minor) Check algorithm
				writeBuffer.resize (contentObjectsInPackageQuery.GetInt64 (7));

				decryptor_->Decrypt (readBuffer, writeBuffer,
					UnpackAES256IvSalt (contentObjectsInPackageQuery.GetBlob (5)));

				std::swap (readBuffer, writeBuffer);
			}

			SHA256Digest contentDigest, storageDigest;
			contentObjectsInPackageQuery.GetBlob (8, storageDigest);
			contentObjectsInPackageQuery.GetBlob (9, contentDigest);

			auto actualHash = ComputeSHA256 (readBuffer);
			auto hashString = ToString (actualHash);

			if (actualHash != storageDigest) {
				repairCallback (hashString.c_str (), RepairResult::Corrupted);
			} else {
				repairCallback (hashString.c_str (), RepairResult::Ok);
			}
		}

		contentObjectsInPackageQuery.Reset ();
	}
}
} // namespace kyla
