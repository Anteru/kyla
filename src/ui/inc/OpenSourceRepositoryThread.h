/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#ifndef KYLA_UI_OPENSOURCEREPOSITORYTHREAD_H
#define KYLA_UI_OPENSOURCEREPOSITORYTHREAD_H

#include <QThread>
#include <QString>

class SetupContext;

/**
Separate thread which opens the source repository - as this can go over the
network, it may take a while and we don't want to block the main thread/UI
while it is being opened.
*/
class OpenSourceRepositoryThread : public QThread
{
	Q_OBJECT

public:
	OpenSourceRepositoryThread (SetupContext* context, 
		const QString& sourceRepositoryPath);

	void run ();

signals:
	void RepositoryOpened (const bool success);

private:
	SetupContext* context_;
	QString sourceRepositoryPath_;
};

#endif