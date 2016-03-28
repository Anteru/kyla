#include "Repository.h"

#include "sql/Database.h"
#include "FileIO.h"
#include "Hash.h"

#include <unordered_map>

namespace kyla {
///////////////////////////////////////////////////////////////////////////////
void IRepository::Validate (const ValidationCallback& validationCallback)
{
	ValidateImpl (validationCallback);
}

///////////////////////////////////////////////////////////////////////////////
void IRepository::Repair (IRepository& other)
{
	RepairImpl (other);
}

///////////////////////////////////////////////////////////////////////////////
void IRepository::GetContentObjects (const ArrayRef<SHA256Digest>& requestedObjects,
	const GetContentObjectCallback& getCallback)
{
	GetContentObjectsImpl (requestedObjects, getCallback);
}

///////////////////////////////////////////////////////////////////////////////
std::vector<FilesetInfo> IRepository::GetFilesetInfos ()
{
	return GetFilesetInfosImpl ();
}

std::vector<FilesetInfo> GetFilesetInfoInternal (Sql::Database& db)
{
	static const char* querySql =
		"SELECT file_sets.Uuid, COUNT(content_objects.Id), SUM(content_objects.size) "
		"FROM file_sets INNER JOIN files "
		"ON file_sets.Id = files.FileSetId "
		"INNER JOIN content_objects "
		"ON content_objects.Id = files.ContentObjectId";

	auto query = db.Prepare (querySql);

	std::vector<FilesetInfo> result;

	while (query.Step ()) {
		FilesetInfo info;

		query.GetBlob (0, info.id);
		info.fileCount = query.GetInt64 (1);
		info.fileSize = query.GetInt64 (2);

		result.push_back (info);
	}

	return result;
}

struct LooseRepository::Impl
{
public:
	Impl (const char* path)
		: db_ (Sql::Database::Open (Path (path) / ".ky" / "repository.db"))
		, path_ (path)
	{
	}

	void GetContentObjects (const ArrayRef<SHA256Digest>& requestedObjects,
		const IRepository::GetContentObjectCallback& getCallback)
	{
		// This assumes the repository is in a valid state - i.e. content
		// objects contain the right data and we're only requested content
		// objects we can serve. If a content object is requested which we
		// don't have, this will throw an exception

		for (const auto& hash: requestedObjects) {
			const auto filePath = Path{ path_ } / Path{ ".ky" }
				/ Path{ "objects" } / ToString (hash);

			auto file = OpenFile (filePath, FileOpenMode::Read);
			auto pointer = file->Map ();

			const ArrayRef<> fileContents{ pointer, file->GetSize () };
			getCallback (hash, fileContents);

			file->Unmap (pointer);
		}
	}

	void Validate (const IRepository::ValidationCallback& validationCallback)
	{
		// Get a list of (file, hash, size)
		// We sort by size first so we get small objects out of the way first
		// (slower progress, but more things getting processed) and speed up 
		// towards the end (larger files, higher throughput)
		static const char* querySql =
			"SELECT Hash, Size "
			"FROM content_objects "
			"ORDER BY Size";

		///@TODO(major) On Windows, sort this by disk cluster to get best
		/// disk access pattern
		/// See: https://msdn.microsoft.com/en-us/library/windows/desktop/aa364572%28v=vs.85%29.aspx

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
					kylaValidationResult_Missing);

				continue;
			}

			const auto statResult = Stat (filePath);

			///@TODO Try/catch here and report corrupted if something goes wrong?
			/// This would indicate the file got deleted or is read-protected
			/// while the validation is running

			if (statResult.size != size) {
				validationCallback (hash,
					filePath.string ().c_str (),
					kylaValidationResult_Corrupted);

				continue;
			}

			// For size 0 files, don't bother checking the hash
			///@TODO Assert hash is the null hash
			if (size != 0 && ComputeSHA256 (filePath) != hash) {
				validationCallback (hash, 
					filePath.string ().c_str (),
					kylaValidationResult_Corrupted);

				continue;
			}

