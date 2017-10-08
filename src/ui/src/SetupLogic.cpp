/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#include "SetupLogic.h"

#include "OpenSourceRepositoryThread.h"

#include <QJsonDocument>
#include <QFile>
#include <QJsonObject>

///////////////////////////////////////////////////////////////////////////////
SetupLogic::SetupLogic (SetupContext* context, QObject* parent)
	: QObject (parent)
	, context_ (context)
{
	QFile setupInfoFile{ "info.json" };
	setupInfoFile.open (QIODevice::ReadOnly);

	const auto setupInfoDocument = QJsonDocument::fromJson (
		setupInfoFile.readAll ());

	context->setupInfo = setupInfoDocument.object ();

	applicationName_ = context->setupInfo ["applicationName"].toString ();
	emit ApplicationNameChanged (applicationName_);

	status_ = QStringLiteral ("Opening source repository ...");
	emit StatusChanged (status_);

	openSourceRepositoryThread_.reset (new OpenSourceRepositoryThread (context,
		context->setupInfo ["repository"].toString ()));

	connect (openSourceRepositoryThread_.get (), &OpenSourceRepositoryThread::RepositoryOpened,
		[this](const bool success) -> void {
		emit RepositoryOpened (success);
		status_ = "Ready";
		emit StatusChanged (status_);
	});
	openSourceRepositoryThread_->start ();
}

///////////////////////////////////////////////////////////////////////////////
SetupLogic::~SetupLogic ()
{
	openSourceRepositoryThread_->wait ();
}
