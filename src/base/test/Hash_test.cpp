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