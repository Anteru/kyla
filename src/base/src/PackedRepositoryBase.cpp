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

#include <fmt/core.h>

#include "install-db-structure.h"

#include <unordered_map>
#include <set>

#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

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

/**
Create a decryptor based on the execution context.

If the EncryptionKey variable is set, the returning decryptor will be non-null.
*/
std::unique_ptr<PackedRepositoryBase::Decryptor> CreateDecryptor (const Repository::ExecutionContext& context)
{
	auto it = context.variables.find (Repository::ExecutionContext::EncryptionKey);
	if (it != context.variables.end ()) {
		return std::make_unique<PackedRepositoryBase::Decryptor> (it->second.GetString ());
	} else {
		return std::unique_ptr<PackedRepositoryBase::Decryptor> ();
	}
}
}

///////////////////////////////////////////////////////////////////////////////
struct PackedRepositoryBase::Decryptor final
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

namespace {
/**
Data shared between all request types.
*/
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

/**
Request to read data from a source repository.
*/
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

/**
Request to process raw read data into data which can be written to disk.
*/
struct ProcessRequest
{
	std::unique_ptr<RequestData> requestData;
	std::vector<byte> inputBuffer;
	int64 size = 0;

	ProcessRequest (std::unique_ptr<RequestData>&& requestData,
		std::vector<byte>&& inputBuffer)
		: requestData (std::move (requestData))
		, inputBuffer (std::move (inputBuffer))
	{
		size = static_cast<int64> (this->inputBuffer.size ());
	}

	ProcessRequest () = default;

	ProcessRequest (ProcessRequest&& other)
		: requestData (std::move (other.requestData))
		, inputBuffer (std::move (other.inputBuffer))
		, size (other.size)
	{
	}

	ProcessRequest& operator= (ProcessRequest&& other)
	{
		requestData = std::move (other.requestData);
		inputBuffer = std::move (other.inputBuffer);
		size = other.size;

		return *this;
	}
};

/**
Request to write some data into a file.
*/
struct OutputRequest
{
	std::unique_ptr<RequestData> requestData;
	std::vector<byte> data;
	int64 size = 0;

	OutputRequest (std::unique_ptr<RequestData>&& requestData,
		std::vector<byte>&& data)
		: requestData (std::move (requestData))
		, data (std::move (data))
	{
		size = static_cast<int64> (this->data.size ());
	}

	OutputRequest () = default;

	OutputRequest (OutputRequest&& other)
		: requestData (std::move (other.requestData))
		, data (std::move (other.data))
		, size (other.size)
	{
	}

	OutputRequest& operator= (OutputRequest&& other)
	{
		requestData = std::move (other.requestData);
		data = std::move (other.data);
		size = other.size;

		other.size = 0;

		return *this;
	}
};

class ProducerConsumerQueueBase
{
protected:
	~ProducerConsumerQueueBase () {};

public:
	virtual void Poison () = 0;
};

/**
This is an internally synchronized producer-consumer-queue. It supports inserting from 
multiple threads, and retrieving from multiple threads. Threads will block on a condition
variable if the queue is empty (during retrieval) or optionally, if it is full (during 
insertion).

The rate limiting works as follows: Any item inserted has a "value" assigned to it (which is
retrieved using a callback). The limit is based on the item value. If the queue has more value
pending than the specified limit, it will start blocking on insertions until enough item value
has been retrieved.

If no limit is specified, it will never block during inserts.
*/
template <typename T>
class ProducerConsumerQueue : public ProducerConsumerQueueBase
{
public:
	ProducerConsumerQueue (std::function<int64 (const T&)> itemValueFunction,
		int64 maxPendingItemValue)
		: itemValueFunction_ (itemValueFunction)
		, maxPendingItemValue_ (maxPendingItemValue)
	{
		assert (maxPendingItemValue > 0);
	}

	ProducerConsumerQueue () = default;

