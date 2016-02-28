#ifndef KYLA_CORE_INTERNAL_SOURCEPACKAGE_H
#define KYLA_CORE_INTERNAL_SOURCEPACKAGE_H

#include "Types.h"
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
	uint8	packageId [16];
	int32	version;
	int32	indexEntryCount;
	int64	indexOffset;
};

struct SourcePackageIndexEntry
{
	byte SHA256digest [32];
	int64 offset;		// Absolute offset within the source package
								// to the entry
};

struct SourcePackageChunk
{
	byte SHA256digest [32];
	int64 offset;				// Offset into the target file
	int64 size;					// Size of the uncompressed data

	int64 compressedSize;		// Size of this package chunk
	CompressionMode compressionMode;
	uint8 reserved [7];
};
}

#endif
