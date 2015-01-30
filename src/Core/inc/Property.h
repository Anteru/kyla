#ifndef KYLA_CORE_INTERNAL_PROPERTY_H
#define KYLA_CORE_INTERNAL_PROPERTY_H

#include <cstring>

namespace kyla {
enum class PropertyType
{
	String,
	Int,
	Binary,
	Null
};

class Property final
{
public:
	Property (const Property& p);
	Property& operator= (const Property& p);

	Property (Property&& p);
	Property& operator=(Property&& p);

	Property () = default;
	Property (const int i);
	Property (const char* s);
	Property (const void* b, const int size);

	~Property ();

	int GetInt () const
	{
		return i_;
	}

	const char* GetString () const
	{
		return str_;
	}

	const void* GetBinary (int* size) const
	{
		if (size) {
			*size = size_;
		}

		return binary_;
	}

	int GetSize () const
	{
		return size_;
	}

	PropertyType GetType () const
	{
		return type_;
	}

private:

	PropertyType type_ = PropertyType::Null;

	union {
		char* str_;
		int i_;
		void* binary_;
	};

	int size_ = 0;
};
}

#endif
