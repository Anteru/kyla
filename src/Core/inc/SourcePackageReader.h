#ifndef KYLA_CORE_INTERNAL_SOURCEPACKAGEREADER_H
#define KYLA_CORE_INTERNAL_SOURCEPACKAGEREADER_H

#include <boost/filesystem.hpp>
#include <functional>
#include <memory>

#include "Hash.h"

namespace kyla {
class Log;

class SourcePackageReader
{
public:
	SourcePackageReader () = default;
	virtual ~SourcePackageReader () = default;
	SourcePackageReader (const SourcePackageReader&) = delete;
	SourcePackageReader& operator= (const SourcePackageReader&) = delete;

	void Store (const std::function<bool (const Hash&)>& filter,
		const boost::filesystem::path& directory,
		Log& log);

private:
	virtual void StoreImpl (const std::function<bool (const Hash&)>& filter,
		const boost::filesystem::path& directory,
		Log& log) = 0;
};

class FileSourcePackageReader final : public SourcePackageReader
{
public:
	FileSourcePackageReader (const boost::filesystem::path& filename);
	~FileSourcePackageReader ();

private:
	void StoreImpl (const std::function<bool (const Hash&)>& filter,
		const boost::filesystem::path& directory,
		Log& log) override;

	struct Impl;
	std::unique_ptr<Impl> impl_;
};
}

#endif
