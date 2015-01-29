#include "Kyla.h"

#include <sqlite3.h>
#include <string.h>
#include <stdlib.h>
#include <map>

enum class KylaPropertyType
{
	String,
	Int,
	Binary
};

struct KylaProperty
{
	KylaPropertyType type;

	union {
		char* s;
		int i;
		void* b;
	};

	int size;
};

struct KylaFeatures
{
	// We have a string table in here for all the C strings
};

struct KylaInstallationPackage
{
	KylaInstallationPackage (const char* path)
	{
		sqlite3_open (path, &db_);
	}

	~KylaInstallationPackage ()
	{
		sqlite3_close (db_);
	}

	void SetProperty (const char* name, const KylaProperty* value)
	{
		properties_ [name] = value;
	}

	void Install ()
	{
	}

private:
	std::map<std::string, const struct KylaProperty*> properties_;
	sqlite3* db_;
};

////////////////////////////////////////////////////////////////////////////////
int OpenInstallationPackage (const char* path, KylaInstallationPackage** output)
{
	*output = new KylaInstallationPackage (path);
	return KylaSuccess;
}

////////////////////////////////////////////////////////////////////////////////
int CloseInstallationPackage (KylaInstallationPackage* package)
{
	delete package;
}

////////////////////////////////////////////////////////////////////////////////
KylaProperty* CreateStringProperty (const char* s)
{
	KylaProperty* p = new KylaProperty;

	p->type = KylaPropertyType::String;
	p->s = ::strdup (s);
	p->size = ::strlen (p->s) + 1;

	return p;
}

////////////////////////////////////////////////////////////////////////////////
KylaProperty* CreateIntProperty (const int i)
{
	KylaProperty* p = new KylaProperty;

	p->type = KylaPropertyType::Int;
	p->i = i;
	p->size = sizeof (i);

	return p;
}

////////////////////////////////////////////////////////////////////////////////
KylaProperty* CreateBinaryProperty (const void* d, const int size)
{
	KylaProperty* p = new KylaProperty;

	p->type = KylaPropertyType::Binary;
	p->b = ::malloc (size);
	::memcpy (p->b, d, size);
	p->size = size;

	return p;
}

////////////////////////////////////////////////////////////////////////////////
int DeleteProperty (struct KylaProperty* property)
{
	switch (property->type) {
	case KylaPropertyType::String:
		::free (property->s);
		break;

	case KylaPropertyType::Binary:
		::free (property->b);
		break;

	default:
		break;
	}

	delete property;

	return KylaSuccess;
}
