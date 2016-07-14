/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
	~SetupContext ();
	void Setup (const char* sourceRepositoryPath);

	KylaInstaller* installer = nullptr;
	KylaSourceRepository sourceRepository = nullptr;

private:
	QLibrary* kylaLib_ = nullptr;

	using kylaCreateInstallerFunction = int (*) (int kylaApiVersion, KylaInstaller** installer);
	using kylaDestroyInstallerFunction = int (*) (KylaInstaller* installer);

	kylaCreateInstallerFunction createInstaller_ = nullptr;
	kylaDestroyInstallerFunction destroyInstaller_ = nullptr;
}; 

class SetupThread : public QThread
{
	Q_OBJECT

public:
	SetupThread (SetupContext* context, const char* sourceRepositoryPath)
		: context_ (context)
		, sourceRepositoryPath_ (sourceRepositoryPath)
	{
	}

	void run ()
	{
		context_->Setup (sourceRepositoryPath_);
		context_->installer->OpenSourceRepository (context_->installer,
			sourceRepositoryPath_,
			kylaRepositoryOption_ReadOnly,
			&context_->sourceRepository);
	}

private:
	SetupContext* context_;
	const char* sourceRepositoryPath_;
};



#endif
