#include "SourcePackageWriter.h"

#include <unordered_map>

#include "FileIO.h"
#include "SourcePackage.h"

namespace kyla {
struct SourcePackageWriter::Impl
{
public:
	std::unordered_map<SHA256Digest, std::vector<boost::filesystem::path>, HashDigestHash, HashDigestEqual> hashChunkMap;
	boost::filesystem::path filename;

	byte packageUuid [16];
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
void SourcePackageWriter::Open (const boost::filesystem::path& filename, const void* uuid)
{
	impl_->filename = filename;
	::memcpy (impl_->packageUuid, uuid, 16);
}

////////////////////////////////////////////////////////////////////////////////
void SourcePackageWriter::Add (const SHA256Digest& digest,
	const boost::filesystem::path& chunkPath)
{
	impl_->hashChunkMap [digest].push_back (chunkPath);
}

////////////////////////////////////////////////////////////////////////////////
namespace {
std::int64_t BlockCopy (const boost::filesystem::path& file, kyla::File& out,
	std::vector<byte>& buffer)
{
	auto input = kyla::OpenFile (file.string ().c_str (), kyla::FileOpenMode::Read);

	std::int64_t bytesReadTotal = 0;

	for (;;) {
		const auto bytesRead = input->Read (buffer);
		bytesReadTotal += bytesRead;
		out.Write (ArrayRef<byte> (buffer.data (), bytesRead));

		if (bytesRead < buffer.size ()) {
			return bytesReadTotal;
		}
	}
}
}

////////////////////////////////////////////////////////////////////////////////
SHA256Digest SourcePackageWriter::Finalize()
{
	auto output = CreateFile (impl_->filename.string ().c_str ());

	// Write header
	SourcePackageHeader header;
	::memset (&header, 0, sizeof (header));
	::memcpy (header.id, "KYLAPACK", 8);
	header.version = 1;
	::memcpy (header.packageId, impl_->packageUuid, sizeof (impl_->packageUuid));
	header.indexEntryCount = static_cast<std::int32_t> (impl_->hashChunkMap.size ());
	header.indexOffset = sizeof (header);

	// Reserve space for index
	std::int64_t offset = sizeof (header);

	// Count all chunks, as we will create one entry per chunk
	for (const auto& hashChunks : impl_->hashChunkMap) {
		header.indexEntryCount += hashChunks.second.size ();
		offset += sizeof (SourcePackageIndexEntry) * hashChunks.second.size ();
	}

	output->Write (ArrayRef<decltype(header)> (header));
	output->Seek (offset);

	// This is our I/O buffer, we read/write in blocks of this size, and also
	// compute the final hash in this buffer
	std::vector<unsigned char> buffer (4 << 20);

	std::vector<SourcePackageIndexEntry> packageIndex;
	packageIndex.reserve (impl_->hashChunkMap.size ());

	for (const auto& hashChunks : impl_->hashChunkMap) {
		for (const auto& chunk : hashChunks.second) {
			SourcePackageIndexEntry indexEntry;
			::memcpy (indexEntry.SHA256digest, hashChunks.first.bytes, sizeof (hashChunks.first.bytes));
			indexEntry.offset = offset;

			offset += BlockCopy (chunk, *output, buffer);
			packageIndex.push_back (indexEntry);
		}
	}

	output->Seek (sizeof (header));
	output->Write (packageIndex);

	output->Close ();

	// We have to read again, as we can't make a fully streaming update due to
	// our delayed index write. Hopefully, the file is still in the disk cache,
	// so this should be quick
	return ComputeSHA256 (impl_->filename, buffer);
}

////////////////////////////////////////////////////////////////////////////////
bool SourcePackageWriter::IsOpen () const
{
	return ! impl_->filename.empty ();
}
}
