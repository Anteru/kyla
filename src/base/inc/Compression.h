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
