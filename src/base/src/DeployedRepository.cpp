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
void DeployedRepository::RepairImpl (Repository& source,
	ExecutionContext& context,
	RepairCallback repairCallback,
	bool restore)
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
	// Get a list of (file, hash, size)
	// We sort by size first so we get small objects out of the way first
	// (slower progress, but more things getting processed) and speed up
	// towards the end (larger files, higher throughput)
	static const char* queryFilesContentSql =
		"SELECT fs_files.path, fs_contents.Hash, fs_contents.Size "
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
			if (restore) {
				requiredContentObjects.push_back (hash);
			} else {
				repairCallback (filePath.string ().c_str (),
					RepairResult::Missing);
			}

			++progress;
			continue;
		}

		const auto statResult = Stat (filePath);

		///@TODO(minor) Try/catch here and report corrupted if something goes wrong?
		/// This would indicate the file got deleted or is read-protected
		/// while the validation is running

		if (statResult.size != size && !restore) {
			if (restore) {
				requiredContentObjects.push_back (hash);
			} else {
				repairCallback (filePath.string ().c_str (),
					RepairResult::Corrupted);
			}

			++progress;
			continue;
		}

		// For size 0 files, don't bother checking the hash
		///@TODO(minor) Assert hash is the null hash
		if (size != 0 && ComputeSHA256 (filePath) != hash) {
			if (restore) {
				requiredContentObjects.push_back (hash);
			} else {
				repairCallback (filePath.string ().c_str (),
					RepairResult::Corrupted);
			}

			++progress;
			continue;
		}

		repairCallback (filePath.string ().c_str (),
			RepairResult::Ok);

		++progress;
	}

	if (restore) {
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
					file = OpenFile (it->second, FileAccess::Write);
				}

				byte* pointer = static_cast<byte*> (file->Map ());
				::memcpy (pointer + offset, contents.GetData (), contents.GetSize ());
				file->Unmap (pointer);

				repairCallback (it->second.string ().c_str (), 
					RepairResult::Restored);
			}
		});
	}
}

///////////////////////////////////////////////////////////////////////////////
void DeployedRepository::GetContentObjectsImpl (const ArrayRef<SHA256Digest>& requestedObjects,
	const Repository::GetContentObjectCallback& getCallback)
{
	auto query = db_.Prepare (
		R"_(SELECT Path FROM fs_files 
		WHERE ContentId=(SELECT Id FROM fs_contents WHERE Hash=?)
		LIMIT 1)_");

	for (const auto& hash : requestedObjects) {
		query.BindArguments (hash);
		query.Step ();

		const auto filePath = path_ / Path{ query.GetText (0) };

		auto file = OpenFile (filePath, FileAccess::Read);
		auto pointer = file->Map ();

		const ArrayRef<> fileContents{ pointer, file->GetSize () };
		getCallback (hash, fileContents, 0, file->GetSize ());

		file->Unmap (pointer);

		query.Reset ();
	}
}

class ConfigurePhase
{
public:
	using UpdateProgress = std::function<void (const std::string& action, const int64_t cost)>;

	virtual ~ConfigurePhase () = default;

	void Simulate (int64_t* cost)
	{
		return SimulateImpl (cost);
	}

	void Execute (Log& log, UpdateProgress progress)
	{
		ExecuteImpl (log, progress);
	}

private:
	virtual void SimulateImpl (int64_t* cost) = 0;
	virtual void ExecuteImpl (Log& log, UpdateProgress progress) = 0;
};

///////////////////////////////////////////////////////////////////////////////
class RemoveChangedFilesPhase : public ConfigurePhase
{
public:
	RemoveChangedFilesPhase (Sql::Database& database, Path& path)
		: db_ (database)
		, path_ (path)
	{
	}

private:
	virtual void SimulateImpl (int64_t* cost) override
	{
		auto changedFilesTable = CreateChangedFileTable ();

		auto countChangedFilesQuery = db_.Prepare ("SELECT COUNT(*) from pending_changed_files");
		countChangedFilesQuery.Step ();

		if (cost) {
			*cost = countChangedFilesQuery.GetInt64 (0);
		}

		auto deleteChangedFilesQuery = db_.Prepare (
			"DELETE FROM fs_files WHERE Path=(SELECT Path FROM pending_changed_files)");
		deleteChangedFilesQuery.Step ();

		db_.Execute (
			R"_(DELETE FROM fs_contents
			WHERE Id IN
			(SELECT Id FROM fs_contents_with_reference_count WHERE ReferenceCount = 0))_");
	}

