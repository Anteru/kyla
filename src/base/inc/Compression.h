/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#ifndef KYLA_CORE_INTERNAL_COMPRESSION_H
#define KYLA_CORE_INTERNAL_COMPRESSION_H

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "ArrayRef.h"

namespace kyla {
enum class CompressionAlgorithm : std::uint8_t
{
	Uncompressed,
	Zip,
	Brotli
};

const char* IdFromCompressionAlgorithm (CompressionAlgorithm algorithm);
CompressionAlgorithm CompressionAlgorithmFromId (const char* id);

struct BlockCompressor
{
public:
	BlockCompressor ();
	virtual ~BlockCompressor ();

	BlockCompressor (const BlockCompressor&) = delete;
	BlockCompressor& operator= (const BlockCompressor&) = delete;

	int64 GetCompressionBound (const int64 inputSize) const;
	int64 Compress (const ArrayRef<>& input, const MutableArrayRef<>& output);
	void Decompress (const ArrayRef<>& input, const MutableArrayRef<>& output);

private:
	virtual int64 GetCompressionBoundImpl (const int64 inputSize) const = 0;
	virtual int64 CompressImpl (const ArrayRef<>& input,
		const MutableArrayRef<>& output) const = 0;
	virtual void DecompressImpl (const ArrayRef<>& input,
		const MutableArrayRef<>& output) const = 0;
};

std::unique_ptr<BlockCompressor> CreateBlockCompressor (CompressionAlgorithm compression);
}

#endif
