#ifndef KYLA_PUBLIC_API_H
#define KYLA_PUBLIC_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
enum kylaResult
{
	kylaResult_Ok = 0,
	kylaResult_Error = 1,
	kylaResult_ErrorInvalidArgument = 2
};

typedef void (*KylaProgressCallback)(const int stageCount,
	const int stageProgress, const char* stageDescription);

struct kylaBuildEnvironment
{
	const char* sourceDirectory;
	const char* targetDirectory;
};

int kylaBuildRepository (const char* repositoryDescription,
	const struct kylaBuildEnvironment* buildEnvironment);

enum kylaValidationResult
{
	kylaValidationResult_Ok,
	kylaValidationResult_Corrupted,
	kylaValidationResult_Missing
};

struct kylaValidationItemInfo
{
	const char* filename;
};

typedef void (*KylaValidateCallback)(
	const int validationResult,
	const struct kylaValidationItemInfo* info,
	void* callbackContext);

int kylaValidateRepository (const char* repositoryPath,
	KylaValidateCallback validationCallback,
	void* callbackContext);

int kylaRepairRepository (const char* repositoryPath,
	const char* sourceRepositoryPath);

enum kylaQueryRepositoryKey
{
	kylaQueryRepositoryKey_AvailableFileSets,
	kylaQueryRepositoryKey_GetFileSetName
};

struct kylaFileSetInfo
{
	uint8_t id [16];
	int64_t fileCount;
	int64_t fileSize;
};

int kylaQueryRepository (const char* repository,
	int query,
	void* queryContext,
	int* queryResultSize,
	void* queryResult);

#ifdef __cplusplus
}
#endif

#endif
