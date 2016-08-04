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

#include "Log.h"

namespace kyla {

///////////////////////////////////////////////////////////////////////////////
Log::Log (const LogCallback& callback)
	: callback_ (callback)
{
}

///////////////////////////////////////////////////////////////////////////////
void Log::SetCallback (const LogCallback& callback)
{
	callback_ = callback;
}

///////////////////////////////////////////////////////////////////////////////
void Log::Debug (const char* source, const std::string& message)
{
	if (callback_) {
		callback_ (LogLevel::Debug, source, message.c_str ());
	}
}

///////////////////////////////////////////////////////////////////////////////
void Log::Info (const char* source, const std::string& message)
{
	if (callback_) {
		callback_ (LogLevel::Info, source, message.c_str ());
	}
}

///////////////////////////////////////////////////////////////////////////////
void Log::Warning (const char* source, const std::string& message)
{
	if (callback_) {
		callback_ (LogLevel::Warning, source, message.c_str ());
	}
}

///////////////////////////////////////////////////////////////////////////////
void Log::Error (const char* source, const std::string& message)
{
	if (callback_) {
		callback_ (LogLevel::Error, source, message.c_str ());
	}
}
}
