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

#ifndef KYLA_CORE_INTERNAL_REPOSITORY_H
#define KYLA_CORE_INTERNAL_REPOSITORY_H

#include <functional>
#include <memory>

#include "ArrayRef.h"
#include "FileIO.h"
#include "Hash.h"
#include "Uuid.h"

namespace kyla {
namespace Sql {
	class Database;
}

class Log;

class Progress
{
public:
	using ProgressCallback = std::function<void (const int sc, const int cs, const float p, const char* s, const char* a)>;

	Progress (ProgressCallback callback)
		: callback_ (callback)
	{
	}

	void operator () (const int currentStage, const int stageCount, const float p, const char* s, const char* a)
	{
		callback_ (currentStage, stageCount, p, s, a);
	}

private:
	ProgressCallback callback_;
};

class ProgressHelper
{
public:
	ProgressHelper (Progress progressCallback)
		: progressCallback_ (progressCallback)
	{
	}

	void Start (const int stageCount)
	{
		stageCount_ = stageCount;
		currentStage_ = -1;
	}

	void AdvanceStage (const char* stageName)
	{
		++currentStage_;
		stageName_ = stageName;
		currentStageTarget_ = 0;
		current_ = 0;
		progressCallback_ (currentStage_, stageCount_, 0,
			stageName_.c_str (), nullptr);
	}

	void SetStageTarget (const int64 target)
	{
		currentStageTarget_ = target;
	}

	void SetAction (const char* action)
	{
		action_ = action;
	}

	void operator++()
	{
		++current_;

		progressCallback_ (currentStage_, stageCount_, GetInStageProgress (),
			stageName_.c_str (), action_.c_str ());
	}

	void operator++(int)
	{
		++current_;

		progressCallback_ (currentStage_, stageCount_, GetInStageProgress (), 
			stageName_.c_str (), action_.c_str ());
	}

private:
	float GetInStageProgress () const
	{
		assert (current_ <= currentStageTarget_);
		return static_cast<float> (current_) / static_cast<float> (currentStageTarget_);
	}

	Progress progressCallback_;
	int64 current_ = 0;
	int64 currentStageTarget_ = 0;
	int stageCount_ = 1;
	int currentStage_ = 0;
	std::string action_;
	std::string stageName_;
};

struct FilesetInfo
{
	Uuid id;
	int64_t fileCount;
	int64_t fileSize;
};

enum class ValidationResult
{
	Ok,
	Corrupted,
	Missing
};

struct Repository
{
	virtual ~Repository () = default;

	using ValidationCallback = std::function<void (const SHA256Digest& contentObject,
		const char* path,
		const ValidationResult validationResult)>;

	void Validate (const ValidationCallback& validationCallback);

	using GetContentObjectCallback = std::function<void (const SHA256Digest& objectDigest,
		const ArrayRef<>& contents)>;

	void GetContentObjects (const ArrayRef<SHA256Digest>& requestedObjects,
		const GetContentObjectCallback& getCallback);

	void Repair (Repository& source);

	void Configure (Repository& other,
		const ArrayRef<Uuid>& filesets,
		Log& log, Progress& progress);

	std::vector<FilesetInfo> GetFilesetInfos ();
	std::string GetFilesetName (const Uuid& filesetId);

	Sql::Database& GetDatabase ();

private:
	virtual void ValidateImpl (const ValidationCallback& validationCallback) = 0;
	virtual void GetContentObjectsImpl (const ArrayRef<SHA256Digest>& requestedObjects,
		const GetContentObjectCallback& getCallback) = 0;
	virtual void RepairImpl (Repository& source) = 0;
	virtual std::vector<FilesetInfo> GetFilesetInfosImpl () = 0;
	virtual std::string GetFilesetNameImpl (const Uuid& filesetId) = 0;
	virtual void ConfigureImpl (Repository& other,
		const ArrayRef<Uuid>& filesets,
		Log& log, Progress& progress) = 0;
	virtual Sql::Database& GetDatabaseImpl () = 0;
};

std::unique_ptr<Repository> OpenRepository (const char* path,
	const bool allowWriteAccess);

std::unique_ptr<Repository> DeployRepository (Repository& source,
	const char* targetPath,
	const ArrayRef<Uuid>& selectedFilesets,
	Log& log, Progress& progressHelper);
}

#endif
