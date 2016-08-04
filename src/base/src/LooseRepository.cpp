/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matth�us G. Chajdas

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

#include "LooseRepository.h"

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
LooseRepository::LooseRepository  (const char* path)
	: db_ (Sql::Database::Open (Path (path) / ".ky" / "repository.db"))
	, path_ (path)
{
}

///////////////////////////////////////////////////////////////////////////////
LooseRepository::~LooseRepository ()
{
}

///////////////////////////////////////////////////////////////////////////////
Sql::Database& LooseRepository::GetDatabaseImpl ()
{
	return db_;
}

///////////////////////////////////////////////////////////////////////////////
void LooseRepository::GetContentObjectsImpl (const ArrayRef<SHA256Digest>& requestedObjects,
	const Repository::GetContentObjectCallback& getCallback)
{
	// This assumes the repository is in a valid state - i.e. content
	// objects contain the right data and we're only requested content
	// objects we can serve. If a content object is requested which we
	// don't have, this will throw an exception

	for (const auto& hash : requestedObjects) {
		const auto filePath = Path{ path_ } / Path{ ".ky" }
			/ Path{ "objects" } / ToString (hash);

		auto file = OpenFile (filePath, FileOpenMode::Read);
		const auto fileSize = file->GetSize ();

		if (fileSize > 0) {
			auto pointer = file->Map ();

			const ArrayRef<> fileContents{ pointer, file->GetSize () };
			getCallback (hash, fileContents, 0, file->GetSize ());

			file->Unmap (pointer);
		} else {
			getCallback (hash, ArrayRef<> {}, 0, 0);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
void LooseRepository::ValidateImpl (const Repository::ValidationCallback& validationCallback)
{
	// Get a list of (file, hash, size)
	// We sort by size first so we get small objects out of the way first
	// (slower progress, but more things getting processed) and speed up
	// towards the end (larger files, higher throughput)
	static const char* querySql =
		"SELECT Hash, Size "
		"FROM content_objects "
		"ORDER BY Size";
	
	auto query = db_.Prepare (querySql);

	while (query.Step ()) {
		SHA256Digest hash;
		query.GetBlob (0, hash);
		const auto size = query.GetInt64 (1);

		const auto filePath = Path{ path_ } / Path{ ".ky" }
		/ Path{ "objects" } / ToString (hash);

		if (!boost::filesystem::exists (filePath)) {
			validationCallback (hash,
				filePath.string ().c_str (),
				ValidationResult::Missing);

			continue;
		}

		const auto statResult = Stat (filePath);

		///@TODO(minor) Try/catch here and report corrupted if something goes wrong?
		/// This would indicate the file got deleted or is read-protected
		/// while the validation is running

		if (statResult.size != size) {
			validationCallback (hash,
				filePath.string ().c_str (),
				ValidationResult::Corrupted);

			continue;
		}

		// For size 0 files, don't bother checking the hash
		///@TODO(minor) Assert hash is the null hash
		if (size != 0 && ComputeSHA256 (filePath) != hash) {
			validationCallback (hash,
				filePath.string ().c_str (),
				ValidationResult::Corrupted);

			continue;
		}

		validationCallback (hash,
			filePath.string ().c_str (),
			ValidationResult::Ok);
	}
}

///////////////////////////////////////////////////////////////////////////////
void LooseRepository::RepairImpl (Repository& source)
{
	// We use the validation logic here to find missing content objects
	// and fetch them from the source repository
	///@TODO(major) Handle the case that the database itself is corrupted
	/// In this case, we should probably prompt and ask what file sets need
	/// to be recovered.

	std::vector<SHA256Digest> requiredContentObjects;

	Validate ([&](const SHA256Digest& hash, const char*, const ValidationResult result) -> void {
		if (result != ValidationResult::Ok) {
			// Missing or corrupted
			requiredContentObjects.push_back (hash);
		}
	});

	source.GetContentObjects (requiredContentObjects, [&](const SHA256Digest& hash,
		const ArrayRef<>& contents,
		const int64 offset, const int64 totalSize) -> void {
		const auto filePath = Path{ path_ } / Path{ ".ky" }
		/ Path{ "objects" } / ToString (hash);

		std::unique_ptr<File> file;
		if (offset == 0) {
			file = CreateFile (filePath);
			file->SetSize (contents.GetSize ());
		} else {
			file = OpenFile (filePath, FileOpenMode::Write);
		}

		byte* pointer = static_cast<byte*> (file->Map ());
		::memcpy (pointer + offset, contents.GetData (), contents.GetSize ());
		file->Unmap (pointer);
	});
}
}