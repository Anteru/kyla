/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
[LICENSE END]
*/

#include "Hash.h"

#include <openssl/sha.h>

#include "FileIO.h"

namespace kyla {
////////////////////////////////////////////////////////////////////////////////
SHA256Digest ComputeSHA256 (const ArrayRef<>& data)
{
	SHA256Digest result;

	SHA256 (static_cast<const unsigned char*> (data.GetData()), data.GetSize (),
		reinterpret_cast<unsigned char*> (result.bytes));

	return result;
}

////////////////////////////////////////////////////////////////////////////////
SHA256Digest ComputeSHA256 (const boost::filesystem::path& p)
{
	std::vector<unsigned char> buffer (4 << 20 /* 4 MiB */);
	return ComputeSHA256 (p, buffer);
}

////////////////////////////////////////////////////////////////////////////////
SHA256Digest ComputeSHA256(const boost::filesystem::path& p,
	const MutableArrayRef<>& fileReadBuffer)
{
	auto input = kyla::OpenFile (p.string ().c_str (), kyla::FileOpenMode::Read);

	SHA256StreamHasher hasher;
	hasher.Initialize ();

	for (;;) {
		const auto bytesRead = input->Read (fileReadBuffer);

		hasher.Update (ArrayRef<> (fileReadBuffer.GetData (), bytesRead));

		if (bytesRead < fileReadBuffer.GetSize ()) {
			break;
		}
	}

	return hasher.Finalize ();
}

struct SHA256StreamHasher::Impl
{
public:
	Impl ()
	{
	}

	~Impl ()
	{
	}

	void Initialize ()
	{
		SHA256_Init (&ctx_);
	}

	void Update (const void* p, const std::int64_t size)
	{
		assert (size >= 0);
		SHA256_Update (&ctx_, p, static_cast<size_t> (size));
	}

	SHA256Digest Finalize ()
	{
		SHA256Digest result;
		SHA256_Final (result.bytes, &ctx_);
		return result;
	}

private:
	SHA256_CTX ctx_;
};

////////////////////////////////////////////////////////////////////////////////
SHA256StreamHasher::SHA256StreamHasher ()
: impl_ (new Impl)
{
}

////////////////////////////////////////////////////////////////////////////////
SHA256StreamHasher::~SHA256StreamHasher ()
{
}

////////////////////////////////////////////////////////////////////////////////
void SHA256StreamHasher::Initialize ()
{
	impl_->Initialize ();
}

////////////////////////////////////////////////////////////////////////////////
void SHA256StreamHasher::Update (const ArrayRef<>& data)
{
	impl_->Update (data.GetData (), data.GetSize ());
}

////////////////////////////////////////////////////////////////////////////////
SHA256Digest SHA256StreamHasher::Finalize ()
{
	return impl_->Finalize ();
}
}
