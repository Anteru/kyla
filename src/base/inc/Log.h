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

#ifndef KYLA_CORE_INTERNAL_LOG_H
#define KYLA_CORE_INTERNAL_LOG_H

#include <functional>
#include <string>
#include <boost/format.hpp>

namespace kyla {
enum class LogLevel
{
	Debug,
	Info,
	Warning,
	Error
};

class Log
{
public:
	using LogCallback = std::function<void (LogLevel logLevel, const char* source, const char* message)>;

	Log (const LogCallback& callback);

	void SetCallback (const LogCallback& callback);

	void Debug (const char* source, const std::string& message);
	void Info (const char* source, const std::string& message);
	void Warning (const char* source, const std::string& message);
	void Error (const char* source, const std::string& message);

	template<class charT, class Traits>
	void Debug (const char* source, const boost::basic_format<charT, Traits>& message)
	{
		Debug (source, str (message));
	}

	template<class charT, class Traits>
	void Info (const char* source, const boost::basic_format<charT, Traits>& message)
	{
		Info (source, str (message));
	}

	template<class charT, class Traits>
	void Warning (const char* source, const boost::basic_format<charT, Traits>& message)
	{
		Warning (source, str (message));
	}

	template<class charT, class Traits>
	void Error (const char* source, const boost::basic_format<charT, Traits>& message)
	{
		Error (source, str (message));
	}

private:
	std::function<void (LogLevel logLevel, const char* source, const char* message)> callback_;
};
}

#endif
