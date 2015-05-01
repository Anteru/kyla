#ifndef KYLA_CORE_INTERNAL_INSTALLPACKAGE_H
#define KYLA_CORE_INTERNAL_INSTALLPACKAGE_H

#include "Types.h"

namespace kyla {
struct InstallPackageHeader
{
	unsigned char	id [8];
	int32	version;
	int32	indexEntryCount;
	int64	indexOffset;
	uint8	reserved [256];
};

struct InstallPackageIndexEntry
{
	byte	sha512digest [64];

	/*
	One of:
	- InstallationDatabase
	- SourcePackage#UUID
	- Possible extension point
	*/
	char			name [64];

	int64	offset;
	int64	compressedSize;
	int64	uncompressedSize;
	CompressionMode	compressionMode;
	uint8	reserved [7];
};
}

#endif
