#include "SourcePackage.h"

#include <unordered_map>
#include <boost/filesystem/fstream.hpp>
#include <openssl/evp.h>

#include <zlib.h>

#include "FileIO.h"

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
void SourcePackageWriter::Add (const Hash& hash,
	const boost::filesystem::path& chunkPath)
{
	impl_->hashChunkMap [hash].push_back (chunkPath);
}

////////////////////////////////////////////////////////////////////////////////
namespace {
std::int64_t BlockCopy (const boost::filesystem::path& file, kyla::File& out,
	std::vector<char>& buffer)
{
	boost::filesystem::ifstream input (file, std::ios::binary);

	std::int64_t bytesReadTotal = 0;

	for (;;) {
		input.read (buffer.data (), buffer.size ());
		const auto bytesRead = input.gcount ();
		bytesReadTotal += bytesRead;
		out.Write (buffer.data (), bytesRead);

		if (bytesRead < buffer.size ()) {
			return bytesReadTotal;
		}
	}
}
}

////////////////////////////////////////////////////////////////////////////////
Hash SourcePackageWriter::Finalize()
{
	auto output = kyla::CreateFile (impl_->filename.c_str ());

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

	output->Write (&header, sizeof (header));
	output->Seek (offset);

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

			offset += BlockCopy (chunk, *output, buffer);
			packageIndex.push_back (indexEntry);
		}
	}

	output->Seek (sizeof (header));
	output->Write (packageIndex.data (),
		sizeof (PackageIndex) * packageIndex.size ());

	output->Close ();

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
		: input_ (kyla::OpenFile (packageFilename.c_str (), kyla::FileOpenMode::Read))
	{
	}

	void Store (const std::function<bool (const Hash &)>& filter,
		const boost::filesystem::path& directory)
	{
		PackageHeader header;
		input_->Read (&header, sizeof (header));
		input_->Seek (header.indexOffset);

		std::vector<PackageIndex> index (header.indexEntries);
		input_->Read (index.data (), sizeof (PackageIndex) * index.size ());

		std::vector<char> buffer;
		for (const auto& entry : index) {
			Hash hash;
			static_assert (sizeof (hash.hash) == sizeof (entry.hash), "Hash size mismatch!");
			::memcpy (hash.hash, entry.hash, sizeof (entry.hash));

			if (filter (hash)) {
				input_->Seek (entry.offset);

				PackageDataChunk chunkEntry;
				input_->Read (&chunkEntry, sizeof (chunkEntry));

				if (chunkEntry.compressionMode == CompressionMode_Zip) {
					if (buffer.size () < chunkEntry.compressedSize) {
						buffer.resize (chunkEntry.compressedSize);
					}

					input_->Read (buffer.data (), chunkEntry.compressedSize);

					const auto targetPath = directory / ToString (hash);

					auto targetFile = kyla::CreateFile (targetPath.c_str ());

					// we expect that the target is already pre-allocated at
					// the right size, otherwise, the mmap below will fail

					unsigned char* mapping = static_cast<unsigned char*> (
						targetFile->Map (chunkEntry.offset, chunkEntry.size));

					uLongf destinationBufferSize = chunkEntry.size;
					::uncompress (
						mapping,
						&destinationBufferSize,
						reinterpret_cast<unsigned char*> (buffer.data ()),
						static_cast<int> (chunkEntry.compressedSize));

					targetFile->Unmap (mapping);
				}
			}
		}
	}

private:
	std::unique_ptr<kyla::File> input_;
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
