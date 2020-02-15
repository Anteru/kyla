/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#include "Log.h"

namespace kyla {

///////////////////////////////////////////////////////////////////////////////
Log::Log (const LogCallback& callback)
	: callback_ (callback)
	, startTime_ (std::chrono::steady_clock::now ())
{
}

///////////////////////////////////////////////////////////////////////////////
void Log::SetCallback (const LogCallback& callback)
{
	callback_ = callback;
}

///////////////////////////////////////////////////////////////////////////////
void Log::RemoveCallback ()
{
	callback_ = LogCallback ();
}

///////////////////////////////////////////////////////////////////////////////
void Log::Debug (const char* source, const std::string& message)
{
	if (callback_) {
		auto duration = std::chrono::steady_clock::now () - startTime_;
		callback_ (LogLevel::Debug, source, message.c_str (), duration.count ());
	}
}

///////////////////////////////////////////////////////////////////////////////
void Log::Info (const char* source, const std::string& message)
{
	if (callback_) {
		auto duration = std::chrono::steady_clock::now () - startTime_;
		callback_ (LogLevel::Info, source, message.c_str (), duration.count ());
	}
}

///////////////////////////////////////////////////////////////////////////////
void Log::Warning (const char* source, const std::string& message)
{
	if (callback_) {
		auto duration = std::chrono::steady_clock::now () - startTime_;
		callback_ (LogLevel::Warning, source, message.c_str (), duration.count ());
	}
}

///////////////////////////////////////////////////////////////////////////////
void Log::Error (const char* source, const std::string& message)
{
	if (callback_) {
		auto duration = std::chrono::steady_clock::now () - startTime_;
		callback_ (LogLevel::Error, source, message.c_str (), duration.count ());
	}
}
}
