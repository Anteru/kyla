#ifndef KYLA_REPOSITORY_BUILDER_H
#define KYLA_REPOSITORY_BUILDER_H

#include "Kyla.h"

namespace kyla {
	void BuildRepository (const char* descriptorFile,
		const kylaBuildEnvironment* environment);
}

#endif
