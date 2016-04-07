#include "Kyla.h"

#include "Repository.h"
#include "RepositoryBuilder.h"

#define KYLA_C_API_BEGIN() try {
#define KYLA_C_API_END() } catch (...) { return kylaResult_Error; }

struct kylaRepositoryImpl
{
	std::unique_ptr<kyla::IRepository> p;
};

///////////////////////////////////////////////////////////////////////////////
int kylaBuildRepository (const char* descriptorFile,
	const kylaBuildEnvironment* environment)
{
	KYLA_C_API_BEGIN ()

	if (descriptorFile == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	if (environment == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	kyla::BuildRepository (descriptorFile,
		environment);

	return kylaResult_Ok;

	KYLA_C_API_END()
}

///////////////////////////////////////////////////////////////////////////////
int kylaValidateRepository (kylaRepository repository,
	KylaValidateCallback validationCallback,
	void* callbackContext)
{
	KYLA_C_API_BEGIN ()

	if (repository == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	if (validationCallback == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	repository->p->Validate ([=] (const kyla::SHA256Digest& hash, 
		const char* file, kylaValidationResult result) -> void {
		kylaValidationItemInfo info;

		info.filename = file;

		validationCallback (result, &info, callbackContext);
	});

	return kylaResult_Ok;

	KYLA_C_API_END ()
}

///////////////////////////////////////////////////////////////////////////////
int kylaRepairRepository (kylaRepository target, kylaRepository source,
	KylaProgressCallback progressCallback,
	void* progressContext)
{
	KYLA_C_API_BEGIN ()

	if (target == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	if (source == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	target->p->Repair (*source->p);

	return kylaResult_Ok;

	KYLA_C_API_END ()
}

///////////////////////////////////////////////////////////////////////////////
int kylaQueryRepository (kylaRepository repository,
	int query,
	const void* queryContext,
	int* queryResultSize,
	void* queryResult)
{
	KYLA_C_API_BEGIN ()

	if (repository == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	if (queryResultSize == nullptr && queryResult == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	switch (query) {
	case kylaQueryRepositoryKey_AvailableFileSets:
		{
			const auto result = repository->p->GetFilesetInfos ();

			if (queryResultSize) {
				auto resultSize = static_cast<int> (result.size () * sizeof (kylaFileSetInfo));
				if (resultSize < 0) {
					// overflow
					return kylaResult_ErrorInvalidArgument;
				}

				*queryResultSize = resultSize;
			}

			if (queryResult) {
				kylaFileSetInfo* output = static_cast<kylaFileSetInfo*> (queryResult);

				for (const auto item : result) {
					output->fileCount = item.fileCount;
					output->fileSize = item.fileSize;
					::memcpy (output->id, item.id.GetData (), sizeof (output->id));

					++output;
				}
			}

			break;
		}

	case kylaQueryRepositoryKey_GetFileSetName:
		{			
			const kyla::Uuid uuid{ static_cast<const std::uint8_t*> (queryContext) };
			const auto result = repository->p->GetFilesetName (uuid);

			if (queryResultSize) {
				auto resultSize = static_cast<int> (result.size () + 1 /* trailing zero */);
				if (resultSize < 0) {
					// overflow
					return kylaResult_ErrorInvalidArgument;
				}

				*queryResultSize = resultSize;
			}

			if (queryResult) {
				::memcpy (queryResult, result.c_str (), result.size () + 1);
			}

			break;
		}
	}

	return kylaResult_Ok;

	KYLA_C_API_END ()
}

///////////////////////////////////////////////////////////////////////////////
int kylaOpenRepository (const char* path,
	kylaRepositoryAccessMode accessMode,
	kylaRepository* repository)
{
	KYLA_C_API_BEGIN ()

	if (path == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	if (repository == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}
	
	kylaRepository repo = new kylaRepositoryImpl;
	repo->p = kyla::OpenRepository (path, accessMode == kylaRepositoryAccessMode_ReadWrite);

	*repository = repo;

	return kylaResult_Ok;

	KYLA_C_API_END ()
}

///////////////////////////////////////////////////////////////////////////////
int kylaCloseRepository (kylaRepository repository)
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
int kylaInstall (const char* targetPath,
	kylaRepository sourceRepository,
	int filesetCount,
	const uint8_t* const * pFilesetIds,
	KylaProgressCallback progressCallback,
	void* progressContext,
	kylaRepository* targetRepository)
{
	KYLA_C_API_BEGIN ()

	if (targetPath == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	if (sourceRepository == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	if (filesetCount <= 0) {
		return kylaResult_ErrorInvalidArgument;
	}

	if (pFilesetIds == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	for (int i = 0; i < filesetCount; ++i) {
		if (pFilesetIds [i] == nullptr) {
			return kylaResult_ErrorInvalidArgument;
		}
	}

	std::vector<kyla::Uuid> filesetIds (filesetCount);
	for (int i = 0; i < filesetCount; ++i) {
		filesetIds [i] = kyla::Uuid{ pFilesetIds [i] };
	}

	if (targetRepository) {
		kylaRepository target = new kylaRepositoryImpl;
		target->p = kyla::DeployRepository (*sourceRepository->p, targetPath, filesetIds);
		*targetRepository = target;
	}

	return kylaResult_Ok;

	KYLA_C_API_END ()
}

///////////////////////////////////////////////////////////////////////////////
int kylaConfigure (kylaRepository targetRepository,
	kylaRepository sourceRepository,
	int filesetCount,
	const uint8_t* const * pFilesetIds,
	KylaProgressCallback progressCallback,
	void* progressContext)
{
	KYLA_C_API_BEGIN ()

	if (targetRepository == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	if (sourceRepository == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	if (filesetCount <= 0) {
		return kylaResult_ErrorInvalidArgument;
	}

	if (pFilesetIds == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	for (int i = 0; i < filesetCount; ++i) {
		if (pFilesetIds [i] == nullptr) {
			return kylaResult_ErrorInvalidArgument;
		}
	}

	std::vector<kyla::Uuid> filesetIds (filesetCount);
	for (int i = 0; i < filesetCount; ++i) {
		filesetIds [i] = kyla::Uuid{ pFilesetIds [i] };
	}

	targetRepository->p->Configure (*sourceRepository->p, filesetIds);

	return kylaResult_Ok;

	KYLA_C_API_END ()
}
