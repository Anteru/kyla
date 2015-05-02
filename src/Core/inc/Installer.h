#ifndef KYLA_CORE_INTERNAL_INSTALLER_H
#define KYLA_CORE_INTERNAL_INSTALLER_H

#include <vector>
#include <unordered_map>

#include "Property.h"

namespace kyla {
namespace Sql {
class Database;
}
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

private:
	std::unordered_map<std::string, Property> installationProperties_;
	std::unordered_map<std::string, Property> internalProperties_;
};

class Installer
{
public:
	void InstallProduct (Sql::Database& installationDatabase,
		InstallationEnvironment environment,
		const std::vector<int>& selectedFeatures);
};
}

#endif
