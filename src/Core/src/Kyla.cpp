#include "Kyla.h"

#include "Repository.h"
#include "RepositoryBuilder.h"

#define KYLA_C_API_BEGIN() try {
#define KYLA_C_API_END() } catch (...) { return kylaResult_Error; }

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
int kylaValidateRepository (const char* repositoryPath,
	KylaValidateCallback validationCallback,
	void* callbackContext)
{
	KYLA_C_API_BEGIN ()

	if (repositoryPath == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	if (validationCallback == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	auto repository = kyla::OpenRepository (repositoryPath);
	repository->Validate ([=] (const kyla::SHA256Digest& hash, 
		const char* file, kylaValidationResult result) -> void {
		kylaValidationItemInfo info;

		info.filename = file;

		validationCallback (result, &info, callbackContext);
	});

	return kylaResult_Ok;

	KYLA_C_API_END ()
}

///////////////////////////////////////////////////////////////////////////////
int kylaRepairRepository (const char* targetPath, const char* sourcePath)
{
	KYLA_C_API_BEGIN ()

	if (targetPath == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	if (sourcePath == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	auto targetRepository = kyla::OpenRepository (targetPath);
	auto sourceRepository = kyla::OpenRepository (sourcePath);

	targetRepository->Repair (*sourceRepository);

	return kylaResult_Ok;

	KYLA_C_API_END ()
}

///////////////////////////////////////////////////////////////////////////////
int kylaQueryRepository (const char* repositoryPath,
	int query,
	void* queryContext,
	int* queryResultSize,
	void* queryResult)
{
	KYLA_C_API_BEGIN ()

	if (repositoryPath == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	if (queryResultSize == nullptr && queryResult == nullptr) {
		return kylaResult_ErrorInvalidArgument;
	}

	auto repository = kyla::OpenRepository (repositoryPath);

	switch (query) {
	case kylaQueryRepositoryKey_AvailableFileSets:
		{
			const auto result = repository->GetFilesetInfos ();

			if (queryResultSize) {
				*queryResultSize = result.size () * sizeof (kylaFileSetInfo);
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
	}

	return kylaResult_Ok;

	KYLA_C_API_END ()
}