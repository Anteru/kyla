#ifndef _NIM_COMMON_SOURCE_PACKAGE_H_
#define _NIM_COMMON_SOURCE_PACKAGE_H_

#include <stdint.h>
#include "Hash.h"

#include <vector>
#include <boost/filesystem.hpp>
#include <memory>

struct PackageHeader
{
    unsigned char id [8];
    int32_t   version;
    int32_t   indexEntries;
    int64_t   indexOffset;
    int64_t   dataOffset;
};

struct PackageIndex
{
    uint8_t hash [64];
    int64_t offset;
};

enum CompressionMode
{
    CompressionMode_Uncompressed,
    CompressionMode_Zip,
    CompressionMode_LZMA,
    CompressionMode_LZ4
};

struct PackageDataChunk
{
    uint8_t hash [64];
    int64_t offset;
    int64_t size;

    int64_t compressedSize;
    int8_t  compressionMode;
    uint8_t reserved [7];
};

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

	void Open (const boost::filesystem::path& outputFile);
	void Add (const Hash& hash, const boost::filesystem::path& chunkPath);
	Hash Finalize ();

	bool IsOpen () const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

class ISourcePackageReader
{
public:
    virtual ~ISourcePackageReader () = default;
    ISourcePackageReader (const ISourcePackageReader&) = delete;
    ISourcePackageReader& operator= (const ISourcePackageReader&) = delete;

    std::vector<Hash>   GetHashes () const;
    std::vector<unsigned char>  GetEntry (const Hash& hash);

private:
    virtual std::vector<Hash> GetHashesImpl () const = 0;
    virtual std::vector<unsigned char> GetEntryImpl () const = 0;
};

class FileSourcePackageReader final : public ISourcePackageReader
{
public:
    FileSourcePackageReader (const char* filename);

private:
    std::vector<Hash> GetHashesImpl () const;
    std::vector<unsigned char> GetEntryImpl () const;
};

#endif