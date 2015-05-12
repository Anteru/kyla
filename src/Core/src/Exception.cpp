#include "Exception.h"

namespace kyla {
RuntimeException::RuntimeException (const char *msg, const char *file, const int line)
: std::runtime_error (msg)
, file_ (file)
, line_ (line)
{
}
}
