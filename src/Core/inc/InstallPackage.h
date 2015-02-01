#ifndef KYLA_CORE_INTERNAL_INSTALLPACKAGE_H
#define KYLA_CORE_INTERNAL_INSTALLPACKAGE_H

#include <cstdint>

namespace kyla {
struct InstallPackageHeader
{
	unsigned char	id [8];
	std::int32_t	version;
	std::int32_t	indexEntryCount;
	std::int64_t	indexOffset;
};

struct InstallPackageChunk
{
	std::uint8_t	hash [64];

	/*
	One of:
	- InstallationDatabase
	- SourcePackage#UUID
	- Possible extension point
	*/
	char			name [64];

	std::int64_t	offset;
	std::int64_t	compressedSize;
	std::int64_t	uncompressedSize;
	CompressionMode	compressionMode;
	std::uint8_t	reserved [7];
};
}

#endif
