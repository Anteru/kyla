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
		validationCallback (sizeof (hash.bytes), hash.bytes, 
			file, result, callbackContext);
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