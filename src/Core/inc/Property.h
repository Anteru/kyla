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

	PropertyType type = PropertyType::Null;

	union {
		char* s;
		int i;
		void* b;
	};

	int size = 0;
};
}

#endif
