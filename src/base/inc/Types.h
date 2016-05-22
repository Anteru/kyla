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

#ifndef KYLA_CORE_INTERNAL_STANDARDTYPES_H
#define KYLA_CORE_INTERNAL_STANDARDTYPES_H

#include <cstdint>

namespace kyla {
typedef std::uint8_t		uint8;
typedef std::uint16_t		uint16;
typedef std::uint32_t		uint32;
typedef std::uint64_t		uint64;
typedef unsigned int		uint;

typedef std::int8_t			int8;
typedef std::int16_t		int16;
typedef std::int32_t		int32;
typedef std::int64_t		int64;

typedef uint8				byte;

typedef char				char8;
typedef uint16				char16;
typedef uint32				char32;
}

#endif
