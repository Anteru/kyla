/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#ifndef KYLA_CORE_INTERNAL_WEB_REPOSITORY_H
#define KYLA_CORE_INTERNAL_WEB_REPOSITORY_H

#include "PackedRepositoryBase.h"
#include "sql/Database.h"

namespace kyla {
class WebRepository final : public PackedRepositoryBase
{
public:
	WebRepository (const std::string& path);
	~WebRepository ();

private:
	Sql::Database& GetDatabaseImpl () override;
	std::unique_ptr<PackageFile> OpenPackage (const std::string& packageName) const override;

	Sql::Database db_;
	Path dbPath_;
	std::string url_;

public:
	struct Impl;

private:
	std::unique_ptr<Impl> impl_;
};
} // namespace kyla

#endif
