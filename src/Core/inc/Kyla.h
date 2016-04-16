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
	const int stageProgress, const char* stageDescription, void* context);

enum kylaLogSeverity
{
	kylaLogSeverity_Debug,
	kylaLogSeverity_Info,
	kylaLogSeverity_Warning
};

typedef void (*KylaLogCallback)(const char* source,
	const kylaLogSeverity severity, const char* message, void* context);

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

typedef void (*KylaValidationCallback)(kylaValidationResult result,
	const kylaValidationItemInfo* info, void* context);

typedef struct KylaRepositoryImpl* KylaSourceRepository;
typedef struct KylaRepositoryImpl* KylaTargetRepository;
typedef struct KylaRepositoryImpl* KylaRepository;

enum kylaRepositoryOption
{
	kylaRepositoryOption_Create,
	kylaRepositoryOption_ReadOnly,
	kylaRepositoryOption_Discover
};

struct KylaFilesetInfo
{
	uint8_t id [16];
	int64_t fileCount;
	int64_t fileSize;
};

enum kylaAction
{
	kylaAction_Install,
	kylaAction_Configure,
	kylaAction_Repair,
	kylaAction_Verify
};

struct KylaDesiredState
{
	int filesetCount;
	const uint8_t* const* filesetIds;
};

struct KylaInstaller
{
	int (*SetLogCallback)(KylaInstaller* installer, KylaLogCallback logCallback, void* callbackContext);
	int (*SetProgressCallback)(KylaInstaller* installer, KylaProgressCallback, void* progressContext);
	int (*SetValidationCallback)(KylaInstaller* installer, KylaValidationCallback validationCallback, void* validationContext);
	int (*OpenSourceRepository)(KylaInstaller* installer, const char* path,
		int options, KylaSourceRepository* repository);
	int (*OpenTargetRepository)(KylaInstaller* installer, const char* path,
		int options, KylaTargetRepository* repository);
	
	int (*CloseRepository)(KylaInstaller* installer,
		KylaRepository impl);
	
	int (*QueryFilesets)(KylaInstaller* installer, KylaSourceRepository repository,
		int* filesetCount, KylaFilesetInfo* filesetInfos);

	int (*QueryFilesetName)(KylaInstaller* installer,
		KylaSourceRepository repository,
		const uint8_t* id,
		int* length,
		char* result);

	int (*Execute)(KylaInstaller* installer, kylaAction action,
		KylaTargetRepository target, KylaSourceRepository source,
		const KylaDesiredState* desiredState);
};

#define KYLA_MAKE_API_VERSION(major,minor,patch) (major << 22 | minor << 12 | patch);
#define KYLA_API_VERSION_1_0 (1<<22)

int kylaCreateInstaller (int kylaApiVersion, KylaInstaller** installer);
int kylaDestroyInstaller (KylaInstaller* installer);

#ifdef __cplusplus
}
#endif

#endif
