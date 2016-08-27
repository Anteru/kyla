/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#ifndef KYLA_CORA_INTERNAL_DEPLOYED_REPOSITORY_H
#define KYLA_CORE_INTERNAL_DEPLOYED_REPOSITORY_H

#include "BaseRepository.h"
#include "sql/Database.h"

namespace kyla {
class DeployedRepository final : public BaseRepository
{
public:
	DeployedRepository (const char* path, Sql::OpenMode openMode);
	~DeployedRepository ();

	DeployedRepository (DeployedRepository&& other);
	DeployedRepository& operator= (DeployedRepository&& other);

	static std::unique_ptr<DeployedRepository> CreateFrom (Repository& source,
		const ArrayRef<Uuid>& filesets,
		const Path& targetDirectory,
		Log& log, Progress& progress);

private:
	void ValidateImpl (const ValidationCallback& validationCallback) override;

	void GetContentObjectsImpl (const ArrayRef<SHA256Digest>& requestedObjects,
		const GetContentObjectCallback& getCallback) override;
	void RepairImpl (Repository& source) override;
	void ConfigureImpl (Repository& other,
		const ArrayRef<Uuid>& filesets,
		Log& log, Progress& progress) override;

	Sql::Database& GetDatabaseImpl () override;

	void PreparePendingFilesets (Log& log, const ArrayRef<Uuid>& filesets,
		ProgressHelper& progress);
	void UpdateFilesets ();
	void UpdateFilesetIdsForUnchangedFiles ();
	void RemoveChangedFiles (Log& log);
	void GetNewContentObjects (Repository& source, Log& log,
		ProgressHelper& progress);
	void CopyExistingFiles (Log& log);
	void Cleanup (Log& log);

	Sql::Database db_;
	Path path_;
};
} // namespace kyla

#endif