	virtual void ExecuteImpl (Log& log, UpdateProgress progress) override
	{
		auto changedFilesTable = CreateChangedFileTable ();

		// First, get rid of all files that are changing
		// That is, if a file is referencing a different content_object,
		// we have to remove it (those files will get replaced)
		auto changedFilesQuery = db_.Prepare ("SELECT Path FROM pending_changed_files;");

		// files
		{
			auto deleteFileQuery = db_.Prepare (
				"DELETE FROM fs_files WHERE Path=?");

			while (changedFilesQuery.Step ()) {
				deleteFileQuery.BindArguments (changedFilesQuery.GetText (0));
				deleteFileQuery.Step ();
				deleteFileQuery.Reset ();

				boost::filesystem::remove (path_ / Path{ changedFilesQuery.GetText (0) });

				const auto actionDescription = str (boost::format ("Deleted file '%1%'") 
					% changedFilesQuery.GetText (0));

				progress (actionDescription, 1);
				log.Debug ("Configure", actionDescription);
			}

			log.Debug ("Configure", "Deleted changed files from repository");
		}

		// content objects
		db_.Execute ("DELETE FROM fs_contents "
			"WHERE Id IN "
			"(SELECT Id FROM fs_contents_with_reference_count WHERE ReferenceCount = 0)");
	}

	Sql::TemporaryTable CreateChangedFileTable ()
	{
		auto table = db_.CreateTemporaryTable ("pending_changed_files", "Path VARCHAR NOT NULL UNIQUE");

		auto insertPendingFeatureQuery = db_.Prepare (
			R"_(INSERT INTO pending_changed_files (Path)
			SELECT Path FROM (
				SELECT 
					main.fs_files.Path AS Path, 
					main.fs_contents.Hash AS CurrentHash, 
					source.fs_contents.Hash AS NewHash 
				FROM main.fs_files
				INNER JOIN main.fs_contents ON main.fs_files.ContentId = main.fs_contents.Id
				INNER JOIN source.fs_files ON source.fs_files.Path = main.fs_files.Path
				INNER JOIN source.fs_contents ON source.fs_files.ContentId = source.fs_contents.Id
				WHERE CurrentHash IS NOT NewHash
				AND source.fs_files.FeatureId IN
				(SELECT Id FROM source.features
				WHERE Uuid IN (SELECT Uuid FROM pending_features))
				) AS t)_");

		insertPendingFeatureQuery.Step ();
		return table;
	}

	Sql::Database& db_;
	Path path_;
};

class GetContentPhase : public ConfigurePhase
{
public:
	GetContentPhase (Sql::Database& database, Path& path, Repository& sourceRepository)
		: db_ (database)
		, path_ (path)
		, source_ (sourceRepository)
	{
	}

private:
	virtual void SimulateImpl (int64_t* cost) override
	{
		int64_t totalCost = 0;
		
		auto requestedContentObjectTable = CreateRequestedContentObjectTable ();

		auto insertFileQuery = PrepareInsertFileQuery ();

		auto insertContentObjectQuery = db_.Prepare (
			"INSERT INTO fs_contents (Hash, Size) "
			"VALUES (?, ?);");

		auto getTargetFilesQuery = db_.Prepare (
			"SELECT Path FROM source.fs_files "
			"WHERE source.fs_files.ContentId = (SELECT Id FROM source.fs_contents WHERE source.fs_contents.Hash = ?)");

		auto selectRequestedContentObjectsQuery = db_.Prepare (
			R"_(SELECT requested_content_objects.Hash AS Hash,
			       	   source.fs_contents.Size AS Size
				FROM requested_content_objects
				INNER JOIN source.fs_contents ON
					source.fs_contents.Hash = requested_content_objects.Hash;
			)_"
		);

