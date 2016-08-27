/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#include "PackedRepository.h"

#include "sql/Database.h"
#include "Exception.h"
#include "FileIO.h"
#include "Log.h"

#include "Compression.h"

#include <boost/format.hpp>

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