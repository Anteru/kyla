#ifndef KYLA_CORA_INTERNAL_REPOSITORY_H
#define KYLA_CORE_INTERNAL_REPOSITORY_H

#include <functional>

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

/**
Stores all data stored in a repository
*/
struct IRepositoryDatabase
{
	virtual ~IRepositoryDatabase () = default;


};

/**
Content files stored directly, not deployed
*/
class LooseRepository final : public IRepository
{
private:
};

/**
Files as if the repository has been deployed
*/
class DeployedRepository final : public IRepository
{
public:
	static DeployedRepository Open (const char* location);

private:
	void ValidateImpl (const ValidationCallback& validationCallback) override;
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
