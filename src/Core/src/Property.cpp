#include "Property.h"

#include <cstring>
#include <cstdlib>

namespace kyla {
////////////////////////////////////////////////////////////////////////////////
Property::Property (const Property& p)
	: type (p.type)
	, size (p.size)
{
	switch (p.type) {
	case PropertyType::Int:
		i = p.i;
		break;
	case PropertyType::String:
		s = ::strdup (p.s);
		break;
	case PropertyType::Binary:
		b = ::malloc (size);
		::memcpy (b, p.b, size);
		break;
	}
}

////////////////////////////////////////////////////////////////////////////////
Property& Property::operator=(const Property& p)
{
	type = p.type;
	size = p.size;

	switch (p.type) {
	case PropertyType::Int:
		i = p.i;
		break;
	case PropertyType::String:
		s = ::strdup (p.s);
		break;
	case PropertyType::Binary:
		b = ::malloc (size);
		::memcpy (b, p.b, size);
		break;
	}

	return *this;
}

////////////////////////////////////////////////////////////////////////////////
Property::Property (Property&& p)
	: type (p.type)
	, size (p.size)
{
	switch (p.type) {
	case PropertyType::Int:
		i = p.i;
		break;
	case PropertyType::String:
		s = p.s;
		break;
	case PropertyType::Binary:
		b = p.b;
		break;
	}

	// Undefined but safe state, we take ownership of memory, so null it
	// out
	p.type = PropertyType::Null;
}

////////////////////////////////////////////////////////////////////////////////
Property& Property::operator=(Property&& p)
{
	type = p.type;
	size = p.size;

	switch (p.type) {
	case PropertyType::Int:
		i = p.i;
		break;
	case PropertyType::String:
		s = p.s;
		break;
	case PropertyType::Binary:
		b = p.b;
		break;
	}

	// Undefined but safe state, we take ownership of memory, so null it
	// out
	p.type = PropertyType::Null;

	return *this;
}

////////////////////////////////////////////////////////////////////////////////
Property::Property (const int i)
	: type (PropertyType::Int)
	, i (i)
	, size (sizeof (i))
{
}

////////////////////////////////////////////////////////////////////////////////
Property::Property (const char* s)
	: type (PropertyType::String)
	, s (::strdup (s))
	, size (::strlen (s) + 1)
{
}

////////////////////////////////////////////////////////////////////////////////
Property::Property (const void* b, const int size)
	: type (PropertyType::Binary)
	, size (size)
{
	this->b = ::malloc (size);
	::memcpy (this->b, b, size);
}

////////////////////////////////////////////////////////////////////////////////
Property::~Property ()
{
	switch (type) {
	case PropertyType::String:
		::free (s);
		break;

	case PropertyType::Binary:
		::free (b);
		break;

	default:
		break;
	}
}
}
