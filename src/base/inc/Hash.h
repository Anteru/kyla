/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#ifndef KYLA_CORE_INTERNAL_HASH_H
#define KYLA_CORE_INTERNAL_HASH_H

#include <stdint.h>
#include <string.h>
#include <string>
#include <filesystem>

#include "ArrayAdapter.h"
#include "ArrayRef.h"
#include "Types.h"

namespace kyla {
// Copy-pasted from Boost
template <class T>
inline void hash_combine (std::size_t& seed, const T& v)
{
	std::hash<T> hasher;
	seed ^= hasher (v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <typename Iterator>
inline std::size_t hash_range (Iterator it, Iterator end)
{
	std::size_t result = 0;
	while (it != end) {
		hash_combine (result, *it);
		++it;
	}

	return result;
}

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

struct ArrayRefEqual
{
	bool operator () (const ArrayRef<>& a, const ArrayRef<>& b) const
	{
		if (a.GetSize () != b.GetSize ()) {
			return 0;
		} else {
			return ::memcmp (a.GetData (), b.GetData (), a.GetSize ()) == 0;
		}
	}
};

struct ArrayRefHash
{
	std::size_t operator () (const ArrayRef<>& a) const
	{
		auto asByteRef = a.ToByteRef ();
		return hash_range (asByteRef.GetData (), asByteRef.GetData () + asByteRef.GetSize ());
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
SHA256Digest ComputeSHA256 (const std::filesystem::path& p);
SHA256Digest ComputeSHA256 (const std::filesystem::path& p,
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
