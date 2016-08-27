/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
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
	RuntimeException (const std::string& msg, const char* file, const int line);
	RuntimeException (const std::string& source, const std::string& msg, const char* file, const int line);

	const char* GetSource () const
	{
		return source_.c_str ();
	}
private:
	std::string detail_;
	std::string source_;
	const char* file_;
	int line_;
};
}

#endif