		int64_t totalSize = 0;
		while (selectRequestedContentObjectsQuery.Step ()) {
			SHA256Digest hash;
			selectRequestedContentObjectsQuery.GetBlob (0, hash);
			int64_t objectSize = selectRequestedContentObjectsQuery.GetInt64 (1);

			int64 contentId = -1;
			{
				insertContentObjectQuery.BindArguments (hash, objectSize);
				insertContentObjectQuery.Step ();
				insertContentObjectQuery.Reset ();

				contentId = db_.GetLastRowId ();
			}

			getTargetFilesQuery.BindArguments (hash);

			auto insertFile = [&](const Path& targetPath, const int64 ContentId) -> void {
				insertFileQuery.BindArguments (targetPath.string (), ContentId);
				insertFileQuery.Step ();
				insertFileQuery.Reset ();
			};
			
			while (getTargetFilesQuery.Step ()) {
				const Path targetPath{ getTargetFilesQuery.GetText (0) };
					
				insertFile (targetPath, contentId);
				totalSize += objectSize;
			}

			getTargetFilesQuery.Reset ();
		}

		if (cost) {
			*cost = totalSize;
		}
	}

	virtual void ExecuteImpl (Log& log, UpdateProgress progress) override
	{
		// Find all missing content objects in this database
		std::vector<SHA256Digest> requiredContentObjects;

		auto requestedContentObjectTable = CreateRequestedContentObjectTable ();

		{
			auto requestedContentObjectQuery = db_.Prepare (
				"SELECT Hash FROM requested_content_objects");

			while (requestedContentObjectQuery.Step ()) {
				SHA256Digest contentObjectHash;
				requestedContentObjectQuery.GetBlob (0, contentObjectHash);

				requiredContentObjects.push_back (contentObjectHash);

				log.Debug ("Configure", boost::format ("Discovered content '%1%'") % ToString (contentObjectHash));
			}

			auto totalRequiredContentQuery = db_.Prepare ("SELECT SUM(Size) FROM source.fs_contents "
				"INNER JOIN requested_content_objects ON "
				"source.fs_contents.Hash = requested_content_objects.Hash");

			totalRequiredContentQuery.Step ();
		}

		// Create directories
		{
			auto filePathTable = db_.CreateTemporaryTable ("requested_file_paths", "Path VARCHAR");
			auto insertPathQuery = db_.Prepare ("INSERT INTO requested_file_paths VALUES (?)");

			auto getTargetFilesQuery = db_.Prepare (
				R"_(SELECT Path FROM source.fs_files
				INNER JOIN source.fs_contents ON
				source.fs_files.ContentID = source.fs_contents.Id
				INNER JOIN requested_content_objects ON
				source.fs_contents.Hash = requested_content_objects.Hash)_"
			);

			while (getTargetFilesQuery.Step ()) {
				const Path targetPath{ getTargetFilesQuery.GetText (0) };

				insertPathQuery.BindArguments (targetPath.parent_path ().string ().c_str ());
				insertPathQuery.Step ();
				insertPathQuery.Reset ();
			}

			getTargetFilesQuery.Reset ();

			auto uniqueFilePathQuery = db_.Prepare ("SELECT DISTINCT Path "
				"FROM requested_file_paths ORDER BY Path");
			while (uniqueFilePathQuery.Step ()) {
				boost::filesystem::create_directories (path_ /
					Path{ uniqueFilePathQuery.GetText (0) });
			}
		}

		static const int64 TransactionDataSize = 4 << 20;

		auto transaction = db_.BeginTransaction ();
		int64 currentTransactionDeployedSize = 0;
		int64 currentTransactionSize = 0;

		auto insertFileQuery = PrepareInsertFileQuery ();

		auto insertContentObjectQuery = db_.Prepare (
			"INSERT INTO fs_contents (Hash, Size) "
			"VALUES (?, ?);");

		auto getTargetFilesQuery = db_.Prepare (
			"SELECT Path FROM source.fs_files "
			"WHERE source.fs_files.ContentId = (SELECT Id FROM source.fs_contents WHERE source.fs_contents.Hash = ?)");

		std::unique_ptr<File> stagingFile;
#ifndef NDEBUG
		SHA256Digest stagingFileHash;
