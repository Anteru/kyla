#include "Hash.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "FileIO.h"

namespace kyla {
////////////////////////////////////////////////////////////////////////////////
SHA512Digest ComputeSHA512 (const ArrayRef<>& data)
{
	SHA512Digest result;

	SHA512 (static_cast<const unsigned char*> (data.GetData()), data.GetSize (),
        reinterpret_cast<unsigned char*> (result.bytes));

    return result;
}

////////////////////////////////////////////////////////////////////////////////
SHA512Digest ComputeSHA512 (const boost::filesystem::path& p)
{
	std::vector<unsigned char> buffer (4 << 20 /* 4 MiB */);
	return ComputeSHA512 (p, buffer);
}

////////////////////////////////////////////////////////////////////////////////
SHA512Digest ComputeSHA512(const boost::filesystem::path& p,
	std::vector<byte>& fileReadBuffer)
{
	auto input = kyla::OpenFile (p.c_str (), kyla::FileOpenMode::Read);

	SHA512StreamHasher hasher;
	hasher.Initialize ();

	for (;;) {
		const auto bytesRead = input->Read (fileReadBuffer.data (),
			fileReadBuffer.size ());

		hasher.Update (ArrayRef<byte> (fileReadBuffer.data (), bytesRead));

		if (bytesRead < fileReadBuffer.size ()) {
			break;
		}
	}

	return hasher.Finalize ();
}

struct SHA512StreamHasher::Impl
{
public:
	Impl ()
	{
		ctx_ = EVP_MD_CTX_create ();
	}

	~Impl ()
	{
		EVP_MD_CTX_destroy (ctx_);
	}

	void Initialize ()
	{
		EVP_DigestInit_ex (ctx_, EVP_sha512 (), nullptr);
	}

	void Update (const void* p, const std::int64_t size)
	{
		EVP_DigestUpdate (ctx_, p, size);
	}

	SHA512Digest Finalize ()
	{
		SHA512Digest result;
		EVP_DigestFinal_ex (ctx_, result.bytes, nullptr);
		return result;
	}

private:
	EVP_MD_CTX*	ctx_;
};

////////////////////////////////////////////////////////////////////////////////
SHA512StreamHasher::SHA512StreamHasher ()
: impl_ (new Impl)
{
}

////////////////////////////////////////////////////////////////////////////////
SHA512StreamHasher::~SHA512StreamHasher ()
{
}

////////////////////////////////////////////////////////////////////////////////
void SHA512StreamHasher::Initialize ()
{
	impl_->Initialize ();
}

////////////////////////////////////////////////////////////////////////////////
void SHA512StreamHasher::Update (const ArrayRef<>& data)
{
	impl_->Update (data.GetData (), data.GetSize ());
}

////////////////////////////////////////////////////////////////////////////////
SHA512Digest SHA512StreamHasher::Finalize ()
{
	return impl_->Finalize ();
}
}
