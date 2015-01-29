#ifndef KYLA_CORE_INTERNAL_SOURCEPACKAGEREADER_H
#define KYLA_CORE_INTERNAL_SOURCEPACKAGEREADER_H

#include <boost/filesystem.hpp>
#include <functional>
#include <memory>

#include "Hash.h"

namespace kyla {
class ISourcePackageReader
{
public:
	ISourcePackageReader () = default;
	virtual ~ISourcePackageReader () = default;
	ISourcePackageReader (const ISourcePackageReader&) = delete;
	ISourcePackageReader& operator= (const ISourcePackageReader&) = delete;

	void Store (const std::function<bool (const Hash&)>& filter,
		const boost::filesystem::path& directory);

private:
	virtual void StoreImpl (const std::function<bool (const Hash&)>& filter,
		const boost::filesystem::path& directory) = 0;
};

class FileSourcePackageReader final : public ISourcePackageReader
{
public:
	FileSourcePackageReader (const boost::filesystem::path& filename);
	~FileSourcePackageReader ();

private:
	void StoreImpl (const std::function<bool (const Hash&)>& filter,
		const boost::filesystem::path& directory) override;

	struct Impl;
	std::unique_ptr<Impl> impl_;
};
}

#endif
