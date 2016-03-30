#ifndef KYLA_CORA_INTERNAL_REPOSITORY_H
#define KYLA_CORE_INTERNAL_REPOSITORY_H

#include <functional>
#include <memory>

#include "Kyla.h"

#include "ArrayRef.h"
#include "FileIO.h"
#include "Hash.h"
#include "Uuid.h"

namespace kyla {
namespace Sql {
	class Database;
}
struct FilesetInfo
{
	Uuid id;
	int64_t fileCount;
	int64_t fileSize;
};

struct IRepository
{
	virtual ~IRepository () = default;

	using ValidationCallback = std::function<void (const SHA256Digest& contentObject,
		const char* path,
		const kylaValidationResult validationResult)>;

	void Validate (const ValidationCallback& validationCallback);

	using GetContentObjectCallback = std::function<void (const SHA256Digest& objectDigest,
		const ArrayRef<>& contents)>;

	void GetContentObjects (const ArrayRef<SHA256Digest>& requestedObjects,
		const GetContentObjectCallback& getCallback);

	void Repair (IRepository& source);

	std::vector<FilesetInfo> GetFilesetInfos ();

	std::string GetFilesetName (const Uuid& filesetId);

	Sql::Database& GetDatabase ();

private:
	virtual void ValidateImpl (const ValidationCallback& validationCallback) = 0;
	virtual void GetContentObjectsImpl (const ArrayRef<SHA256Digest>& requestedObjects,
		const GetContentObjectCallback& getCallback) = 0;
	virtual void RepairImpl (IRepository& source) = 0;
	virtual std::vector<FilesetInfo> GetFilesetInfosImpl () = 0;
	virtual Sql::Database& GetDatabaseImpl () = 0;
	virtual std::string GetFilesetNameImpl (const Uuid& filesetId) = 0;
};

std::unique_ptr<IRepository> OpenRepository (const char* path);

std::unique_ptr<IRepository> DeployRepository (IRepository& source,
	const char* targetPath,
	const ArrayRef<Uuid>& selectedFilesets);

/**
Content files stored directly, not deployed
*/
class LooseRepository final : public IRepository
{
public:
	LooseRepository (const char* path);
	~LooseRepository ();

	LooseRepository (LooseRepository&& other);
	LooseRepository& operator= (LooseRepository&& other);

private:
	void ValidateImpl (const ValidationCallback& validationCallback) override;

	void GetContentObjectsImpl (const ArrayRef<SHA256Digest>& requestedObjects,
		const GetContentObjectCallback& getCallback) override;
	void RepairImpl (IRepository& source) override;

	std::vector<FilesetInfo> GetFilesetInfosImpl () override;
	std::string GetFilesetNameImpl (const Uuid& filesetId) override;
	Sql::Database& GetDatabaseImpl () override;

	struct Impl;
	std::unique_ptr<Impl> impl_;
};

/**
Files as if the repository has been deployed
*/
class DeployedRepository final : public IRepository
{
public:
	DeployedRepository (const char* path);
	~DeployedRepository ();

	DeployedRepository (DeployedRepository&& other);
	DeployedRepository& operator= (DeployedRepository&& other);

	static std::unique_ptr<DeployedRepository> CreateFrom (IRepository& other,
		const ArrayRef<Uuid>& filesets,
		const Path& targetDirectory);

private:
	void ValidateImpl (const ValidationCallback& validationCallback) override;
	void RepairImpl (IRepository& source) override;
	void GetContentObjectsImpl (const ArrayRef<SHA256Digest>& requestedObjects,
		const GetContentObjectCallback& getCallback) override;

	std::vector<FilesetInfo> GetFilesetInfosImpl () override;
	std::string GetFilesetNameImpl (const Uuid& filesetId) override;
	Sql::Database& GetDatabaseImpl () override;

	struct Impl;
	std::unique_ptr<Impl> impl_;
};

/**
Everything packed into per-file-set files
*/
class PackedRepository final : public IRepository
{

private:
};

/**
Repository bundled into a single file
*/
class BundledRepository final : public IRepository
{

private:
};
}

#endif
