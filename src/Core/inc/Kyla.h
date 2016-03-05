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

typedef void (*KylaValidateCallback)(
	const char* filename,
	const int validationResult,
	void* callbackContext);

int kylaValidateRepository (const char* repositoryPath,
	KylaValidateCallback validationCallback,
	void* callbackContext);

int kylaRepairRepository (const char* repositoryPath,
	// If nullptr, this means we have to try to repair using the target
	// only - that can work if a duplicated file is missing, or a zero-sized
	// file is missing
	const char* sourceRepositoryPath);

#ifdef __cplusplus
}
#endif

#endif
