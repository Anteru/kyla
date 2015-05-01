#include "InstallPackageWriter.h"

#include "FileIO.h"
#include "Hash.h"
#include "InstallPackage.h"

namespace kyla {
struct InstallPackageWriter::Impl
{
	Impl (const boost::filesystem::path& outputFile)
	: fileHandle (CreateFile (outputFile.c_str ()))
	{

	}

	struct Entry
	{
		std::string name;
		boost::filesystem::path source;
		CompressionMode compressionMode;
	};

	std::vector<Entry> pendingEntries;
	std::unique_ptr<File>	fileHandle;
};

////////////////////////////////////////////////////////////////////////////////
InstallPackageWriter::InstallPackageWriter ()
{
}

////////////////////////////////////////////////////////////////////////////////
InstallPackageWriter::~InstallPackageWriter ()
{
}

////////////////////////////////////////////////////////////////////////////////
void InstallPackageWriter::Open (const boost::filesystem::path& outputFile)
{
	impl_.reset (new Impl (outputFile));
}

////////////////////////////////////////////////////////////////////////////////
void InstallPackageWriter::Add (const char* name,
	const boost::filesystem::path& file)
{
	Add (name, file, CompressionMode::Uncompressed);
}

////////////////////////////////////////////////////////////////////////////////
void InstallPackageWriter::Add (const char* name,
	const boost::filesystem::path& file, CompressionMode compressionMode)
{
	impl_->pendingEntries.emplace_back (Impl::Entry {name, file, compressionMode});
}

namespace {
void HashCompressBlockCopy (File& input, File& output,
	CompressionMode compressionMode,
	SHA512Digest& result,
	int64& uncompressedSize, int64& compressedSize)
{
	std::vector<byte> buffer (4 << 20);

	SHA512StreamHasher hasher;
	hasher.Initialize ();

	auto compressor = CreateStreamCompressor (compressionMode);
	compressor->Initialize ([&](const void* data, const std::int64_t size) -> void {
		compressedSize += size;
		output.Write (data, size);
	});

	for (;;) {
		const auto bytesRead = input.Read (buffer.data (), buffer.size ());

		hasher.Update (ArrayRef<byte> (buffer.data (), bytesRead));
		uncompressedSize += bytesRead;

		compressor->Update (buffer.data (), bytesRead);

		if (bytesRead < buffer.size ()) {
			break;
		}
	}

	// This will flush the output and write the correct compressed size
	compressor->Finalize ();
	result = hasher.Finalize ();
}
}

////////////////////////////////////////////////////////////////////////////////
void InstallPackageWriter::Finalize ()
{
	InstallPackageHeader header;
	::memset (&header, 0, sizeof (header));
	::memcpy (header.id, "KYLAIPKG", 8);
	header.version = 1;
	header.indexEntryCount = static_cast<std::int32_t> (impl_->pendingEntries.size ());
	header.indexOffset = sizeof (header);

	impl_->fileHandle->Write (&header, sizeof (header));

	const auto dataOffset = sizeof (header)
		+ sizeof (InstallPackageIndexEntry) * header.indexEntryCount;

	impl_->fileHandle->Seek (dataOffset);

	std::vector<InstallPackageIndexEntry> index;

	for (const auto& entry : impl_->pendingEntries) {
		InstallPackageIndexEntry indexEntry;
		::memset (&indexEntry, 0, sizeof (indexEntry));
		indexEntry.offset = impl_->fileHandle->Tell ();

		SHA512Digest digest;
		HashCompressBlockCopy (
			*OpenFile (entry.source.c_str (), FileOpenMode::Read),
			*impl_->fileHandle, entry.compressionMode,
			digest, indexEntry.uncompressedSize, indexEntry.compressedSize);
		::memcpy (indexEntry.sha512digest, digest.bytes, sizeof (indexEntry.sha512digest));

		assert (entry.name.size () <= sizeof (indexEntry.name));
		::memcpy (indexEntry.name, entry.name.c_str (), entry.name.size ());

		index.push_back (indexEntry);
	}

	impl_->fileHandle->Seek (header.indexOffset);
	impl_->fileHandle->Write (index.data (),
		sizeof (InstallPackageIndexEntry) * index.size ());
}

////////////////////////////////////////////////////////////////////////////////
bool InstallPackageWriter::IsOpen () const
{
	return static_cast<bool> (impl_);
}
}
