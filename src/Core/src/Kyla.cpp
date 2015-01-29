#include "Kyla.h"

#include <sqlite3.h>
#include <string.h>
#include <stdlib.h>
#include <map>

#include "Installer.h"
#include "Property.h"

#include <memory>

template <typename T>
struct Pool
{
private:
	template <int BlockSize = 1048576>
	struct Block
	{
		T* Alloc (const int c)
		{
			T* result = nullptr;

			if (((next_  - d_) + c) <= BlockSize) {
				result = next_;
				next_ += c;
			}

			return result;
		}

		Block ()
		{
			d_ = new T [BlockSize];
			next_ = d_;
		}

		~Block ()
		{
			delete [] d_;
		}

	private:
		T* next_;
		T* d_;
	};

public:
	Pool ()
	{
		blocks_.emplace_back (new Block<>);
	}

	T* Alloc (const int c = 1)
	{
		T* r = blocks_.back ()->Alloc (c);
		if (r) {
			return r;
		} else {
			blocks_.emplace_back (new Block<>);
			return blocks_.back ()->Alloc (c);
		}
	}

	std::vector<std::unique_ptr<Block<>>> blocks_;
};

////////////////////////////////////////////////////////////////////////////////
class KylaFeatures final
{
public:
	KylaFeatures (sqlite3* db)
	{
		sqlite3_stmt* selectFeaturesStatement;
		sqlite3_prepare_v2 (db,
			"SELECT Id, Name, UIName, UIDescription, ParentId FROM features;", -1,
			&selectFeaturesStatement, nullptr);

		while (sqlite3_step (selectFeaturesStatement) == SQLITE_ROW) {
			KylaFeature* kf = new KylaFeature;
			kf->id = sqlite3_column_int (selectFeaturesStatement, 0);

			const std::string featureName = reinterpret_cast<const char*> (
				sqlite3_column_text (selectFeaturesStatement, 2));
			auto name = stringPool_.Alloc (featureName.size ());
			::memcpy (name, featureName.data (), featureName.size ());
			kf->name = name;

			// Description may be null
			if (sqlite3_column_text (selectFeaturesStatement, 3)) {
				const std::string featureDescription = reinterpret_cast<const char*> (
					sqlite3_column_text (selectFeaturesStatement, 3));
				auto description = stringPool_.Alloc (featureDescription.size ());
				::memcpy (description, featureDescription.data (), featureDescription.size ());
				kf->description = description;
			} else {
				kf->description = nullptr;
			}

			if (sqlite3_column_type (selectFeaturesStatement, 4) == SQLITE_NULL) {
				kf->parentId = -1;
			} else {
				kf->parentId = sqlite3_column_int (selectFeaturesStatement, 4);
			}

			features.push_back(kf);
		}

		sqlite3_finalize (selectFeaturesStatement);

	}

	~KylaFeatures ()
	{
		for (auto f : features) {
			delete f;
		}
	}

	std::vector<KylaFeature*> features;

private:
	Pool<char> stringPool_;
};

////////////////////////////////////////////////////////////////////////////////
struct KylaProperty
{
	kyla::Property property;
};

////////////////////////////////////////////////////////////////////////////////
struct KylaInstaller
{
	KylaInstaller (const char* path)
	{
		sqlite3_open (path, &db_);
	}

	~KylaInstaller ()
	{
		sqlite3_close (db_);
	}

	void SetProperty (const char* name, const KylaProperty* value)
	{
		environment_.SetProperty (name, value->property);
	}

	bool HasProperty (const char* name) const
	{
		return environment_.HasProperty (name);
	}

	KylaProperty* GetProperty (const char* name) const
	{
		KylaProperty* result = new KylaProperty;
		result->property = environment_.GetProperty (name);
		return result;
	}

	void Install ()
	{
		kyla::Installer installer;
		installer.Install (db_, environment_);
	}

	sqlite3* GetInstallationDatabase ()
	{
		return db_;
	}

	kyla::InstallationEnvironment& GetEnvironment ()
	{
		return environment_;
	}

private:
	kyla::InstallationEnvironment environment_;
	sqlite3* db_;
};

////////////////////////////////////////////////////////////////////////////////
int kylaOpenInstallationPackage (const char* path, KylaInstaller** installer)
{
	*installer = new KylaInstaller (path);
	return KylaSuccess;
}

////////////////////////////////////////////////////////////////////////////////
int kylaCoseInstallationPackage (KylaInstaller* package)
{
	delete package;
}