			validationCallback (hash, 
				filePath.string ().c_str (),
				kylaValidationResult_Ok);
		}
	}
	
	void Repair (IRepository& other)
	{
		// We use the validation logic here to find missing content objects
		// and fetch them from the other repository
		///@TODO(major) Handle the case that the database itself is corrupted
		/// In this case, we should probably prompt and ask what file sets need
		/// to be recovered.

		std::vector<SHA256Digest> requiredContentObjects;

		Validate ([&](const SHA256Digest& hash, const char*, kylaValidationResult result) -> void {
			if (result != kylaValidationResult_Ok) {
				// Missing or corrupted
				requiredContentObjects.push_back (hash);
			}
		});

		other.GetContentObjects (requiredContentObjects, [&](const SHA256Digest& hash,
			const ArrayRef<>& contents) -> void {
			const auto filePath = Path{ path_ } / Path{ ".ky" }
				/ Path{ "objects" } / ToString (hash);

			auto file = CreateFile (filePath);
			file->SetSize (contents.GetSize ());

			auto pointer = file->Map ();
			::memcpy (pointer, contents.GetData (), contents.GetSize ());
			file->Unmap (pointer);
		});
	}

	std::vector<FilesetInfo> GetFilesetInfos ()
	{
		return GetFilesetInfoInternal (db_);
	}

private:
	Sql::Database db_;
	Path path_;
};

///////////////////////////////////////////////////////////////////////////////
LooseRepository::~LooseRepository ()
{
}

///////////////////////////////////////////////////////////////////////////////
LooseRepository::LooseRepository (const char* path)
	: impl_ (new Impl{ path })
{
}

///////////////////////////////////////////////////////////////////////////////
LooseRepository::LooseRepository (LooseRepository&& other)
	: impl_ (std::move (other.impl_))
{
}

///////////////////////////////////////////////////////////////////////////////
LooseRepository& LooseRepository::operator= (LooseRepository&& other)
{
	impl_ = std::move (other.impl_);
	return *this;
}

///////////////////////////////////////////////////////////////////////////////
void LooseRepository::ValidateImpl (const ValidationCallback& validationCallback)
{
	impl_->Validate (validationCallback);
}

///////////////////////////////////////////////////////////////////////////////
void LooseRepository::GetContentObjectsImpl (const ArrayRef<SHA256Digest>& requestedObjects,
	const IRepository::GetContentObjectCallback& getCallback)
{
	impl_->GetContentObjects (requestedObjects, getCallback);
}

///////////////////////////////////////////////////////////////////////////////
void LooseRepository::RepairImpl (IRepository& other)
{
	impl_->Repair (other);
}

///////////////////////////////////////////////////////////////////////////////
std::vector<FilesetInfo> LooseRepository::GetFilesetInfosImpl ()
{
	return impl_->GetFilesetInfos ();
}

///////////////////////////////////////////////////////////////////////////////
struct DeployedRepository::Impl
{
public:
	Impl (const char* path)
		: db_ (Sql::Database::Open (Path (path) / "k.db"))
		, path_ (path)
	{
	}

	void Validate (const IRepository::ValidationCallback& validationCallback)
	{
		// Get a list of (file, hash, size)
		// We sort by size first so we get small objects out of the way first
		// (slower progress, but more things getting processed) and speed up 
		// towards the end (larger files, higher throughput)
		static const char* querySql = 
			"SELECT files.path, content_objects.Hash, content_objects.Size "
			"FROM files "
			"LEFT JOIN content_objects ON content_objects.Id = files.ContentObjectId "
			"ORDER BY size";

		///@TODO(major) On Windows, sort this by disk cluster to get best
		/// disk access pattern
		/// See: https://msdn.microsoft.com/en-us/library/windows/desktop/aa364572%28v=vs.85%29.aspx

		auto query = db_.Prepare (querySql);

		while (query.Step ()) {
			const Path path = query.GetText (0);
			SHA256Digest hash;
			query.GetBlob (1, hash);
			const auto size = query.GetInt64 (2);

			const auto filePath = path_ / path;
			if (!boost::filesystem::exists (filePath)) {
				validationCallback (hash, filePath.string ().c_str (),
					kylaValidationResult_Missing);

				continue;
			}

			const auto statResult = Stat (filePath);

			///@TODO Try/catch here and report corrupted if something goes wrong?
			/// This would indicate the file got deleted or is read-protected
			/// while the validation is running

			if (statResult.size != size) {
				validationCallback (hash, filePath.string ().c_str (),
					kylaValidationResult_Corrupted);

				continue;
			}

			// For size 0 files, don't bother checking the hash
			///@TODO Assert hash is the null hash
			if (size != 0 && ComputeSHA256 (filePath) != hash) {
				validationCallback (hash, filePath.string ().c_str (),
					kylaValidationResult_Corrupted);

				continue;
			}

			validationCallback (hash, filePath.string ().c_str (),
				kylaValidationResult_Ok);
		}
	}

