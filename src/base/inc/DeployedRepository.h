/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
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