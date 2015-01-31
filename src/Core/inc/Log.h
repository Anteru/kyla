#ifndef KYLA_CORE_INTERNAL_LOG_H
#define KYLA_CORE_INTERNAL_LOG_H

#include <spdlog.h>
#include <sinks/file_sinks.h>
#include <sinks/null_sink.h>

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
		 const LogLevel logLevel = LogLevel::Info)
	{
		if (filename) {
			log_ = spdlog::create<spdlog::sinks::simple_file_sink_mt> (name,
				filename);
				switch (logLevel) {
				case LogLevel::Trace:
					log_->set_level (spdlog::level::trace);
					break;
				case LogLevel::Debug:
					log_->set_level (spdlog::level::debug);
					break;
				case LogLevel::Info:
					log_->set_level (spdlog::level::info);
					break;
				case LogLevel::Warning:
					log_->set_level (spdlog::level::warn);
					break;
				case LogLevel::Error:
					log_->set_level (spdlog::level::err);
					break;
				}
		} else {
			log_ = spdlog::create<spdlog::sinks::null_sink_mt> (name);
		}
	}

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
