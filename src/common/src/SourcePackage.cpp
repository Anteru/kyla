#include "SourcePackage.h"

#include <unordered_map>
#include <boost/filesystem/fstream.hpp>
#include <openssl/evp.h>

#include <zlib.h>

// For Linux memory mapping
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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

	// Reserve space for index
	std::int64_t offset = sizeof (header);

	// Count all chunks, as we will create one entry per chunk
	for (const auto& hashChunks : impl_->hashChunkMap) {
		header.indexEntries += hashChunks.second.size ();
		offset += sizeof (PackageIndex) * hashChunks.second.size ();
	}

	output.write (reinterpret_cast<const char*> (&header), sizeof (header));
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
	return ComputeHash (impl_->filename, buffer);
}

////////////////////////////////////////////////////////////////////////////////
bool SourcePackageWriter::IsOpen () const
{
	return ! impl_->filename.empty ();
}

////////////////////////////////////////////////////////////////////////////////
void ISourcePackageReader::Store (const std::function<bool (const Hash&)>& filter,
	const boost::filesystem::path& directory)
{
	StoreImpl (filter, directory);
}

////////////////////////////////////////////////////////////////////////////////
struct FileSourcePackageReader::Impl
{
public:
	Impl (const boost::filesystem::path& packageFilename)
	: input_ (packageFilename, std::ios::binary)
	{
	}

	void Store (const std::function<bool (const Hash &)>& filter,
		const boost::filesystem::path& directory)
	{
		PackageHeader header;
		input_.read (reinterpret_cast<char*> (&header), sizeof (header));
		input_.seekg (header.indexOffset);

		std::vector<PackageIndex> index (header.indexEntries);
		input_.read (reinterpret_cast<char*> (index.data ()),
			sizeof (PackageIndex) * index.size ());

		std::vector<char> buffer;
		for (const auto& entry : index) {
			Hash hash;
			static_assert (sizeof (hash.hash) == sizeof (entry.hash), "Hash size mismatch!");
			::memcpy (hash.hash, entry.hash, sizeof (entry.hash));

			if (filter (hash)) {
				input_.seekg (entry.offset);

				PackageDataChunk chunkEntry;
				input_.read (reinterpret_cast<char*> (&chunkEntry),
					sizeof (chunkEntry));

				if (chunkEntry.compressionMode == CompressionMode_Zip) {
					if (buffer.size () < chunkEntry.compressedSize) {
						buffer.resize (chunkEntry.compressedSize);
					}

					input_.read (buffer.data (), chunkEntry.compressedSize);

					const auto targetPath = directory / ToString (hash);

					// Linux specific
					auto fd = open (targetPath.c_str (), O_RDWR, S_IRUSR | S_IWUSR);
					// we expect that the target is already pre-allocated at
					// the right size, otherwise, the mmap below will fail

					unsigned char* mapping = static_cast<unsigned char*> (mmap (
						nullptr, chunkEntry.size,
						PROT_WRITE | PROT_READ, MAP_SHARED, fd, chunkEntry.offset));

					uLongf destinationBufferSize = chunkEntry.size;
					::uncompress (
						mapping,
						&destinationBufferSize,
						reinterpret_cast<unsigned char*> (buffer.data ()),
						static_cast<int> (chunkEntry.compressedSize));

					munmap (mapping, chunkEntry.size);
					close (fd);
				}
			}
		}
	}

private:
	boost::filesystem::ifstream input_;
};

////////////////////////////////////////////////////////////////////////////////
FileSourcePackageReader::FileSourcePackageReader (const boost::filesystem::path& filename)
: impl_ (new Impl (filename))
{
}

////////////////////////////////////////////////////////////////////////////////
FileSourcePackageReader::~FileSourcePackageReader ()
{
}

////////////////////////////////////////////////////////////////////////////////
void FileSourcePackageReader::StoreImpl (const std::function<bool (const Hash &)>& filter,
	const boost::filesystem::path& directory)
{
	impl_->Store (filter, directory);
}
