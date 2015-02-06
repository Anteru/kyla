#ifndef KYLA_CORE_INTERNAL_SOURCEPACKAGE_H
#define KYLA_CORE_INTERNAL_SOURCEPACKAGE_H

#include <cstdint>
#include "Compression.h"

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
	std::uint8_t	packageId [16];
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

struct SourcePackageChunk
{
	std::uint8_t hash [64];
	std::int64_t offset;				// Offset into the target file
	std::int64_t size;					// Size of the uncompressed data

	std::int64_t compressedSize;		// Size of this package chunk
	CompressionMode compressionMode;
	std::uint8_t reserved [7];
};
}

#endif
