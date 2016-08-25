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

#include "PackedRepository.h"

#include "sql/Database.h"
#include "Exception.h"
#include "FileIO.h"
#include "Hash.h"
#include "Log.h"

#include "Compression.h"

#include <boost/format.hpp>

#include "install-db-structure.h"
#include "temp-db-structure.h"

#include <unordered_map>
#include <set>

namespace kyla {
///////////////////////////////////////////////////////////////////////////////
PackedRepository::PackedRepository (const char* path)
	: path_ (path)
{
	db_ = Sql::Database::Open (Path (path) / "repository.db");
}

///////////////////////////////////////////////////////////////////////////////
PackedRepository::~PackedRepository ()
{
}

namespace {
struct LocalPackageFile final : public PackedRepositoryBase::PackageFile
{
public:
	LocalPackageFile (std::unique_ptr<File>&& file)
		: file_ (std::move (file))
	{
	}

	bool Read (const int64 offset, const MutableArrayRef<>& buffer) override
	{
		file_->Seek (offset);
		return file_->Read (buffer) == buffer.GetSize ();
	}

private:
	std::unique_ptr<File> file_;
};
}

///////////////////////////////////////////////////////////////////////////////
std::unique_ptr<PackedRepositoryBase::PackageFile> PackedRepository::OpenPackage (const std::string& packageName) const
{
	return std::unique_ptr<PackageFile> { new LocalPackageFile{
		OpenFile (path_ / packageName, FileOpenMode::Read)
	}};
}

///////////////////////////////////////////////////////////////////////////////
Sql::Database& PackedRepository::GetDatabaseImpl ()
{
	return db_;
}
} // namespace kyla