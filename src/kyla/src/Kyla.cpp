/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#include "Kyla.h"

#include "Exception.h"

#include "Repository.h"

#include "Log.h"

#define KYLA_C_API_BEGIN() try {
#define KYLA_C_API_END() } catch (const kyla::RuntimeException& e) {    	  \
		if (installer) {													  \
			GetInternalInstaller (installer)								  \
				->log->Error (e.GetSource (), e.what ());					  \
		}																	  \
		return kylaResult_Error;										      \
	} catch (const std::exception& e) {										  \
		if (installer) {													  \
			GetInternalInstaller (installer)								  \
				->log->Error ("Unknown", e.what ());						  \
		}																	  \
		return kylaResult_Error;										      \
	} catch (...) {															  \
			return kylaResult_Error;										  \
	}

///////////////////////////////////////////////////////////////////////////////
struct KylaRepositoryImpl
{
	std::unique_ptr<kyla::Repository> p;

	enum class RepositoryType
	{
		Source, Target
	} repositoryType;

	kyla::Path path;
	int options = 0;
};

namespace {
struct KylaInstaller_2_0
{
	int (*SetLogCallback)(KylaInstaller* installer,
		KylaLogCallback logCallback, void* callbackContext);

	int (*SetProgressCallback)(KylaInstaller* installer,
		KylaProgressCallback, void* progressContext);

	int (*SetValidationCallback)(KylaInstaller* installer,
		KylaValidationCallback validationCallback, void* validationContext);

	int (*OpenSourceRepository)(KylaInstaller* installer, const char* path,
		int options, KylaSourceRepository* repository);

	int (*OpenTargetRepository)(KylaInstaller* installer, const char* path,
		int options, KylaTargetRepository* repository);

	int (*CloseRepository)(KylaInstaller* installer,
		KylaRepository impl);

	int (*GetRepositoryProperty)(KylaInstaller* installer,
		KylaSourceRepository repository,
		int propertyId,
		size_t* resultSize,
		void* result);

	int (*GetFeatureProperty)(KylaInstaller* installer,
		KylaSourceRepository repository,
		struct KylaUuid id,
		int propertyId,
		size_t* resultSize,
		void* result);

	int (*Execute)(KylaInstaller* installer, kylaAction action,
		KylaTargetRepository target, KylaSourceRepository source,
		const KylaDesiredState* desiredState);
};

///////////////////////////////////////////////////////////////////////////////
struct KylaInstallerInternal
{
	KylaValidationCallback validationCallback = nullptr;
	void* validationCallbackContext = nullptr;
	std::unique_ptr<kyla::Log> log;
	std::unique_ptr<kyla::Progress> progress;