	void Repair (IRepository& other)
	{
		// We use the validation logic here to find missing content objects
		// and fetch them from the other repository
		///@TODO(major) Handle the case that the database itself is corrupted
		/// In this case, we should probably prompt and ask what file sets need
		/// to be recovered.

		std::unordered_multimap<SHA256Digest, Path,
			HashDigestHash, HashDigestEqual> requiredEntries;

		// Extract keys
		std::vector<SHA256Digest> requiredContentObjects;

		Validate ([&](const SHA256Digest& hash, const char* path, kylaValidationResult result) -> void {
			if (result != kylaValidationResult_Ok) {
				// Missing or corrupted

				// New entry, so put it into the unique content objects as well
				if (requiredEntries.find (hash) == requiredEntries.end ()) {
					requiredContentObjects.push_back (hash);
				}

				requiredEntries.emplace (std::make_pair (hash, Path{ path }));
			}
		});

		other.GetContentObjects (requiredContentObjects, [&](const SHA256Digest& hash,
			const ArrayRef<>& contents) -> void {
			// We lookup all paths from the map here - could do a query as well
			// but as we built it anyway during validation, we reuse that

			auto range = requiredEntries.equal_range (hash);
			for (auto it = range.first; it != range.second; ++it) {
				auto file = CreateFile (it->second);
				file->SetSize (contents.GetSize ());

				auto pointer = file->Map ();
				::memcpy (pointer, contents.GetData (), contents.GetSize ());
				file->Unmap (pointer);
			}
		});
	}

	std::vector<FilesetInfo> GetFilesetInfos ()
	{
		return GetFilesetInfoInternal (db_);
	}

private:
	Sql::Database db_;
	Path path_;
};

///////////////////////////////////////////////////////////////////////////////
DeployedRepository::~DeployedRepository ()
{
}

///////////////////////////////////////////////////////////////////////////////
DeployedRepository::DeployedRepository (const char* path)
	: impl_ (new Impl { path })
{
}

///////////////////////////////////////////////////////////////////////////////
DeployedRepository::DeployedRepository (DeployedRepository&& other)
	: impl_ (std::move (other.impl_))
{
}

///////////////////////////////////////////////////////////////////////////////
DeployedRepository& DeployedRepository::operator= (DeployedRepository&& other)
{
	impl_ = std::move (other.impl_);
	return *this;
}

///////////////////////////////////////////////////////////////////////////////
void DeployedRepository::ValidateImpl (const ValidationCallback& validationCallback)
{
	impl_->Validate (validationCallback);
}

///////////////////////////////////////////////////////////////////////////////
void DeployedRepository::RepairImpl (IRepository& other)
{
	impl_->Repair (other);
}

///////////////////////////////////////////////////////////////////////////////
void DeployedRepository::GetContentObjectsImpl (
	const ArrayRef<SHA256Digest>& requestedObjects,
	const GetContentObjectCallback& getCallback)
{
	///@TODO(minor) Implement
}

///////////////////////////////////////////////////////////////////////////////
std::vector<FilesetInfo> DeployedRepository::GetFilesetInfosImpl ()
{
	return impl_->GetFilesetInfos ();
}

///////////////////////////////////////////////////////////////////////////////
std::unique_ptr<IRepository> OpenRepository (const char* path)
{
	if (boost::filesystem::exists (Path{ path } / Path{ ".ky" })) {
		// .ky indicates a loose repository
		return std::unique_ptr<IRepository> (new LooseRepository{ path });
	} else {
		// Assume deployed repository for now
		return std::unique_ptr<IRepository> (new DeployedRepository{ path });
	}
}
}