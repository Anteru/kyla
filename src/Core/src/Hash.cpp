#include "Hash.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "FileIO.h"

namespace kyla {
////////////////////////////////////////////////////////////////////////////////
Hash ComputeHash (const void* data, const std::int64_t size)
{
    Hash result;

    SHA512 (static_cast<const unsigned char*> (data), size,
        reinterpret_cast<unsigned char*> (result.hash));

    return result;
}

////////////////////////////////////////////////////////////////////////////////
Hash ComputeHash (const boost::filesystem::path& p)
{
	std::vector<unsigned char> buffer (4 << 20 /* 4 MiB */);
	return ComputeHash (p, buffer);
}

////////////////////////////////////////////////////////////////////////////////
Hash ComputeHash (const boost::filesystem::path& p, std::vector<unsigned char>& buffer)
{
	auto input = kyla::OpenFile (p.c_str (), kyla::FileOpenMode::Read);

	StreamHasher hasher;
	hasher.Initialize ();

	for (;;) {
		const auto bytesRead = input->Read (buffer.data (), buffer.size ());

		hasher.Update (buffer.data (), bytesRead);

		if (bytesRead < buffer.size ()) {
			break;
		}
	}

	return hasher.Finalize ();
}

struct StreamHasher::Impl
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

	Hash Finalize ()
	{
		Hash result;
		EVP_DigestFinal_ex (ctx_, result.hash, nullptr);
	}

private:
	EVP_MD_CTX*	ctx_;
};

////////////////////////////////////////////////////////////////////////////////
StreamHasher::StreamHasher ()
: impl_ (new Impl)
{
}

////////////////////////////////////////////////////////////////////////////////
StreamHasher::~StreamHasher ()
{
}

////////////////////////////////////////////////////////////////////////////////
void StreamHasher::Initialize ()
{
	impl_->Initialize ();
}

////////////////////////////////////////////////////////////////////////////////
void StreamHasher::Update (const void* data, const std::int64_t size)
{
	impl_->Update (data, size);
}

////////////////////////////////////////////////////////////////////////////////
Hash StreamHasher::Finalize ()
{
	return impl_->Finalize ();
}
}
