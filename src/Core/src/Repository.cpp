#include "Repository.h"

#include "sql/Database.h"
#include "FileIO.h"
#include "Hash.h"

#include "install-db-structure.h"
#include "temp-db-structure.h"

#include <unordered_map>
#include <set>

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

///////////////////////////////////////////////////////////////////////////////
std::string IRepository::GetFilesetName (const Uuid& id)
{
	return GetFilesetNameImpl (id);
}

///////////////////////////////////////////////////////////////////////////////
Sql::Database& IRepository::GetDatabase ()
{
	return GetDatabaseImpl ();
}

///////////////////////////////////////////////////////////////////////////////
std::vector<FilesetInfo> GetFilesetInfoInternal (Sql::Database& db)
{
	static const char* querySql =
		"SELECT file_sets.Uuid, COUNT(content_objects.Id), SUM(content_objects.size) "
		"FROM file_sets INNER JOIN files "
		"ON file_sets.Id = files.FileSetId "
		"INNER JOIN content_objects "
		"ON content_objects.Id = files.ContentObjectId "
		"GROUP BY file_sets.Uuid";

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

///////////////////////////////////////////////////////////////////////////////
std::string GetFilesetNameInternal (Sql::Database& db, const Uuid& id)
{
	static const char* querySql =
		"SELECT Name FROM file_sets "
		"WHERE Uuid = ?";

	auto query = db.Prepare (querySql);
	query.BindArguments (id);
	query.Step ();

	return query.GetText (0);
}

struct LooseRepository::Impl
{
public:
	Impl (const char* path)
		: db_ (Sql::Database::Open (Path (path) / ".ky" / "repository.db"))
		, path_ (path)
	{
	}

	Sql::Database& GetDatabase ()
	{
		return db_;
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
	return GetFilesetInfoInternal (impl_->GetDatabase ());
}

///////////////////////////////////////////////////////////////////////////////
Sql::Database& LooseRepository::GetDatabaseImpl ()
{
	return impl_->GetDatabase ();
}

///////////////////////////////////////////////////////////////////////////////
std::string LooseRepository::GetFilesetNameImpl (const Uuid& id)
{
	return GetFilesetNameInternal (impl_->GetDatabase (), id);
}

///////////////////////////////////////////////////////////////////////////////
struct DeployedRepository::Impl
{
public:
	Impl (const char* path, Sql::OpenMode openMode)
		: db_ (Sql::Database::Open (Path (path) / "k.db", openMode))
		, path_ (path)
	{
	}

	Sql::Database& GetDatabase ()
	{
		return db_;
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

	void GetContentObjects (const ArrayRef<SHA256Digest>& requestedObjects,
		const IRepository::GetContentObjectCallback& getCallback)
	{
		auto query = db_.Prepare (
			"SELECT Path FROM files "
			"WHERE ContentObjectId=(SELECT Id FROM content_objects WHERE Hash=?) "
			"LIMIT 1");

		for (const auto& hash : requestedObjects) {
			query.BindArguments (hash);
			query.Step ();

			const auto filePath = path_ / Path{ query.GetText (0) };

			auto file = OpenFile (filePath, FileOpenMode::Read);
			auto pointer = file->Map ();

			const ArrayRef<> fileContents{ pointer, file->GetSize () };
			getCallback (hash, fileContents);

			file->Unmap (pointer);

			query.Reset ();
		}
	}

	void Configure (IRepository& other,
		const ArrayRef<Uuid>& filesets)
	{
		// This copies everything over, so we can do joins on source and target
		// now
		db_.AttachTemporaryCopy ("source",  other.GetDatabase ());

		db_.Execute ("CREATE TEMPORARY TABLE pending_file_sets "
			"(Uuid BLOB NOT NULL UNIQUE);");

		{
			auto transaction = db_.BeginTransaction ();
			auto insertFilesetQuery = db_.Prepare (
				"INSERT INTO pending_file_sets (Uuid) VALUES (?);");

			for (const auto& fileset : filesets) {
				insertFilesetQuery.BindArguments (fileset);
				insertFilesetQuery.Step ();
				insertFilesetQuery.Reset ();
			}
			transaction.Commit ();
		}

		// Insert those we don't have yet into our file_sets, but which are
		// pending
		db_.Execute (
			"INSERT INTO file_sets (Name, Uuid) "
			"SELECT Name, Uuid FROM source.file_sets "
			"WHERE source.file_sets.Uuid IN (SELECT Uuid FROM pending_file_sets) "
			"AND NOT source.file_sets.Uuid IN (SELECT Uuid FROM file_sets)");

		// Find all missing content objects in this database
		std::vector<SHA256Digest> requiredContentObjects;

		{
			auto diffQuery = db_.Prepare (
				"SELECT Hash FROM source.content_objects "
				"INNER JOIN source.files ON "
				"source.content_objects.Id = source.files.ContentObjectId "
				"WHERE source.files.FileSetId IN "
				"(SELECT Id FROM source.file_sets "
				"WHERE Uuid IN (SELECT Uuid FROM pending_file_sets)) "
				"AND NOT Hash IN (SELECT Hash FROM main.content_objects)");

			while (diffQuery.Step ()) {
				SHA256Digest contentObjectHash;

				diffQuery.GetBlob (0, contentObjectHash);
				requiredContentObjects.push_back (contentObjectHash);
			}
		}

		// Fetch the missing ones now and store in the right places
		other.GetContentObjects (requiredContentObjects, [&](const SHA256Digest& hash,
			const ArrayRef<>& contents) -> void {

			int64 contentObjectId = -1;
			{
				auto insertContentObjectQuery = db_.Prepare (
					"INSERT INTO content_objects (Hash, Size) "
					"VALUES (?, ?);");

				insertContentObjectQuery.BindArguments (hash, contents.GetSize ());
				insertContentObjectQuery.Step ();

				contentObjectId = db_.GetLastRowId ();
			}

			auto insertFileQuery = db_.Prepare (
				"INSERT INTO files (Path, ContentObjectId, FileSetId) "
				"SELECT ?, ?, main.file_sets.Id FROM source.files "
				"INNER JOIN source.content_objects "
				"ON source.content_objects.Id = source.files.ContentObjectId "
				"INNER JOIN source.file_sets ON source.file_sets.Id = source.files.FileSetId "
				"INNER JOIN file_sets ON source.file_sets.Uuid = main.file_sets.Uuid "
				"WHERE source.content_objects.Hash = ?"
			);

			auto getTargetFilesQuery = db_.Prepare (
				"SELECT Path FROM source.files "
				"WHERE source.files.ContentObjectId = (SELECT Id FROM source.content_objects WHERE source.content_objects.Hash = ?)");
			getTargetFilesQuery.BindArguments (hash);

			while (getTargetFilesQuery.Step ()) {
				const Path targetPath{ getTargetFilesQuery.GetText (0) };

				boost::filesystem::create_directories (path_ / targetPath.parent_path ());

				auto file = CreateFile (path_ / targetPath);

				file->SetSize (contents.GetSize ());

				auto pointer = file->Map ();
				::memcpy (pointer, contents.GetData (), contents.GetSize ());
				file->Unmap (pointer);

				insertFileQuery.BindArguments (targetPath.string ().c_str (), contentObjectId, hash);
				insertFileQuery.Step ();
				insertFileQuery.Reset ();
			}
		});

		// We may have files that only require local copies - find those
		// and execute them now
		// That is, we search for all files, that are in a pending_file_set,
		// but not present in our local copy yet (where we would add them above)
		///@TODO(minor) implement this

		// Delete files
		{
			auto unusedFilesQuery = db_.Prepare (
				"SELECT Path FROM files WHERE FileSetId NOT IN ("
				"    SELECT FileSetId FROM file_sets WHERE file_sets.Uuid IN "
				"        (SELECT Uuid FROM pending_file_sets)"
				"    )"
			);

			while (unusedFilesQuery.Step ()) {
				boost::filesystem::remove (Path{ unusedFilesQuery.GetText (0) });
			}
		}

		// Remove all unused files now, by removing all files which are not
		// in a pending fileset, and then all non-pending filesets
		db_.Execute ("DELETE FROM files "
			"WHERE FileSetId NOT IN (SELECT FileSetId FROM file_sets WHERE "
			"file_sets.Uuid IN (SELECT Uuid FROM pending_file_sets))");

		// Now we just delete the content objects
		db_.Execute ("DELETE FROM content_objects "
			"WHERE Id IN "
			"(SELECT Id FROM content_objects_with_reference_count WHERE ReferenceCount = 0)");

		db_.Detach ("source");

		db_.Execute ("ANALYZE");
	}

private:
	Sql::Database db_;
	Path path_;
};

///////////////////////////////////////////////////////////////////////////////
std::unique_ptr<DeployedRepository> DeployedRepository::CreateFrom (IRepository& other,
	const ArrayRef<Uuid>& filesets,
	const Path& targetDirectory)
{
	auto db = Sql::Database::Create ((targetDirectory / "k.db").string ().c_str ());

	db.Execute (install_db_structure);

	db.Close ();

	std::unique_ptr<DeployedRepository> result (new DeployedRepository{ targetDirectory.string ().c_str (), true });

	result->impl_->Configure (other, filesets);

	return std::move (result);
}

///////////////////////////////////////////////////////////////////////////////
DeployedRepository::~DeployedRepository ()
{
}

///////////////////////////////////////////////////////////////////////////////
DeployedRepository::DeployedRepository (const char* path)
	: DeployedRepository (path, false)
{
}

///////////////////////////////////////////////////////////////////////////////
DeployedRepository::DeployedRepository (const char* path, const bool allowWriteAccess)
	: impl_ (new Impl{ path, allowWriteAccess ? Sql::OpenMode::ReadWrite : Sql::OpenMode::Read })
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
	impl_->GetContentObjects (requestedObjects, getCallback);
}

///////////////////////////////////////////////////////////////////////////////
std::vector<FilesetInfo> DeployedRepository::GetFilesetInfosImpl ()
{
	return GetFilesetInfoInternal (impl_->GetDatabase ());
}

///////////////////////////////////////////////////////////////////////////////
Sql::Database& DeployedRepository::GetDatabaseImpl ()
{
	return impl_->GetDatabase ();
}

///////////////////////////////////////////////////////////////////////////////
std::string DeployedRepository::GetFilesetNameImpl (const Uuid& id)
{
	return GetFilesetNameInternal (impl_->GetDatabase (), id);
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

///////////////////////////////////////////////////////////////////////////////
std::unique_ptr<IRepository> DeployRepository (IRepository& source,
	const char* destinationPath,
	const ArrayRef<Uuid>& filesets)
{
	Path targetPath{ destinationPath };
	boost::filesystem::create_directories (destinationPath);

	return DeployedRepository::CreateFrom (source, filesets, targetPath);
}
}