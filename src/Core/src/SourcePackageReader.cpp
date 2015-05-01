#include "SourcePackageReader.h"

#include <zlib.h>

#include "SourcePackage.h"
#include "FileIO.h"

#include "Log.h"

namespace kyla {
////////////////////////////////////////////////////////////////////////////////
void SourcePackageReader::Store (const std::function<bool (const SHA512Digest&)>& filter,
	const boost::filesystem::path& directory, Log& log)
{
	StoreImpl (filter, directory, log);
}

////////////////////////////////////////////////////////////////////////////////
struct FileSourcePackageReader::Impl
{
public:
	Impl (const boost::filesystem::path& packageFilename)
		: input_ (kyla::OpenFile (packageFilename.c_str (), kyla::FileOpenMode::Read))
	{
	}

	void Store (const std::function<bool (const SHA512Digest&)>& filter,
		const boost::filesystem::path& directory,
		Log& log)
	{
		SourcePackageHeader header;
		input_->Read (&header, sizeof (header));
		input_->Seek (header.indexOffset);

		std::vector<SourcePackageIndexEntry> index (header.indexEntryCount);
		input_->Read (index.data (), sizeof (SourcePackageIndexEntry) * index.size ());

		std::vector<unsigned char> buffer;
		for (const auto& entry : index) {
			SHA512Digest digest;
			static_assert (sizeof (digest.bytes) == sizeof (entry.sha512digest), "Hash size mismatch!");
			::memcpy (digest.bytes, entry.sha512digest, sizeof (entry.sha512digest));

			if (filter (digest)) {
				input_->Seek (entry.offset);

				log.Debug () << "Extracing content object " << ToString (digest) << " to "
					<< absolute (directory / ToString (digest)).c_str ();

				SourcePackageChunk chunkEntry;
				input_->Read (&chunkEntry, sizeof (chunkEntry));

				if (chunkEntry.compressionMode == CompressionMode::Zip) {
					if (buffer.size () < chunkEntry.compressedSize) {
						buffer.resize (chunkEntry.compressedSize);
					}

					input_->Read (buffer.data (), chunkEntry.compressedSize);

					const auto targetPath = directory / ToString (digest);

					auto targetFile = kyla::CreateFile (targetPath.c_str ());

					// we expect that the target is already pre-allocated at
					// the right size, otherwise, the mmap below will fail

					unsigned char* mapping = static_cast<unsigned char*> (
						targetFile->Map (chunkEntry.offset, chunkEntry.size));

					uLongf destinationBufferSize = chunkEntry.size;
					::uncompress (
						mapping,
						&destinationBufferSize,
						buffer.data (),
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
void FileSourcePackageReader::StoreImpl (const std::function<bool (const SHA512Digest&)>& filter,
	const boost::filesystem::path& directory, Log& log)
{
	impl_->Store (filter, directory, log);
}
}
