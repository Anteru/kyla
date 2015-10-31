#ifndef KYLA_CORE_INTERNAL_COMPRESSION_H
#define KYLA_CORE_INTERNAL_COMPRESSION_H

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "ArrayRef.h"

namespace kyla {
enum class CompressionMode : std::uint8_t
{
	Uncompressed,
	Zip,
	LZMA,
	LZ4,
	LZHAM
};

struct BlockCompressor
{
public:
	BlockCompressor ();
	virtual ~BlockCompressor ();

	BlockCompressor (const BlockCompressor&) = delete;
	BlockCompressor& operator= (const BlockCompressor&) = delete;

	int GetCompressionBound (const int inputSize) const;
	int Compress (const ArrayRef<>& input, const MutableArrayRef<>& output);

private:
	virtual int GetCompressionBoundImpl (const int inputSize) const = 0;
	virtual int CompressImpl (const ArrayRef<>& input,
		const MutableArrayRef<>& output) const = 0;
};
std::unique_ptr<BlockCompressor> CreateBlockCompressor (CompressionMode compression);
}

#endif