	void Poison () override
	{
		std::unique_lock<std::mutex> lock{ mutex_ };

		poisoned_ = true;
		pendingItemValue_ = 0;

		lock.unlock ();
		conditionVariable_.notify_all ();
	}

	void Insert (T&& t)
	{
		std::unique_lock<std::mutex> lock{ mutex_ };

		if (maxPendingItemValue_ && pendingItemValue_ > maxPendingItemValue_) {
			conditionVariable_.wait (lock, 
				[this]() { return pendingItemValue_ < maxPendingItemValue_ || poisoned_; });
		}

		if (poisoned_) {
			return;
		}

		queue_.emplace_back (std::move (t));
		if (maxPendingItemValue_) {
			pendingItemValue_ += itemValueFunction_ (queue_.back ());
		}

		lock.unlock ();
		conditionVariable_.notify_one ();
	}

	T Get ()
	{
		std::unique_lock<std::mutex> lock{ mutex_ };

		while (queue_.empty () && !poisoned_) {
			conditionVariable_.wait (lock);
		}

		if (poisoned_) {
			return T{};
		}

		auto t = std::move (queue_.front ());
		queue_.pop_front ();

		if (maxPendingItemValue_) {
			pendingItemValue_ -= itemValueFunction_ (t);
		}

		lock.unlock ();
		conditionVariable_.notify_one ();

		return std::move (t);
	}

private:
	std::mutex mutex_;
	std::condition_variable conditionVariable_;
	std::deque<T> queue_;

	std::function<int64 (const T&)> itemValueFunction_;
	int64 maxPendingItemValue_ = 0;
	int64 pendingItemValue_ = 0;
	bool poisoned_ = false;
};

///////////////////////////////////////////////////////////////////////////////
struct ErrorState
{
	std::atomic_bool errorOccurred_;

	std::mutex mutex_;
	std::exception_ptr exception_;

	std::vector<ProducerConsumerQueueBase*> queues_;

	ErrorState ()
	{
		errorOccurred_.store (false);
	}

	void RegisterQueue (ProducerConsumerQueueBase* queue)
	{
		queues_.push_back (queue);
	}

	bool IsSignaled () const
	{
		return static_cast<bool> (errorOccurred_);
	}

	void RegisterException (std::exception_ptr exception)
	{
		errorOccurred_ = true;
		std::lock_guard<std::mutex> lock{ mutex_ };
		exception = exception;

		for (auto& queue : queues_) {
			queue->Poison ();
		}
	}

	void RethrowException ()
	{
		if (!exception_) {
			std::rethrow_exception (exception_);
		}
	}
};

///////////////////////////////////////////////////////////////////////////////
/**
Reads data and produces read requests.
*/
class ReadThread
{
public:
	ReadThread (std::vector<ReadRequest>&& readRequests,
		ProducerConsumerQueue<ProcessRequest>& processRequestQueue,
		ErrorState* errorState)
		: queue_ (processRequestQueue)
		, readRequests_ (std::move (readRequests))
		, errorState_ (errorState)
	{
	}

