#ifndef KYLA_PUBLIC_API_H
#define KYLA_PUBLIC_API_H

#ifdef __cplusplus
extern "C" {
#endif

struct KylaInstallationPackage;
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

struct KylaProperty;

KylaProperty* kylaCreateStringProperty (const char* s);
KylaProperty* kylaCreateIntProperty (const int value);
KylaProperty* kylaCreateBinaryProperty (const void* d, const int size);

int kylaDeleteProperty (struct KylaProperty* property);

int kylaOpenInstallationPackage (const char* path, KylaInstallationPackage** output);
int kylaGetFeatures (KylaInstallationPackage* package,
	KylaFeatures** features);
int kylaDeleteFeatures (KylaFeatures* features);

int kylaEnumerateFeatures (KylaFeatures* features,
	int* count,
	KylaFeature*** first);

int kylaSelectFeatures (KylaInstallationPackage* package,
	int count,
	KylaFeature** selected);

int kylaSetProperty (KylaInstallationPackage* package,
	const char* name, const struct KylaProperty* value);

typedef void (*KylaProgressCallback)(const int stageCount, const int stageProgress, const char* stageDescription);

int kylaInstallPackage (KylaInstallationPackage* package, KylaProgressCallback callback);

int kylaCloseInstallationPackage (KylaInstallationPackage* package);

#ifdef __cplusplus
}
#endif

#endif
