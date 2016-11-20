/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#ifndef KYLA_PRIVATE_BUILD_API_H
#define KYLA_PRIVATE_BUILD_API_H

#include <Kyla.h>

#ifdef __cplusplus
extern "C" {
#endif

struct KylaBuildStatistics
{
	int64_t uncompressedContentSize;
	int64_t compressedContentSize;
	float compressionRatio;

	double compressionTimeSeconds;
	double hashTimeSeconds;
	double encryptionTimeSeconds;
};

struct KylaBuildSettings
{
	const char* descriptorFile;
	const char* sourceDirectory;
	const char* targetDirectory;

	KylaLogCallback logCallback;
	KylaProgressCallback progressCallback;

	KylaBuildStatistics* buildStatistics;
};

KYLA_EXPORT int kylaBuildRepository (
	const struct KylaBuildSettings* settings);
#ifdef __cplusplus
}
#endif

#endif
