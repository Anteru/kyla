#ifndef KYLA_PUBLIC_API_H
#define KYLA_PUBLIC_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct KylaInstaller;
struct KylaFeatures;

struct KylaFeature
{
	int id;
	int parentId; /* -1 if no parent */

	const char* name;
	const char* description;

	/** Required disk space for this feature alone, excluding any children */
	int64_t	requiredDiskSpace;
};

enum KylaResult
{
	KylaSuccess = 0,
	KylaError = 1
};

enum KylaLogLevel
{
	KylaLogLevelTrace,
	KylaLogLevelDebug,
	KylaLogLevelInfo,
	KylaLogLevelWarning,
	KylaLogLevelError
};

enum KylaPropertyCategory
{
	KylaPropertyCategoryInstallation,
	KylaPropertyCategoryEnvironment
};

struct KylaProperty;

struct KylaRepository;

enum KylaRepositoryOpenMode
{
	Read,
	Write
};

enum KylaRepositoryFormat
{
	Unpacked,
	Packed
};

struct KylaContext;

typedef int64_t KylaFileSetId;

struct KylaFileSetInfo
{
	char identifier [256];
	int64_t size;
	int64_t fileCount;
};

int kylaCreateContext (struct KylaContext** context);
int kylaDestroyContext (struct KylaContext** context);

struct KylaRepositoryOpenInfo
{
	const char* path;
	enum KylaRepositoryOpenMode mode;
	enum KylaRepositoryFormat format;
};

int kylaOpenRepository (struct KylaContext* context, const struct KylaRepositoryOpenInfo* openInfo,
	struct KylaRepository** result);
int kylaEnumerateFileSets (struct KylaContext* context, struct KylaRepository* repository, int* count, KylaFileSetInfo* infos);
int kylaChangeRepository (struct KylaContext* context,
	struct KylaRepository* target,
	int fileSetCount,
	const KylaFileSetId* fileSets,
	struct KylaRepository* source
	);

int kylaVerifyRepository (struct KylaContext* context, struct KylaRepository* repository);

int kylaCreateStringProperty (const char* s, KylaProperty** result);
int kylaCreateIntProperty (const int value, KylaProperty** result);
int kylaCreateBinaryProperty (const void* d, const int size, KylaProperty** result);

int kylaPropertyGetStringValue (const struct KylaProperty* property, const char** value);
int kylaPropertyGetIntValue (const struct KylaProperty* property, int* value);
int kylaPropertyGetBinaryValue (const struct KylaProperty* property, const void** d, int* size);

int kylaDeleteProperty (struct KylaProperty* property);

int kylaConfigureLog (struct KylaInstaller* installer, const char* logFileName,
	const int logLevel);

int kylaOpenInstallationPackage (const char* path, struct KylaInstaller** installer);
int kylaGetFeatures (struct KylaInstaller* installer,
	struct KylaFeatures** features);
int kylaDeleteFeatures (struct KylaFeatures* features);

int kylaEnumerateFeatures (struct KylaFeatures* features,
	int* count,
	struct KylaFeature*** first);

int kylaSelectFeatures (struct KylaInstaller* installer,
	int count,
	struct KylaFeature** selected);

int kylaSetProperty (struct KylaInstaller* installer,
	KylaPropertyCategory category,
	const char* name, const struct KylaProperty* value);

int kylaGetProperty (struct KylaInstaller* installer,
	KylaPropertyCategory category,
	const char* name, struct KylaProperty** output);

typedef void (*KylaProgressCallback)(const int stageCount, const int stageProgress, const char* stageDescription);

int kylaInstall (struct KylaInstaller* package, KylaProgressCallback callback);

int kylaCloseInstallationPackage (struct KylaInstaller* package);

#ifdef __cplusplus
}
#endif

#endif
