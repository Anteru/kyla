#ifndef KYLA_CORE_INTERNAL_INSTALL_PACKAGE_WRITER_H
#define KYLA_CORE_INTERNAL_INSTALL_PACKAGE_WRITER_H

#include <memory>
#include <boost/filesystem.hpp>

#include "Compression.h"
#include "Hash.h"

namespace kyla {
/*
The install package writer writes the package during Finalize ().
*/
class InstallPackageWriter final
{
public:
	InstallPackageWriter ();
	~InstallPackageWriter ();

	InstallPackageWriter (const InstallPackageWriter&) = delete;
	InstallPackageWriter& operator=(const InstallPackageWriter&) = delete;

	void Open (const boost::filesystem::path& outputFile);

	void Add (const char *name, const boost::filesystem::path &fileStream);
	void Add (const char* name, const boost::filesystem::path& fileStream,
		CompressionMode compressionMode);
	void Finalize ();

	bool IsOpen () const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};
}

#endif
