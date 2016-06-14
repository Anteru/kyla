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

#include "Exception.h"

namespace kyla {
///////////////////////////////////////////////////////////////////////////////	
RuntimeException::RuntimeException (const char* msg, 
	const char* file, const int line)
: std::runtime_error (msg)
, file_ (file)
, line_ (line)
{
}

///////////////////////////////////////////////////////////////////////////////
RuntimeException::RuntimeException (const std::string& msg, 
	const char *file, const int line)
: std::runtime_error (msg)
, file_ (file)
, line_ (line)
{
}
}
