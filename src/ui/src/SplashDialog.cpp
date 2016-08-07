/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matth�us G. Chajdas

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

#include "SplashDialog.h"
#include <ui_SplashDialog.h>
#include <QJsonDocument>
#include <QFile>
#include <QJsonObject>

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

	QJsonDocument setupInfoDocument = QJsonDocument::fromJson (
		setupInfoFile.readAll ());

	auto setupInfo = setupInfoDocument.object ();

	ui->productNameLabel->setText (setupInfo ["applicationName"].toString ());

	preparationThread_ = new PreparationThread (context,
		setupInfo ["repository"].toString ());

	connect (preparationThread_, &QThread::finished,
		this, &SplashDialog::OnSetupCompleted);
	preparationThread_->start ();
}

///////////////////////////////////////////////////////////////////////////////
SplashDialog::~SplashDialog ()
{
	preparationThread_->wait ();
	delete preparationThread_;
	delete ui;
}

///////////////////////////////////////////////////////////////////////////////
void SplashDialog::OnSetupCompleted ()
{
	hide ();

	SetupDialog setupDialog (context_, this);
	setupDialog.exec ();
}