	KylaInstallerInternal ()
		: log (new kyla::Log ([](kyla::LogLevel, const char*, const char*) -> void {
	}))
		, progress (new kyla::Progress ([](const float totalProgress,
			const char* stageName, const char* action) -> void {
	}))
	{
	}
};

KylaInstallerInternal* GetInternalInstaller (KylaInstaller* installer)
{
	return *reinterpret_cast<KylaInstallerInternal**> (reinterpret_cast<intptr_t>(installer) - sizeof (KylaInstallerInternal*));
}

///////////////////////////////////////////////////////////////////////////////
int kylaOpenSourceRepository_2_0 (
	KylaInstaller* installer,
	const char* path, int options,
	KylaSourceRepository* repository)
{
	KYLA_C_API_BEGIN ()

	if (installer == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	auto internal = GetInternalInstaller (installer);

	if (path == nullptr) {
		internal->log->Error ("kylaOpenSourceRepository", "path was null");
		return kylaResult_ErrorInvalidArgument;
	}

	if (repository == nullptr) {
		internal->log->Error ("kylaOpenSourceRepository", "repository was null");
		return kylaResult_ErrorInvalidArgument;
	}

	if ((options & kylaRepositoryOption_Create) == kylaRepositoryOption_Create) {
		internal->log->Error ("kylaOpenSourceRepository", "Cannot create source repository with "
			"kylaRepositoryOption_Create");
		return kylaResult_ErrorInvalidArgument;
	}

	KylaSourceRepository repo = new KylaRepositoryImpl;
	repo->p = kyla::OpenRepository (path, false);
	repo->repositoryType = KylaRepositoryImpl::RepositoryType::Source;

	*repository = repo;

	return kylaResult_Ok;

	KYLA_C_API_END ()
}

///////////////////////////////////////////////////////////////////////////////
int kylaOpenTargetRepository_2_0 (
	KylaInstaller* installer,
	const char* path, int options,
	KylaTargetRepository* repository)
{
	KYLA_C_API_BEGIN ()

	if (installer == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	auto internal = GetInternalInstaller (installer);

	if (path == nullptr) {
		internal->log->Error ("kylaOpenTargetRepository", "path was null");
		return kylaResult_ErrorInvalidArgument;
	}

	if (repository == nullptr) {
		internal->log->Error ("kylaOpenTargetRepository", "repository was null");
		return kylaResult_ErrorInvalidArgument;
	}

	KylaTargetRepository repo = new KylaRepositoryImpl;
	repo->repositoryType = KylaRepositoryImpl::RepositoryType::Target;
	repo->path = path;

	// If create is not set, we're opening it right away
	if ((options & kylaRepositoryOption_Create) == 0) {
		repo->p = kyla::OpenRepository (path,
			(options & kylaRepositoryOption_ReadOnly) != 1);
	}

	*repository = repo;

	return kylaResult_Ok;

	KYLA_C_API_END ()
}

///////////////////////////////////////////////////////////////////////////////
int kylaCloseRepository_2_0 (KylaInstaller* installer,
	KylaRepository repository)
{
	KYLA_C_API_BEGIN ()

	if (installer == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	auto internal = GetInternalInstaller (installer);

	if (repository == nullptr) {
		internal->log->Error ("kylaCloseRepository", "repository was null");
		return kylaResult_ErrorInvalidArgument;
	}

	if (!repository->p) {
		internal->log->Error ("kylaCloseRepository", "repository is already closed");
		return kylaResult_ErrorInvalidArgument;
	}

	repository->p.reset ();

	delete repository;

	return kylaResult_Ok;

	KYLA_C_API_END ()
}

///////////////////////////////////////////////////////////////////////////////
int kylaExecute_2_0 (
	KylaInstaller* installer,
	kylaAction action,
	KylaTargetRepository targetRepository,
	KylaSourceRepository sourceRepository,
	const KylaDesiredState* desiredState)
{
	KYLA_C_API_BEGIN ()

	if (installer == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	auto internal = GetInternalInstaller (installer);

	if (targetRepository == nullptr) {
		internal->log->Error ("kylaExecute", "target repository was null");
		return kylaResult_ErrorInvalidArgument;
	}

	if (targetRepository->repositoryType != KylaRepositoryImpl::RepositoryType::Target) {
		internal->log->Error ("kylaExecute", "target repository has is not a valid target. "
			"A target repository must be opened using OpenTargetRepository.");
		return kylaResult_ErrorInvalidArgument;
	}

	if (action != kylaAction_Verify) {
		if (sourceRepository == nullptr) {
			return kylaResult_ErrorInvalidArgument;
		}

		if (sourceRepository->repositoryType != KylaRepositoryImpl::RepositoryType::Source) {
			internal->log->Error ("kylaExecute", "source repository has is not a valid source. "
				"A source repository must be opened using OpenSourceRepository.");
			return kylaResult_ErrorInvalidArgument;
		}
	}

	std::vector<kyla::Uuid> filesetIds;

	if (desiredState == nullptr) {
		switch (action) {
		case kylaAction_Configure:
		case kylaAction_Install:
			internal->log->Error ("kylaExecute",
				"desired state must not be null for kylaAction_Configure and kylaAction_Install");
			return kylaResult_ErrorInvalidArgument;
		}
	} else {
		if (desiredState->filesetCount <= 0) {
			internal->log->Error ("kylaExecute",
				"desired state file set count must be greater than or equal to 1");
			return kylaResult_ErrorInvalidArgument;
		}

		if (desiredState->filesetIds == nullptr) {
			internal->log->Error ("kylaExecute",
				"desired state must contain at least one file set id");
			return kylaResult_ErrorInvalidArgument;
		}

		for (int i = 0; i < desiredState->filesetCount; ++i) {
			if (desiredState->filesetIds [i] == nullptr) {
				internal->log->Error ("kylaExecute",
					"desired state file set id must not be null");
				return kylaResult_ErrorInvalidArgument;
			}
		}

		filesetIds.resize (desiredState->filesetCount);
		for (int i = 0; i < desiredState->filesetCount; ++i) {
			filesetIds [i] = kyla::Uuid{ desiredState->filesetIds [i] };
		}
	}

	kyla::Repository::ExecutionContext executionContext {
		*internal->log, *internal->progress };

	switch (action) {
	case kylaAction_Install:
		targetRepository->p = kyla::DeployRepository (*sourceRepository->p,
			targetRepository->path.string ().c_str (), filesetIds,
			executionContext);
		break;

	case kylaAction_Configure:
		if ((targetRepository->options & kylaRepositoryOption_ReadOnly) == kylaRepositoryOption_ReadOnly) {
			internal->log->Error ("kylaExecute",
				"target repository cannot be opened in read-only mode for kylaAction_Configure");
			return kylaResult_Error;
		}
		targetRepository->p->Configure (
			*sourceRepository->p, filesetIds, executionContext);

		break;

	case kylaAction_Repair:
		if ((targetRepository->options & kylaRepositoryOption_ReadOnly) == kylaRepositoryOption_ReadOnly) {
			internal->log->Error ("kylaExecute",
				"target repository cannot be opened in read-only mode for kylaAction_Repair");
			return kylaResult_Error;
		}

		///@TODO(minor) Pass through the fileset ids
		targetRepository->p->Repair (*sourceRepository->p, executionContext);

		break;

	case kylaAction_Verify:
		targetRepository->p = kyla::OpenRepository (
			targetRepository->path.string ().c_str (),
			(targetRepository->options & kylaRepositoryOption_ReadOnly) == kylaRepositoryOption_ReadOnly);

		///@TODO(minor) Pass through the source file set and fileset ids
		targetRepository->p->Validate ([&](const kyla::SHA256Digest& object,
			const char* path, const kyla::ValidationResult result) -> void {
			kylaValidationItemInfo info;

			info.filename = path;

			if (internal->validationCallback) {
				internal->validationCallback (
					static_cast<kylaValidationResult> (result),
					&info,
					internal->validationCallbackContext);
			}
		}, executionContext);

		break;

	default:
		internal->log->Error ("kylaExecute",
			"invalid action");
		return kylaResult_ErrorInvalidArgument;
	}

	return kylaResult_Ok;

	KYLA_C_API_END ()
}

///////////////////////////////////////////////////////////////////////////////
template <typename T>
int KylaGet (const T& value,
	size_t* pResultSize, void* pResult,
	kyla::Log& log, const char* logSource)
{
	const auto resultSize = sizeof (T);

	if (pResultSize && !pResult) {
		*pResultSize = resultSize;
	} else if (pResultSize && pResult) {
		if (*pResultSize < resultSize) {
			log.Error (logSource, "result size is too small");
			return kylaResult_ErrorInvalidArgument;
		} else {
			*pResultSize = resultSize;
		}

		::memcpy (pResult, &value, resultSize);
	} else {
		log.Error (logSource, "at least one of {result size, result pointer} must be set");
		return kylaResult_ErrorInvalidArgument;
	}

	return kylaResult_Ok;
}

///////////////////////////////////////////////////////////////////////////////
template <typename T>
int KylaGet (const std::vector<T>& value,
	size_t* pResultSize, void* pResult,
	kyla::Log& log, const char* logSource)
{
	const auto resultSize = static_cast<int> (value.size ()) * sizeof (T);

	if (pResultSize && !pResult) {
		*pResultSize = resultSize;
	} else if (pResultSize && pResult) {
		if (*pResultSize < resultSize) {
			log.Error (logSource, "result size is too small");
			return kylaResult_ErrorInvalidArgument;
		} else {
			*pResultSize = resultSize;
		}

		::memcpy (pResult, value.data (), resultSize);
	} else {
		log.Error (logSource, "at least one of {result size, result pointer} must be set");
		return kylaResult_ErrorInvalidArgument;
	}

	return kylaResult_Ok;
}

///////////////////////////////////////////////////////////////////////////////
/**
For strings, we need to set the terminating 0 as well, so we need special
case handling.
*/
int KylaGet (const std::string& value,
	size_t* pResultSize, void* pResult,
	kyla::Log& log, const char* logSource)
{
	const auto resultSize = static_cast<int> (value.size ()) + 1;

	if (pResultSize && !pResult) {
		*pResultSize = resultSize;
	} else if (pResultSize && pResult) {
		if (*pResultSize < resultSize) {
			log.Error (logSource, "result size is too small");
			return kylaResult_ErrorInvalidArgument;
		} else {
			*pResultSize = resultSize;
		}

		::memset (pResult, resultSize, 0);
		::memcpy (pResult, value.data (), resultSize - 1);
	} else {
		log.Error (logSource, "at least one of {result size, result pointer} must be set");
		return kylaResult_ErrorInvalidArgument;
	}

	return kylaResult_Ok;
}

///////////////////////////////////////////////////////////////////////////////
int kylaGetRepositoryProperty_2_0 (KylaInstaller* installer,
	KylaSourceRepository repository,
	int propertyId,
	size_t* pResultSize,
	void* pResult)
{
	KYLA_C_API_BEGIN ()

	if (installer == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	auto internal = GetInternalInstaller (installer);

	if (repository == nullptr) {
		internal->log->Error ("kylaGetRepositoryProperty", "repository was null");

		return kylaResult_ErrorInvalidArgument;
	}

	if (repository->repositoryType != KylaRepositoryImpl::RepositoryType::Source) {
		internal->log->Error ("kylaGetRepositoryProperty", "repository must be a source repository");
		return kylaResult_ErrorInvalidArgument;
	}

	switch (propertyId) {
	case kylaRepositoryProperty_AvailableFeatures:
	{
		///@TODO(minor) implement this	}
	}

	case kylaRepositoryProperty_IsEncrypted:
	{
		int value = repository->p->IsEncrypted ();

		return KylaGet (value,
			pResultSize, pResult, *internal->log, "kylaGetRepositoryProperty");
	}

	default:
		internal->log->Error ("kylaQueryRepository", "invalid property id");
		return kylaResult_ErrorInvalidArgument;
	}

	return kylaResult_Ok;

	KYLA_C_API_END ()
}

///////////////////////////////////////////////////////////////////////////////
int kylaSetRepositoryProperty_1_1 (KylaInstaller* installer,
	KylaSourceRepository repository,
	int propertyId,
	size_t propertySize,
	const void* propertyValue)
{
	KYLA_C_API_BEGIN ()

	if (installer == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	auto internal = GetInternalInstaller (installer);

	if (repository == nullptr) {
		internal->log->Error ("kylaQueryRepository", "repository was null");

		return kylaResult_ErrorInvalidArgument;
	}

	if (repository->repositoryType != KylaRepositoryImpl::RepositoryType::Source) {
		internal->log->Error ("kylaQueryRepository", "repository must be a source repository");
		return kylaResult_ErrorInvalidArgument;
	}

	switch (propertyId) {
	case kylaRepositoryProperty_AvailableFeatures:
	{
		internal->log->Error ("kylaSetRepositoryProperty",
			"Cannot set read-only property 'AvailableFilesets'");
		return kylaResult_ErrorInvalidArgument;
	}

	case kylaRepositoryProperty_IsEncrypted:
	{
		internal->log->Error ("kylaSetRepositoryProperty",
			"Cannot set read-only property 'IsEncrypted'");
		return kylaResult_ErrorInvalidArgument;
	}

	default:
		internal->log->Error ("kylaSetRepositoryProperty", "invalid property id");
		return kylaResult_ErrorInvalidArgument;
	}

	return kylaResult_Ok;

	KYLA_C_API_END ()
}

///////////////////////////////////////////////////////////////////////////////
int kylaQueryFileset_1_0 (KylaInstaller* installer,
	KylaSourceRepository repository,
	struct KylaUuid id,
	int propertyId,
	size_t* pResultSize,
	void* pResult)
{
	KYLA_C_API_BEGIN ()

	if (installer == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	auto internal = GetInternalInstaller (installer);

	if (repository == nullptr) {
		internal->log->Error ("kylaQueryFileset", "repository was null");
		return kylaResult_ErrorInvalidArgument;
	}

	if (repository->repositoryType != KylaRepositoryImpl::RepositoryType::Source) {
		internal->log->Error ("kylaQueryFileset", 
			"repository must be a source repository");
		return kylaResult_ErrorInvalidArgument;
	}

	const kyla::Uuid uuid{ id.bytes };

	switch (propertyId) {
	case kylaFeatureProperty_Size:
	{
		return KylaGet (repository->p->GetFilesetSize (uuid),
			pResultSize, pResult, *internal->log,
			"kylaQueryFileset");
	}
	default:
		internal->log->Error ("kylaQueryFileset", "invalid property id");
		return kylaResult_ErrorInvalidArgument;
	}

	return kylaResult_Ok;

	KYLA_C_API_END ()
}

///////////////////////////////////////////////////////////////////////////////
int kylaSetLogCallback_2_0 (KylaInstaller* installer, KylaLogCallback logCallback, 
	void* callbackContext)
{
	KYLA_C_API_BEGIN ()

	if (installer == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	auto internal = GetInternalInstaller (installer);

	internal->log->SetCallback (
		[=](kyla::LogLevel level, const char* source, const char* message) -> void {
		kylaLogSeverity severity;
		switch (level) {
		case kyla::LogLevel::Debug:
			severity = kylaLogSeverity_Debug; break;

		case kyla::LogLevel::Warning:
			severity = kylaLogSeverity_Warning; break;

		case kyla::LogLevel::Info:
			severity = kylaLogSeverity_Warning; break;

		case kyla::LogLevel::Error:
			severity = kylaLogSeverity_Error; break;

		default:
			severity = kylaLogSeverity_Debug; break;
		}

		logCallback (source, severity, message, callbackContext);
	});

	return kylaResult_Ok;

	KYLA_C_API_END ()
}

///////////////////////////////////////////////////////////////////////////////
int kylaSetProgressCallback_2_0 (KylaInstaller* installer, 
	KylaProgressCallback progressCallback, void* callbackContext)
{
	KYLA_C_API_BEGIN ()

	if (installer == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	auto internal = GetInternalInstaller (installer);

	internal->progress.reset (new kyla::Progress ([=](
		const float f, const char* s, const char* a) -> void {
		KylaProgress progress;
		progress.detailMessage = a;
		progress.action = s;
		progress.totalProgress = f;

		progressCallback (&progress, callbackContext);
	}));

	return kylaResult_Ok;

	KYLA_C_API_END ()
}

///////////////////////////////////////////////////////////////////////////////
int kylaSetValidationCallback_2_0 (KylaInstaller* installer, 
	KylaValidationCallback validationCallback, void* callbackContext)
{
	KYLA_C_API_BEGIN ()

	if (installer == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	auto internal = GetInternalInstaller (installer);

	internal->validationCallback = validationCallback;
	internal->validationCallbackContext = callbackContext;

	return kylaResult_Ok;

	KYLA_C_API_END ()
};
}

///////////////////////////////////////////////////////////////////////////////
int kylaCreateInstaller (int kylaApiVersion, KylaInstaller** pInstaller)
{
	// Again, this is for the C_API macros
	KylaInstaller* installer = nullptr;

	KYLA_C_API_BEGIN ()

	if (pInstaller == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	if (kylaApiVersion < KYLA_API_VERSION_2_0 || kylaApiVersion > KYLA_API_VERSION_2_0) {
		return kylaResult_ErrorUnsupportedApiVersion;
	}
	
	switch (kylaApiVersion) {
	case KYLA_API_VERSION_2_0:
	{
		static_assert (alignof (void*) >= alignof (KylaInstaller_2_0),
			"Function table must not require larger alignment than a pointer.");
		auto bundle = malloc (sizeof (KylaInstallerInternal*) + sizeof (KylaInstaller_2_0));

		auto p = static_cast<unsigned char*> (bundle);
		KylaInstaller_2_0* installer = static_cast<KylaInstaller_2_0*> (static_cast<void*> (p + sizeof (void*)));
		KylaInstallerInternal** internal = static_cast<KylaInstallerInternal**> (bundle);

		*internal = new KylaInstallerInternal;

		installer->CloseRepository = kylaCloseRepository_2_0;
		installer->Execute = kylaExecute_2_0;
		installer->OpenSourceRepository = kylaOpenSourceRepository_2_0;
		installer->OpenTargetRepository = kylaOpenTargetRepository_2_0;
		installer->GetRepositoryProperty = kylaGetRepositoryProperty_2_0;
		installer->GetFeatureProperty = nullptr;
		installer->SetLogCallback = kylaSetLogCallback_2_0;
		installer->SetProgressCallback = kylaSetProgressCallback_2_0;
		installer->SetValidationCallback = kylaSetValidationCallback_2_0;
		
		*reinterpret_cast<void**>(pInstaller) = installer;

		break;
	}
	}

	return kylaResult_Ok;

	KYLA_C_API_END ()
}

///////////////////////////////////////////////////////////////////////////////
int kylaDestroyInstaller (KylaInstaller* installer)
{
	KYLA_C_API_BEGIN ()

	if (installer == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	auto internal = GetInternalInstaller (installer);
	delete internal;
	free (
		static_cast<unsigned char*> (static_cast<void*> (installer))
			- sizeof (KylaInstallerInternal*)
	);

	return kylaResult_Ok;

	KYLA_C_API_END ()
}
