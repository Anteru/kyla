#include "Kyla.h"

#include "Repository.h"
#include "RepositoryBuilder.h"

#define KYLA_C_API_BEGIN() try {
#define KYLA_C_API_END() } catch (...) { return kylaResult_Error; }

///////////////////////////////////////////////////////////////////////////////
struct KylaRepositoryImpl
{
	std::unique_ptr<kyla::IRepository> p;

	enum class RepositoryType
	{
		Source, Target
	} repositoryType;

	kyla::Path path;
};

namespace {
///////////////////////////////////////////////////////////////////////////////
struct KylaInstallerInternal
{
	KylaValidationCallback validationCallback = nullptr;
	void* validationCallbackContext = nullptr;
	KylaLogCallback logCallback = nullptr;
	void* logCallbackContext = nullptr;
	KylaProgressCallback progressCallback = nullptr;
	void* progressCallbackContext = nullptr;

	KylaInstaller installer;
};

///////////////////////////////////////////////////////////////////////////////
KylaInstallerInternal* GetInstallerInternal (KylaInstaller* installer)
{
	int offset = offsetof (KylaInstallerInternal, installer);

	KylaInstallerInternal* internal =
		reinterpret_cast<KylaInstallerInternal*> (reinterpret_cast<ptrdiff_t> (installer) - offset);

	return internal;
}

///////////////////////////////////////////////////////////////////////////////
int kylaOpenSourceRepository (
	KylaInstaller* installer,
	const char* path, int /* options */,
	KylaSourceRepository* repository)
{
	KYLA_C_API_BEGIN ()

	if (path == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	if (repository == nullptr) {
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
	const char* path, int /* options */,
	KylaTargetRepository* repository)
{
	KYLA_C_API_BEGIN ()

		if (path == nullptr) {
			return kylaResult_ErrorInvalidArgument;
		}

	if (repository == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	KylaTargetRepository repo = new KylaRepositoryImpl;
	repo->repositoryType = KylaRepositoryImpl::RepositoryType::Target;
	repo->path = path;

	*repository = repo;

	return kylaResult_Ok;

	KYLA_C_API_END ()
}

///////////////////////////////////////////////////////////////////////////////
int kylaCloseRepository (KylaInstaller* /* installer */,
	KylaRepository repository)
{
	KYLA_C_API_BEGIN ()

	if (repository == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	if (!repository->p) {
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

	auto internal = GetInstallerInternal (installer);

	if (targetRepository == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	if (targetRepository->repositoryType != KylaRepositoryImpl::RepositoryType::Target) {
		return kylaResult_ErrorInvalidArgument;
	}

	if (sourceRepository == nullptr && action != kylaAction_Verify) {
		return kylaResult_ErrorInvalidArgument;
	}

	std::vector<kyla::Uuid> filesetIds;
	
	if (desiredState == nullptr) {
		switch (action) {
		case kylaAction_Configure:
		case kylaAction_Install:
			return kylaResult_ErrorInvalidArgument;
		}
	}

	if (desiredState) {
		if (desiredState->filesetCount <= 0) {
			return kylaResult_ErrorInvalidArgument;
		}

		if (desiredState->filesetIds == nullptr) {
			return kylaResult_ErrorInvalidArgument;
		}

		for (int i = 0; i < desiredState->filesetCount; ++i) {
			if (desiredState->filesetIds [i] == nullptr) {
				return kylaResult_ErrorInvalidArgument;
			}
		}

		filesetIds.resize (desiredState->filesetCount);
		for (int i = 0; i < desiredState->filesetCount; ++i) {
			filesetIds [i] = kyla::Uuid{ desiredState->filesetIds [i] };
		}
	}

	switch (action) {
	case kylaAction_Install:
		targetRepository->p = kyla::DeployRepository (*sourceRepository->p,
			targetRepository->path.string ().c_str (), filesetIds);
		break;

	case kylaAction_Configure:
		targetRepository->p = kyla::OpenRepository (
			targetRepository->path.string ().c_str (), true);
		targetRepository->p->Configure (
			*sourceRepository->p, filesetIds);

		break;

	case kylaAction_Repair:
		targetRepository->p = kyla::OpenRepository (
			targetRepository->path.string ().c_str (), true);

		///@TODO(minor) Pass through the fileset ids
		targetRepository->p->Repair (*sourceRepository->p);

		break;

	case kylaAction_Verify:
		targetRepository->p = kyla::OpenRepository (
			targetRepository->path.string ().c_str (), false);

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
		});

		break;

	default:
		return kylaResult_ErrorInvalidArgument;
	}

	return kylaResult_Ok;

	KYLA_C_API_END ()
}

///////////////////////////////////////////////////////////////////////////////
int kylaQueryFilesets (KylaInstaller*,
	KylaSourceRepository repository,
	int* pFilesetCount, KylaFilesetInfo* pFilesetInfos)
{
	KYLA_C_API_BEGIN ()

	if (repository == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	const auto result = repository->p->GetFilesetInfos ();

	if (pFilesetCount) {
		auto resultSize = static_cast<int> (result.size ());
		if (resultSize < 0) {
			// overflow
			return kylaResult_ErrorInvalidArgument;
		}

		*pFilesetCount = resultSize;
	}

	if (pFilesetInfos) {
		for (const auto item : result) {
			pFilesetInfos->fileCount = item.fileCount;
			pFilesetInfos->fileSize = item.fileSize;
			::memcpy (pFilesetInfos->id, item.id.GetData (), sizeof (pFilesetInfos->id));

			++pFilesetInfos;
		}
	}

	return kylaResult_Ok;

	KYLA_C_API_END ()
}

///////////////////////////////////////////////////////////////////////////////
int kylaQueryFilesetName (KylaInstaller*,
	KylaSourceRepository repository,
	const uint8_t* pId,
	int* pLength,
	char* pResult)
{
	KYLA_C_API_BEGIN ()

	if (repository == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	const kyla::Uuid uuid{ pId };
	const auto result = repository->p->GetFilesetName (uuid);

	if (pLength) {
		auto resultSize = static_cast<int> (result.size () + 1 /* trailing zero */);
		if (resultSize < 0) {
			// overflow
			return kylaResult_ErrorInvalidArgument;
		}

		*pLength = resultSize;
	}

	if (pResult) {
		::memcpy (pResult, result.c_str (), result.size () + 1);
	}

	return kylaResult_Ok;

	KYLA_C_API_END ()
}
}

///////////////////////////////////////////////////////////////////////////////
KYLA_EXPORT int kylaBuildRepository (const char* descriptorFile,
	const char* sourceDirectory, const char* targetDirectory)
{
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
int kylaCreateInstaller (int /* kylaApiVersion */, KylaInstaller** installer)
{
	KYLA_C_API_BEGIN ()

	if (installer == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	KylaInstallerInternal* internal = new KylaInstallerInternal;
	*installer = &internal->installer;

	internal->installer.CloseRepository = kylaCloseRepository;
	internal->installer.Execute = kylaExecute;
	internal->installer.OpenSourceRepository = kylaOpenSourceRepository;
	internal->installer.OpenTargetRepository = kylaOpenTargetRepository;
	internal->installer.QueryFilesetName = kylaQueryFilesetName;
	internal->installer.QueryFilesets = kylaQueryFilesets;
	internal->installer.SetLogCallback = 
	[](KylaInstaller* installer, KylaLogCallback logCallback, void* callbackContext) -> int {
		KYLA_C_API_BEGIN ()

		auto internal = GetInstallerInternal (installer);
		internal->logCallback = logCallback;
		internal->logCallbackContext = callbackContext;

		return kylaResult_Ok;

		KYLA_C_API_END ()
	}; 
	internal->installer.SetProgressCallback =
		[](KylaInstaller* installer, KylaProgressCallback progressCallback, void* callbackContext) -> int {
		KYLA_C_API_BEGIN ()
			
		auto internal = GetInstallerInternal (installer);
		internal->progressCallback = progressCallback;
		internal->progressCallbackContext = callbackContext;

		return kylaResult_Ok;

		KYLA_C_API_END()
	};
	internal->installer.SetValidationCallback =
		[](KylaInstaller* installer, KylaValidationCallback validationCallback, void* callbackContext) -> int {
		KYLA_C_API_BEGIN ()

		auto internal = GetInstallerInternal (installer);
		internal->validationCallback = validationCallback;
		internal->validationCallbackContext = callbackContext;

		return kylaResult_Ok;

		KYLA_C_API_END ()
	};

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

	delete GetInstallerInternal (installer);

	return kylaResult_Ok;

	KYLA_C_API_END ()
}
