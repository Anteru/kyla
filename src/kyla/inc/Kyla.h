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

typedef void (*KylaLogCallback)(const char* source,
	const kylaLogSeverity severity, const char* message, 
	const int64_t timeStamp, void* context);

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

struct KylaFeatureTreeNode
{
	const struct KylaFeatureTreeNode* parent;
	const char* name;
	const char* description;
};

enum kylaRepositoryOption
{
	/**
	Create the repository. If it's present already, it will be overwritten by
	subsequent operation.

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

enum kylaInstallationLevel
{
	kylaInstallationLevel_Default = 1000
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

enum kylaFeatureRelationship
{
	kylaFeatureRelationship_Requires
};

struct KylaFeatureDependency
{
	KylaUuid source;
	KylaUuid target;
	kylaFeatureRelationship relationship;
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
	kylaRepositoryProperty_IsEncrypted = 2,

	/**
	The decryption key which will be used if the repository is encrypted.
	@since 2.0
	*/
	kylaRepositoryProperty_DecryptionKey = 3
};

enum kylaFeatureProperty
{
	/**
	The deploy size of a feature, provided as an int64_t.
	*/
	kylaFeatureProperty_Size = 1,

	/**
	All dependencies of this feature, as KylaFeatureDependency instances

	@since 2.0
	*/
	kylaFeatureProperty_Dependencies = 2,

	/**
	
	The default installation level at which this feature should be installed.

	The feature level is stored as an int64_t. kylaInstallationLevel_Default
	is returned for features which should be installed by default.

	@since 2.0
	*/
	kylaFeatureProperty_InstallationLevel = 3
};

enum kylaFeatureTreeProperty
{
	/**
	Returns all nodes in the feature tree.

	The result is an array of kylaFeatureTreeNode pointers. The list order
	is guaranteed to stay the same for one source repository.
	*/
	kylaFeatureTreeProperty_Nodes = 1,

	/**
	Returns all features associated with a feature tree node.

	The result is a list of tightly packed KylaUuid instances. The feature
	tree node must be passed in as the object into the query function.
	*/
	kylaFeatureTreeProperty_NodeFeatures = 2,

	/**
	The default installation level at which this feature should be installed.

	The feature level is stored as an int64_t. kylaInstallationLevel_Default
	is returned for features which should be installed by default. The
	feature tree node must be passed in as the object into the query function.

	This function is guaranteed to return the minimum installation level of all
	features referenced by the feature tree node.

	@since 2.0
	*/
	kylaFeatureTreeProperty_InstallationLevel = 3
};

struct KylaInstaller
{
	/**
	Set the log callback. The callbackContext will be passed on into the
	log callback function.
	*/
	int (*SetLogCallback)(KylaInstaller* installer,
		KylaLogCallback logCallback, void* callbackContext);

	/**
	Set the progress callback. The callbackContext will be passed on into the
	progress callback function.
	*/
	int (*SetProgressCallback)(KylaInstaller* installer,
		KylaProgressCallback, void* progressContext);

	/**
	Set the validation callback. The callbackContext will be passed on into the
	validation callback function.
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
	Set a repository property.

	The propertyId must be one of enumeration values from
	kylaRepositoryProperty.

	@since 2.0
	*/
	int (*SetRepositoryProperty)(KylaInstaller* installer,
		KylaSourceRepository repository,
		int propertyId,
		size_t propertySize,
		const void* propertyValue);

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
	Get a feature tree property.	
	
	The propertyId must be one of enumeration values from
	kylaFeatureTreeProperty. If resultSize is non-null, and result is null, the result
	size is written into resultSize. If resultSize and result are both non-null, the
	result is written into result if the resultSize is greater than or equal to the
	required size. The actually written size will be stored in resultSize in this case.

	The result size must be always provided.

	@since 2.0
	*/
	int (*GetFeatureTreeProperty)(KylaInstaller* installer,
		KylaSourceRepository repository,
		int propertyId,
		const void* object,
		size_t* resultSize, void* result);

	/**
	Execute an action on the target repository.

	Most actions require a desired state.
	*/
	int (*Execute)(KylaInstaller* installer, kylaAction action,
		KylaTargetRepository target, KylaSourceRepository source,
		const KylaDesiredState* desiredState);
};

#define KYLA_MAKE_API_VERSION(major,minor,patch) (major << 22 | minor << 12 | patch)
#define KYLA_API_VERSION_2_0 KYLA_MAKE_API_VERSION(2,0,0)

/**
Create a new installer. Installer must be non-null, and kylaApiVersion must be
a supported version created using either KYLA_MAKE_API_VERSION or by using one
of the pre-defined constants like KYLA_API_VERSION_1_0.
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
