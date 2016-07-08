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

#include "WebRepository.h"

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

#if KYLA_PLATFORM_WINDOWS
#pragma comment(lib, "wininet.lib")

#include <Windows.h>
#include <Wininet.h>

#undef min
#undef max
#undef CreateFile
#endif

namespace kyla {
struct WebRepository::Impl
{
#if KYLA_PLATFORM_WINDOWS
	Impl ()
	{
		internet_ = InternetOpen ("kyla",
			INTERNET_OPEN_TYPE_DIRECT,
			NULL,
			NULL,
			0);
	}

	struct File
	{
	public:
		File (const File&) = delete;
		File& operator= (const File&) = delete;

		File (HINTERNET internet, const char* url)
		{
			handle_ = InternetOpenUrl (internet,
				url,
				NULL, /* headers */
				0, /* header length */
				0, /* flags */
				NULL /* context */);
		}

		~File ()
		{
			InternetCloseHandle (handle_);
		}
		
		int64 Read (const MutableArrayRef<>& buffer)
		{
			int64 readTotal = 0;
			DWORD read = 0;

			while (readTotal < buffer.GetSize ()) {
				DWORD toRead = static_cast<DWORD> (
					std::min<int64> (buffer.GetSize () - readTotal, 
					std::numeric_limits<DWORD>::max ()));
				InternetReadFile (handle_,
					buffer.GetData (), toRead, &read);
				readTotal += read;

				if (read == 0) {
					break;
				}
			}

			return readTotal;
		}

		void Seek (int64 offset)
		{
			LONG upperBits = offset >> 32;
			InternetSetFilePointer (handle_, offset & 0xFFFFFFFF,
				&upperBits, FILE_BEGIN, NULL);
		}

		HINTERNET handle_;
	};

	std::unique_ptr<File> Open (const std::string& file)
	{
		return std::unique_ptr<File> (new File{ internet_, file.c_str () });
	}

	~Impl ()
	{
		InternetCloseHandle (internet_);
	}

	HINTERNET internet_;
#endif
};

///////////////////////////////////////////////////////////////////////////////
WebRepository::WebRepository (const char* path)
	: impl_ (new Impl)
{
	const auto dbWebFile = impl_->Open (std::string (path) + "repository.db");
	url_ = path;
	dbPath_ = GetTemporaryFilename ();

	// Extra scope so it's closed by the time we try to open
	{
		auto dbLocalFile = CreateFile (dbPath_);
		std::vector<byte> buffer;
		buffer.resize (1 << 20); // 1 MiB

		for (;;) {
			const auto read = dbWebFile->Read (buffer);
		
			if (read == 0) {
				break;
			}
		
			dbLocalFile->Write (ArrayRef<byte> {buffer}.Slice (0, read));
		}
	}

	db_ = Sql::Database::Open (dbPath_);
}

///////////////////////////////////////////////////////////////////////////////
WebRepository::~WebRepository ()
{
	db_.Close ();
	boost::filesystem::remove (dbPath_);
}

///////////////////////////////////////////////////////////////////////////////
Sql::Database& WebRepository::GetDatabaseImpl ()
{
	return db_;
}

///////////////////////////////////////////////////////////////////////////////
void WebRepository::GetContentObjectsImpl (const ArrayRef<SHA256Digest>& requestedObjects,
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
		"    storage_mapping.PackageOffset AS PackageOffset,  "
		"    storage_mapping.PackageSize AS PackageSize, "
		"    storage_mapping.SourceOffset AS SourceOffset,  "
		"    content_objects.Hash AS Hash, "
		"    content_objects.Size as TotalSize, "
		"    storage_mapping.Compression AS Compression, "
		"	 storage_mapping.SourceSize AS SourceSize "
		"FROM storage_mapping "
		"INNER JOIN content_objects ON storage_mapping.ContentObjectId = content_objects.Id "
		"INNER JOIN source_packages ON storage_mapping.SourcePackageId = source_packages.Id "
		"WHERE content_objects.Hash IN (SELECT Hash FROM requested_content_objects) "
		"    AND source_packages.Id = ? "
		"ORDER BY PackageOffset ");

	std::vector<byte> compressionOutputBuffer;
	std::vector<byte> readBuffer;

	while (findSourcePackagesQuery.Step ()) {
		const auto filename = findSourcePackagesQuery.GetText (0);
		const auto id = findSourcePackagesQuery.GetInt64 (1);

		auto packageFile = impl_->Open (std::string (url_) + filename);

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

			packageFile->Seek (packageOffset);
			readBuffer.resize (packageSize);
			packageFile->Read (readBuffer);

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

			getCallback (hash, compressionOutputBuffer, sourceOffset, totalSize);
		}

		contentObjectsInPackageQuery.Reset ();
	}
}
} // namespace kyla
