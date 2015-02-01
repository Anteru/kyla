#ifndef KYLA_CORE_INTERNAL_COMPRESSION_H
#define KYLA_CORE_INTERNAL_COMPRESSION_H

#include <cstdint>

namespace kyla {
enum class CompressionMode : std::uint8_t
{
	Uncompressed,
	Zip,
	LZMA,
	LZ4,
	LZHAM
};
}

#endif
