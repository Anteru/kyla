#include "Log.h"

#include <sinks/file_sinks.h>
#include <sinks/null_sink.h>

namespace kyla {
Log::Log (const char* name, const char* filename,
	const LogLevel logLevel)
{
	if (filename) {
		log_ = spdlog::create<spdlog::sinks::simple_file_sink_mt> (name, filename);

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
}