	void Run ()
	{
		std::thread readThread{ [&] () -> void {
			for (auto& readRequest : readRequests_) {
				if (errorState_->IsSignaled ()) {
					break;
				}

				try {
					auto& rd = readRequest.requestData;

					std::vector<byte> inputBuffer;
					inputBuffer.resize (rd->packageSize);

					rd->packageFile->Read (rd->packageOffset, inputBuffer);

					queue_.Insert ({ std::move (readRequest.requestData), std::move (inputBuffer) });
				} catch (const std::exception&) {
					errorState_->RegisterException (std::current_exception ());

					break;
				}
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
	ErrorState* errorState_ = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
/**
Handles all chunk processing: Decompression, decryption, and hashing.

The process thread consumes read requests, and produces output requests.
*/
class ProcessThread
{
public:
	ProcessThread (ProducerConsumerQueue<ProcessRequest>& processRequestQueue,
		ProducerConsumerQueue<OutputRequest>& outputRequestQueue,
		ErrorState* errorState)
	: inputQueue_ (processRequestQueue)
	, outputQueue_ (outputRequestQueue)
	, errorState_ (errorState)
	{
	}

	void Run ()
	{
		std::thread processThread{ [&] () -> void {
			for (;;) {
				if (errorState_->IsSignaled ()) {
					break;
				}

				try {
					auto processRequest = inputQueue_.Get ();

					if (!processRequest.requestData) {
						break;
					}

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
								fmt::format ("Source data for chunk '{0}' is corrupted",
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

					outputQueue_.Insert ({ 
						std::move (processRequest.requestData), 
						std::move (outputBuffer) });
				} catch (const std::exception&) {
					errorState_->RegisterException (std::current_exception ());

					break;
				}
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
	ErrorState* errorState_;
};

class OutputThread
{
public:
	OutputThread (ProducerConsumerQueue<OutputRequest>& outputRequestQueue,
		ErrorState* errorState)
		: queue_ (outputRequestQueue)
		, errorState_ (errorState)
	{
	}

	void Run ()
	{
		std::thread outputThread{ [&] () -> void {
			for (;;) {
				if (errorState_->IsSignaled ()) {
					break;
				}

				try {
					auto outputRequest = queue_.Get ();

					if (!outputRequest.requestData) {
						break;
					}

					auto& rd = outputRequest.requestData;

					outputRequest.requestData->callback (rd->contentHash, outputRequest.data,
						rd->sourceOffset, rd->totalSize);
				} catch (const std::exception&) {
					errorState_->RegisterException (std::current_exception ());
				
					break;
				}
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
	ErrorState* errorState_;
};
}

///////////////////////////////////////////////////////////////////////////////
void PackedRepositoryBase::GetContentObjectsImpl (const ArrayRef<SHA256Digest>& requestedObjects,
	const Repository::GetContentObjectCallback& getCallback,
	ExecutionContext& context)
{
	auto& db = GetDatabase ();

	std::unique_ptr<Decryptor> decryptor = CreateDecryptor (context);

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

	static constexpr auto MaxPendingProcessSize = 64 << 20;
	static constexpr auto MaxPendingOutputSize = 64 << 20;

	while (findSourcePackagesQuery.Step ()) {
		ProducerConsumerQueue<ProcessRequest> processRequestQueue{
			[] (const ProcessRequest& processRequest) { 
				return static_cast<int64> (processRequest.size); 
		},
			MaxPendingProcessSize
		};
		ProducerConsumerQueue<OutputRequest> outputRequestQueue{
			[] (const OutputRequest& outputRequest) {
			return static_cast<int64> (outputRequest.size);
		},
			MaxPendingOutputSize
		};
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
				if (!decryptor) {
					throw RuntimeException ("PackedRepository",
						"Repository is encrypted but no key has been set",
						KYLA_FILE_LINE);
				}

				requestData->decryptor = decryptor.get ();
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

		ErrorState errorState;

		errorState.RegisterQueue (&processRequestQueue);
		errorState.RegisterQueue (&outputRequestQueue);

		ReadThread readThread{ std::move (readRequests), processRequestQueue, &errorState };
		ProcessThread processThread{ processRequestQueue, outputRequestQueue, &errorState };
		OutputThread outputThread{ outputRequestQueue, &errorState };

		readThread.Run ();
		processThread.Run ();
		outputThread.Run ();

		readThread.Join ();
		processThread.Join ();
		outputThread.Join ();

		contentObjectsInPackageQuery.Reset ();

		if (errorState.IsSignaled ()) {
			errorState.RethrowException ();
		}
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

	std::unique_ptr<Decryptor> decryptor = CreateDecryptor (context);

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
				if (!decryptor) {
					throw RuntimeException ("PackedRepository",
						"Repository is encrypted but no key has been set",
						KYLA_FILE_LINE);
				}

				//@TODO(minor) Check algorithm
				writeBuffer.resize (contentObjectsInPackageQuery.GetInt64 (7));

				decryptor->Decrypt (readBuffer, writeBuffer,
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
