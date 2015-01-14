#include "SourcePackage.h"

void SourcePackageWriter::Open (const boost::filesystem::path& filename)
{

}

void SourcePackageWriter::Add (const Hash& hash, const boost::filesystem::path& chunkPath)
{
	// Register
}

void SourcePackageWriter::Finalize ()
{
	// Write header
	// Write index

	// Insert chunks, one-by-one
}