////////////////////////////////////////////////////////////////////////////////
KylaProperty* kylaCreateStringProperty (const char* s)
{
	KylaProperty* p = new KylaProperty;
	p->property = kyla::Property (s);

	return p;
}

////////////////////////////////////////////////////////////////////////////////
KylaProperty* CreateIntProperty (const int i)
{
	KylaProperty* p = new KylaProperty;
	p->property = kyla::Property (i);

	return p;
}

////////////////////////////////////////////////////////////////////////////////
KylaProperty* kylaCreateBinaryProperty (const void* d, const int size)
{
	KylaProperty* p = new KylaProperty;
	p->property = kyla::Property (d, size);

	return p;
}

////////////////////////////////////////////////////////////////////////////////
int kylaDeleteProperty (struct KylaProperty* property)
{
	delete property;

	return KylaSuccess;
}

////////////////////////////////////////////////////////////////////////////////
int kylaGetFeatures (KylaInstaller* installer,
	KylaFeatures** features)
{
	*features = new KylaFeatures (installer->GetInstallationDatabase ());

	return KylaSuccess;
}

////////////////////////////////////////////////////////////////////////////////
int kylaDeleteFeatures (KylaFeatures* features)
{
	delete features;
}

////////////////////////////////////////////////////////////////////////////////
int kylaEnumerateFeatures(KylaFeatures* features, int* count, KylaFeature*** first)
{
	if (count) {
		*count = features->features.size ();
	}

	if (first) {
		*first = features->features.data ();
	}

	return KylaSuccess;
}

////////////////////////////////////////////////////////////////////////////////
int kylaSelectFeatures (KylaInstaller* installer,
	int count, KylaFeature** selected)
{
	std::vector<int> selectedIds (count, -1);

	for (int i = 0; i < count; ++i) {
		selectedIds [i] = selected [i]->id;
	}

	installer->GetEnvironment().SelectFeatures (selectedIds);

	return KylaSuccess;
}

////////////////////////////////////////////////////////////////////////////////
int kylaInstall (KylaInstaller* package, KylaProgressCallback callback)
{
	package->Install ();

	return KylaSuccess;
}

////////////////////////////////////////////////////////////////////////////////
int kylaSetProperty (KylaInstaller* installer, const char* name, const KylaProperty* value)
{
	// Property names starting with $ are reserved
	if (name && name [0] == '$') {
		return KylaError;
	}

	installer->SetProperty (name, value);

	return KylaSuccess;
}

////////////////////////////////////////////////////////////////////////////////
int kylaGetProperty (KylaInstaller *installer, const char *name, KylaProperty **output)
{
	if (installer->HasProperty (name)) {
		*output = installer->GetProperty (name);
		return KylaSuccess;
	} else {
		return KylaError;
	}
}

////////////////////////////////////////////////////////////////////////////////
int kylaCloseInstallationPackage (KylaInstaller* package)
{
	delete package;
}

////////////////////////////////////////////////////////////////////////////////
int kylaPropertyGetBinaryValue (const KylaProperty* property, void** d, int* size)
{
	if (property->property.type != kyla::PropertyType::Binary) {
		return KylaError;
	}

	if (d) {
		*d = property->property.b;
	}

	if (size) {
		*size = property->property.size;
	}

	return KylaSuccess;
}

////////////////////////////////////////////////////////////////////////////////
int kylaPropertyGetStringValue (const KylaProperty* property, const char** o)
{
	if (property->property.type != kyla::PropertyType::String) {
		return KylaError;
	}

	if (o) {
		*o = property->property.s;
	}

	return KylaSuccess;
}

////////////////////////////////////////////////////////////////////////////////
int kylaPropertyGetIntValue (const KylaProperty* property, int* i)
{
	if (property->property.type != kyla::PropertyType::Int) {
		return KylaError;
	}

	if (i) {
		*i = property->property.i;
	}

	return KylaSuccess;
}

////////////////////////////////////////////////////////////////////////////////
int kylaLog (KylaInstaller *installer, const char *logFileName, const int logLevel)
{
	if (!installer) {
		return KylaError;
	}

	if (logLevel < KylaLogLevelDebug || logLevel > KylaLogLevelError) {
		return KylaError;
	}

	installer->GetEnvironment().SetProperty ("$LogFilename", kyla::Property (logFileName));
	installer->GetEnvironment().SetProperty ("$LogLevel", kyla::Property (logLevel));
}
