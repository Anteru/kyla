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
The default hash in Kyla is a 64-byte SHA512.
*/
typedef HashDigest<64> SHA512Digest;

class SHA512StreamHasher final
{
public:
	SHA512StreamHasher ();
	~SHA512StreamHasher ();

	SHA512StreamHasher (const SHA512StreamHasher&) = delete;
	SHA512StreamHasher& operator= (const SHA512StreamHasher&) = delete;

	void Initialize ();
	void Update (const ArrayRef<>& data);
	SHA512Digest Finalize();

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

SHA512Digest ComputeSHA512 (const ArrayRef<>& data);
SHA512Digest ComputeSHA512 (const boost::filesystem::path& p);
SHA512Digest ComputeSHA512 (const boost::filesystem::path& p,
	std::vector<byte>& fileReadBuffer);

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
