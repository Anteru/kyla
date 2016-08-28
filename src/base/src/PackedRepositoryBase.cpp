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

namespace kyla {
/**
@class PackedRepositoryBase
@brief Base class for packed repositories

This class provides the basic implementation for a packed repository, that is,
a repository which stores the data in one or more source packages indexed using
the storage_mapping and source_packages tables.

The storage access itself is abstracted into the PackageFile class. This class
is used instead of the generic File class as a package file only supports
reading and may not be mapped.
*/

///////////////////////////////////////////////////////////////////////////////
PackedRepositoryBase::~PackedRepositoryBase ()
{
}

///////////////////////////////////////////////////////////////////////////////
void PackedRepositoryBase::GetContentObjectsImpl (const ArrayRef<SHA256Digest>& requestedObjects,
	const Repository::GetContentObjectCallback& getCallback)
{
	auto& db = GetDatabase ();

	// We need to join the requested objects on our existing data, so
	// store them in a temporary table
	auto requestedContentObjects = db.CreateTemporaryTable (
		"requested_content_objects", "Hash BLOB NOT NULL UNIQUE");

	auto tempObjectInsert = db.Prepare ("INSERT INTO requested_content_objects "
		"(Hash) VALUES (?)");

	for (const auto& obj : requestedObjects) {
		tempObjectInsert.BindArguments (obj);
		tempObjectInsert.Step ();
		tempObjectInsert.Reset ();
	}

	// Find all source packages we need to handle
	auto findSourcePackagesQuery = db.Prepare (
		"SELECT DISTINCT "
		"   source_packages.Filename AS Filename, "
		"   source_packages.Id AS Id "
		"FROM storage_mapping "
		"    INNER JOIN content_objects ON storage_mapping.ContentObjectId = content_objects.Id "
		"    INNER JOIN source_packages ON storage_mapping.SourcePackageId = source_packages.Id "
		"WHERE content_objects.Hash IN (SELECT Hash FROM requested_content_objects) "
	);

	// Finds the content objects we need in a particular source package, and
	// sorts them by the in-package offset
	auto contentObjectsInPackageQuery = db.Prepare ("SELECT  "
		"    storage_mapping.PackageOffset AS PackageOffset,  "	// = 0
		"    storage_mapping.PackageSize AS PackageSize, "		// = 1
		"    storage_mapping.SourceOffset AS SourceOffset,  "	// = 2
		"    content_objects.Hash AS Hash, "					// = 3
		"    content_objects.Size as TotalSize, "				// = 4
		"    storage_mapping.Compression AS Compression, "		// = 5
		"	 storage_mapping.SourceSize AS SourceSize, "		// = 6
		"    storage_mapping.Id AS StorageMappingId "			// = 7
		"FROM storage_mapping "
		"INNER JOIN content_objects ON storage_mapping.ContentObjectId = content_objects.Id "
		"INNER JOIN source_packages ON storage_mapping.SourcePackageId = source_packages.Id "
		"WHERE content_objects.Hash IN (SELECT Hash FROM requested_content_objects) "
		"    AND source_packages.Id = ? "
		"ORDER BY PackageOffset ");

	// Data stored in a package may be optionally protected by a hash - those
	// hases live in storage_hashes
	auto getStorageHashQuery = db.Prepare (
		"SELECT Hash "
		"FROM storage_hashes "
		"WHERE StorageMappingId = ?");

	std::vector<byte> compressionOutputBuffer;
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
			SHA256Digest hash;
			contentObjectsInPackageQuery.GetBlob (3, hash);
			const auto totalSize = contentObjectsInPackageQuery.GetInt64 (4);
			const char* compression = contentObjectsInPackageQuery.GetText (5);
			const auto sourceSize = contentObjectsInPackageQuery.GetInt64 (6);
			const auto storageMappingId = contentObjectsInPackageQuery.GetInt64 (7);
			
			readBuffer.resize (packageSize);
			packageFile->Read (packageOffset, readBuffer);

			// If we find an entry in the storage_hashes table, validate the
			// compressed source data
			getStorageHashQuery.BindArguments (storageMappingId);

			if (getStorageHashQuery.Step ()) {
				SHA256Digest digest;
				getStorageHashQuery.GetBlob (0, digest);

				if (ComputeSHA256 (readBuffer) != digest) {
					throw RuntimeException ("PackedRepository",
						str (boost::format ("Source data for chunk '%1%' is corrupted") %
							ToString (hash)),
						KYLA_FILE_LINE);
				}
			}
			getStorageHashQuery.Reset ();

			if (compression == nullptr) {
				getCallback (hash, readBuffer, sourceOffset, totalSize);
				continue;
			}

			if (compression != currentCompressorId) {
				// we assume the compressors change infrequently
				compressor = CreateBlockCompressor (
					CompressionAlgorithmFromId (compression)
				);
			}

			compressionOutputBuffer.resize (sourceSize);
			compressor->Decompress (readBuffer,	compressionOutputBuffer);

			getCallback (hash, compressionOutputBuffer, sourceOffset, 
				totalSize);
		}

