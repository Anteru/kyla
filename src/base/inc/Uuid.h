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

#ifndef KYLA_CORE_INTERNAL_UUID_H
#define KYLA_CORE_INTERNAL_UUID_H

#include "ArrayAdapter.h"
#include "StringRef.h"
#include "Types.h"

#include <string>

namespace kyla {
class Uuid final
{
public:
	static Uuid CreateRandom ();

	Uuid ();
	Uuid (const uint32 a, const uint32 b, const uint32 c, const uint32 d);
	Uuid (const uint8 bytes [16]);

	static bool TryParse (StringRef s, Uuid& output);
	static Uuid Parse (StringRef s);

	std::string ToString() const;

	byte operator [] (const int index) const;

	bool operator == (const Uuid& other) const;
	bool operator != (const Uuid& other) const;
	bool operator < (const Uuid& other) const;

	inline const byte* GetData () const
	{
		return uuid_;
	}

private:
	byte uuid_ [16];
};

std::string ToString (const Uuid& uuid);

template <>
struct ArrayAdapter<Uuid>
{
	static const byte* GetDataPointer (const Uuid& s)
	{
		return s.GetData ();
	}

	static int64 GetSize (const Uuid& s)
	{
		return 16;
	}

	static int64 GetCount (const Uuid& s)
	{
		return 16;
	}

	typedef byte Type;
};
}

#endif
