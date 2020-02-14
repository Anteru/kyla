#include "Hash.h"

#include <catch.hpp>

TEST_CASE ("HashEqual", "[hash]")
{
	kyla::HashDigest<2> a, b;
	a.bytes[0] = 0;
	a.bytes[1] = 0;

	b.bytes[0] = 1;
	b.bytes[1] = 1;

	REQUIRE (a != b);
	REQUIRE (a == a);
	REQUIRE (b == b);
}

TEST_CASE ("HashToString", "[hash]")
{
	kyla::HashDigest<2> a;
	a.bytes[0] = 0xFA;
	a.bytes[1] = 0xBC;

	const auto s = ToString (a);
	REQUIRE ("fabc" == s);
}

TEST_CASE ("SHA256", "[hash]")
{
	const kyla::SHA256Digest digest { 
		0x7b, 0x7c, 0xa3, 0xe6, 
		0xbf, 0x9c, 0xad, 0xe1, 
		0x53, 0x00, 0x3a, 0x0e, 
		0xa6, 0x70, 0x4c, 0x78, 
		0x07, 0x59, 0x42, 0x0b, 
		0x87, 0x3e, 0x4d, 0xf7, 
		0x0f, 0x04, 0x32, 0xf6, 
		0x18, 0x0a, 0x59, 0xd0
	};
	const kyla::byte data[] = { 13, 37, 42, 0 };

	auto computedDigest = kyla::ComputeSHA256 (kyla::ArrayRef<kyla::byte> (data));
	REQUIRE (digest == computedDigest);
}