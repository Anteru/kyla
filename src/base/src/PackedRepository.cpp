/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
[LICENSE END]
*/

#include "PackedRepository.h"

#include "sql/Database.h"
#include "Exception.h"
#include "FileIO.h"
#include "Hash.h"
#include "Log.h"

#include "Compression.h"

#include <boost/format.hpp>

#include "install-db-structure.h"
#include "temp-db-structure.h"

#include <unordered_map>
#include <set>

namespace kyla {
///////////////////////////////////////////////////////////////////////////////
PackedRepository::PackedRepository (const char* path)
	: db_ (Sql::Database::Open (Path (path) / "repository.db"))
	, path_ (path)
{
}

///////////////////////////////////////////////////////////////////////////////
PackedRepository::~PackedRepository ()
{
}

///////////////////////////////////////////////////////////////////////////////
Sql::Database& PackedRepository::GetDatabaseImpl ()
{
	return db_;
}

///////////////////////////////////////////////////////////////////////////////
void PackedRepository::GetContentObjectsImpl (const ArrayRef<SHA256Digest>& requestedObjects,
	const Repository::GetContentObjectCallback& getCallback)
{
	// We need to join the requested objects on our existing data, so
	// store them in a temporary table
	auto requestedContentObjects = db_.CreateTemporaryTable (
		"requested_content_objects", "Hash BLOB NOT NULL UNIQUE");

	auto tempObjectInsert = db_.Prepare ("INSERT INTO requested_content_objects "
		"(Hash) VALUES (?)");

	for (const auto& obj : requestedObjects) {
		tempObjectInsert.BindArguments (obj);
		tempObjectInsert.Step ();
		tempObjectInsert.Reset ();
	}

	// Find all source packages we need to handle
	auto findSourcePackagesQuery = db_.Prepare (
		"SELECT DISTINCT "
		"   source_packages.Filename AS Filename, "
		"   source_packages.Id AS Id "
		"FROM storage_mapping "
		"    INNER JOIN content_objects ON storage_mapping.ContentObjectId = content_objects.Id "
		"    INNER JOIN source_packages ON storage_mapping.SourcePackageId = source_packages.Id "
		"WHERE content_objects.Hash IN (SELECT Hash FROM requested_content_objects) "
	);

	auto contentObjectsInPackageQuery = db_.Prepare ("SELECT  "
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

	auto getStorageHashQuery = db_.Prepare (
		"SELECT Hash "
		"FROM storage_hashes "
		"WHERE StorageMappingId = ?");

	std::vector<byte> compressionOutputBuffer;

	while (findSourcePackagesQuery.Step ()) {
		const auto filename = findSourcePackagesQuery.GetText (0);
		const auto id = findSourcePackagesQuery.GetInt64 (1);

		auto packageFile = OpenFile (
			path_ / filename, FileOpenMode::Read
		);

		///@TODO(minor) Validate it's a valid file

		byte* fileMapping = static_cast<byte*> (packageFile->Map ());

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
			
			const auto contentObjectSourceData = ArrayRef<byte>{ fileMapping + packageOffset,
				packageSize };

			// If we find an entry in the storage_hashes table, validate the
			// compressed source data
			getStorageHashQuery.BindArguments (storageMappingId);

			if (getStorageHashQuery.Step ()) {
				SHA256Digest digest;
				getStorageHashQuery.GetBlob (0, digest);

				if (ComputeSHA256 (contentObjectSourceData) != digest) {
					throw RuntimeException ("PackedRepository",
						str (boost::format ("Source data for chunk '%1%' is corrupted") %
							ToString (hash)),
						KYLA_FILE_LINE);
				}
			}
			getStorageHashQuery.Reset ();

			if (compression == nullptr) {
				getCallback (hash,
					contentObjectSourceData, sourceOffset, totalSize);
				continue;
			}

			if (compression != currentCompressorId) {
				// we assume the compressors change infrequently
				compressor = CreateBlockCompressor (
					CompressionAlgorithmFromId (compression)
				);
			}

			compressionOutputBuffer.resize (sourceSize);
			compressor->Decompress (contentObjectSourceData,
				compressionOutputBuffer);

			getCallback (hash, compressionOutputBuffer, sourceOffset, totalSize);
		}

		packageFile->Unmap (fileMapping);

		contentObjectsInPackageQuery.Reset ();
	}
}

///////////////////////////////////////////////////////////////////////////////
void PackedRepository::ValidateImpl (const Repository::ValidationCallback& validationCallback)
{
	// Find all source packages we need to handle
	auto findSourcePackagesQuery = db_.Prepare (
		"SELECT DISTINCT "
		"   source_packages.Filename AS Filename, "
		"   source_packages.Id AS Id "
		"FROM storage_mapping "
		"    INNER JOIN content_objects ON storage_mapping.ContentObjectId = content_objects.Id "
		"    INNER JOIN source_packages ON storage_mapping.SourcePackageId = source_packages.Id"
	);

	auto contentObjectsInPackageQuery = db_.Prepare (
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

	while (findSourcePackagesQuery.Step ()) {
		auto packageFile = OpenFile (
			path_ / findSourcePackagesQuery.GetText (1), FileOpenMode::Read
		);

		///@TODO(minor) Validate it's a valid file

		byte* fileMapping = static_cast<byte*> (packageFile->Map ());

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

			if (compression == nullptr) {
				if (hash != ComputeSHA256 (ArrayRef<byte> (fileMapping + packageOffset,
					packageSize))) {
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
			compressor->Decompress (ArrayRef<byte> {
				fileMapping + packageOffset, packageSize},
				compressionOutputBuffer);

			// Assume for now the content object is not chunked
			if (hash != ComputeSHA256 (compressionOutputBuffer)) {
				// We don't have filenames here - we could fine one if 
				// needed
				validationCallback (hash, nullptr, ValidationResult::Corrupted);
			} else {
				validationCallback (hash, nullptr, ValidationResult::Ok);
			}
		}

		packageFile->Unmap (fileMapping);

		contentObjectsInPackageQuery.Reset ();
	}
}
} // namespace kyla