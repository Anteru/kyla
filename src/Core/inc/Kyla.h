#ifndef KYLA_PUBLIC_API_H
#define KYLA_PUBLIC_API_H

#ifdef __cplusplus
extern "C" {
#endif

struct KylaInstaller;
struct KylaFeatures;

struct KylaFeature
{
	int id;
	const char* name;
	const char* description;
	int parentId; /* -1 if no parent */
};

enum KylaResult
{
	KylaSuccess = 0,
	KylaError = 1
};

enum KylaLogLevel
{
	KylaLogLevelDebug,
	KylaLogLevelInfo,
	KylaLogLevelWarning,
	KylaLogLevelError
};

struct KylaProperty;

KylaProperty* kylaCreateStringProperty (const char* s);
KylaProperty* kylaCreateIntProperty (const int value);
KylaProperty* kylaCreateBinaryProperty (const void* d, const int size);

int kylaPropertyGetStringValue (const struct KylaProperty* property, const char** value);
int kylaPropertyGetIntValue (const struct KylaProperty* property, int* value);
int kylaPropertyGetBinaryValue (const struct KylaProperty* property, void** d, int* size);

int kylaDeleteProperty (struct KylaProperty* property);

int kylaLog (struct KylaInstaller* installer, const char* logFileName,
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
	const char* name, const struct KylaProperty* value);

int kylaGetProperty (struct KylaInstaller* installer,
	const char* name, struct KylaProperty** output);

typedef void (*KylaProgressCallback)(const int stageCount, const int stageProgress, const char* stageDescription);

int kylaInstall (struct KylaInstaller* package, KylaProgressCallback callback);

int kylaCloseInstallationPackage (struct KylaInstaller* package);

#ifdef __cplusplus
}
#endif

#endif
