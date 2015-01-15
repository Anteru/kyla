#include "Hash.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <boost/filesystem/fstream.hpp>

////////////////////////////////////////////////////////////////////////////////
Hash ComputeHash (const int64_t size, const void* data)
{
    Hash result;

    SHA512 (static_cast<const unsigned char*> (data), size,
        reinterpret_cast<unsigned char*> (result.hash));

    return result;
}

////////////////////////////////////////////////////////////////////////////////
Hash ComputeHash (const boost::filesystem::path& p)
{
	std::vector<char> buffer (4 << 20 /* 4 MiB */);
	return ComputeHash (p, buffer);
}

////////////////////////////////////////////////////////////////////////////////
Hash ComputeHash (const boost::filesystem::path& p, std::vector<char>& buffer)
{
	boost::filesystem::ifstream input (p, std::ios::binary);

	EVP_MD_CTX* fileCtx = EVP_MD_CTX_create ();
	EVP_DigestInit_ex (fileCtx, EVP_sha512 (), nullptr);

	for (;;) {
		input.read (buffer.data (), buffer.size ());
		const auto bytesRead = input.gcount();

		EVP_DigestUpdate (fileCtx, buffer.data (), bytesRead);

		if (bytesRead < buffer.size ()) {
			break;
		}
	}

	Hash result;
	EVP_DigestFinal_ex (fileCtx, result.hash, nullptr);

	return result;
}