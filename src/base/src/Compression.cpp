/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#include "Compression.h"

#include <cstring>
#include <zlib.h>
#include <vector>
#include <encode.h>
#include <decode.h>

#include "Exception.h"

namespace kyla {
///////////////////////////////////////////////////////////////////////////////
struct NullBlockCompressor final : public BlockCompressor
{
	int64 GetCompressionBoundImpl (const int64 inputSize) const override;
	int64 CompressImpl (const ArrayRef<>& input,
		const MutableArrayRef<>& output) const override;
	void DecompressImpl (const ArrayRef<>& input,
		const MutableArrayRef<>& output) const override;
};

///////////////////////////////////////////////////////////////////////////////
struct ZipBlockCompressor final : public BlockCompressor
{
	int64 GetCompressionBoundImpl (const int64 inputSize) const override;
	int64 CompressImpl (const ArrayRef<>& input,
		const MutableArrayRef<>& output) const override;
	void DecompressImpl (const ArrayRef<>& input,
		const MutableArrayRef<>& output) const override;
};

///////////////////////////////////////////////////////////////////////////////
struct BrotliBlockCompressor final : public BlockCompressor
{
	int64 GetCompressionBoundImpl (const int64 inputSize) const override;
	int64 CompressImpl (const ArrayRef<>& input,
		const MutableArrayRef<>& output) const override;
	void DecompressImpl (const ArrayRef<>& input,
		const MutableArrayRef<>& output) const override;
};

///////////////////////////////////////////////////////////////////////////////
BlockCompressor::BlockCompressor ()
{
}

///////////////////////////////////////////////////////////////////////////////
BlockCompressor::~BlockCompressor ()
{
}

///////////////////////////////////////////////////////////////////////////////
int64 BlockCompressor::GetCompressionBound (const int64 inputSize) const
{
	return GetCompressionBoundImpl (inputSize);
}

///////////////////////////////////////////////////////////////////////////////
int64 BlockCompressor::Compress (const ArrayRef<>& input,
	const MutableArrayRef<>& output)
{
	return CompressImpl (input, output);
}

///////////////////////////////////////////////////////////////////////////////
void BlockCompressor::Decompress (const ArrayRef<>& input,
	const MutableArrayRef<>& output)
{
	DecompressImpl (input, output);
}

///////////////////////////////////////////////////////////////////////////////
int64 ZipBlockCompressor::GetCompressionBoundImpl (const int64 inputSize) const
{
	if (inputSize > std::numeric_limits<uLong>::max ()) {
		throw RuntimeException ("Invalid buffer size",
			KYLA_FILE_LINE);
	}

	return ::compressBound (static_cast<uLong> (inputSize));
}

///////////////////////////////////////////////////////////////////////////////
int64 ZipBlockCompressor::CompressImpl (const ArrayRef<>& input,
	const MutableArrayRef<>& output) const
{
	if (input.GetSize () > std::numeric_limits<uLong>::max ()) {
		throw RuntimeException ("Invalid buffer size",
			KYLA_FILE_LINE);
	}

	::uLongf compressedSize = static_cast<uLongf> (output.GetSize ());
	::compress (
		static_cast<::Bytef*> (output.GetData()),
		&compressedSize,
		static_cast<const ::Bytef*> (input.GetData ()),
		static_cast<uLong> (input.GetSize ()));
	return compressedSize;
}

///////////////////////////////////////////////////////////////////////////////
void ZipBlockCompressor::DecompressImpl (const ArrayRef<>& input,
	const MutableArrayRef<>& output) const
{
	if (output.GetSize () > std::numeric_limits<uLong>::max ()) {
		throw RuntimeException ("Invalid buffer size",
			KYLA_FILE_LINE);
	}

	::uLongf decompressedSize = static_cast<uLongf> (output.GetSize ());
	::uncompress (
		static_cast<::Bytef*> (output.GetData ()),
		&decompressedSize,
		static_cast<const ::Bytef*> (input.GetData ()),
		static_cast<uLong> (input.GetSize ()));
}

///////////////////////////////////////////////////////////////////////////////
int64 NullBlockCompressor::GetCompressionBoundImpl (const int64 inputSize) const
{
	return inputSize;
}

///////////////////////////////////////////////////////////////////////////////
int64 NullBlockCompressor::CompressImpl (const ArrayRef<>& input,
	const MutableArrayRef<>& output) const
{
	::memcpy (output.GetData (), input.GetData (), input.GetSize ());

	return static_cast<int> (input.GetSize ());
}

///////////////////////////////////////////////////////////////////////////////
void NullBlockCompressor::DecompressImpl (const ArrayRef<>& input,
	const MutableArrayRef<>& output) const
{
	::memcpy (output.GetData (), input.GetData (), input.GetSize ());
}

///////////////////////////////////////////////////////////////////////////////
int64 BrotliBlockCompressor::GetCompressionBoundImpl (const int64 input_size) const
{
	// Only relevant on 32-bit systems
	if ((sizeof (input_size) > sizeof (size_t))
		&& (input_size > static_cast<int64> (std::numeric_limits<size_t>::max ()))) {
		throw RuntimeException ("Invalid buffer size",
			KYLA_FILE_LINE);
	}

	///@TODO(minor) Replace with BrotliEncoderMaxCompressedSize once available
	/*
	This is copy-pasted from Brotli 0.5 which isn't available as a stable
	release yet.

	*/
	size_t num_large_blocks = input_size >> 24;
	size_t tail = input_size - (num_large_blocks << 24);
	size_t tail_overhead = (tail > (1 << 20)) ? 4 : 3;
	size_t overhead = 2 + (4 * num_large_blocks) + tail_overhead + 1;
	size_t result = input_size + overhead;
	if (input_size == 0) return 1;
	return (result < static_cast<size_t> (input_size)) ? 0 : result;
}

///////////////////////////////////////////////////////////////////////////////
int64 BrotliBlockCompressor::CompressImpl (const ArrayRef<>& input,
	const MutableArrayRef<>& output) const
{
	size_t encodedSize = output.GetSize ();
	auto brotliParams = brotli::BrotliParams ();
	brotliParams.quality = 5;

	brotli::BrotliCompressBuffer (brotliParams,
		input.GetSize (), static_cast<const uint8_t*> (input.GetData ()),
		&encodedSize, reinterpret_cast<uint8_t*> (output.GetData ()));
	///@TODO(minor) check for overflow
	return encodedSize;
}

///////////////////////////////////////////////////////////////////////////////
void BrotliBlockCompressor::DecompressImpl (const ArrayRef<>& input,
	const MutableArrayRef<>& output) const
{
	size_t decodedSize = output.GetSize ();
	BrotliDecompressBuffer (input.GetSize (),
		static_cast<const uint8_t*> (input.GetData ()),
		&decodedSize, static_cast<uint8_t*> (output.GetData ()));
	assert (decodedSize == output.GetSize ());
}

///////////////////////////////////////////////////////////////////////////////
std::unique_ptr<BlockCompressor> CreateBlockCompressor (CompressionAlgorithm compression)
{
	switch (compression) {
	case CompressionAlgorithm::Zip:
		return std::unique_ptr<BlockCompressor> (new ZipBlockCompressor);

	case CompressionAlgorithm::Uncompressed:
		return std::unique_ptr<BlockCompressor> (new NullBlockCompressor);

	case CompressionAlgorithm::Brotli:
		return std::unique_ptr<BlockCompressor> (new BrotliBlockCompressor);
	}

	return std::unique_ptr<BlockCompressor> ();
}

///////////////////////////////////////////////////////////////////////////////
const char* IdFromCompressionAlgorithm (CompressionAlgorithm algorithm)
{
	switch (algorithm) {
	case CompressionAlgorithm::Uncompressed:
		return nullptr;
	case CompressionAlgorithm::Zip:
		return "ZIP";
	case CompressionAlgorithm::Brotli:
		return "Brotli";
	default:
		return nullptr;
	}
}

///////////////////////////////////////////////////////////////////////////////
CompressionAlgorithm CompressionAlgorithmFromId (const char* id)
{
	if (id == nullptr) {
		return CompressionAlgorithm::Uncompressed;
	} else if (strcmp (id, "ZIP") == 0) {
		return CompressionAlgorithm::Zip;
	} else if (strcmp (id, "Brotli") == 0) {
		return CompressionAlgorithm::Brotli;
	}

	return CompressionAlgorithm::Uncompressed;
}
}
