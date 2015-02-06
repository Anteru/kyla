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
	Hash* result, std::int64_t* uncompressedSize, std::int64_t* compressedSize)
{
	std::vector<std::uint8_t> buffer (4 << 20);

	StreamHasher hasher;
	hasher.Initialize ();

	for (;;) {
		const auto bytesRead = input.Read (buffer.data (), buffer.size ());

		hasher.Update (buffer.data (), bytesRead);

		if (bytesRead < buffer.size ()) {
			break;
		}
	}

	*result = hasher.Finalize ();
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
		// Block copy with hash and compression
	}
}

////////////////////////////////////////////////////////////////////////////////
bool InstallPackageWriter::IsOpen () const
{
	return static_cast<bool> (impl_);
}
}
