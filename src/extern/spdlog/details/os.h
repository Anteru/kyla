/*************************************************************************/
/* spdlog - an extremely fast and easy to use c++11 logging library.     */
/* Copyright (c) 2014 Gabi Melman.                                       */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#pragma once

#include<string>
#include<cstdio>
#include<ctime>

#ifdef _WIN32
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <Windows.h>
#endif

namespace spdlog
{
namespace details
{
namespace os
{

inline std::tm localtime(const std::time_t &time_tt)
{

#ifdef _WIN32
    std::tm tm;
    localtime_s(&tm, &time_tt);
#else
    std::tm tm;
    localtime_r(&time_tt, &tm);
#endif
    return tm;
}

inline std::tm localtime()
{
    std::time_t now_t = time(0);
    return localtime(now_t);
}


inline std::tm gmtime(const std::time_t &time_tt)
{

#ifdef _WIN32
    std::tm tm;
    gmtime_s(&tm, &time_tt);
#else
    std::tm tm;
    gmtime_r(&time_tt, &tm);
#endif
    return tm;
}

inline std::tm gmtime()
{
    std::time_t now_t = time(0);
    return gmtime(now_t);
}
inline bool operator==(const std::tm& tm1, const std::tm& tm2)
{
    return (tm1.tm_sec == tm2.tm_sec &&
            tm1.tm_min == tm2.tm_min &&
            tm1.tm_hour == tm2.tm_hour &&
            tm1.tm_mday == tm2.tm_mday &&
            tm1.tm_mon == tm2.tm_mon &&
            tm1.tm_year == tm2.tm_year &&
            tm1.tm_isdst == tm2.tm_isdst);
}

inline bool operator!=(const std::tm& tm1, const std::tm& tm2)
{
    return !(tm1 == tm2);
}

#ifdef _WIN32
inline const char* eol()
{
    return "\r\n";
}
#else
constexpr inline const char* eol()
{
    return "\n";
}
#endif

#ifdef _WIN32
inline unsigned short eol_size()
{
    return 2;
}
#else
constexpr inline unsigned short eol_size()
{
    return 1;
}
#endif

//fopen_s on non windows for writing
inline int fopen_s(FILE** fp, const std::string& filename, const char* mode)
{
#ifdef _WIN32
    *fp = _fsopen((filename.c_str()), mode, _SH_DENYWR);
    return *fp == nullptr;
#else
    *fp = fopen((filename.c_str()), mode);
    return *fp == nullptr;
#endif


}

//Return utc offset in minutes or -1 on failure
inline int utc_minutes_offset(const std::tm& tm = details::os::localtime())
{

#ifdef _WIN32
    (void)tm; // avoid unused param warning
    DYNAMIC_TIME_ZONE_INFORMATION tzinfo;
    auto rv = GetDynamicTimeZoneInformation(&tzinfo);
    if (!rv)
        return -1;
    return -1 * (tzinfo.Bias + tzinfo.DaylightBias);
#else
    return static_cast<int>(tm.tm_gmtoff / 60);
#endif
}


} //os
} //details
} //spdlog



