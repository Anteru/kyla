/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#ifndef KYLA_UI_SETUPLOGIC_H
#define KYLA_UI_SetupLogic_H

#include <QObject>
#include <memory>

#include "SetupContext.h"

class OpenSourceRepositoryThread;

class SetupLogic : public QObject
{
	Q_OBJECT

public:
	explicit SetupLogic (SetupContext* context, QObject* parent = nullptr);
	~SetupLogic ();

	Q_PROPERTY(QString applicationName MEMBER applicationName_ NOTIFY applicationNameChanged)
	Q_PROPERTY(QString status MEMBER status_ NOTIFY statusChanged)
	Q_PROPERTY(bool ready MEMBER ready_ NOTIFY readyChanged)

signals:
	void repositoryOpened (const bool success);
	void applicationNameChanged (const QString& newName);
	void statusChanged (const QString& status);
	void readyChanged (const bool ready);

private:
	QString applicationName_;
	QString status_;
	bool ready_ = false;
	SetupContext* context_;
	std::unique_ptr<OpenSourceRepositoryThread> openSourceRepositoryThread_;	
};

#endif // STARTDIALOG_H
