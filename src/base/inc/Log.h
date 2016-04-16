#ifndef KYLA_CORE_INTERNAL_LOG_H
#define KYLA_CORE_INTERNAL_LOG_H

#include <spdlog.h>

#ifdef CreateFile
#undef CreateFile
#endif

namespace kyla {
enum class LogLevel
{
	Trace,
	Debug,
	Info,
	Warning,
	Error
};

class Log
{
public:
	Log (const char* name, const char* filename = nullptr,
		const LogLevel logLevel = LogLevel::Info);

	spdlog::details::line_logger Trace ()
	{
		return log_->trace ();
	}

	spdlog::details::line_logger Debug ()
	{
		return log_->debug ();
	}

	spdlog::details::line_logger Info ()
	{
		return log_->info ();
	}

	spdlog::details::line_logger Warning ()
	{
		return log_->warn ();
	}

	spdlog::details::line_logger Error ()
	{
		return log_->error ();
	}

private:
	std::shared_ptr<spdlog::logger> log_;
};
}

#endif
