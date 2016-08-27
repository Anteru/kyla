/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#ifndef KYLA_UI_SETUPDIALOG_H
#define KYLA_UI_SETUPDIALOG_H

#include <QDialog>
#include <QThread>

#include "SetupContext.h"

namespace Ui {
class SetupDialog;
}

class SetupDialog;

class InstallThread : public QThread
{
	Q_OBJECT 
public:
	InstallThread (const SetupDialog* dialog);

	void run ();

signals:
	void ProgressChanged (const int progress, const char* action,
		const char* detail);
	void InstallationFinished (const bool success);

private:
	const SetupDialog* parent_;
};

class SetupDialog : public QDialog
{
	Q_OBJECT

public:
	explicit SetupDialog(SetupContext* context, QWidget *parent = 0);
	~SetupDialog();
	
	std::vector<KylaUuid> GetSelectedFilesets () const;
	QString GetTargetDirectory () const;
	SetupContext* GetSetupContext () const;

public slots:
	void UpdateProgress (const int progress, const char* action,
		const char* detail);
	void InstallationFinished (const bool success);

private:
	Ui::SetupDialog *ui;
	SetupContext* context_;
	InstallThread* installThread_ = nullptr;
};

#endif // STARTDIALOG_H
