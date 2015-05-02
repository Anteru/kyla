#include "Kyla.h"

#include <sqlite3.h>
#include <string.h>
#include <stdlib.h>
#include <map>

#include "Installer.h"
#include "Property.h"

#include <memory>

#include "sql/Database.h"

#define KYLA_C_API_BEGIN() try {
#define KYLA_C_API_END() } catch (...) { return KylaError; }

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
	KylaFeatures (kyla::Sql::Database& db)
	{
		auto selectFeaturesStatement = db.Prepare (
			"SELECT Id, Name, UIName, UIDescription, ParentId FROM features;");

		while (selectFeaturesStatement.Step ()) {
			KylaFeature* kf = new KylaFeature;
			kf->id = selectFeaturesStatement.GetInt64 (0);

			const std::string featureName = selectFeaturesStatement.GetText (2);
			auto name = stringPool_.Alloc (featureName.size ());
			::memcpy (name, featureName.data (), featureName.size ());
			kf->name = name;

			// Description may be null
			if (selectFeaturesStatement.GetText (3)) {
				const std::string featureDescription = selectFeaturesStatement.GetText (3);
				auto description = stringPool_.Alloc (featureDescription.size ());
				::memcpy (description, featureDescription.data (), featureDescription.size ());
				kf->description = description;
			} else {
				kf->description = nullptr;
			}

			if (selectFeaturesStatement.GetColumnType (4) == kyla::Sql::Type::Null) {
				kf->parentId = -1;
			} else {
				kf->parentId = selectFeaturesStatement.GetInt64 (4);
			}

			features.push_back(kf);
		}
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
		db_ = kyla::Sql::Database::Open (path);
	}

	~KylaInstaller ()
	{
	}

	void SetProperty (kyla::PropertyCategory category, const std::string& name,
		const KylaProperty* value)
	{
		environment_.SetProperty (category, name, value->property);
	}

	bool HasProperty (kyla::PropertyCategory category,
		const std::string& name) const
	{
		return environment_.HasProperty (category, name);
	}

	KylaProperty* GetProperty (kyla::PropertyCategory category,
		const std::string& name) const
	{
		KylaProperty* result = new KylaProperty;
		result->property = environment_.GetProperty (category, name);
		return result;
	}

	void SelectFeatures (const std::vector<int>& featureIds)
	{
		selectedFeatures_ = featureIds;
	}

	void InstallProduct ()
	{
		kyla::Installer installer;
		installer.InstallProduct (db_, environment_, selectedFeatures_);
	}

	kyla::Sql::Database& GetInstallationDatabase ()
	{
		return db_;
	}

	kyla::InstallationEnvironment& GetEnvironment ()
	{
		return environment_;
	}

private:
	kyla::InstallationEnvironment	environment_;
	std::vector<int>				selectedFeatures_;
	kyla::Sql::Database				db_;
};

////////////////////////////////////////////////////////////////////////////////
int kylaOpenInstallationPackage (const char* path, KylaInstaller** installer)
{
	KYLA_C_API_BEGIN ();

	if (path == nullptr) {
		return KylaError;
	}

	if (installer == nullptr) {
		return KylaError;
	}

	*installer = new KylaInstaller (path);
	return KylaSuccess;
	KYLA_C_API_END ();
}

////////////////////////////////////////////////////////////////////////////////
int kylaCoseInstallationPackage (KylaInstaller* package)
{
	KYLA_C_API_BEGIN ();

	if (package == nullptr) {
		return KylaError;
	}

	delete package;
	return KylaSuccess;
	KYLA_C_API_END ();
}

////////////////////////////////////////////////////////////////////////////////
int kylaCreateStringProperty (const char* s, KylaProperty** p)
{
	KYLA_C_API_BEGIN ();
	*p = new KylaProperty;
	(*p)->property = kyla::Property (s);

	return KylaSuccess;
	KYLA_C_API_END ();
}

////////////////////////////////////////////////////////////////////////////////
int CreateIntProperty (const int i, KylaProperty** p)
{
	KYLA_C_API_BEGIN ();
	*p = new KylaProperty;
	(*p)->property = kyla::Property (i);

	return KylaSuccess;
	KYLA_C_API_END ();
}

////////////////////////////////////////////////////////////////////////////////
int kylaCreateBinaryProperty (const void* d, const int size, KylaProperty** p)
{
	KYLA_C_API_BEGIN ();
	*p = new KylaProperty;
	(*p)->property = kyla::Property (d, size);

	return KylaSuccess;
	KYLA_C_API_END ();
}

////////////////////////////////////////////////////////////////////////////////
int kylaDeleteProperty (struct KylaProperty* property)
{
	KYLA_C_API_BEGIN ();

	if (property == nullptr) {
		return KylaError;
	}

	delete property;

	return KylaSuccess;
	KYLA_C_API_END ();
}

////////////////////////////////////////////////////////////////////////////////
int kylaGetFeatures (KylaInstaller* installer,
	KylaFeatures** features)
{
	KYLA_C_API_BEGIN ();

	if (installer == nullptr) {
		return KylaError;
	}

	if (features == nullptr) {
		return KylaError;
	}

	*features = new KylaFeatures (installer->GetInstallationDatabase ());

	return KylaSuccess;
	KYLA_C_API_END ();
}

////////////////////////////////////////////////////////////////////////////////
int kylaDeleteFeatures (KylaFeatures* features)
{
	KYLA_C_API_BEGIN ();

	if (features == nullptr) {
		return KylaError;
	}

	delete features;
	return KylaSuccess;
	KYLA_C_API_END ();
}

