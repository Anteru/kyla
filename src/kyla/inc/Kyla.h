/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
[LICENSE END]
*/

#ifndef KYLA_PUBLIC_API_H
#define KYLA_PUBLIC_API_H

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
	/**
	Create the repository. If it's present already, it will be overriden.

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

struct KylaFilesetInfo
{
	uint8_t id [16];
	int64_t fileCount;
	int64_t fileSize;
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
	int filesetCount;
	const uint8_t* const* filesetIds;
};

struct KylaUuid
{
	uint8_t bytes [16];
};

enum kylaRepositoryProperty
{
	kylaRepositoryProperty_AvailableFilesets
};

enum kylaFilesetProperty
{
	/**
	The name of the fileset, as a null-terminated, UTF8 encoded string.
	*/
	kylaFilesetProperty_Name,

	/**
	The size of the file set when deployed, stored in an int64_t.
	*/
	kylaFilesetProperty_Size,

	/**
	The number of files when deployed, stored in an int64_t.
	*/
	kylaFilesetProperty_FileCount
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
	*/
	int (*OpenSourceRepository)(KylaInstaller* installer, const char* path,
		int options, KylaSourceRepository* repository);

	/**
	Open a target repository.

	The options is a combination of kylaRepositoryOption. By default, it's
	opened for writing (and assumed to exist already).
	*/
	int (*OpenTargetRepository)(KylaInstaller* installer, const char* path,
		int options, KylaTargetRepository* repository);
	
	/**
	Close a source or target repository.
	*/
	int (*CloseRepository)(KylaInstaller* installer,
		KylaRepository impl);

	/**
	Query a repository property.

	The propertyId must be one of enumeration values from 
	kylaRepositoryProperty. If resultSize is provided, the size of the result
	is written into it. If result is provided, the result is written into it.
	If result is not null, resultSize must be set to the size of the buffer
	result points to.
	*/
	int (*QueryRepository)(KylaInstaller* installer,
		KylaSourceRepository repository,
		int propertyId,
		size_t* resultSize,
		void* result);

	/**
	Query a file set property.

	The propertyId must be one of enumeration values from
	kylaFilesetProperty. If resultSize is provided, the size of the result
	is written into it. If result is provided, the result is written into it.
	If result is not null, resultSize must be set to the size of the buffer
	result points to.
	*/
	int (*QueryFileset)(KylaInstaller* installer,
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

#define KYLA_MAKE_API_VERSION(major,minor,patch) (major << 22 | minor << 12 | patch);
#define KYLA_API_VERSION_1_0 (1<<22)

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
