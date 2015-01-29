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

	EVP_MD_CTX* fileCtx = EVP_MD_CTX_create ();
	EVP_DigestInit_ex (fileCtx, EVP_sha512 (), nullptr);

	for (;;) {
		const auto bytesRead = input->Read (buffer.data (), buffer.size ());

		EVP_DigestUpdate (fileCtx, buffer.data (), bytesRead);

		if (bytesRead < buffer.size ()) {
			break;
		}
	}

	Hash result;
	EVP_DigestFinal_ex (fileCtx, result.hash, nullptr);

	return result;
}
}
