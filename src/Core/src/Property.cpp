#include "Property.h"

#include <cstring>
#include <cstdlib>

namespace kyla {
////////////////////////////////////////////////////////////////////////////////
Property::Property (const Property& p)
	: type_ (p.type_)
	, size_ (p.size_)
{
	switch (p.type_) {
	case PropertyType::Int:
		i_ = p.i_;
		break;
	case PropertyType::String:
		str_ = ::strdup (p.str_);
		break;
	case PropertyType::Binary:
		binary_ = ::malloc (size_);
		::memcpy (binary_, p.binary_, size_);
		break;
	}
}

////////////////////////////////////////////////////////////////////////////////
Property& Property::operator= (const Property& p)
{
	type_ = p.type_;
	size_ = p.size_;

	switch (p.type_) {
	case PropertyType::Int:
		i_ = p.i_;
		break;
	case PropertyType::String:
		str_ = ::strdup (p.str_);
		break;
	case PropertyType::Binary:
		binary_ = ::malloc (size_);
		::memcpy (binary_, p.binary_, size_);
		break;
	}

	return *this;
}

////////////////////////////////////////////////////////////////////////////////
Property::Property (Property&& p)
	: type_ (p.type_)
	, size_ (p.size_)
{
	switch (p.type_) {
	case PropertyType::Int:
		i_ = p.i_;
		break;
	case PropertyType::String:
		str_ = p.str_;
		break;
	case PropertyType::Binary:
		binary_ = p.binary_;
		break;
	}

	// Undefined but safe state, we take ownership of memory, so null it
	// out
	p.type_ = PropertyType::Null;
}

////////////////////////////////////////////////////////////////////////////////
Property& Property::operator=(Property&& p)
{
	type_ = p.type_;
	size_ = p.size_;

	switch (p.type_) {
	case PropertyType::Int:
		i_ = p.i_;
		break;
	case PropertyType::String:
		str_ = p.str_;
		break;
	case PropertyType::Binary:
		binary_ = p.binary_;
		break;
	}

	// Undefined but safe state, we take ownership of memory, so null it
	// out
	p.type_ = PropertyType::Null;

	return *this;
}

////////////////////////////////////////////////////////////////////////////////
Property::Property (const int i)
	: type_ (PropertyType::Int)
	, i_ (i)
	, size_ (sizeof (i))
{
}

////////////////////////////////////////////////////////////////////////////////
Property::Property (const char* s)
	: type_ (PropertyType::String)
	, str_ (::strdup (s))
	, size_ (::strlen (s) + 1)
{
}

////////////////////////////////////////////////////////////////////////////////
Property::Property (const void* b, const int size)
	: type_ (PropertyType::Binary)
	, size_ (size)
{
	this->binary_ = ::malloc (size);
	::memcpy (this->binary_, b, size);
}

////////////////////////////////////////////////////////////////////////////////
Property::~Property ()
{
	switch (type_) {
	case PropertyType::String:
		::free (str_);
		break;

	case PropertyType::Binary:
		::free (binary_);
		break;

	default:
		break;
	}
}
}
