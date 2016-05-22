/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
[LICENSE END]
*/

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
	///@TODO(minor) Check for overflow
	::uLongf compressedSize = static_cast<uLongf> (output.GetSize ());
	::compress (
		static_cast<::Bytef*> (output.GetData()),
		&compressedSize,
		static_cast<const ::Bytef*> (input.GetData ()),
		static_cast<uLong> (input.GetSize ()));
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

	return static_cast<int> (input.GetSize ());
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

	return std::unique_ptr<BlockCompressor> ();
}
}