////////////////////////////////////////////////////////////////////////////////
int kylaEnumerateFeatures(KylaFeatures* features, int* count, KylaFeature*** first)
{
	KYLA_C_API_BEGIN ();

	if (features == nullptr) {
		return KylaError;
	}

	if (count) {
		*count = features->features.size ();
	}

	if (first) {
		*first = features->features.data ();
	}

	return KylaSuccess;
	KYLA_C_API_END ();
}

////////////////////////////////////////////////////////////////////////////////
int kylaSelectFeatures (KylaInstaller* installer,
	int count, KylaFeature** selected)
{
	KYLA_C_API_BEGIN ();

	if (installer == nullptr) {
		return KylaError;
	}

	if (count == 0) {
		return KylaSuccess;
	}

	if (selected == nullptr) {
		return KylaError;
	}

	std::vector<int> selectedIds (count, -1);

	for (int i = 0; i < count; ++i) {
		selectedIds [i] = selected [i]->id;
	}

	installer->SelectFeatures (selectedIds);

	return KylaSuccess;
	KYLA_C_API_END ();
}

////////////////////////////////////////////////////////////////////////////////
int kylaInstall (KylaInstaller* package, KylaProgressCallback /* callback */)
{
	KYLA_C_API_BEGIN ();
	package->InstallProduct ();

	return KylaSuccess;
	KYLA_C_API_END ();
}

namespace {
static kyla::PropertyCategory ToKylaPropertyCategory (KylaPropertyCategory category)
{
	switch (category) {
	case KylaPropertyCategoryInstallation:
		return kyla::PropertyCategory::Installation;
	case KylaPropertyCategoryEnvironment:
		return kyla::PropertyCategory::Environment;
	}
}
}

////////////////////////////////////////////////////////////////////////////////
int kylaSetProperty (KylaInstaller* installer, KylaPropertyCategory category, const char* name, const KylaProperty* value)
{
	KYLA_C_API_BEGIN ();
	if (installer == nullptr) {
		return KylaError;
	}

	if (name == nullptr) {
		return KylaError;
	}

	if (value == nullptr) {
		return KylaError;
	}

	installer->SetProperty (ToKylaPropertyCategory (category), name, value);

	return KylaSuccess;
	KYLA_C_API_END ();
}

////////////////////////////////////////////////////////////////////////////////
int kylaGetProperty (KylaInstaller *installer, KylaPropertyCategory category, const char *name, KylaProperty **output)
{
	KYLA_C_API_BEGIN ();
	if (installer == nullptr) {
		return KylaError;
	}

	if (name == nullptr) {
		return KylaError;
	}

	if (installer->HasProperty (ToKylaPropertyCategory (category), name)) {
		*output = installer->GetProperty (ToKylaPropertyCategory (category), name);
		return KylaSuccess;
	} else {
		return KylaError;
	}
	KYLA_C_API_END ();
}

////////////////////////////////////////////////////////////////////////////////
int kylaCloseInstallationPackage (KylaInstaller* package)
{
	KYLA_C_API_BEGIN ();
	if (package == nullptr) {
		return KylaError;
	}

	delete package;

	return KylaSuccess;
	KYLA_C_API_END ();
}

////////////////////////////////////////////////////////////////////////////////
int kylaPropertyGetBinaryValue (const KylaProperty* property, const void** d, int* size)
{
	KYLA_C_API_BEGIN ();
	if (property == nullptr) {
		return KylaError;
	}

	if (property->property.GetType () != kyla::PropertyType::Binary) {
		return KylaError;
	}

	const void* r = property->property.GetBinary (size);
	if (d) {
		*d = r;
	}

	return KylaSuccess;
	KYLA_C_API_END ();
}

////////////////////////////////////////////////////////////////////////////////
int kylaPropertyGetStringValue (const KylaProperty* property, const char** o)
{
	KYLA_C_API_BEGIN ();
	if (property == nullptr) {
		return KylaError;
	}

	if (property->property.GetType () != kyla::PropertyType::String) {
		return KylaError;
	}

	if (o) {
		*o = property->property.GetString ();
	}

	return KylaSuccess;
	KYLA_C_API_END ();
}

////////////////////////////////////////////////////////////////////////////////
int kylaPropertyGetIntValue (const KylaProperty* property, int* i)
{
	KYLA_C_API_BEGIN ();
	if (property == nullptr) {
		return KylaError;
	}

	if (property->property.GetType () != kyla::PropertyType::Int) {
		return KylaError;
	}

	if (i) {
		*i = property->property.GetInt ();
	}

	return KylaSuccess;
	KYLA_C_API_END ();
}

////////////////////////////////////////////////////////////////////////////////
int kylaConfigureLog (KylaInstaller *installer, const char *logFileName, const int logLevel)
{
	KYLA_C_API_BEGIN ();
	if (!installer) {
		return KylaError;
	}

	if (logLevel < KylaLogLevelTrace || logLevel > KylaLogLevelError) {
		return KylaError;
	}

	installer->GetEnvironment().SetProperty (kyla::PropertyCategory::Internal,
		"LogFilename",
		kyla::Property (logFileName));
	installer->GetEnvironment().SetProperty (kyla::PropertyCategory::Internal,
		"LogLevel",
		kyla::Property (logLevel));

	return KylaSuccess;
	KYLA_C_API_END ();
}
