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

	EVP_CIPHER_CTX*	decryptionContext_;
	const EVP_CIPHER* decryptionCypher_;
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

	std::vector<byte> writeBuffer;
	std::vector<byte> readBuffer;

	while (findSourcePackagesQuery.Step ()) {
		const auto filename = findSourcePackagesQuery.GetText (0);
		const auto id = findSourcePackagesQuery.GetInt64 (1);

		auto packageFile = OpenPackage (filename);

		contentObjectsInPackageQuery.BindArguments (id);

		std::string currentCompressorId;
		std::unique_ptr<BlockCompressor> compressor;

		while (contentObjectsInPackageQuery.Step ()) {
			const auto packageOffset = contentObjectsInPackageQuery.GetInt64 (0);
			const auto packageSize = contentObjectsInPackageQuery.GetInt64 (1);
			const auto sourceOffset = contentObjectsInPackageQuery.GetInt64 (2);
			const auto totalSize = contentObjectsInPackageQuery.GetInt64 (4);
			const auto sourceSize = contentObjectsInPackageQuery.GetInt64 (5);

			readBuffer.resize (packageSize);
			packageFile->Read (packageOffset, readBuffer);

			// If encrypted, we need to decrypt first
			if (contentObjectsInPackageQuery.GetText (10)) {
				if (!decryptor_) {
					throw RuntimeException ("PackedRepository",
						"Repository is encrypted but no key has been set",
						KYLA_FILE_LINE);
				}

				//@TODO(minor) Check algorithm
				writeBuffer.resize (contentObjectsInPackageQuery.GetInt64 (12));
				
				decryptor_->Decrypt (readBuffer, writeBuffer,
					UnpackAES256IvSalt (
						contentObjectsInPackageQuery.GetBlob (10)));
				std::swap (readBuffer, writeBuffer);
			}

			SHA256Digest hash;
			contentObjectsInPackageQuery.GetBlob (3, hash);

			if (contentObjectsInPackageQuery.GetColumnType (13) != Sql::Type::Null) {
				SHA256Digest storageDigest;
				contentObjectsInPackageQuery.GetBlob (13, storageDigest);
				if (ComputeSHA256 (readBuffer) != storageDigest) {
					throw RuntimeException ("PackedRepository",
						str (boost::format ("Source data for chunk '%1%' is corrupted") %
							ToString (hash)),
						KYLA_FILE_LINE);
				}
			} else {
				assert (sourceSize == 0);
			}

			if (contentObjectsInPackageQuery.GetText (6)) {
				const auto compression = contentObjectsInPackageQuery.GetText (6);
				const auto uncompressedSize = contentObjectsInPackageQuery.GetInt64 (7);
				const auto compressedSize = contentObjectsInPackageQuery.GetInt64 (8);

				if (compression != currentCompressorId) {
					// we assume the compressors change infrequently
					compressor = CreateBlockCompressor (
						CompressionAlgorithmFromId (compression)
					);
				}

				writeBuffer.resize (uncompressedSize);
				compressor->Decompress (readBuffer, writeBuffer);

				getCallback (hash, writeBuffer, sourceOffset,
					totalSize);
			} else {
				getCallback (hash, readBuffer, sourceOffset, totalSize);
			}
		}

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
		"WHERE ContentHash IN (SELECT Hash FROM requested_fs_contents) "
		"    AND PackageId = ? ");

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
