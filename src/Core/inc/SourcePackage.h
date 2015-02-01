#ifndef KYLA_CORE_INTERNAL_SOURCEPACKAGE_H
#define KYLA_CORE_INTERNAL_SOURCEPACKAGE_H

#include <cstdint>

namespace kyla {
/**
A source package consists of:
- The package header
- The package index, containing zero or more package index entries
- Zero or more data chunks
*/

struct SourcePackageHeader
{
	unsigned char	id [8];
	std::int32_t	version;
	std::int32_t	indexEntryCount;
	std::int64_t	indexOffset;
};

struct SourcePackageIndexEntry
{
	std::uint8_t hash [64];
	std::int64_t offset;		// Absolute offset within the source package
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

struct SourcePackageDataChunk
{
	std::uint8_t hash [64];
	std::int64_t offset;				// Offset into the target file
	std::int64_t size;				// Size of the uncompressed data

	std::int64_t compressedSize;		// Size of this package chunk
	std::int8_t  compressionMode;	// One of the CompressionMode entries
	std::uint8_t reserved [7];
};
}

#endif
