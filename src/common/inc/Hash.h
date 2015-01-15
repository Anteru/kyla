#ifndef _NIM_COMMON_H_HASH_
#define _NIM_COMMON_H_HASH_

#include <stdint.h>
#include <string.h>
#include <boost/functional/hash.hpp>
#include <string>
#include <boost/filesystem.hpp>

template <int size>
struct THash
{
    uint8_t hash [size];
};

struct HashEqual
{
	template <int size>
	bool operator () (const THash<size>& a, const THash<size>& b) const
	{
		return ::memcmp (a.hash, b.hash, size) == 0;
	}
};

struct HashHash
{
	template <int size>
	std::size_t operator () (const THash<size>& a) const
	{
		return boost::hash_range (a.hash, a.hash + size);
	}
};

typedef THash<64> Hash;

Hash ComputeHash (const int64_t size, const void* data);
Hash ComputeHash (const boost::filesystem::path& p);
Hash ComputeHash (const boost::filesystem::path& p, std::vector<char>& buffer);

template <int size>
std::string ToString (const std::uint8_t (&hash) [size])
{
	char result [size*2] = { 0 };
	char* p = result;

	static const char* byteToChar = "0123456789abcdef";

	for (auto b : hash) {
		p [0] = byteToChar [b >> 4];
		p [1] = byteToChar [b & 0xF];

		p += 2;
	}

	return std::string (result, result + size*2);
}

template <int size>
std::string ToString (const THash<size>& hash)
{
	return ToString (hash.hash);
}

#endif