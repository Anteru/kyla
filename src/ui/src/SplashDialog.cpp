/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#include "SplashDialog.h"
#include <ui_SplashDialog.h>
#include <QJsonDocument>
#include <QFile>
#include <QJsonObject>
#include <QMessageBox>

#include "SetupDialog.h"

///////////////////////////////////////////////////////////////////////////////
SplashDialog::SplashDialog (SetupContext* context, const QString& appName,
	QWidget *parent)
	: QDialog (parent)
	, ui (new Ui::SplashDialog)
	, context_ (context)
{
	ui->setupUi (this);

	QFile setupInfoFile{ "info.json" };
	setupInfoFile.open (QIODevice::ReadOnly);

	const auto setupInfoDocument = QJsonDocument::fromJson (
		setupInfoFile.readAll ());

	auto setupInfo = setupInfoDocument.object ();

	ui->productNameLabel->setText (setupInfo ["applicationName"].toString ());

	openSourceRepositoryThread_ = new OpenSourceRepositoryThread (context,
		setupInfo ["repository"].toString ());

	connect (openSourceRepositoryThread_, &OpenSourceRepositoryThread::RepositoryOpened,
		this, &SplashDialog::OnRepositoryOpened);
	openSourceRepositoryThread_->start ();
}

///////////////////////////////////////////////////////////////////////////////
SplashDialog::~SplashDialog ()
{
	openSourceRepositoryThread_->wait ();
	delete openSourceRepositoryThread_;
	delete ui;
}

///////////////////////////////////////////////////////////////////////////////
void SplashDialog::OnRepositoryOpened (const bool success)
{
	if (!success) {
		QMessageBox::critical (this, "Error while opening source repository",
			"The source repository could not be opened.",
			QMessageBox::Close);
		close ();
	} else {
		hide ();

		SetupDialog setupDialog (context_, this);
		setupDialog.exec ();
	}
}