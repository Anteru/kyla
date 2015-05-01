#ifndef KYLA_CORE_INTERNAL_COMPRESSION_H
#define KYLA_CORE_INTERNAL_COMPRESSION_H

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "ArrayRef.h"

namespace kyla {
enum class CompressionMode : std::uint8_t
{
	Uncompressed,
	Zip,
	LZMA,
	LZ4,
	LZHAM
};

struct BlockCompressor
{
public:
	BlockCompressor ();
	virtual ~BlockCompressor ();

	BlockCompressor (const BlockCompressor&) = delete;
	BlockCompressor& operator= (const BlockCompressor&) = delete;

	int GetCompressionBound (const int inputSize) const;
	int Compress (const ArrayRef<>& input, const MutableArrayRef<>& output);

private:
	virtual int GetCompressionBoundImpl (const int inputSize) const = 0;
	virtual int CompressImpl (const ArrayRef<>& input,
		const MutableArrayRef<>& output) const = 0;
};

struct StreamCompressor
{
public:
	StreamCompressor ();
	virtual ~StreamCompressor ();

	void Initialize (std::function<void (const void* data, const std::int64_t)> writeCallback);
	void Update (const void* data, const std::int64_t size);
	void Finalize ();
	void Compress (const void* data, const std::int64_t size,
		std::vector<std::uint8_t>& buffer);

	StreamCompressor (const StreamCompressor&) = delete;
	StreamCompressor& operator= (const StreamCompressor&) = delete;

private:
	virtual void InitializeImpl (std::function<void (const void* data, const std::int64_t)> writeCallback) = 0;
	virtual void UpdateImpl (const void* data, const std::int64_t size) = 0;
	virtual void FinalizeImpl () = 0;
	virtual void CompressImpl (const void* data, const std::int64_t size,
		std::vector<std::uint8_t>& buffer);
};

std::unique_ptr<StreamCompressor> CreateStreamCompressor (CompressionMode compression);
std::unique_ptr<BlockCompressor> CreateBlockCompressor (CompressionMode compression);
}

#endif
