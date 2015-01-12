#include "Hash.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

Hash ComputeHash (const int64_t size, const void* data)
{
    Hash result;

    SHA512 (static_cast<const unsigned char*> (data), size,
        reinterpret_cast<unsigned char*> (result.hash));

    return result;
}
