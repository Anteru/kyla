/**
[LICENSE BEGIN]
kyla Copyright (C) 2015-2017 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#ifndef KYLA_PUBLIC_API_H
#define KYLA_PUBLIC_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef KYLA_BUILD_LIBRARY
	#if defined(_WIN32)
		#define KYLA_EXPORT __declspec(dllexport)
	#elif __GNUC__ >= 4
		#define KYLA_EXPORT __attribute__ ((visibility ("default")))
	#else
		#error unsupported platform
	#endif
#else
	#define KYLA_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif
enum kylaResult
{
	kylaResult_Ok = 0,
	kylaResult_Error = 1,
	kylaResult_ErrorInvalidArgument = 2,
	kylaResult_ErrorUnsupportedApiVersion = 3
};

struct KylaProgress
{
	float totalProgress;

	const char* action;
	const char* detailMessage;
};

typedef void (*KylaProgressCallback)(const struct KylaProgress* progress,
	void* context);

enum kylaLogSeverity
{
	kylaLogSeverity_Debug,
	kylaLogSeverity_Info,
	kylaLogSeverity_Warning,
	kylaLogSeverity_Error
};

struct KylaLog
{
	const kylaLogSeverity severity;
	const int64_t timestamp;

	const char* source;
	const char* message;
};

typedef void (*KylaLogCallback)(const struct KylaLog* log,
	void* context);

enum kylaValidationResult
{
	kylaValidationResult_Ok,
	kylaValidationResult_Corrupted,
	kylaValidationResult_Missing
};

enum kylaValidationItemType
{
	kylaValidationItemType_File
};

struct KylaValidationInfoFile
{
	const char* filename;
};

struct KylaValidation
{
	kylaValidationItemType itemType;
	kylaValidationResult result;
	union {
		const struct KylaValidationInfoFile* infoFile;
	};
};

typedef void (*KylaValidationCallback)(const struct KylaValidation* validation,
	void* context);

typedef struct KylaRepositoryImpl* KylaSourceRepository;
typedef struct KylaRepositoryImpl* KylaTargetRepository;
typedef struct KylaRepositoryImpl* KylaRepository;

enum kylaRepositoryOption
{
	/**
	Create the repository. If it's present already, it will be overwritten by
	subsequent operations.

	Cannot be set for a source repository.
	*/
	kylaRepositoryOption_Create		= 1 << 0,
	/**
	Open the repository in read-only mode.

	This is useful for operations like validation.

	For a source repository, this flag is always set.
	*/
	kylaRepositoryOption_ReadOnly	= 1 << 1
};

enum kylaAction
{
	kylaAction_Install		= 1,
	kylaAction_Configure	= 2,
	kylaAction_Repair		= 3,
	kylaAction_Verify		= 4
};

struct KylaDesiredState
{
	int featureCount;
	const uint8_t* const* featureIds;
};

struct KylaUuid
{
	uint8_t bytes [16];
};

enum kylaRepositoryProperty
{
	/**
	The list of available features, provided as KylaUuids.

	The result is a tightly packed array of KylaUuid instances.
	*/
	kylaRepositoryProperty_AvailableFeatures = 1,

	/**
	Whether the repository is encrypted or not.

	The result is an int which is 0 if not encrypted and 1 if
	the repository is encrypted. For encrypted repositories, set
	the key by setting the kylaRepositoryProperty_DecryptionKey

	@since 2.0
	*/
	kylaRepositoryProperty_IsEncrypted = 2
};

enum kylaInstallerVariable
{
	/**
	The decryption key to use if the repository was encrypted.

	@since 3.0
	*/
	kylaInstallerVariable_DecryptionKey
};

enum kylaFeatureProperty
{
	/**
	The deploy size of a feature, provided as an int64_t.
	*/
	kylaFeatureProperty_Size = 1,

	/**
	All subfeatures of this feature.

	Returns a list of feature IDs. This returns a tighly packed list of 
	Uuids.

	@since 3.0
	*/
	kylaFeatureProperty_SubfeatureIds = 2,

	/**
	The title of this feature.

	Returns a null-terminated string. This string may be empty in case no title
	has been set.

	@since 3.0
	*/
	kylaFeatureProperty_Title = 3,

	/**
	The description of this feature.

	Returns a null-terminated string. This string may be empty in case no 
	description has been set.

	@since 3.0
	*/
	kylaFeatureProperty_Description = 4
};

