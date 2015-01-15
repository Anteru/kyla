#include "SourcePackage.h"

#include <unordered_map>
#include <boost/filesystem/fstream.hpp>
#include <openssl/evp.h>

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
std::int64_t BlockCopy (const boost::filesystem::path& file, std::ostream& out,
	std::vector<char>& buffer)
{
	boost::filesystem::ifstream input (file, std::ios::binary);

	std::int64_t bytesReadTotal = 0;

	for (;;) {
		input.read (buffer.data (), buffer.size ());
		const auto bytesRead = input.gcount();
		bytesReadTotal += bytesRead;
		out.write (buffer.data (), bytesRead);

		if (bytesRead < buffer.size ()) {
			return bytesReadTotal;
		}
	}
}
}

////////////////////////////////////////////////////////////////////////////////
Hash SourcePackageWriter::Finalize()
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

	// This is our I/O buffer, we read/write in blocks of this size, and also
	// compute the final hash in this buffer
	std::vector<char> buffer (4 << 20);

	std::vector<PackageIndex> packageIndex;
	packageIndex.reserve (impl_->hashChunkMap.size ());

	for (const auto& hashChunks : impl_->hashChunkMap) {
		for (const auto& chunk : hashChunks.second) {
			PackageIndex indexEntry;
			::memcpy (indexEntry.hash, hashChunks.first.hash, sizeof (hashChunks.first.hash));
			indexEntry.offset = offset;

			offset += BlockCopy (chunk, output, buffer);
			packageIndex.push_back (indexEntry);
		}
	}

	output.seekp (sizeof (header));
	output.write (reinterpret_cast<const char*> (packageIndex.data ()),
		sizeof (PackageIndex) * packageIndex.size ());

	output.close ();

	// We have to read again, as we can't make a fully streaming update due to
	// our delayed index write. Hopefully, the file is still in the disk cache,
	// so this should be quick
	boost::filesystem::ifstream input (impl_->filename, std::ios::binary);

	EVP_MD_CTX* fileCtx = EVP_MD_CTX_create ();
	EVP_DigestInit_ex (fileCtx, EVP_sha512 (), nullptr);

	for (;;) {
		input.read (buffer.data (), buffer.size ());
		const auto bytesRead = input.gcount();

		EVP_DigestUpdate (fileCtx, buffer.data (), bytesRead);

		if (bytesRead < buffer.size ()) {
			break;
		}
	}

	Hash result;
	EVP_DigestFinal_ex (fileCtx, result.hash, nullptr);
	return result;
}

////////////////////////////////////////////////////////////////////////////////
bool SourcePackageWriter::IsOpen () const
{
	return ! impl_->filename.empty ();
}
