#ifndef KYLA_CORE_INTERNAL_EXCEPTION_H
#define KYLA_CORE_INTERNAL_EXCEPTION_H

#include <stdexcept>
#include <string>

#define KYLA_FILE_LINE __FILE__,__LINE__

namespace kyla {
class RuntimeException : public std::runtime_error
{
public:
	RuntimeException (const char* msg, const char* file, const int line);

private:
	std::string detail_;
	const char* file_;
	int line_;
};
}

#endif
