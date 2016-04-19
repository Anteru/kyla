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
