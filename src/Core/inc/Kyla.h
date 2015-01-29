#ifndef KYLA_PUBLIC_API_H
#define KYLA_PUBLIC_API_H

#ifdef __cplusplus
extern "C" {
#endif

struct KylaInstallationPackage;
struct KylaFeatures;

struct KylaFeature
{
	const char* id;
	const char* name;
	const char* description;
	const struct KylaFeature* parent;
};

enum KylaResult
{
	KylaSuccess = 0,
	KylaError = 1
};

struct KylaProperty;

KylaProperty* CreateStringProperty (const char* s);
KylaProperty* CreateIntProperty (const int value);
KylaProperty* CreateBinaryProperty (const void* d, const int size);

int DeleteProperty (struct KylaProperty* property);

int OpenInstallationPackage (const char* path, KylaInstallationPackage** output);
int GetFeatures (KylaInstallationPackage* package,
	KylaFeatures** features);
int DeleteFeatures (KylaFeatures* features);

int EnumerateFeatures (KylaFeatures* features,
	int* count,
	KylaFeature** first);

int SelectFeatures (KylaInstallationPackage* package,
	int count,
	KylaFeature* selected);

int SetProperty (const char* name, const struct KylaProperty* value);

typedef void (*KylaProgressCallback)(const int stageCount, const int stageProgress, const char* stageDescription);

int InstallPackage (KylaInstallationPackage* package, KylaProgressCallback callback);

int CloseInstallationPackage (KylaInstallationPackage* package);

#ifdef __cplusplus
}
#endif

#endif
