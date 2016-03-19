#ifndef KYLA_CORA_INTERNAL_REPOSITORY_H
#define KYLA_CORE_INTERNAL_REPOSITORY_H

#include <functional>
#include <memory>

#include "Kyla.h"

namespace kyla {
struct IRepository
{
	virtual ~IRepository () = default;

	using ValidationCallback = std::function<void (const char* file, const kylaValidationResult validationResult)>;

	void Validate (const ValidationCallback& validationCallback);

private:
	virtual void ValidateImpl (const ValidationCallback& validationCallback) = 0;
};

std::unique_ptr<IRepository> OpenRepository (const char* path);

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

private:
	void ValidateImpl (const ValidationCallback& validationCallback) override;

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
