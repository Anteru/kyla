#include "Hash.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
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
		ctx_ = EVP_MD_CTX_create ();
	}

	~Impl ()
	{
		EVP_MD_CTX_destroy (ctx_);
	}

	void Initialize ()
	{
		EVP_DigestInit_ex (ctx_, EVP_sha256 (), nullptr);
	}

	void Update (const void* p, const std::int64_t size)
	{
		EVP_DigestUpdate (ctx_, p, size);
	}

	SHA256Digest Finalize ()
	{
		SHA256Digest result;
		EVP_DigestFinal_ex (ctx_, result.bytes, nullptr);
		return result;
	}

private:
	EVP_MD_CTX*	ctx_;
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
