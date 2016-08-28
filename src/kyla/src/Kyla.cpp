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
#include "RepositoryBuilder.h"

#include "Log.h"

#define KYLA_C_API_BEGIN() try {
#define KYLA_C_API_END() } catch (const kyla::RuntimeException& e) {    	  \
		if (installer) {													  \
			static_cast<KylaInstallerInternal*> (installer)					  \
				->log->Error (e.GetSource (), e.what ());					  \
		}																	  \
		return kylaResult_Error;										      \
	} catch (const std::exception& e) {										  \
		if (installer) {													  \
			static_cast<KylaInstallerInternal*> (installer)					  \
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
///////////////////////////////////////////////////////////////////////////////
struct KylaInstallerInternal : public KylaInstaller
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

///////////////////////////////////////////////////////////////////////////////
int kylaOpenSourceRepository (
	KylaInstaller* installer,
	const char* path, int options,
	KylaSourceRepository* repository)
{
	KYLA_C_API_BEGIN ()

	if (installer == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	auto internal = static_cast<KylaInstallerInternal*> (installer);

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
int kylaOpenTargetRepository (
	KylaInstaller* installer,
	const char* path, int options,
	KylaTargetRepository* repository)
{
	KYLA_C_API_BEGIN ()

	if (installer == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	auto internal = static_cast<KylaInstallerInternal*> (installer);

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
int kylaCloseRepository (KylaInstaller* installer,
	KylaRepository repository)
{
	KYLA_C_API_BEGIN ()

	if (installer == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	auto internal = static_cast<KylaInstallerInternal*> (installer);

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
int kylaExecute (
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

	auto internal = static_cast<KylaInstallerInternal*> (installer);

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
int kylaQueryRepository (KylaInstaller* installer,
	KylaSourceRepository repository,
	int propertyId,
	size_t* pResultSize,
	void* pResult)
{
	KYLA_C_API_BEGIN ()

	if (installer == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	auto i = static_cast<KylaInstallerInternal*> (installer);

	if (repository == nullptr) {
		i->log->Error ("kylaQueryRepository", "repository was null");

		return kylaResult_ErrorInvalidArgument;
	}

	if (repository->repositoryType != KylaRepositoryImpl::RepositoryType::Source) {
		i->log->Error ("kylaQueryRepository", "repository must be a source repository");
		return kylaResult_ErrorInvalidArgument;
	}

	switch (propertyId) {
	case kylaRepositoryProperty_AvailableFilesets:
	{
		const auto result = repository->p->GetFilesets ();
		const auto resultSize = static_cast<int> (result.size ()) * sizeof (KylaUuid);

		if (pResultSize && !pResult) {
			*pResultSize = resultSize;
		} else if (pResultSize && pResult) {
			if (*pResultSize < resultSize) {
				i->log->Error ("kylaQueryRepository", "result size is too small");
				return kylaResult_ErrorInvalidArgument;
			} else {
				*pResultSize = resultSize;
			}

			::memcpy (pResult, result.data (), resultSize);
		} else {
			i->log->Error ("kylaQueryRepository", "at least one of {result size, result pointer} must be set");
			return kylaResult_ErrorInvalidArgument;
		}

		break;
	}

	default:
		i->log->Error ("kylaQueryRepository", "invalid property id");
		return kylaResult_ErrorInvalidArgument;
	}

	return kylaResult_Ok;

	KYLA_C_API_END ()
}

///////////////////////////////////////////////////////////////////////////////
int kylaQueryFileset (KylaInstaller* installer,
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

	auto i = static_cast<KylaInstallerInternal*> (installer);

	if (repository == nullptr) {
		i->log->Error ("kylaQueryFileset", "repository was null");
		return kylaResult_ErrorInvalidArgument;
	}

	if (repository->repositoryType != KylaRepositoryImpl::RepositoryType::Source) {
		i->log->Error ("kylaQueryFileset", "repository must be a source repository");
		return kylaResult_ErrorInvalidArgument;
	}

	const kyla::Uuid uuid{ id.bytes };
	
	switch (propertyId) {
	case kylaFilesetProperty_FileCount:
	{
		const auto resultSize = sizeof (std::int64_t);
		if (pResultSize && !pResult) {
			*pResultSize = resultSize;
		} else if (pResultSize && pResult) {
			if (*pResultSize < resultSize) {
				i->log->Error ("kylaQueryFileset", "result size is too small");
				return kylaResult_ErrorInvalidArgument;
			} else {
				*pResultSize = resultSize;
			}

			*static_cast<std::int64_t*> (pResult) =
				repository->p->GetFilesetFileCount (uuid);
		} else {
			i->log->Error ("kylaQueryFileset", "at least one of {result size, result pointer} must be set");
			return kylaResult_ErrorInvalidArgument;
		}

		break;
	}
	case kylaFilesetProperty_Size:
	{
		const auto resultSize = sizeof (std::int64_t);
		if (pResultSize && !pResult) {
			*pResultSize = resultSize;
		} else if (pResultSize && pResult) {
			if (*pResultSize < resultSize) {
				i->log->Error ("kylaQueryFileset", "result size is too small");
				return kylaResult_ErrorInvalidArgument;
			} else {
				*pResultSize = resultSize;
			}

			*static_cast<std::int64_t*> (pResult) =
				repository->p->GetFilesetSize (uuid);
		} else {
			i->log->Error ("kylaQueryFileset", "at least one of {result size, result pointer} must be set");
			return kylaResult_ErrorInvalidArgument;
		}

		break;
	}
	case kylaFilesetProperty_Name:
	{
		const auto name = repository->p->GetFilesetName (uuid);
		const auto resultSize = name.size () + 1;

		if (pResultSize && !pResult) {
			*pResultSize = resultSize;
		} else if (pResultSize && pResult) {
			if (*pResultSize < resultSize) {
				i->log->Error ("kylaQueryFileset", "result size is too small");
				return kylaResult_ErrorInvalidArgument;
			} else {
				*pResultSize = resultSize;
			}
			
			::memset (pResult, 0, name.size () + 1);
			::memcpy (pResult, name.data (), name.size ());
		} else {
			i->log->Error ("kylaQueryFileset", "at least one of {result size, result pointer} must be set");
			return kylaResult_ErrorInvalidArgument;
		}

		break;
	}

	default:
		i->log->Error ("kylaQueryFileset", "invalid property id");
		return kylaResult_ErrorInvalidArgument;
	}

	return kylaResult_Ok;

	KYLA_C_API_END ()
}
}

///////////////////////////////////////////////////////////////////////////////
KYLA_EXPORT int kylaBuildRepository (const char* descriptorFile,
	const char* sourceDirectory, const char* targetDirectory)
{
	// Only needed for the C_API macros which assume we're int the normal
	// installer
	void* installer = nullptr;

	KYLA_C_API_BEGIN ()

	if (descriptorFile == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	if (sourceDirectory == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	if (targetDirectory == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	kyla::BuildRepository (descriptorFile,
		sourceDirectory, targetDirectory);

	return kylaResult_Ok;

	KYLA_C_API_END()
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

	if (kylaApiVersion != KYLA_API_VERSION_1_0) {
		return kylaResult_ErrorUnsupportedApiVersion;
	}

	KylaInstallerInternal* internal = new KylaInstallerInternal;

	internal->CloseRepository = kylaCloseRepository;
	internal->Execute = kylaExecute;
	internal->OpenSourceRepository = kylaOpenSourceRepository;
	internal->OpenTargetRepository = kylaOpenTargetRepository;
	internal->QueryRepository = kylaQueryRepository;
	internal->QueryFileset = kylaQueryFileset;
	internal->SetLogCallback =
	[](KylaInstaller* installer, KylaLogCallback logCallback, void* callbackContext) -> int {
		KYLA_C_API_BEGIN ()

		if (installer == nullptr) {
			return kylaResult_ErrorInvalidArgument;
		}

		auto internal = static_cast<KylaInstallerInternal*> (installer);

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
	};
	internal->SetProgressCallback =
		[](KylaInstaller* installer, KylaProgressCallback progressCallback, void* callbackContext) -> int {
		KYLA_C_API_BEGIN ()

		if (installer == nullptr) {
			return kylaResult_ErrorInvalidArgument;
		}

		auto internal = static_cast<KylaInstallerInternal*> (installer);
		
		internal->progress.reset (new kyla::Progress ([=](
			const float f, const char* s, const char* a) -> void {
			KylaProgress progress;
			progress.detailMessage = a;
			progress.action = s;
			progress.totalProgress = f;

			progressCallback (&progress, callbackContext);
		}));

		return kylaResult_Ok;

		KYLA_C_API_END()
	};
	internal->SetValidationCallback =
		[](KylaInstaller* installer, KylaValidationCallback validationCallback, void* callbackContext) -> int {
		KYLA_C_API_BEGIN ()

		if (installer == nullptr) {
			return kylaResult_ErrorInvalidArgument;
		}

		auto internal = static_cast<KylaInstallerInternal*> (installer);
		internal->validationCallback = validationCallback;
		internal->validationCallbackContext = callbackContext;

		return kylaResult_Ok;

		KYLA_C_API_END ()
	};

	*pInstaller = internal;

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

	delete static_cast<KylaInstallerInternal*> (installer);

	return kylaResult_Ok;

	KYLA_C_API_END ()
}
