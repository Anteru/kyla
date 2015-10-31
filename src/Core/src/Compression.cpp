#include "Compression.h"

#include <cstring>
#include <zlib.h>
#include <vector>

namespace kyla {
struct NullBlockCompressor final : public BlockCompressor
{
	int GetCompressionBoundImpl (const int inputSize) const override;
	int CompressImpl (const ArrayRef<>& input,
		const MutableArrayRef<>& output) const override;
};

struct ZipBlockCompressor final : public BlockCompressor
{
	int GetCompressionBoundImpl (const int inputSize) const override;
	int CompressImpl (const ArrayRef<>& input,
		const MutableArrayRef<>& output) const override;
};

////////////////////////////////////////////////////////////////////////////////
BlockCompressor::BlockCompressor ()
{
}

////////////////////////////////////////////////////////////////////////////////
BlockCompressor::~BlockCompressor ()
{
}

////////////////////////////////////////////////////////////////////////////////
int BlockCompressor::GetCompressionBound (const int inputSize) const
{
	return GetCompressionBoundImpl (inputSize);
}

////////////////////////////////////////////////////////////////////////////////
int BlockCompressor::Compress (const ArrayRef<>& input,
	const MutableArrayRef<>& output)
{
	return CompressImpl (input, output);
}

////////////////////////////////////////////////////////////////////////////////
int ZipBlockCompressor::GetCompressionBoundImpl (const int inputSize) const
{
	return ::compressBound (inputSize);
}

////////////////////////////////////////////////////////////////////////////////
int ZipBlockCompressor::CompressImpl (const ArrayRef<>& input,
	const MutableArrayRef<>& output) const
{
	::uLongf compressedSize = output.GetSize ();
	::compress (static_cast<::Bytef*> (output.GetData()), &compressedSize,
			static_cast<const ::Bytef*> (input.GetData ()), input.GetSize ());
	return compressedSize;
}

////////////////////////////////////////////////////////////////////////////////
int NullBlockCompressor::GetCompressionBoundImpl (const int inputSize) const
{
	return inputSize;
}

////////////////////////////////////////////////////////////////////////////////
int NullBlockCompressor::CompressImpl (const ArrayRef<>& input,
	const MutableArrayRef<>& output) const
{
	::memcpy (output.GetData (), input.GetData (), input.GetSize ());
}

////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<BlockCompressor> CreateBlockCompressor (CompressionMode compression)
{
	switch (compression) {
	case CompressionMode::Zip:
		return std::unique_ptr<BlockCompressor> (new ZipBlockCompressor);

	case CompressionMode::Uncompressed:
		return std::unique_ptr<BlockCompressor> (new NullBlockCompressor);
	}
}
}
