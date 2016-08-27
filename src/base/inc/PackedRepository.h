/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#ifndef KYLA_CORE_INTERNAL_PACKED_REPOSITORY_H
#define KYLA_CORE_INTERNAL_PACKED_REPOSITORY_H

#include "PackedRepositoryBase.h"
#include "sql/Database.h"

namespace kyla {
class PackedRepository final : public PackedRepositoryBase
{
public:
	PackedRepository (const char* path);
	~PackedRepository ();

private:
	std::unique_ptr<PackageFile> OpenPackage (const std::string& packageName) const override;

	Sql::Database& GetDatabaseImpl () override;

	Sql::Database db_;

	Path path_;
};
} // namespace kyla

#endif