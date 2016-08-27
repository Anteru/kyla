/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#ifndef KYLA_REPOSITORY_BUILDER_H
#define KYLA_REPOSITORY_BUILDER_H

namespace kyla {
void BuildRepository (const char* descriptorFile,
	const char* sourceDirectory, const char* targetDirectory);
}

#endif
