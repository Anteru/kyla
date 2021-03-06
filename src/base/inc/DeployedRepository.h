/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#ifndef KYLA_CORE_INTERNAL_DEPLOYED_REPOSITORY_H
#define KYLA_CORE_INTERNAL_DEPLOYED_REPOSITORY_H

#include "BaseRepository.h"
#include "sql/Database.h"

namespace kyla {
class DeployedRepository final : public BaseRepository
{
public:
	DeployedRepository (const char* path, Sql::OpenMode openMode);
	~DeployedRepository ();

	static std::unique_ptr<DeployedRepository> CreateFrom (Repository& source,
		const ArrayRef<Uuid>& featureIds,
		const Path& targetDirectory,
		ExecutionContext& context);

private:
	void GetContentObjectsImpl (const ArrayRef<SHA256Digest>& requestedObjects,
		const GetContentObjectCallback& getCallback,
		ExecutionContext& context) override;
	void RepairImpl (Repository& source,
		ExecutionContext& context,
		RepairCallback repairCallback,
		bool restore) override;
	void ConfigureImpl (Repository& other,
		const ArrayRef<Uuid>& featureIds,
		ExecutionContext& context) override;

	Sql::Database& GetDatabaseImpl () override;

	Sql::Database db_;
	Path path_;
};
} // namespace kyla

#endif