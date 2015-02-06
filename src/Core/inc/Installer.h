#ifndef KYLA_CORE_INTERNAL_INSTALLER_H
#define KYLA_CORE_INTERNAL_INSTALLER_H

#include <vector>
#include <unordered_map>

#include <sqlite3.h>

#include "Property.h"

namespace kyla {
enum class PropertyCategory
{
	Installation,
	Environment,
	Internal
};

class InstallationEnvironment
{
public:
	void SetProperty (PropertyCategory category, const std::string& name,
		const Property& value);

	bool HasProperty (PropertyCategory category,const std::string& name) const;
	const Property& GetProperty (PropertyCategory category,const std::string& name) const;

	void SelectFeatures (const std::vector<int>& ids);
	const std::vector<int>& GetSelectedFeatures () const;

private:
	std::unordered_map<std::string, Property> installationProperties_;
	std::unordered_map<std::string, Property> internalProperties_;

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
