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

#ifndef KYLA_CORE_INTERNAL_EXCEPTION_H
#define KYLA_CORE_INTERNAL_EXCEPTION_H

#include <stdexcept>
#include <string>

#define KYLA_FILE_LINE __FILE__,__LINE__

namespace kyla {
class RuntimeException : public std::runtime_error
{
public:
	RuntimeException (const char* msg, const char* file, const int line);

private:
	std::string detail_;
	const char* file_;
	int line_;
};
}

#endif
