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

#include <memory>

#include "SetupContext.h"

namespace Ui {
class SetupDialog;
}

class SetupDialog;

/**
The install thread executes the actual installation. It's on a separate thread
to not block the UI.
*/
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
	
	std::vector<KylaUuid> GetSelectedFeatures () const;
	QString GetTargetDirectory () const;
	SetupContext* GetSetupContext () const;

public slots:
	void UpdateProgress (const int progress, const char* action,
		const char* detail);
	void InstallationFinished (const bool success);

private:
	void UpdateRequiredDiskSpace ();

	Ui::SetupDialog *ui;
	SetupContext* context_;
	InstallThread* installThread_ = nullptr;
	std::int64_t requiredDiskSpace_ = 0;

public:
	class FeatureTreeNode;

private:
	std::vector<std::unique_ptr<FeatureTreeNode>> featureTreeNodes_;
};

#endif // STARTDIALOG_H