		contentObjectsInPackageQuery.Reset ();
	}
}

///////////////////////////////////////////////////////////////////////////////
void PackedRepositoryBase::ValidateImpl (const Repository::ValidationCallback& validationCallback,
	ExecutionContext& context)
{
	auto& db = GetDatabase ();

	// Queries as above
	auto findSourcePackagesQuery = db.Prepare (
		"SELECT DISTINCT "
		"   source_packages.Filename AS Filename, "
		"   source_packages.Id AS Id "
		"FROM storage_mapping "
		"    INNER JOIN content_objects ON storage_mapping.ContentObjectId = content_objects.Id "
		"    INNER JOIN source_packages ON storage_mapping.SourcePackageId = source_packages.Id"
	);

	auto contentObjectsInPackageQuery = db.Prepare (
		"SELECT  "
		"    storage_mapping.PackageOffset AS PackageOffset,  "
		"    storage_mapping.PackageSize AS PackageSize, "
		"    storage_mapping.SourceOffset AS SourceOffset,  "
		"    content_objects.Hash AS Hash, "
		"    storage_mapping.Compression AS Compression, "
		"	 storage_mapping.SourceSize AS SourceSize "
		"FROM storage_mapping "
		"INNER JOIN content_objects ON storage_mapping.ContentObjectId = content_objects.Id "
		"INNER JOIN source_packages ON storage_mapping.SourcePackageId = source_packages.Id "
		"WHERE source_packages.Id = ? "
		"ORDER BY PackageOffset ");

	std::vector<byte> compressionOutputBuffer;
	std::vector<byte> readBuffer;

	while (findSourcePackagesQuery.Step ()) {
		auto packageFile = OpenPackage (findSourcePackagesQuery.GetText (0));
		
		contentObjectsInPackageQuery.BindArguments (
			findSourcePackagesQuery.GetInt64 (0));

		std::string currentCompressorId;
		std::unique_ptr<BlockCompressor> compressor;

		while (contentObjectsInPackageQuery.Step ()) {
			const auto packageOffset = contentObjectsInPackageQuery.GetInt64 (0);
			const auto packageSize = contentObjectsInPackageQuery.GetInt64 (1);
			SHA256Digest hash;
			contentObjectsInPackageQuery.GetBlob (3, hash);
			const char* compression = contentObjectsInPackageQuery.GetText (4);
			const auto sourceSize = contentObjectsInPackageQuery.GetInt64 (5);

			readBuffer.resize (packageSize);
			packageFile->Read (packageOffset, readBuffer);

			if (compression == nullptr) {
				if (hash != ComputeSHA256 (readBuffer)) {
					// We don't have filenames here - we could fine one if 
					// needed
					validationCallback (hash, nullptr, ValidationResult::Corrupted);
				} else {
					validationCallback (hash, nullptr, ValidationResult::Ok);
				}
				continue;
			}

			if (compression != currentCompressorId) {
				// we assume the compressors change infrequently
				compressor = CreateBlockCompressor (
					CompressionAlgorithmFromId (compression)
				);
			}

			compressionOutputBuffer.resize (sourceSize);
			compressor->Decompress (readBuffer, compressionOutputBuffer);

			// Assume for now the content object is not chunked
			if (hash != ComputeSHA256 (compressionOutputBuffer)) {
				// We don't have filenames here - we could fine one if 
				// needed
				validationCallback (hash, nullptr, ValidationResult::Corrupted);
			} else {
				validationCallback (hash, nullptr, ValidationResult::Ok);
			}
		}

		contentObjectsInPackageQuery.Reset ();
	}
}
} // namespace kyla