/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#ifndef KYLA_CORE_INTERNAL_LOG_H
#define KYLA_CORE_INTERNAL_LOG_H

#include <functional>
#include <string>
#include <chrono>

#include "Types.h"

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
	using LogCallback = std::function<void (LogLevel logLevel, const char* source, const char* message, const int64 timestamp)>;

	Log (const LogCallback& callback);

	void SetCallback (const LogCallback& callback);

	void Debug (const char* source, const std::string& message);
	void Info (const char* source, const std::string& message);
	void Warning (const char* source, const std::string& message);
	void Error (const char* source, const std::string& message);

private:
	LogCallback callback_;
	std::chrono::steady_clock::time_point startTime_;
};
}

#endif
