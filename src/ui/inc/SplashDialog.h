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

#ifndef KYLA_UI_SPLASHDIALOG_H
#define KYLA_UI_SPLASHDIALOG_H

#include <QDialog>

#include "SetupContext.h"

namespace Ui {
	class SplashDialog;
}

class PreparationThread : public QThread
{
	Q_OBJECT

public:
	PreparationThread (SetupContext* context, const QString& sourceRepositoryPath)
		: context_ (context)
		, sourceRepositoryPath_ (sourceRepositoryPath)
	{
	}

	void run ()
	{
		auto sourceRepositoryPath = sourceRepositoryPath_.toUtf8 ();
		context_->Setup (sourceRepositoryPath.data ());
		auto result = context_->installer->OpenSourceRepository (
			context_->installer,
			sourceRepositoryPath.data (),
			kylaRepositoryOption_ReadOnly,
			&context_->sourceRepository);

		emit PreparationFinished (result == kylaResult_Ok);
	}

signals:
	void PreparationFinished (const bool success);

private:
	SetupContext* context_;
	QString sourceRepositoryPath_;
};

class SplashDialog : public QDialog
{
	Q_OBJECT

public:
	explicit SplashDialog (SetupContext* context, const QString& appName, QWidget *parent = 0);
	~SplashDialog ();

public slots:
	void OnSetupCompleted ();

private:
	Ui::SplashDialog *ui;
	SetupContext* context_;
	PreparationThread* preparationThread_;
};

#endif // STARTDIALOG_H
