/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#include "SetupContext.h"

///////////////////////////////////////////////////////////////////////////////
SetupContext::SetupContext ()
{
	///@TODO(minor) Handle errors
	kylaLib_ = new QLibrary ("kyla", this);
	createInstaller_ = reinterpret_cast<kylaCreateInstallerFunction> (
		kylaLib_->resolve ("kylaCreateInstaller"));
	destroyInstaller_ = reinterpret_cast<kylaDestroyInstallerFunction> (
		kylaLib_->resolve ("kylaDestroyInstaller"));

	createInstaller_ (KYLA_API_VERSION_1_0, &installer);
}

///////////////////////////////////////////////////////////////////////////////
SetupContext::~SetupContext ()
{
	if (installer) {
		if (sourceRepository) {
			installer->CloseRepository (installer, sourceRepository);
		}

		destroyInstaller_ (installer);
	}

	if (kylaLib_) {
		delete kylaLib_;
	}
}
