#ifndef KYLA_CORE_INTERNAL_SOURCEPACKAGEWRITER_H
#define KYLA_CORE_INTERNAL_SOURCEPACKAGEWRITER_H

#include <memory>
#include <boost/filesystem.hpp>

#include "Hash.h"

namespace kyla {
/*
The source package writer writes the package during Finalize ().
*/
class SourcePackageWriter final
{
public:
	SourcePackageWriter ();
	~SourcePackageWriter ();

	SourcePackageWriter (const SourcePackageWriter&) = delete;
	SourcePackageWriter& operator=(const SourcePackageWriter&) = delete;

	void Open (const boost::filesystem::path& outputFile,
		const void* uuid);
	void Add (const SHA512Digest& digest, const boost::filesystem::path& chunkPath);
	SHA512Digest Finalize ();

	bool IsOpen () const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};
}

#endif