#endif

		// Fetch the missing ones now and store in the right places
		source_.GetContentObjects (requiredContentObjects, [&] (const SHA256Digest& hash,
			const ArrayRef<>& contents,
			const int64 offset,
			const int64 totalSize) -> void {
			const auto hashString = ToString (hash);

			const auto stagingFilePath = path_ / (hashString + ".kytmp");
			if ((offset != 0) || (contents.GetSize () != totalSize)) {
				if (offset == 0) {
					log.Debug ("Configure",
						boost::format ("Creating staging file %1%")
						% stagingFilePath);

					stagingFile = CreateFile (stagingFilePath);
					stagingFile->SetSize (totalSize);

#ifndef NDEBUG
					stagingFileHash = hash;
#endif
				} else {
					log.Debug ("Configure",
						boost::format ("Writing into staging file %1%")
						% stagingFilePath);

#ifndef NDEBUG
					// If we're staging a file, we have only one file handle
					// open. So we need this to remain the same until we're done
					assert (stagingFile);
					assert (hash == stagingFileHash);
#endif
				}

				stagingFile->Seek (offset);
				stagingFile->Write (contents);

				if ((offset + contents.GetSize ()) != totalSize) {
					return;
				}
			}

			log.Debug ("Configure", boost::format ("Received content object '%1%'") % hashString);

			int64 contentId = -1;
			{
				insertContentObjectQuery.BindArguments (hash, totalSize);
				insertContentObjectQuery.Step ();
				insertContentObjectQuery.Reset ();

				contentId = db_.GetLastRowId ();

				log.Debug ("Configure",
					boost::format ("Persisted content object '%1%', id %2%") % hashString % contentId);
			}

			getTargetFilesQuery.BindArguments (hash);

			auto insertFile = [&](const Path& targetPath, const int64 ContentId) -> void {
				insertFileQuery.BindArguments (targetPath.string (), ContentId);
				insertFileQuery.Step ();
				insertFileQuery.Reset ();
			};

			/**
			In the first case, we have the contents in a staging file. We need to rename
			the staging file, then copy it to all other files with the same contents

			In the second case, we have the contents in memory. Just write them to all
			destination files.
			*/
			if (stagingFile) {
				stagingFile.reset ();

				bool isFirstFile = true;
				Path lastFilePath;

				while (getTargetFilesQuery.Step ()) {
					const Path targetPath{ getTargetFilesQuery.GetText (0) };

					progress (getTargetFilesQuery.GetText (0), contents.GetSize ());
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
							% lastFilePath % targetPath);

						assert (!lastFilePath.empty ());
						boost::filesystem::copy_file (lastFilePath,
							path_ / targetPath);
					}

					insertFile (targetPath, contentId);

					lastFilePath = path_ / targetPath;
				}
			} else {
				while (getTargetFilesQuery.Step ()) {
					const Path targetPath{ getTargetFilesQuery.GetText (0) };

					progress (getTargetFilesQuery.GetText (0), contents.GetSize ());

					log.Debug ("Configure",
						boost::format ("Creating file %1%") % targetPath);

					auto file = CreateFile (path_ / targetPath, FileAccess::Write);
					file->Write (contents);

					insertFile (targetPath, contentId);

					log.Debug ("Configure", boost::format ("Wrote file %1%") % targetPath);
				}
			}

			getTargetFilesQuery.Reset ();

			currentTransactionDeployedSize += contents.GetSize ();
			currentTransactionSize++;

			if (currentTransactionDeployedSize > TransactionDataSize) {
				log.Debug ("Configure", 
					boost::format ("Committing transaction with %1% operations") % currentTransactionSize);
				transaction.Commit ();
				transaction = db_.BeginTransaction ();
				currentTransactionDeployedSize = 0;
				currentTransactionSize = 0;
			}
		});

		log.Debug ("Configure", 
			boost::format ("Committing transaction with %1% operations") % currentTransactionSize);
		transaction.Commit ();
	}

	Sql::TemporaryTable CreateRequestedContentObjectTable ()
	{
		auto requestedContentObjectTable = db_.CreateTemporaryTable (
			"requested_content_objects",
			"Hash BLOB UNIQUE NOT NULL");

		{
			auto requestedContentObjectQuery = db_.Prepare (
				R"_(INSERT INTO requested_content_objects
					SELECT DISTINCT Hash 
						FROM source.fs_contents
					INNER JOIN source.fs_files ON source.fs_contents.Id = source.fs_files.ContentId
					WHERE source.fs_files.FeatureId IN
					(
						SELECT Id FROM source.features
						WHERE Uuid IN (SELECT Uuid FROM pending_features)
					)
					AND NOT Hash IN 
					(
						SELECT Hash FROM main.fs_contents
					)
				)_");

			requestedContentObjectQuery.Step ();
			requestedContentObjectQuery.Reset ();
		}

		return requestedContentObjectTable;
	}

	Sql::Statement PrepareInsertFileQuery ()
	{
		return db_.Prepare (
			R"_(INSERT INTO main.fs_files (Path, ContentId, FeatureId)
			SELECT ?1, ?2, main.features.Id FROM source.fs_files
			INNER JOIN source.features ON source.features.Id = source.fs_files.FeatureId
			INNER JOIN features ON source.features.Uuid = main.features.Uuid
			WHERE source.fs_files.path = ?1)_"
		);
	}

	Sql::Database& db_;
	Path path_;
	Repository& source_;
};

