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
	InstallThread* setupThread_ = nullptr;
};

#endif // STARTDIALOG_H
