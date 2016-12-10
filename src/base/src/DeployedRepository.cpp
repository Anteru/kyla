/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#include "DeployedRepository.h"

#include "sql/Database.h"
#include "Exception.h"
#include "FileIO.h"
#include "Hash.h"
#include "Log.h"

#include "Compression.h"

#include <boost/format.hpp>

#include "install-db-structure.h"

#include <unordered_map>
#include <set>

namespace kyla {
///////////////////////////////////////////////////////////////////////////////
DeployedRepository::DeployedRepository (const char* path, Sql::OpenMode openMode)
	: db_ (Sql::Database::Open (Path (path) / "k.db", openMode))
	, path_ (path)
{
}

///////////////////////////////////////////////////////////////////////////////
DeployedRepository::~DeployedRepository ()
{
}

///////////////////////////////////////////////////////////////////////////////
Sql::Database& DeployedRepository::GetDatabaseImpl ()
{
	return db_;
}

///////////////////////////////////////////////////////////////////////////////
void DeployedRepository::ValidateImpl (const Repository::ValidationCallback& validationCallback,
	ExecutionContext& context)
{
	// Get a list of (file, hash, size)
	// We sort by size first so we get small objects out of the way first
	// (slower progress, but more things getting processed) and speed up
	// towards the end (larger files, higher throughput)
	static const char* queryFilesContentSql =
		"SELECT files.path, fs_contents.Hash, fs_contents.Size "
		"FROM fs_files "
		"LEFT JOIN fs_contents ON fs_contents.Id = fs_files.ContentId "
		"ORDER BY size";
	
	auto query = db_.Prepare (queryFilesContentSql);

	const int64 objectCount = [=]() -> int64
	{
		static const char* queryObjectCountSql =
			"SELECT COUNT(*) FROM fs_contents";
		auto countQuery = db_.Prepare (queryObjectCountSql);
		countQuery.Step ();
		return countQuery.GetInt64 (0);
	} ();

	ProgressHelper progress (context.progress);
	progress.SetStageTarget (objectCount);

	while (query.Step ()) {
		const Path path = query.GetText (0);
		SHA256Digest hash;
		query.GetBlob (1, hash);
		const auto size = query.GetInt64 (2);

		const auto filePath = path_ / path;
		if (!boost::filesystem::exists (filePath)) {
			validationCallback (hash, filePath.string ().c_str (),
				ValidationResult::Missing);

			++progress;
			continue;
		}

		const auto statResult = Stat (filePath);

		///@TODO(minor) Try/catch here and report corrupted if something goes wrong?
		/// This would indicate the file got deleted or is read-protected
		/// while the validation is running

		if (statResult.size != size) {
			validationCallback (hash, filePath.string ().c_str (),
				ValidationResult::Corrupted);

			++progress;
			continue;
		}

		// For size 0 files, don't bother checking the hash
		///@TODO(minor) Assert hash is the null hash
		if (size != 0 && ComputeSHA256 (filePath) != hash) {
			validationCallback (hash, filePath.string ().c_str (),
				ValidationResult::Corrupted);

			++progress;
			continue;
		}

		validationCallback (hash, filePath.string ().c_str (),
			ValidationResult::Ok);

		++progress;
	}
}

///////////////////////////////////////////////////////////////////////////////
void DeployedRepository::RepairImpl (Repository& source,
	ExecutionContext& context)
{
	// We use the validation logic here to find missing content objects
	// and fetch them from the source repository
	///@TODO(major) Handle the case that the database itself is corrupted
	/// In this case, we should probably prompt and ask what file sets need
	/// to be recovered.

	std::unordered_multimap<SHA256Digest, Path,
		ArrayRefHash, ArrayRefEqual> requiredEntries;

	// Extract keys
	std::vector<SHA256Digest> requiredContentObjects;

	///@TODO(minor) Handle progress reporting - should call an internal validate
	Validate ([&](const SHA256Digest& hash, const char* path, const ValidationResult result) -> void {
		if (result != ValidationResult::Ok) {
			// Missing or corrupted

			// New entry, so put it into the unique content objects as well
			if (requiredEntries.find (hash) == requiredEntries.end ()) {
				requiredContentObjects.push_back (hash);
			}

			requiredEntries.emplace (std::make_pair (hash, Path{ path }));
		}
	}, context);

	source.GetContentObjects (requiredContentObjects, [&](const SHA256Digest& hash,
		const ArrayRef<>& contents,
		const int64 offset,
		const int64 totalSize) -> void {
		// We lookup all paths from the map here - could do a query as well
		// but as we built it anyway during validation, we reuse that

		auto range = requiredEntries.equal_range (hash);
		for (auto it = range.first; it != range.second; ++it) {
			std::unique_ptr<File> file;

			if (offset == 0) {
				file = CreateFile (it->second);
				file->SetSize (contents.GetSize ());
			} else {
				file = OpenFile (it->second, FileOpenMode::Write);
			}

			byte* pointer = static_cast<byte*> (file->Map ());
			::memcpy (pointer + offset, contents.GetData (), contents.GetSize ());
			file->Unmap (pointer);
		}
	});
}

///////////////////////////////////////////////////////////////////////////////
void DeployedRepository::GetContentObjectsImpl (const ArrayRef<SHA256Digest>& requestedObjects,
	const Repository::GetContentObjectCallback& getCallback)
{
	auto query = db_.Prepare (
		"SELECT Path FROM fs_files "
		"WHERE ContentId=(SELECT Id FROM fs_contents WHERE Hash=?) "
		"LIMIT 1");

	for (const auto& hash : requestedObjects) {
		query.BindArguments (hash);
		query.Step ();

		const auto filePath = path_ / Path{ query.GetText (0) };

		auto file = OpenFile (filePath, FileOpenMode::Read);
		auto pointer = file->Map ();

		const ArrayRef<> fileContents{ pointer, file->GetSize () };
		getCallback (hash, fileContents, 0, file->GetSize ());

		file->Unmap (pointer);

		query.Reset ();
	}
}

///////////////////////////////////////////////////////////////////////////////
void DeployedRepository::ConfigureImpl (Repository& source,
	const ArrayRef<Uuid>& features,
	ExecutionContext& context)
{
	// We do this in WAL mode for performance
	db_.Execute ("PRAGMA journal_mode = WAL");
	db_.Execute ("PRAGMA synchronous = NORMAL");

	// We start by cleaning up all content objects which are not referenced
	// A deployed repository needs at least one file referencing a
	// content object, otherwise, the content object is missing. This
	// allows us to process partially uninstalled repositories (or a
	// repository that has been recovered.)
	db_.Execute (
		"DELETE FROM fs_contents WHERE "
		"Id IN ("
		"SELECT Id from fs_contents_with_reference_count "
		"WHERE ReferenceCount=0"
		");");

	// This copies everything over, so we can do joins on source and target
	// now. Assumes the source contains all file sets, content objects and
	// files we're about to configure
	db_.AttachTemporaryCopy ("source", source.GetDatabase ());

	ProgressHelper progressHelper (context.progress);
	progressHelper.Start (2);

	progressHelper.AdvanceStage ("Setup");

	// Store the file sets we're going to install in a temporary table for
	// joins, etc.
	auto pendingFeaturesTable = db_.CreateTemporaryTable ("pending_features",
		"Uuid BLOB NOT NULL UNIQUE");

	PreparePendingFeatures (context.log, features, progressHelper);
	UpdateFeatures ();
	UpdateFeatureIdsForUnchangedFiles ();
	RemoveChangedFiles (context.log);

	progressHelper.SetStageFinished ();
	progressHelper.AdvanceStage ("Install");
	GetNewContentObjects (source, context.log, progressHelper);
	CopyExistingFiles (context.log);
	Cleanup (context.log);
	progressHelper.SetStageFinished ();

	db_.Detach ("source");

	db_.Execute ("PRAGMA journal_mode = DELETE");
	db_.Execute ("ANALYZE");
	db_.Execute ("VACUUM");
}

///////////////////////////////////////////////////////////////////////////////
/**
Create a new temporary table pending_features which contains the UUIDs
of the file sets we're about to add
*/
void DeployedRepository::PreparePendingFeatures (Log& log, const ArrayRef<Uuid>& filesets,
	ProgressHelper& progress)
{
	{
		auto transaction = db_.BeginTransaction ();
		auto insertFilesetQuery = db_.Prepare (
			"INSERT INTO pending_features (Uuid) VALUES (?);");

		log.Debug ("Configure", "Selecting features for configure");

		progress.SetStageTarget (filesets.GetCount ());
		progress.SetAction ("Configuring filesets");
		for (const auto& fileset : filesets) {
			insertFilesetQuery.BindArguments (fileset);
			insertFilesetQuery.Step ();
			insertFilesetQuery.Reset ();

			log.Debug ("Configure", boost::format ("Selected feature: '%1%'") % ToString (fileset));
			++progress;
		}

		transaction.Commit ();
	}
}

/**
Insert the new filesets we're about to configure from pending_features
*/
void DeployedRepository::UpdateFeatures ()
{
	// Insert those we don't have yet into our features, but which are
	// pending
	db_.Execute (
		"INSERT INTO features (Uuid) "
		"SELECT Uuid FROM source.features "
		"WHERE source.features.Uuid IN (SELECT Uuid FROM pending_features) "
		"AND NOT source.features.Uuid IN (SELECT Uuid FROM features)");
}

/**
Update the file set ids of all files which remain unchanged, but have moved
to a new fileset.
*/
void DeployedRepository::UpdateFeatureIdsForUnchangedFiles ()
{
	// For files which have the same location and hash as before, update
	// the fileset id
	// This long query will find every file where the hash and the path
	// remained the same, and update it to use the new file set id we just
	// inserted above
	db_.Execute (
		"UPDATE files "
		"SET FeatureId=( "
		"    SELECT main.features.Id FROM main.features "
		"    WHERE main.features.Uuid = ( "
		"        SELECT source.features.Uuid FROM source.fs_files "
		"        INNER JOIN source.features ON source.fs_files.FeatureId = source.features.Id "
		"        WHERE source.fs_files.Path=main.fs_files.path) "
		") "
		"WHERE "
		"files.Path IN ( "
		"SELECT main.fs_files.Path FROM fs_files AS MainFiles "
		"    INNER JOIN main.fs_contents ON main.fs_files.ContentId = main.fs_contents.Id  "
		"    INNER JOIN source.fs_files ON source.fs_files.Path = main.fs_files.Path  "
		"    INNER JOIN source.fs_contents ON source.fs_files.ContentId = source.fs_contents.Id "
		"    WHERE main.fs_contents.Hash IS source.fs_contents.Hash "
		") ");
}

///////////////////////////////////////////////////////////////////////////////
/**
Remove all files which reference a different content object now.
*/
void DeployedRepository::RemoveChangedFiles (Log& log)
{
	// First, get rid of all files that are changing
	// That is, if a file is referencing a different content_object,
	// we have to remove it (those files will get replaced)

	auto changedFiles = db_.Prepare (
		"SELECT main.files.Path AS Path, main.fs_contents.Hash AS CurrentHash, source.fs_contents.Hash AS NewHash FROM main.fs_files "
		"INNER JOIN main.fs_contents ON main.fs_files.ContentId = main.fs_contents.Id "
		"INNER JOIN source.fs_files ON source.fs_files.Path = main.fs_files.Path "
		"INNER JOIN source.fs_contents ON source.files.ContentId = source.fs_contents.Id "
		"WHERE CurrentHash IS NOT NewHash "
		"AND source.fs_files.FeatureId IN "
		"(SELECT Id FROM source.features "
		"WHERE Uuid IN (SELECT Uuid FROM pending_features))");

	// files
	{
		auto deleteFileQuery = db_.Prepare (
			"DELETE FROM fs_files WHERE Path=?");

		while (changedFiles.Step ()) {
			deleteFileQuery.BindArguments (changedFiles.GetText (0));
			deleteFileQuery.Step ();
			deleteFileQuery.Reset ();

			boost::filesystem::remove (path_ / Path{ changedFiles.GetText (0) });

			log.Debug ("Configure", boost::format ("Deleted file '%1%'") % changedFiles.GetText (0));
		}

		log.Debug ("Configure", "Deleted changed files from repository");
	}

	// content objects
	db_.Execute ("DELETE FROM fs_contents "
		"WHERE Id IN "
		"(SELECT Id FROM fs_contents_with_reference_count WHERE ReferenceCount = 0)");
}

///////////////////////////////////////////////////////////////////////////////
/**
Get new content objects, but only for those files, for which we don't
have a content object already.
*/
void DeployedRepository::GetNewContentObjects (Repository& source, Log& log,
	ProgressHelper& progress)
{
	// Find all missing content objects in this database
	std::vector<SHA256Digest> requiredContentObjects;

	{
		auto diffQuery = db_.Prepare (
			"SELECT DISTINCT Hash FROM source.fs_contents "
			"INNER JOIN source.fs_files ON "
			"source.fs_contents.Id = source.fs_files.ContentId "
			"WHERE source.fs_files.FeatureId IN "
			"(SELECT Id FROM source.features "
			"WHERE Uuid IN (SELECT Uuid FROM pending_features)) "
			"AND NOT Hash IN (SELECT Hash FROM main.fs_contents)");

		while (diffQuery.Step ()) {
			SHA256Digest contentObjectHash;

			diffQuery.GetBlob (0, contentObjectHash);
			requiredContentObjects.push_back (contentObjectHash);

			log.Debug ("Configure", boost::format ("Discovered content '%1%'") % ToString (contentObjectHash));
		}
	}

	progress.SetStageTarget (requiredContentObjects.size ());

	// Fetch the missing ones now and store in the right places
	source.GetContentObjects (requiredContentObjects, [&](const SHA256Digest& hash,
		const ArrayRef<>& contents,
		const int64 offset,
		const int64 totalSize) -> void {
		const auto hashString = ToString (hash);

		bool hasStagingFile = false;
		const auto stagingFilePath = path_ / (hashString + ".kytmp");
		if ((offset != 0) || (contents.GetSize () != totalSize)) {
			std::unique_ptr<File> file;
			
			if (offset == 0) {
				log.Debug ("Configure",
					boost::format ("Created staging file %1%")
					% stagingFilePath);

				file = CreateFile (stagingFilePath);
				file->SetSize (totalSize);
			} else {
				log.Debug ("Configure",
					boost::format ("Appending to staging file %1%")
					% stagingFilePath);

				file = OpenFile (stagingFilePath, FileOpenMode::Write);
			}

			file->Seek (offset);
			file->Write (contents);

			if ((offset + contents.GetSize ()) != totalSize) {
				return;
			} else {
				hasStagingFile = true;
			}
		}

		auto transaction = db_.BeginTransaction ();
		log.Debug ("Configure", boost::format ("Received content object '%1%'") % hashString);

		int64 ContentId = -1;
		{
			auto insertContentObjectQuery = db_.Prepare (
				"INSERT INTO fs_contents (Hash, Size) "
				"VALUES (?, ?);");

			insertContentObjectQuery.BindArguments (hash, totalSize);
			insertContentObjectQuery.Step ();
			insertContentObjectQuery.Reset ();

			ContentId = db_.GetLastRowId ();

			log.Debug ("Configure", boost::format ("Stored content object '%1%' with id %2%") % hashString % ContentId);
		}

		auto insertFileQuery = db_.Prepare (
			"INSERT INTO main.fs_files (Path, ContentId, FeatureId) "
			"SELECT ?, ?, main.features.Id FROM source.fs_files "
			"INNER JOIN source.features ON source.features.Id = source.fs_files.FeatureId "
			"INNER JOIN features ON source.features.Uuid = main.features.Uuid "
			"WHERE source.files.path = ?"
		);

		auto getTargetFilesQuery = db_.Prepare (
			"SELECT Path FROM source.fs_files "
			"WHERE source.fs_files.ContentId = (SELECT Id FROM source.fs_contents WHERE source.fs_contents.Hash = ?)");

		getTargetFilesQuery.BindArguments (hash);

		bool isFirstFile = true;
		Path lastFilePath;
		while (getTargetFilesQuery.Step ()) {
			const Path targetPath{ getTargetFilesQuery.GetText (0) };

			progress.SetAction (getTargetFilesQuery.GetText (0));

			boost::filesystem::create_directories (path_ / targetPath.parent_path ());

			if (hasStagingFile) {
				if (isFirstFile) {
					log.Debug ("Configure", 
						boost::format ("Renaming staging file %1% to %2%") 
							% stagingFilePath % targetPath);

					boost::filesystem::rename (stagingFilePath,
						path_ / targetPath);
					isFirstFile = false;
				} else {
					log.Debug ("Configure",
						boost::format ("Copying file %1% to %2%")
						% stagingFilePath % targetPath);

					assert (!lastFilePath.empty ());
					boost::filesystem::copy_file (lastFilePath,
						path_ / targetPath);
				}

				lastFilePath = path_ / targetPath;
			} else {
				log.Debug ("Configure", 
					boost::format ("Creating file %1%") % targetPath);

				auto file = CreateFile (path_ / targetPath);
				file->Write (contents);
			}

			insertFileQuery.BindArguments (targetPath.string (), ContentId, targetPath.string ());
			insertFileQuery.Step ();
			insertFileQuery.Reset ();

			log.Debug ("Configure", boost::format ("Wrote file %1%") % targetPath);
		}

		transaction.Commit ();
		++progress;
	});
}

///////////////////////////////////////////////////////////////////////////////
/**
Copy existing files when needed - we don't fetch content objects we
still have.
*/
void DeployedRepository::CopyExistingFiles (Log& log)
{
	// We may have files that only require local copies - find those
	// and execute them now
	// That is, we search for all files, that are in a pending_file_set,
	// but not present in our local copy yet (those haven't been added in
	// the loop above because we specifically excluded objects for which
	// we already have the content.)
	auto transaction = db_.BeginTransaction ();

	auto diffQuery = db_.Prepare (
		"SELECT Path, Hash FROM source.fs_contents "
		"INNER JOIN source.fs_files ON "
		"source.fs_contents.Id = source.fs_files.ContentId "
		"WHERE source.fs_files.FeatureId IN "
		"(SELECT Id FROM source.features "
		"WHERE Uuid IN (SELECT Uuid FROM pending_features)) "
		"AND NOT Path IN (SELECT Path FROM main.fs_files)");

	auto exemplarQuery = db_.Prepare (
		"SELECT Path, Id FROM fs_files "
		"INNER JOIN fs_contents ON files.ContentId = fs_contents.Id "
		"WHERE Hash=?");

	auto insertFileQuery = db_.Prepare (
		"INSERT INTO main.fs_files (Path, ContentId, FeatureId) "
		"SELECT ?, ?, main.features.Id FROM source.fs_files "
		"INNER JOIN source.features ON source.features.Id = source.fs_files.FeatureId "
		"INNER JOIN features ON source.features.Uuid = main.features.Uuid "
		"WHERE source.fs_files.path = ?"
	);

	while (diffQuery.Step ()) {
		SHA256Digest hash;
		diffQuery.GetBlob (1, hash);

		const Path path{ diffQuery.GetText (0) };
		boost::filesystem::create_directories (path_ / path.parent_path ());

		exemplarQuery.BindArguments (hash);
		exemplarQuery.Step ();

		const Path exemplarPath{ exemplarQuery.GetText (0) };
		boost::filesystem::copy_file (path_ / exemplarPath, path_ / path);

		insertFileQuery.BindArguments (path.string (),
			exemplarQuery.GetInt64 (1), path.string ());

		exemplarQuery.Reset ();

		log.Debug ("Configure", boost::format ("Copied file '%1%' to '%2%'") % exemplarPath.string () % path.string ());
	}

	transaction.Commit ();
}

///////////////////////////////////////////////////////////////////////////////
/**
Remove unused features, files and content objects.
*/
void DeployedRepository::Cleanup (Log& log)
{
	// The order here is files, features, fs_contents, to keep
	// referential integrity at all times

	// files
	{
		auto unusedFilesQuery = db_.Prepare (
			"SELECT Path FROM fs_files WHERE FeatureId NOT IN ("
			"    SELECT Id FROM features WHERE features.Uuid IN "
			"        (SELECT Uuid FROM pending_features)"
			"    )"
		);

		auto deleteFileQuery = db_.Prepare (
			"DELETE FROM fs_files WHERE Path=?");

		while (unusedFilesQuery.Step ()) {
			deleteFileQuery.BindArguments (unusedFilesQuery.GetText (0));
			deleteFileQuery.Step ();
			deleteFileQuery.Reset ();

			boost::filesystem::remove (path_ / Path{ unusedFilesQuery.GetText (0) });

			log.Debug ("Configure", boost::format ("Deleted file '%1%'") % unusedFilesQuery.GetText (0));
		}

		log.Debug ("Configure", "Deleted unused files from repository");
	}

	// features
	{
		db_.Execute ("DELETE FROM features "
			"WHERE features.Uuid NOT IN (SELECT Uuid FROM pending_features)");

		log.Debug ("Configure", "Deleted unused file sets from repository");
	}

	// content objects
	db_.Execute ("DELETE FROM fs_contents "
		"WHERE Id IN "
		"(SELECT Id FROM fs_contents_with_reference_count WHERE ReferenceCount = 0)");

	log.Debug ("Configure", "Deleted unused content objects from repository");
}

///////////////////////////////////////////////////////////////////////////////
std::unique_ptr<DeployedRepository> DeployedRepository::CreateFrom (Repository& source,
	const ArrayRef<Uuid>& filesets,
	const Path& targetDirectory,
	Repository::ExecutionContext& context)
{
	boost::filesystem::create_directories (targetDirectory);

	auto db = Sql::Database::Create ((targetDirectory / "k.db").string ().c_str ());

	db.Execute (install_db_structure);

	db.Close ();

	std::unique_ptr<DeployedRepository> result (new DeployedRepository{ 
		targetDirectory.string ().c_str (), Sql::OpenMode::ReadWrite });

	result->Configure (source, filesets, context);

	return std::move (result);
}
} // namespace kyla
