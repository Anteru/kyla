#include "SetupContext.h"

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

///////////////////////////////////////////////////////////////////////////////
void SetupContext::Setup (const char* repositoryPath)
{
	kylaLib_ = new QLibrary ("kyla", this);
	createInstaller_ = reinterpret_cast<kylaCreateInstallerFunction> (kylaLib_->resolve ("kylaCreateInstaller"));
	destroyInstaller_ = reinterpret_cast<kylaDestroyInstallerFunction> (kylaLib_->resolve ("kylaDestroyInstaller"));

	createInstaller_ (KYLA_API_VERSION_1_0, &installer);
}