class CopyExistingFilesPhase : public ConfigurePhase
{
public:
	CopyExistingFilesPhase (Sql::Database& database, Path& path, Repository& sourceRepository)
		: db_ (database)
		, path_ (path)
		, source_ (sourceRepository)
	{
	}

private:
	virtual void SimulateImpl (int64_t* cost) override
	{
		auto diffQuery = db_.Prepare (
			R"_(
				SELECT Path, Hash, Size 
				FROM source.fs_contents
				INNER JOIN source.fs_files ON
					source.fs_contents.Id = source.fs_files.ContentId
				WHERE source.fs_files.FeatureId IN
				(
					SELECT Id FROM source.features
					WHERE Uuid IN 
					(
						SELECT Uuid FROM pending_features
					)
				)
				AND NOT Path IN 
				(
					SELECT Path FROM main.fs_files
				)
			)_"
		);

		auto exemplarQuery = PrepareExemplarQuery ();

		auto insertFileQuery = PrepareInsertFileQuery ();

		int64_t totalCost = 0;

		while (diffQuery.Step ()) {
			SHA256Digest hash;
			diffQuery.GetBlob (1, hash);

			totalCost += diffQuery.GetInt64 (2);

			const Path path{ diffQuery.GetText (0) };

			exemplarQuery.BindArguments (hash);
			exemplarQuery.Step ();

			const Path exemplarPath{ exemplarQuery.GetText (0) };

			insertFileQuery.BindArguments (path.string (),
				exemplarQuery.GetInt64 (1));

			exemplarQuery.Reset ();
		}
	}

	virtual void ExecuteImpl (Log& log, UpdateProgress progress) override
	{
		// We may have files that only require local copies - find those
		// and execute them now
		// That is, we search for all files, that are in a pending_file_set,
		// but not present in our local copy yet (those haven't been added in
		// the loop above because we specifically excluded objects for which
		// we already have the content.)
		auto transaction = db_.BeginTransaction ();

		auto diffQuery = db_.Prepare (
			R"_(SELECT Path, Hash 
				FROM source.fs_contents
				INNER JOIN source.fs_files 
					ON source.fs_contents.Id = source.fs_files.ContentId
				WHERE source.fs_files.FeatureId IN
				(
					SELECT Id FROM source.features
					WHERE Uuid IN 
					(
						SELECT Uuid FROM pending_features
					)
				)
				AND NOT Path IN 
				(
					SELECT Path FROM main.fs_files
				)
		)_"
		);

		auto exemplarQuery = PrepareExemplarQuery ();

		auto insertFileQuery = PrepareInsertFileQuery ();

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
				exemplarQuery.GetInt64 (1));

			exemplarQuery.Reset ();

			log.Debug ("Configure", 
				boost::format ("Copied file '%1%' to '%2%'") % exemplarPath.string () % path.string ());
		}

		transaction.Commit ();
	}

	Sql::Statement PrepareExemplarQuery ()
	{
		return db_.Prepare (
			R"_(SELECT Path, Id FROM fs_files
			INNER JOIN fs_contents ON fs_files.ContentId = fs_contents.Id
			WHERE Hash=?)_");
	}

	Sql::Statement PrepareInsertFileQuery ()
	{
		return db_.Prepare (
			R"_(INSERT INTO main.fs_files (Path, ContentId, FeatureId)
			SELECT ?1, ?2, main.features.Id FROM source.fs_files
			INNER JOIN source.features ON source.features.Id = source.fs_files.FeatureId
			INNER JOIN features ON source.features.Uuid = main.features.Uuid
			WHERE source.fs_files.path = ?1)_"
		);
	}

	Sql::Database& db_;
	Path path_;
	Repository& source_;
};

