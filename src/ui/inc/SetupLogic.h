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

	Q_PROPERTY(QString applicationName MEMBER applicationName_ NOTIFY ApplicationNameChanged)
	Q_PROPERTY(QString status MEMBER status_ NOTIFY StatusChanged)

signals:
	void RepositoryOpened (const bool success);
	void ApplicationNameChanged (const QString& newName);
	void StatusChanged (const QString& status);

private:
	QString applicationName_;
	QString status_;
	SetupContext* context_;
	std::unique_ptr<OpenSourceRepositoryThread> openSourceRepositoryThread_;	
};

#endif // STARTDIALOG_H
