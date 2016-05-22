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

#include "StringRef.h"

#include <boost/range.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/functional/hash.hpp>

namespace kyla {
/**
@ingroup Core
@class StringRef
*/

///////////////////////////////////////////////////////////////////////////////
bool StringRef::operator== (const char* str) const
{
	if (IsEmpty ()) {
		return str == nullptr;
	}

	if (str == nullptr) {
		return false;
	}

	const auto len = ::strlen (str);

	// Safe: If negative, len will be larger than length_ and this will fail
	// At most we compare the smaller range
	if (len != static_cast<std::size_t> (length_)) {
		return false;
	}

	return ::memcmp (data_, str, length_) == 0;
}

///////////////////////////////////////////////////////////////////////////////
bool StringRef::operator!= (const char* str) const
{
	return !(*this == str);
}

///////////////////////////////////////////////////////////////////////////////
bool StringRef::operator== (const StringRef& str) const
{
	return length_ == str.length_ && (::memcmp (data_, str.data_, length_) == 0);
}

///////////////////////////////////////////////////////////////////////////////
bool StringRef::operator!= (const StringRef& str) const
{
	return ! (*this == str);
}

///////////////////////////////////////////////////////////////////////////////
StringRef StringRef::SubString (const int64 start) const
{
	assert (start >= 0);
	assert (start <= length_);

	return StringRef (data_ + start, data_ + length_);
}

///////////////////////////////////////////////////////////////////////////////
StringRef StringRef::SubString (const int64 start, const int64 length) const
{
	CheckRange (start, length);

	return StringRef (data_ + start,
		data_ + static_cast<std::size_t> (start) + length);
}

///////////////////////////////////////////////////////////////////////////////
void StringRef::CheckRange (const int64 pos, const int64 length) const
{
	assert (pos >= 0);
	assert (pos <= length_);
	assert (pos + length >= 0);
	assert (pos + length <= length_);
	assert (length >= 0);
}

///////////////////////////////////////////////////////////////////////////////
std::size_t StringRef::GetHash () const
{
	return boost::hash_range (begin (), end ());
}

///////////////////////////////////////////////////////////////////////////////
bool StringRef::operator< (const StringRef& str) const
{
	return std::lexicographical_compare (begin (), end (), str.begin (), str.end ());
}

///////////////////////////////////////////////////////////////////////////////
std::string StringRef::ToString () const
{
	return std::string (begin (), end ());
}
} // namespace kyla
