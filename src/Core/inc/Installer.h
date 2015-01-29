#ifndef KYLA_CORE_INTERNAL_INSTALLER_H
#define KYLA_CORE_INTERNAL_INSTALLER_H

#include <vector>
#include <unordered_map>

#include <sqlite3.h>

#include "Property.h"

namespace kyla {
class InstallationEnvironment
{
public:
	void SetProperty (const char* name,
		const Property& value);

	bool HasProperty (const char* name) const;
	const Property& GetProperty (const char* name) const;

	void SelectFeatures (const std::vector<int>& ids);
	const std::vector<int>& GetSelectedFeatures () const;

private:
	std::unordered_map<std::string, Property> properties_;
	std::vector<int> selectedFeatures_;
};

class Installer
{
public:
	void Install (sqlite3* installationDatabase,
		InstallationEnvironment environment);
};
}

#endif
