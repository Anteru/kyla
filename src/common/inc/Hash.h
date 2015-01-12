#ifndef _NIM_COMMON_H_HASH_
#define _NIM_COMMON_H_HASH_

#include <stdint.h>
#include <string.h>
#include <boost/functional/hash.hpp>

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

#endif