class CleanupPhase : public ConfigurePhase
{
public:
	CleanupPhase (Sql::Database& database, Path& path)
		: db_ (database)
		, path_ (path)
	{
	}

private:
	virtual void SimulateImpl (int64_t* cost) override
	{			
		// files
		{
			auto unusedFileCountQuery = db_.Prepare (
				R"_(SELECT COUNT(*) FROM fs_files WHERE FeatureId NOT IN (
				    SELECT Id FROM features WHERE features.Uuid IN 
				        (SELECT Uuid FROM pending_features)
				    ))_"
			);

			unusedFileCountQuery.Step ();
			if (cost) {
				*cost = unusedFileCountQuery.GetInt64 (0);
			}

			auto deleteFileQuery = db_.Prepare (
				R"_(DELETE FROM fs_files WHERE Path IN (
				SELECT Path FROM fs_files WHERE FeatureId NOT IN (
				    SELECT Id FROM features WHERE features.Uuid IN 
				        (SELECT Uuid FROM pending_features)
				    )
				))_");

			deleteFileQuery.Step ();
			deleteFileQuery.Reset ();
		}

		DeleteFeaturesContents ();
	}

	virtual void ExecuteImpl (Log& log, UpdateProgress progress) override
	{
		// The order here is files, features, fs_contents, to keep
		// referential integrity at all times

		// files
		{
			auto unusedFilesQuery = db_.Prepare (
				R"_(SELECT Path FROM fs_files WHERE FeatureId NOT IN (
				    SELECT Id FROM features WHERE features.Uuid IN
				        (SELECT Uuid FROM pending_features)
				))_");

			auto deleteFileQuery = db_.Prepare (
				"DELETE FROM fs_files WHERE Path=?");

			while (unusedFilesQuery.Step ()) {
				deleteFileQuery.BindArguments (unusedFilesQuery.GetText (0));
				deleteFileQuery.Step ();
				deleteFileQuery.Reset ();

				boost::filesystem::remove (path_ / Path{ unusedFilesQuery.GetText (0) });

				log.Debug ("Configure", boost::format ("Deleted file '%1%'") % unusedFilesQuery.GetText (0));

				progress (unusedFilesQuery.GetText (0), 1);
			}

			log.Debug ("Configure", "Deleted unused files from repository");
		}

		DeleteFeaturesContents ();

		log.Debug ("Configure", "Deleted unused features and content objects from repository");
	}

	void DeleteFeaturesContents ()
	{
		// features
		db_.Execute ("DELETE FROM features "
			"WHERE features.Uuid NOT IN (SELECT Uuid FROM pending_features)");

		// content objects
		db_.Execute ("DELETE FROM fs_contents "
			"WHERE Id IN "
			"(SELECT Id FROM fs_contents_with_reference_count WHERE ReferenceCount = 0)");
	}

	Sql::Database& db_;
	Path path_;
};

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
		R"_(DELETE FROM fs_contents 
			WHERE Id IN (
				SELECT Id 
				FROM fs_contents_with_reference_count 
				WHERE ReferenceCount=0
			);
		)_");

	// This copies everything over, so we can do joins on source and target
	// now. Assumes the source contains all file sets, content objects and
	// files we're about to configure
	db_.AttachTemporaryCopy ("source", source.GetDatabase ());

	ProgressHelper progressHelper (context.progress);
	progressHelper.Start (1);
	progressHelper.AdvanceStage ("Install");

	// Store the file sets we're going to install in a temporary table for
	// joins, etc.
	auto pendingFeaturesTable = db_.CreateTemporaryTable ("pending_features",
		"Uuid BLOB NOT NULL UNIQUE");

	std::array<std::unique_ptr<ConfigurePhase>, 4> configurePhases = {
		std::make_unique<RemoveChangedFilesPhase> (db_, path_),
		std::make_unique<GetContentPhase> (db_, path_, source),
		std::make_unique<CopyExistingFilesPhase> (db_, path_, source),
		std::make_unique<CleanupPhase> (db_, path_)
	};

	PreparePendingFeatures (context.log, features);
	UpdateFeatures ();
	UpdateFeatureIdsForUnchangedFiles ();

	auto simulationTransaction = db_.BeginTransaction ();

	int64_t totalCost = 0;
	for (auto& phase : configurePhases) {
		int64_t cost = 0;
		phase->Simulate (&cost);

		totalCost += cost;
	}

	simulationTransaction.Rollback ();

	progressHelper.SetStageTarget (totalCost);
	for (auto& phase : configurePhases) {
		phase->Execute (context.log, [&](const std::string& s, const int64_t cost) -> void {
			progressHelper.SetAction (s);
			progressHelper.Advance (cost);
		});
	}

	progressHelper.SetStageFinished ();

	db_.Detach ("source");

	db_.Execute ("PRAGMA journal_mode = DELETE");
	db_.Execute ("PRAGMA optimize;");
	db_.Execute ("VACUUM;");
}

