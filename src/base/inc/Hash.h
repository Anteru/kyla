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

#ifndef KYLA_CORE_INTERNAL_HASH_H
#define KYLA_CORE_INTERNAL_HASH_H

#include <stdint.h>
#include <string.h>
#include <boost/functional/hash.hpp>
#include <string>
#include <boost/filesystem.hpp>

#include "ArrayAdapter.h"
#include "ArrayRef.h"
#include "Types.h"

namespace kyla {
template <int Size>
struct HashDigest
{
	byte bytes [Size];

	template <int OtherSize>
	bool operator== (const HashDigest<OtherSize>& other) const
	{
		if (OtherSize != Size) {
			return false;
		}

		return ::memcmp (bytes, other.bytes, Size) == 0;
	}

	template <int OtherSize>
	bool operator!= (const HashDigest<OtherSize>& other) const
	{
		return !(*this == other);
	}
};

template <int Size>
struct ArrayAdapter<HashDigest<Size>>
{
	static const byte* GetDataPointer (const HashDigest<Size>& s)
	{
		return s.bytes;
	}

	static byte* GetDataPointer (HashDigest<Size>& s)
	{
		return s.bytes;
	}

	static std::size_t GetSize (const HashDigest<Size>&)
	{
		return Size;
	}

	static std::size_t GetCount (const HashDigest<Size>&)
	{
		return Size;
	}

	typedef byte Type;
};

struct HashDigestEqual
{
	template <int Size>
	bool operator () (const HashDigest<Size>& a, const HashDigest<Size>& b) const
	{
		return ::memcmp (a.bytes, b.bytes, Size) == 0;
	}
};

struct HashDigestHash
{
	template <int Size>
	std::size_t operator () (const HashDigest<Size>& a) const
	{
		return boost::hash_range (a.bytes, a.bytes + Size);
	}
};

/*
The default hash in Kyla is a 32-byte SHA256.
*/
typedef HashDigest<32> SHA256Digest;

class SHA256StreamHasher final
{
public:
	SHA256StreamHasher ();
	~SHA256StreamHasher ();

	SHA256StreamHasher (const SHA256StreamHasher&) = delete;
	SHA256StreamHasher& operator= (const SHA256StreamHasher&) = delete;

	void Initialize ();
	void Update (const ArrayRef<>& data);
	SHA256Digest Finalize();

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

SHA256Digest ComputeSHA256 (const ArrayRef<>& data);
SHA256Digest ComputeSHA256 (const boost::filesystem::path& p);
SHA256Digest ComputeSHA256 (const boost::filesystem::path& p,
	const MutableArrayRef<>& fileReadBuffer);

template <int Size>
std::string ToString (const byte (&hash) [Size])
{
	// TODO Use a 6bit mapping to 0-9a-zA-Z_- to get more compact hash names

	char result [Size*2] = { 0 };
	char* p = result;

	static const char* byteToChar = "0123456789abcdef";

	for (auto b : hash) {
		p [0] = byteToChar [b >> 4];
		p [1] = byteToChar [b & 0xF];

		p += 2;
	}

	return std::string (result, result + Size*2);
}

template <int Size>
std::string ToString (const HashDigest<Size>& hash)
{
	return ToString (hash.bytes);
}
}

#endif
