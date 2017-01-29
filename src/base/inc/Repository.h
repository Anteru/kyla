/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
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
	using ProgressCallback = std::function<void (const float p, const char* s, const char* a)>;

	Progress (ProgressCallback callback)
		: callback_ (callback)
	{
	}

	void operator () (const float p, const char* s, const char* a)
	{
		callback_ (p, s, a);
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
		progressCallback_ (GetTotalProgress (),
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

		progressCallback_ (GetTotalProgress (),
			stageName_.c_str (), action_.c_str ());
	}

	void operator++(int)
	{
		++current_;

		progressCallback_ (GetTotalProgress (), 
			stageName_.c_str (), action_.c_str ());
	}

	void SetStageFinished ()
	{
		current_ = currentStageTarget_ = 1;
		progressCallback_ (GetTotalProgress (),
			stageName_.c_str (), action_.c_str ());
	}

private:
	float GetInStageProgress () const
	{
		assert (current_ <= currentStageTarget_);
		return static_cast<float> (current_) / static_cast<float> (currentStageTarget_);
	}

	float GetTotalProgress () const
	{
		const float stageWeight = static_cast<float> (currentStage_) / static_cast<float> (stageCount_);
		return stageWeight * currentStage_ + stageWeight * GetInStageProgress ();
	}

	Progress progressCallback_;
	int64 current_ = 0;
	int64 currentStageTarget_ = 0;
	int stageCount_ = 1;
	int currentStage_ = 0;
	std::string action_;
	std::string stageName_;
};

enum class RepairResult
{
	Ok,
	Corrupted,
	Missing,
	Restored
};

struct FeatureTreeNode
{
	FeatureTreeNode* parent = nullptr;
	
	std::string name;
	std::string description;

	Uuid* featureIds;
	int featureIdCount;
};

class FeatureTree final
{
public:
	std::vector<std::unique_ptr<FeatureTreeNode>> nodes;

	Uuid* SetFeatureIds (const ArrayRef<Uuid>& ids)
	{
		featureIds_.assign (ids.begin (), ids.end ());
		return featureIds_.data ();
	}

private:
	std::vector<Uuid> featureIds_;
};

struct Repository
{
	Repository () = default;
	virtual ~Repository () = default;

	Repository (const Repository&) = delete;
	Repository& operator= (const Repository&) = delete;

	struct ExecutionContext
	{
		Log& log;
		Progress& progress;
	};

	using RepairCallback = std::function<void (
		const char* item,
		const RepairResult repairResult)>;
	
	using GetContentObjectCallback = std::function<void (const SHA256Digest& objectDigest,
		const ArrayRef<>& contents,
		const int64 offset,
		const int64 totalSize)>;

	void GetContentObjects (const ArrayRef<SHA256Digest>& requestedObjects,
		const GetContentObjectCallback& getCallback);

	void Repair (Repository& source,
		ExecutionContext& context,
		RepairCallback repairCallback,
		bool restore);

	void Configure (Repository& other,
		const ArrayRef<Uuid>& features,
		ExecutionContext& context);

	std::vector<Uuid> GetFeatures ();
	int64_t GetFeatureSize (const Uuid& featureId);

	struct Dependency
	{
		Uuid source;
		Uuid target;
	};

	std::vector<Dependency> GetFeatureDependencies (const Uuid& featureId);

	FeatureTree GetFeatureTree ();

	Sql::Database& GetDatabase ();

	bool IsEncrypted ();
	void SetDecryptionKey (const std::string& key);
	std::string GetDecryptionKey () const;

private:
	virtual void GetContentObjectsImpl (const ArrayRef<SHA256Digest>& requestedObjects,
		const GetContentObjectCallback& getCallback) = 0;
	virtual void RepairImpl (Repository& source,
		ExecutionContext& context,
		RepairCallback repairCallback,
		bool restore) = 0;
	virtual std::vector<Uuid> GetFeaturesImpl () = 0;
	virtual int64_t GetFeatureSizeImpl (const Uuid& featureId) = 0;
	virtual void ConfigureImpl (Repository& other,
		const ArrayRef<Uuid>& features,
		ExecutionContext& context) = 0;
	virtual bool IsEncryptedImpl () = 0;
	virtual void SetDecryptionKeyImpl (const std::string& key) = 0;
	virtual std::string GetDecryptionKeyImpl () const = 0;
	virtual Sql::Database& GetDatabaseImpl () = 0;
	virtual std::vector<Dependency> GetFeatureDependenciesImpl (const Uuid& featureId) = 0;
	virtual FeatureTree GetFeatureTreeImpl () = 0;
};

std::unique_ptr<Repository> OpenRepository (const char* path,
	const bool allowWriteAccess);

std::unique_ptr<Repository> DeployRepository (Repository& source,
	const char* targetPath,
	const ArrayRef<Uuid>& selectedFeatures,
	Repository::ExecutionContext& context);
}

#endif
