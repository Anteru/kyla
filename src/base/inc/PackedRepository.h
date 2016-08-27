/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#ifndef KYLA_CORE_INTERNAL_PACKED_REPOSITORY_H
#define KYLA_CORE_INTERNAL_PACKED_REPOSITORY_H

#include "BaseRepository.h"
#include "sql/Database.h"

namespace kyla {
class PackedRepositoryBase : public BaseRepository
{
public:
	~PackedRepositoryBase ();

	struct PackageFile
	{
		virtual ~PackageFile ()
		{
		}

		virtual bool Read (const int64 offset, const MutableArrayRef<>& buffer) = 0;
	};

private:
	void ValidateImpl (const ValidationCallback& validationCallback) override;

	void GetContentObjectsImpl (const ArrayRef<SHA256Digest>& requestedObjects,
		const GetContentObjectCallback& getCallback) override;

	virtual std::unique_ptr<PackageFile> OpenPackage (const std::string& packageName) const = 0;
};

class PackedRepository final : public PackedRepositoryBase
{
public:
	PackedRepository (const char* path);
	~PackedRepository ();

	PackedRepository (const PackedRepository& other) = delete;
	PackedRepository& operator= (const PackedRepository& other) = delete;

private:
	std::unique_ptr<PackageFile> OpenPackage (const std::string& packageName) const override;

	Sql::Database& GetDatabaseImpl () override;

	Sql::Database db_;

	Path path_;
};
} // namespace kyla

#endif