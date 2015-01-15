#include "SourcePackage.h"

#include <unordered_map>
#include <boost/filesystem/fstream.hpp>

struct SourcePackageWriter::Impl
{
public:
	std::unordered_map<Hash, std::vector<boost::filesystem::path>, HashHash, HashEqual> hashChunkMap;
	boost::filesystem::path filename;
};

////////////////////////////////////////////////////////////////////////////////
SourcePackageWriter::SourcePackageWriter ()
: impl_ (new Impl)
{

}

////////////////////////////////////////////////////////////////////////////////
SourcePackageWriter::~SourcePackageWriter ()
{
}

////////////////////////////////////////////////////////////////////////////////
void SourcePackageWriter::Open (const boost::filesystem::path& filename)
{
	impl_->filename = filename;
}

////////////////////////////////////////////////////////////////////////////////
void SourcePackageWriter::Add (const Hash& hash, const boost::filesystem::path& chunkPath)
{
	impl_->hashChunkMap [hash].push_back (chunkPath);
}

////////////////////////////////////////////////////////////////////////////////
namespace {
std::int64_t BlockCopy (const boost::filesystem::path& file, std::ostream& out)
{
	boost::filesystem::ifstream input (file, std::ios::binary);

	static const int blockSize = 4 << 20;
	std::int64_t bytesReadTotal = 0;
	std::vector<char> block (blockSize);

	for (;;) {
		input.read (block.data (), blockSize);
		const auto bytesRead = input.gcount();
		bytesReadTotal += bytesRead;
		out.write (block.data (), bytesRead);

		if (bytesRead < blockSize) {
			return bytesReadTotal;
		}
	}
}
}

////////////////////////////////////////////////////////////////////////////////
void SourcePackageWriter::Finalize ()
{
	boost::filesystem::ofstream output (impl_->filename, std::ios::binary);

	// Write header
	PackageHeader header;
	::memset (&header, 0, sizeof (header));
	::memcpy (header.id, "NIMSRCPK", 8);
	header.version = 1;
	header.indexEntries = static_cast<std::int32_t> (impl_->hashChunkMap.size ());
	header.indexOffset = sizeof (header);

	output.write (reinterpret_cast<const char*> (&header), sizeof (header));

	// Reserve space for index
	std::int64_t offset = sizeof (header) + impl_->hashChunkMap.size () * sizeof (PackageIndex);
	output.seekp (offset);

	std::vector<PackageIndex> packageIndex;
	packageIndex.reserve (impl_->hashChunkMap.size ());

	for (const auto& hashChunks : impl_->hashChunkMap) {
		for (const auto& chunk : hashChunks.second) {
			PackageIndex indexEntry;
			::memcpy (indexEntry.hash, hashChunks.first.hash, sizeof (hashChunks.first.hash));
			indexEntry.offset = offset;

			offset += BlockCopy (chunk, output);
			packageIndex.push_back (indexEntry);
		}
	}

	output.seekp (sizeof (header));
	output.write (reinterpret_cast<const char*> (packageIndex.data ()),
		sizeof (PackageIndex) * packageIndex.size ());
}

////////////////////////////////////////////////////////////////////////////////
bool SourcePackageWriter::IsOpen () const
{
	return ! impl_->filename.empty ();
}