///////////////////////////////////////////////////////////////////////////////
/**
Create a new temporary table pending_features which contains the UUIDs
of the file sets we're about to add
*/
void DeployedRepository::PreparePendingFeatures (Log& log, const ArrayRef<Uuid>& features)
{
	auto transaction = db_.BeginTransaction ();
	auto insertPendingFeatureQuery = db_.Prepare (
		"INSERT INTO pending_features (Uuid) VALUES (?);");

	log.Debug ("Configure", "Selecting features for configure");

	for (const auto& feature : features) {
		insertPendingFeatureQuery.BindArguments (feature);
		insertPendingFeatureQuery.Step ();
		insertPendingFeatureQuery.Reset ();

		log.Debug ("Configure", boost::format ("Selected feature: '%1%'") % ToString (feature));
	}

	transaction.Commit ();
}

/**
Insert the new features we're about to configure from pending_features
*/
void DeployedRepository::UpdateFeatures ()
{
	// Insert those we don't have yet into our features, but which are
	// pending
	db_.Execute (
		R"_(INSERT INTO features (Uuid)
		SELECT Uuid FROM source.features
		WHERE source.features.Uuid IN (SELECT Uuid FROM pending_features)
		AND NOT source.features.Uuid IN (SELECT Uuid FROM features))_");
}

/**
Update the file set ids of all files which remain unchanged, but have moved
to a new features.
*/
void DeployedRepository::UpdateFeatureIdsForUnchangedFiles ()
{
	auto tempTable = db_.CreateTemporaryTable ("temp_file_path_to_new_id", 
		"Path VARCHAR NOT NULL UNIQUE, Id INTEGER NOT NULL");

	db_.Execute (
		R"_(INSERT INTO temp_file_path_to_new_id (Path, Id)
			-- We'll index using Path, FeatureId below
			-- main.features.Id has been updated already
			SELECT main.fs_files.Path AS Path, main.features.Id AS Id
			FROM main.fs_files
			-- Current files <-> contents are linked through the ContentId
			INNER JOIN main.fs_contents ON main.fs_files.ContentId = main.fs_contents.Id
			-- Current files <-> source files are linked through the unique Path
			INNER JOIN source.fs_files ON source.fs_files.Path = main.fs_files.Path
			-- We only want the files which didn't change content, that is, with the
			-- same hash
			INNER JOIN source.fs_contents ON main.fs_contents.Hash = source.fs_contents.Hash
			-- We need to link the features on the source side to their UUID
			INNER JOIN source.features ON source.features.Id = source.fs_files.FeatureId
			-- We need to link our (new) feeature Id to the source feature Id through the UUID
			INNER JOIN main.features ON main.features.Uuid = source.features.Uuid;)_"
	);

	// For files which have the same location and hash as before, update
	// the feature id
	// This long query will find every file where the hash and the path
	// remained the same, and update it to use the new file set id we just
	// inserted above
	db_.Execute (
		R"_(
		UPDATE fs_files 
		SET FeatureId = (
			SELECT Id 
			FROM temp_file_path_to_new_id 
			WHERE Path=temp_file_path_to_new_id.Path)
		WHERE Path = (
			SELECT Path 
			FROM temp_file_path_to_new_id);
		)_");
}

///////////////////////////////////////////////////////////////////////////////
std::unique_ptr<DeployedRepository> DeployedRepository::CreateFrom (Repository& source,
	const ArrayRef<Uuid>& features,
	const Path& targetDirectory,
	Repository::ExecutionContext& context)
{
	boost::filesystem::create_directories (targetDirectory);

	auto db = Sql::Database::Create ((targetDirectory / "k.db").string ().c_str ());

	db.Execute (install_db_structure);

	db.Close ();

	std::unique_ptr<DeployedRepository> result (new DeployedRepository{ 
		targetDirectory.string ().c_str (), Sql::OpenMode::ReadWrite });

	result->Configure (source, features, context);

	return std::move (result);
}
} // namespace kyla
