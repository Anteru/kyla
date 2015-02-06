#include "Compression.h"

#include <cstring>
#include <zlib.h>
#include <vector>

namespace kyla {
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

		for (;;) {
			zStream_.avail_out = buffer_.size ();
			zStream_.next_out = buffer_.data ();

			deflate (&zStream_, Z_NO_FLUSH);

			// Finally, we didn't fill the output buffer
			if (zStream_.avail_out != 0) {
				break;
			}
		}
	}

	void Finalize ()
	{
		deflateEnd (&zStream_);
	}

private:
	std::function<void (const void* data, const std::int64_t)> callback_;
	std::vector<std::uint8_t> buffer_;
	z_stream zStream_;
};
}
