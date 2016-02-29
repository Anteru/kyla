#include "Kyla.h"

#include "RepositoryBuilder.h"

#define KYLA_C_API_BEGIN() try {
#define KYLA_C_API_END() } catch (...) { return kylaResult_Error; }

int kylaBuildRepository (const char* descriptorFile,
	const kylaBuildEnvironment* environment)
{
	KYLA_C_API_BEGIN ()

	kyla::BuildRepository (descriptorFile,
		environment);

	return kylaResult_Ok;

	KYLA_C_API_END()
}