struct KylaInstaller
{
	/**
	Set the log callback. The callbackContext will be passed on into the
	log callback function.

	Setting a null callback disables it.
	*/
	int (*SetLogCallback)(KylaInstaller* installer,
		KylaLogCallback logCallback, void* callbackContext);

	/**
	Set the progress callback. The callbackContext will be passed on into the
	progress callback function.

	Setting a null callback disables it.
	*/
	int (*SetProgressCallback)(KylaInstaller* installer,
		KylaProgressCallback progressCallback, void* progressContext);

	/**
	Set the validation callback. The callbackContext will be passed on into the
	validation callback function.

	Setting a null callback disables it.
	*/
	int (*SetValidationCallback)(KylaInstaller* installer,
		KylaValidationCallback validationCallback, void* validationContext);

	/**
	Open a source repository.

	A source repository is opened for read-only access. The options is a
	combination of kylaRepositoryOption.

	If the path starts with "http", the repository is opened via HTTP.
	*/
	int (*OpenSourceRepository)(KylaInstaller* installer, const char* path,
		int options, KylaSourceRepository* repository);

	/**
	Open a target repository.

	The options is a combination of kylaRepositoryOption. By default, it's
	opened for writing (and assumed to exist already).

	If the repository is used for an initial installation, the options must
	include kylaRepositoryOption_Create.

	Opening a target repository using kylaRepositoryOption_ReadOnly allows only
	verify to be called on the repository. The main advantage of read only
	access is that files are not exclusively locked during access - that makes
	it possible to open the files in other applications while the verification
	is running.
	*/
	int (*OpenTargetRepository)(KylaInstaller* installer, const char* path,
		int options, KylaTargetRepository* repository);

	/**
	Close a source or target repository.
	*/
	int (*CloseRepository)(KylaInstaller* installer,
		KylaRepository impl);

	/**
	Get a repository property.

	The propertyId must be one of enumeration values from
	kylaRepositoryProperty. If resultSize is non-null, and result is null, the result
	size is written into resultSize. If resultSize and result are both non-null, the
	result is written into result if the resultSize is greater than or equal to the
	required size. The actually written size will be stored in resultSize in this case.

	The result size must be always provided.

	@since 2.0
	*/
	int (*GetRepositoryProperty)(KylaInstaller* installer,
		KylaSourceRepository repository,
		int propertyId,
		size_t* resultSize,
		void* result);

	/**
	Set an installer variable by name.

	The variable name must be non-zero.

	@since 3.0
	*/
	int (*SetVariable)(KylaInstaller* installer,
		const char* variableName,
		size_t variableSize,
		const void* variableValue);

	/**
	
	@since 3.0
	*/
	int (*GetVariable)(KylaInstaller* installer,
		const char* variableName,
		size_t* resultSize,
		void* result);

	/**
	Get a feature property.

	The propertyId must be one of enumeration values from
	kylaFeatureProperty. If resultSize is non-null, and result is null, the result
	size is written into resultSize. If resultSize and result are both non-null, the
	result is written into result if the resultSize is greater than or equal to the
	required size. The actually written size will be stored in resultSize in this case.

	The result size must be always provided.

	@since 2.0
	*/
	int (*GetFeatureProperty)(KylaInstaller* installer,
		KylaSourceRepository repository,
		struct KylaUuid id,
		int propertyId,
		size_t* resultSize,
		void* result);

	/**
	Execute an action on the target repository.

	Most actions require a desired state.
	*/
	int (*Execute)(KylaInstaller* installer, kylaAction action,
		KylaTargetRepository target, KylaSourceRepository source,
		const KylaDesiredState* desiredState);
};

#define KYLA_MAKE_API_VERSION(major,minor,patch) (major << 22 | minor << 12 | patch)
#define KYLA_API_VERSION_3_0 KYLA_MAKE_API_VERSION(3,0,0)

/**
Create a new installer. Installer must be non-null, and kylaApiVersion must be
a supported version created using either KYLA_MAKE_API_VERSION or by using one
of the pre-defined constants like KYLA_API_VERSION_3_0.
*/
KYLA_EXPORT int kylaCreateInstaller (int kylaApiVersion, KylaInstaller** installer);

/**
Destroy an installer. All objects queried off the installer become invalid
after this call.
*/
KYLA_EXPORT int kylaDestroyInstaller (KylaInstaller* installer);

#ifdef __cplusplus
}
#endif

#endif
