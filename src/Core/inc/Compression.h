#ifndef KYLA_CORE_INTERNAL_COMPRESSION_H
#define KYLA_CORE_INTERNAL_COMPRESSION_H

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace kyla {
enum class CompressionMode : std::uint8_t
{
	Uncompressed,
	Zip,
	LZMA,
	LZ4,
	LZHAM
};

struct StreamCompressor
{
public:
	virtual ~StreamCompressor ();
	StreamCompressor ();

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

class ZipStreamCompressor final : public StreamCompressor
{
public:
	ZipStreamCompressor ();
	~ZipStreamCompressor ();

private:
	void InitializeImpl (std::function<void (const void* data, const std::int64_t)> writeCallback) override;
	void UpdateImpl (const void* data, const std::int64_t size) override;
	void FinalizeImpl () override;
	void CompressImpl (const void* data, const std::int64_t size,
		std::vector<std::uint8_t>& buffer) override;

	struct Impl;
	std::unique_ptr<Impl> impl_;
};

class PassthroughStreamCompressor final : public StreamCompressor
{
	void InitializeImpl (std::function<void (const void *, const std::int64_t)> writeCallback) override
	{
		writeCallback_ = writeCallback;
	}

	void UpdateImpl (const void *data, const std::int64_t size) override
	{
		writeCallback_ (data, size);
	}

	void FinalizeImpl () override
	{
	}

	std::function<void (const void* data, const std::int64_t)> writeCallback_;
};

std::unique_ptr<StreamCompressor> CreateStreamCompressor (CompressionMode compression);
}

#endif
