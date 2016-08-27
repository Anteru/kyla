/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#include "Uuid.h"

#include "Exception.h"

#include <cassert>

#if KYLA_PLATFORM_WINDOWS
	#include <Objbase.h>
#elif KYLA_PLATFORM_LINUX
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>
#endif

namespace kyla {
namespace {
///////////////////////////////////////////////////////////////////////////////
template <typename T>
byte* ByteCopy (const T& t, byte* output)
{
	for (unsigned i = 0u; i < sizeof (t); ++i) {
#if 1
		*output++ = (t >> ((sizeof (t) - i - 1) * 8)) & 0xFF;
#else /* big endian */
		*output++ = (t >> (i * 8)) & 0xFF;
#endif
	}

	return output;
}

///////////////////////////////////////////////////////////////////////////////
bool CharToByte (const char c, byte& output)
{
	assert (
		   (c >= '0' && c <= '9')
		|| (c >= 'A' && c <= 'F')
		|| (c >= 'a' && c <= 'f'));

	if (c >= '0' && c <= '9') {
		output = c - '0';
		return true;
	} else if (c >= 'A' && c <= 'F') {
		output = 10 + (c - 'A');
		return true;
	} else if (c >= 'a' && c <= 'f') {
		output = 10 + (c - 'a');
		return true;
	} else {
		return false;
	}
}

////////////////////////////////////////////////////////////////////////////
byte		HexToByte (const char input [2])
{
	byte lower, upper;
	CharToByte (input [0], upper);
	CharToByte (input [1], lower);
	return ((upper << 4) | lower);
}

/////////////////////////////////////////////////////////////////////////
uint32 ParseUuidPart (const char* str)
{
	uint32 result = 0;

	for (int i = 0; i < 4; ++i) {
		result |= HexToByte (str + (i * 2)) << ((4 - i - 1) * 8);
	}

	return result;
}
}

/////////////////////////////////////////////////////////////////////////
/**
* Create a new, random Uuid.
*
* The resulting Uuid does not contain any machine-related information like
* MAC addresses.
*/
Uuid Uuid::CreateRandom ()
{
#if KYLA_PLATFORM_WINDOWS
	::GUID id;
	::CoCreateGuid (&id);

	Uuid uuid;
	::memcpy (uuid.uuid_, &id, sizeof (id));
#elif KYLA_PLATFORM_LINUX
	char kernelUuid [36];
	const int f = open ("/proc/sys/kernel/random/uuid", O_RDONLY);
	if (f == -1) {
		throw RuntimeException ("Could not generate Uuid",
			// "Could not open '/proc/sys/kernel/random/uuid' for reading",
			KYLA_FILE_LINE);
	}

	const int bytesRead = read (f, kernelUuid, 36);

	if (bytesRead != 36) {
		throw RuntimeException ("Could not generate Uuid",
			// String::Format ("Expected to read 36 bytes, but got {0}") % bytesRead,
			KYLA_FILE_LINE);
	}

	close (f);

	char byteString [32];
	::memcpy (byteString +  0, kernelUuid, 8);
	::memcpy (byteString +  8, kernelUuid + 9, 4);
	::memcpy (byteString + 12, kernelUuid + 14, 4);
	::memcpy (byteString + 16, kernelUuid + 19, 4);
	::memcpy (byteString + 20, kernelUuid + 24, 12);

	uint32 parts [4] = { 0 };
	for (int i = 0; i < 4; ++i) {
		parts [i] = ParseUuidPart (byteString + i * 8);
	}

	auto uuid = Uuid (parts [0], parts [1], parts [2], parts [3]);
#else
	#error "Unsupported platform"
#endif

	return uuid;
}

/////////////////////////////////////////////////////////////////////////
Uuid::Uuid ()
{
	std::fill (uuid_, uuid_ + 16, 0);
}

/////////////////////////////////////////////////////////////////////////
Uuid::Uuid (const uint8 bytes [16])
{
	::memcpy (uuid_, bytes, 16);
}

/////////////////////////////////////////////////////////////////////////
/**
Create a GUID from the four constants.

@note This function will swap bytes to store the data internally as specified
	by RFC 4122 (http://tools.ietf.org/html/rfc4122), that is, on a
	litte-endian machine, data will be converted to big-endian.
	This is done so that the Uuid constructed has the same byte order when
	printed using ToString() as in this function call.
*/
Uuid::Uuid (const uint32 a, const uint32 b, const uint32 c, const uint32 d)
{
	auto p = ByteCopy (a, uuid_);
	p = ByteCopy (b, p);
	p = ByteCopy (c, p);
	ByteCopy (d, p);
}

/////////////////////////////////////////////////////////////////////////
std::string Uuid::ToString () const
{
	char r [36] = { 0 };

	// -1 is a guard, once we have found all valid break points, otherwise we
	// would access out-of-bounds
	static const int breaks [] = {8, 12, 16, 20, -1};

	// 0..9 -> '0'..'9', 10..15 -> 'A'..'F'
	int output = 0;
	int currentBreak = 0;
	for (int i = 0; i < 32; ++i) {
		if (breaks [currentBreak] == i) {
			currentBreak++;
			r [output++] = '-';
		}

		byte c_i = uuid_ [i / 2];

		// if odd, use upper 4 bit
		if (i % 2) {
			c_i &= 0xF;
		} else {
			c_i = (c_i & 0xF0) >> 4;
		}

		if (c_i >= 0 && c_i < 10) {
			r [output++] = '0' + c_i;
		} else {
			r [output++] = 'a' + (c_i - 10);
		}
	}

	return std::string (r, r + 36);
}

/////////////////////////////////////////////////////////////////////////
std::string ToString (const Uuid& uuid)
{
	return uuid.ToString ();
}

/////////////////////////////////////////////////////////////////////////
bool Uuid::operator == (const Uuid& other) const
{
	return std::equal (uuid_, uuid_ + 16, other.uuid_);
}

/////////////////////////////////////////////////////////////////////////
bool Uuid::operator != (const Uuid& other) const
{
	return ! (*this == other);
}

/////////////////////////////////////////////////////////////////////////
bool Uuid::operator < (const Uuid& other) const
{
	return std::lexicographical_compare (uuid_, uuid_ + 16,
		other.uuid_, other.uuid_ + 16);
}

/////////////////////////////////////////////////////////////////////////
byte Uuid::operator [] (const int index) const
{
	assert (index >= 0 && index < 16);

	return uuid_ [index];
}

/////////////////////////////////////////////////////////////////////////
Uuid Uuid::Parse (StringRef str)
{
	Uuid result;

	if (TryParse (str, result)) {
		return result;
	} else {
		throw RuntimeException ("Invalid Uuid string.",
			KYLA_FILE_LINE);
	}
}


/////////////////////////////////////////////////////////////////////////
bool Uuid::TryParse (StringRef str, Uuid& result)
{
	if (str.IsEmpty ()) {
		return false;
	}

	const int validLengths [] = {
		32, // 32 digits
		36, // 32 digits, 4 hyphens
		38	// 32 digits, 4 hyphens, 2 braces
	};

	bool validLength = false;
	for (auto length : validLengths) {
		if (str.GetLength () == length) {
			validLength = true;
			break;
		}
	}

	if (! validLength) {
		return false;
	}

	// Try to extract 32 consecutive bytes from the input and place them in
	// byteString
	char byteString [32] = {0};

	if (str.GetLength () == 38) {
		// First and last letter must be {}
		if (str [0] != '{' || str [37] != '}') {
			return false;
		}

		str = str.SubString (1, 36);
	}

	// 00000000-0000-0000-0000-000000000000
	//           1         2         3
	// 012345678901234567890123456789012345

	// Check hyphens
	if (str.GetLength () == 36) {
		if (str [8] != '-' || str [13] != '-' || str [18] != '-' || str [23] != '-') {
			return false;
		}

		int output = 0;
		for (int i = 0; i < 36; ++i) {
			if (str [i] != '-') {
				byteString [output++] = str [i];
			}
		}

		assert (output == 32);
	} else {
		assert (str.GetLength () == 32);

		::memcpy (byteString, str.GetData (), 32);
	}

	// Validate
	for (int i = 0; i < 32; ++i) {
		if (! ((byteString [i] >= 'a' && byteString [i] <= 'f')
			|| (byteString [i] >= '0' && byteString [i] <= '9')
			|| (byteString [i] >= 'A' && byteString [i] <= 'F')))
		{
			return false;
		}
	}

	// Now, just parse the 32-digit string
	uint32 parts [4] = { 0 };
	for (int i = 0; i < 4; ++i) {
		parts [i] = ParseUuidPart (byteString + i * 8);
	}

	result = Uuid (parts [0], parts [1], parts [2], parts [3]);
	return true;
}
}
