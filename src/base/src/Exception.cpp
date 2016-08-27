/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#include "Exception.h"

namespace kyla {
///////////////////////////////////////////////////////////////////////////////
RuntimeException::RuntimeException (const std::string& msg, 
	const char *file, const int line)
: RuntimeException ("unknown", msg, file, line)
{
}

///////////////////////////////////////////////////////////////////////////////
RuntimeException::RuntimeException (const std::string& source, const std::string& msg,
	const char *file, const int line)
: std::runtime_error (msg)
, source_ (source)
, file_ (file)
, line_ (line)
{
}
}
