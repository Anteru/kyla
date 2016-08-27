/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#ifndef KYLA_UI_SPLASHDIALOG_H
#define KYLA_UI_SPLASHDIALOG_H

#include <QDialog>

#include "SetupContext.h"

namespace Ui {
	class SplashDialog;
}

class OpenSourceRepositoryThread : public QThread
{
	Q_OBJECT

public:
	OpenSourceRepositoryThread (SetupContext* context, const QString& sourceRepositoryPath)
		: context_ (context)
		, sourceRepositoryPath_ (sourceRepositoryPath)
	{
	}

	void run ()
	{
		auto sourceRepositoryPath = sourceRepositoryPath_.toUtf8 ();
		auto result = context_->installer->OpenSourceRepository (
			context_->installer,
			sourceRepositoryPath.data (),
			kylaRepositoryOption_ReadOnly,
			&context_->sourceRepository);

		emit RepositoryOpened (result == kylaResult_Ok);
	}

signals:
	void RepositoryOpened (const bool success);

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
	void OnRepositoryOpened (const bool success);

private:
	Ui::SplashDialog *ui;
	SetupContext* context_;
	OpenSourceRepositoryThread* openSourceRepositoryThread_;
};

#endif // STARTDIALOG_H
