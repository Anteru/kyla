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
#include <unordered_map>

#include "ArrayRef.h"
#include "FileIO.h"
#include "Hash.h"
#include "Uuid.h"

namespace kyla {
namespace Sql {
	class Database;
}

class Log;

class ProgressHelper
{
public:
	using ProgressCallback = std::function<void (const float p, const char* s, const char* a)>;

	ProgressHelper (ProgressCallback progressCallback, const std::string& what, const int64_t target)
		: progressCallback_ (progressCallback)
		, target_ (target)
		, what_ (what)
	{
		assert (progressCallback_);
	}

	void Advance (const std::string& action, const int64 amount)
	{
		current_ += amount;

		progressCallback_ (GetProgress (), what_.c_str (), action.c_str ());
	}

	void Done ()
	{
		if (GetProgress () < 1.0) {
			current_ = target_ = 1;
			progressCallback_ (1.0f, what_.c_str (), nullptr);
		}
	}

private:
	float GetProgress () const
	{
		assert (current_ <= target_);
		if (target_ != 0) {
			return static_cast<float> (current_) / static_cast<float> (target_);
		} else {
			return 0;
		}
	}

	ProgressCallback progressCallback_;
	int64 current_ = 0;
	int64 target_ = 0;
	std::string what_;
};

enum class RepairResult
{
	Ok,
	Corrupted,
	Missing,
	Restored
};

class Variable final
{
public:
	const char* GetString () const
	{
		return static_cast<const char*> (
			static_cast<const void*> (value_.data ()));
	}

	int GetInt () const
	{
		return Get<int> ();
	}

	void Set (const size_t length, const void* v)
	{
		if (readOnly_) {
			throw std::runtime_error ("Variable is read-only");
		}

		const auto p = static_cast<const byte*> (v);
		value_.assign (p, p + length);
	}

	void Get (size_t* size, void* buffer)
	{
		if (size != nullptr) {
			*size = value_.size ();
		}

		if (buffer != nullptr) {
			::memcpy (buffer, value_.data (), value_.size ());
		}
	}

	bool IsReadOnly () const
	{
		return readOnly_;
	}

private:
	template <typename T>
	T Get () const
	{
		return *static_cast<const T*> (static_cast<const void*> (value_.data ()));
	}

	std::vector<byte> value_;
	bool readOnly_ = false;
};

struct Repository
{
	Repository () = default;
	virtual ~Repository () = default;

	Repository (const Repository&) = delete;
	Repository& operator= (const Repository&) = delete;

	struct ExecutionContext
	{
		using ProgressCallback = std::function<void (const float p, const char* s, const char* a)>;

		ExecutionContext (Log& log) : log (log)
		{
		}

		Log& log;
		ProgressCallback progress = [](const float, const char*, const char*) {};
		std::unordered_map<std::string, Variable> variables;

		static constexpr auto EncryptionKey = "Encryption.Key";
	};

	using RepairCallback = std::function<void (
		const char* item,
		const RepairResult repairResult)>;
	
	using GetContentObjectCallback = std::function<void (const SHA256Digest& objectDigest,
		const ArrayRef<>& contents,
		const int64 offset,
		const int64 totalSize)>;

	void GetContentObjects (const ArrayRef<SHA256Digest>& requestedObjects,
		const GetContentObjectCallback& getCallback,
		ExecutionContext& context);

	void Repair (Repository& source,
		ExecutionContext& context,
		RepairCallback repairCallback,
		bool restore);

	void Configure (Repository& other,
		const ArrayRef<Uuid>& features,
		ExecutionContext& context);

	std::vector<Uuid> GetFeatures ();
	int64_t GetFeatureSize (const Uuid& featureId);
	std::string GetFeatureTitle (const Uuid& featureId);
	std::string GetFeatureDescription (const Uuid& featureId);

	std::vector<Uuid> GetSubfeatures (const Uuid& featureId);

	Sql::Database& GetDatabase ();

	bool IsEncrypted ();

private:
	virtual void GetContentObjectsImpl (const ArrayRef<SHA256Digest>& requestedObjects,
		const GetContentObjectCallback& getCallback,
		ExecutionContext& context) = 0;
	virtual void RepairImpl (Repository& source,
		ExecutionContext& context,
		RepairCallback repairCallback,
		bool restore) = 0;
	virtual std::vector<Uuid> GetFeaturesImpl () = 0;
	virtual int64_t GetFeatureSizeImpl (const Uuid& featureId) = 0;
	virtual std::string GetFeatureTitleImpl (const Uuid& featureId) = 0;
	virtual std::string GetFeatureDescriptionImpl (const Uuid& featureId) = 0;
	virtual void ConfigureImpl (Repository& other,
		const ArrayRef<Uuid>& features,
		ExecutionContext& context) = 0;
	virtual bool IsEncryptedImpl () = 0;
	virtual Sql::Database& GetDatabaseImpl () = 0;
	virtual std::vector<Uuid> GetSubfeaturesImpl (const Uuid& featureId) = 0;
};

std::unique_ptr<Repository> OpenRepository (const char* path,
	const bool allowWriteAccess);

std::unique_ptr<Repository> DeployRepository (Repository& source,
	const char* targetPath,
	const ArrayRef<Uuid>& selectedFeatures,
	Repository::ExecutionContext& context);
}

#endif
