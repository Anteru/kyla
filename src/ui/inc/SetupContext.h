/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#ifndef KYLA_UI_SETUP_CONTEXT_H
#define KYLA_UI_SETUP_CONTEXT_H

#include <QLibrary>
#include <Kyla.h>
#include <QThread>

class SetupContext : public QObject
{
	Q_OBJECT

public:
	SetupContext ();
	~SetupContext ();

	KylaInstaller* installer = nullptr;
	KylaSourceRepository sourceRepository = nullptr;

private:
	QLibrary* kylaLib_ = nullptr;

	using kylaCreateInstallerFunction = int (*) (int kylaApiVersion, KylaInstaller** installer);
	using kylaDestroyInstallerFunction = int (*) (KylaInstaller* installer);

	kylaCreateInstallerFunction createInstaller_ = nullptr;
	kylaDestroyInstallerFunction destroyInstaller_ = nullptr;
};

#endif
