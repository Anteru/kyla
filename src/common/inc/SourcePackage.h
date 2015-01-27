#ifndef _NIM_COMMON_SOURCE_PACKAGE_H_
#define _NIM_COMMON_SOURCE_PACKAGE_H_

#include <stdint.h>
#include "Hash.h"

#include <vector>
#include <boost/filesystem.hpp>
#include <memory>
#include <functional>

/**
A source package consists of:
- The package header
- The package index, containing zero or more package index entries
- Zero or more data chunks
*/

struct PackageHeader
{
	unsigned char	id [8];
	int32_t			version;
	int32_t			indexEntries;
	int64_t			indexOffset;
	int64_t			dataOffset;
};

struct PackageIndex
{
    uint8_t hash [64];
	int64_t offset;				// Absolute offset within the source package
								// to the entry
};

enum CompressionMode
{
    CompressionMode_Uncompressed,
    CompressionMode_Zip,
    CompressionMode_LZMA,
	CompressionMode_LZ4,
	CompressionMode_LZHAM
};

struct PackageDataChunk
{
    uint8_t hash [64];
	int64_t offset;				// Offset into the target file
	int64_t size;				// Size of the uncompressed data

	int64_t compressedSize;		// Size of this package chunk
	int8_t  compressionMode;	// One of the CompressionMode entries
	uint8_t reserved [7];
};

/*
The source package writer writes the package during Finalize ().
*/
class SourcePackageWriter final
{
public:
	SourcePackageWriter ();
	~SourcePackageWriter ();

	SourcePackageWriter (const SourcePackageWriter&) = delete;
	SourcePackageWriter& operator=(const SourcePackageWriter&) = delete;

	void Open (const boost::filesystem::path& outputFile);
	void Add (const Hash& hash, const boost::filesystem::path& chunkPath);
	Hash Finalize ();

	bool IsOpen () const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

class ISourcePackageReader
{
public:
	ISourcePackageReader () = default;
	virtual ~ISourcePackageReader () = default;
    ISourcePackageReader (const ISourcePackageReader&) = delete;
    ISourcePackageReader& operator= (const ISourcePackageReader&) = delete;

	void Store (const std::function<bool (const Hash&)>& filter,
		const boost::filesystem::path& directory);

private:
	virtual void StoreImpl (const std::function<bool (const Hash&)>& filter,
		const boost::filesystem::path& directory) = 0;
};

class FileSourcePackageReader final : public ISourcePackageReader
{
public:
	FileSourcePackageReader (const boost::filesystem::path& filename);
	~FileSourcePackageReader ();

private:
	void StoreImpl (const std::function<bool (const Hash&)>& filter,
		const boost::filesystem::path& directory) override;

	struct Impl;
	std::unique_ptr<Impl> impl_;
};

#endif
