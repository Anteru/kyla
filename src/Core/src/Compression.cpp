#include "Compression.h"

#include <cstring>
#include <zlib.h>
#include <vector>

namespace kyla {
struct NullBlockCompressor final : public BlockCompressor
{
	int GetCompressionBoundImpl (const int inputSize) const override;
	int CompressImpl (const ArrayRef<>& input,
		const MutableArrayRef<>& output) const override;
};

struct ZipBlockCompressor final : public BlockCompressor
{
	int GetCompressionBoundImpl (const int inputSize) const override;
	int CompressImpl (const ArrayRef<>& input,
		const MutableArrayRef<>& output) const override;
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

////////////////////////////////////////////////////////////////////////////////
BlockCompressor::BlockCompressor ()
{
}

////////////////////////////////////////////////////////////////////////////////
BlockCompressor::~BlockCompressor ()
{
}

////////////////////////////////////////////////////////////////////////////////
int BlockCompressor::GetCompressionBound (const int inputSize) const
{
	return GetCompressionBoundImpl (inputSize);
}

////////////////////////////////////////////////////////////////////////////////
int BlockCompressor::Compress (const ArrayRef<>& input,
	const MutableArrayRef<>& output)
{
	return CompressImpl (input, output);
}

////////////////////////////////////////////////////////////////////////////////
int ZipBlockCompressor::GetCompressionBoundImpl (const int inputSize) const
{
	return ::compressBound (inputSize);
}

////////////////////////////////////////////////////////////////////////////////
int ZipBlockCompressor::CompressImpl (const ArrayRef<>& input,
	const MutableArrayRef<>& output) const
{
	::uLongf compressedSize = output.GetSize ();
	::compress (static_cast<::Bytef*> (output.GetData()), &compressedSize,
			static_cast<const ::Bytef*> (input.GetData ()), input.GetSize ());
	return compressedSize;
}

////////////////////////////////////////////////////////////////////////////////
int NullBlockCompressor::GetCompressionBoundImpl (const int inputSize) const
{
	return inputSize;
}

////////////////////////////////////////////////////////////////////////////////
int NullBlockCompressor::CompressImpl (const ArrayRef<>& input,
	const MutableArrayRef<>& output) const
{
	::memcpy (output.GetData (), input.GetData (), input.GetSize ());
}

////////////////////////////////////////////////////////////////////////////////
StreamCompressor::StreamCompressor ()
{
}

////////////////////////////////////////////////////////////////////////////////
StreamCompressor::~StreamCompressor ()
{
}

////////////////////////////////////////////////////////////////////////////////
void StreamCompressor::Initialize (std::function<void (const void* data, const std::int64_t)> writeCallback)
{
	InitializeImpl (writeCallback);
}

////////////////////////////////////////////////////////////////////////////////
void StreamCompressor::Update (const void* data, const std::int64_t size)
{
	UpdateImpl (data, size);
}

////////////////////////////////////////////////////////////////////////////////
void StreamCompressor::Finalize ()
{
	FinalizeImpl ();
}

////////////////////////////////////////////////////////////////////////////////
void StreamCompressor::Compress (const void* data, const std::int64_t size,
	std::vector<std::uint8_t>& buffer)
{
	CompressImpl (data, size, buffer);
}

////////////////////////////////////////////////////////////////////////////////
void StreamCompressor::CompressImpl (const void* data, const std::int64_t size,
	std::vector<std::uint8_t>& buffer)
{
	Initialize ([&](const void* data, const std::int64_t size) -> void {
		buffer.insert (buffer.end (), static_cast<const std::uint8_t*> (data),
									  static_cast<const std::uint8_t*> (data) + size);
	});
	Update (data, size);
	Finalize ();
}

////////////////////////////////////////////////////////////////////////////////
class ZipStreamCompressor::Impl
{
public:
	Impl ()
	: buffer_ (4 << 20)
	{
	}

	~Impl ()
	{
	}

	void Initialize (std::function<void (const void* data, const std::int64_t)> writeCallback)
	{
		::memset (&zStream_, 0, sizeof (zStream_));
		deflateInit (&zStream_, 9);

		callback_ = writeCallback;
	}

	void Update (const void* data, const std::int64_t size)
	{
		zStream_.avail_in = size;
		zStream_.next_in = static_cast<Bytef*> (const_cast<void*> (data));

		zStream_.avail_out = buffer_.size ();
		zStream_.next_out = buffer_.data ();

		do {
			deflate (&zStream_, Z_NO_FLUSH);
			Write ();
		} while (zStream_.avail_in > 0);
	}

	void Finalize ()
	{
		if (zStream_.next_in == nullptr) {
			return;
		}

		int r = Z_OK;

		do {
			r = deflate (&zStream_, Z_FINISH);
			Write ();
		} while (r == Z_OK);

		deflateEnd (&zStream_);
	}

private:
	void Write ()
	{
		const auto written = buffer_.size () - zStream_.avail_out;
		if (written > 0) {
			callback_ (buffer_.data (), written);
			zStream_.avail_out = buffer_.size ();
			zStream_.next_out = buffer_.data ();
		}
	}

	std::function<void (const void* data, const std::int64_t)> callback_;
	std::vector<std::uint8_t> buffer_;
	z_stream zStream_;
};

////////////////////////////////////////////////////////////////////////////////
ZipStreamCompressor::ZipStreamCompressor ()
: impl_ (new Impl)
{
}

////////////////////////////////////////////////////////////////////////////////
ZipStreamCompressor::~ZipStreamCompressor ()
{
}

////////////////////////////////////////////////////////////////////////////////
void ZipStreamCompressor::InitializeImpl (std::function<void (const void* data, const std::int64_t)> writeCallback)
{
	impl_->Initialize (writeCallback);
}

////////////////////////////////////////////////////////////////////////////////
void ZipStreamCompressor::UpdateImpl (const void* data, const std::int64_t size)
{
	impl_->Update (data, size);
}

////////////////////////////////////////////////////////////////////////////////
void ZipStreamCompressor::FinalizeImpl ()
{
	impl_->Finalize ();
}

////////////////////////////////////////////////////////////////////////////////
void ZipStreamCompressor::CompressImpl (const void* data, const std::int64_t size,
	std::vector<std::uint8_t>& buffer)
{
	buffer.resize (::compressBound (size));

	uLongf destLen = buffer.size ();
	compress2 (buffer.data (), &destLen,
		static_cast<const Bytef*> (data), size, Z_BEST_COMPRESSION);
	buffer.resize (destLen);
}

////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<StreamCompressor> CreateStreamCompressor (CompressionMode compression)
{
	switch (compression) {
	case CompressionMode::Zip:
		return std::unique_ptr<StreamCompressor> (new ZipStreamCompressor);

	case CompressionMode::Uncompressed:
		return std::unique_ptr<StreamCompressor> (new PassthroughStreamCompressor);
	}
}

////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<BlockCompressor> CreateBlockCompressor (CompressionMode compression)
{
	switch (compression) {
	case CompressionMode::Zip:
		return std::unique_ptr<BlockCompressor> (new ZipBlockCompressor);

	case CompressionMode::Uncompressed:
		return std::unique_ptr<BlockCompressor> (new NullBlockCompressor);
	}
}
}
