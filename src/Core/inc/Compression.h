#ifndef KYLA_CORE_INTERNAL_COMPRESSION_H
#define KYLA_CORE_INTERNAL_COMPRESSION_H

#include <cstdint>
#include <functional>
#include <memory>

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

	void Initialize (std::function<void (const void* data, const std::int64_t)> writeCallback);
	void Update (const void* data, const std::int64_t size);
	void Finalize ();

private:
	virtual void InitializeImpl (std::function<void (const void* data, const std::int64_t)> writeCallback) = 0;
	virtual void UpdateImpl (const void* data, const std::int64_t size) = 0;
	virtual void FinalizeImpl () = 0;
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

	struct Impl;
	std::unique_ptr<Impl> impl_;
};
}

#endif
