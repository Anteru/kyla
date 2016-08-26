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

#include "SetupDialog.h"
#include <ui_SetupDialog.h>

#include <QFileDialog>
#include <QMessageBox>

#include <vector>

namespace {
///////////////////////////////////////////////////////////////////////////////
class FilesetListItem : public QListWidgetItem
{
public:
	FilesetListItem (const KylaUuid id, const std::int64_t size, const char* description)
	: QListWidgetItem (description)
	, id_ (id)
	, size_ (size)
	{
		this->setFlags (Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
		this->setCheckState (Qt::Unchecked);
	}

	std::int64_t GetSize () const
	{
		return size_;
	}

	const KylaUuid& GetId () const
	{
		return id_;
	}

private:
	KylaUuid id_;
	std::int64_t size_;
};

///////////////////////////////////////////////////////////////////////////////
QString FormatMemorySize (const std::int64_t size, const int precision, const float slack)
{
	static const struct { const char* suffix; std::int64_t divisor; } scale [] = {
		{ "bytes", 1 },
		{ "KiB", std::int64_t (1) << 10 },
		{ "MiB", std::int64_t (1) << 20 },
		{ "GiB", std::int64_t (1) << 30 },
		{ "TiB", std::int64_t (1) << 40 },
		{ "PiB", std::int64_t (1) << 50 },
		{ "EiB", std::int64_t (1) << 60 }
	};

	int unit = 0;

	for (int i = 0; i < static_cast<int>(std::extent<decltype(scale)>::value - 1); ++i) {
		if (size < (scale [i + 1].divisor + scale [i + 1].divisor * slack)) {
			unit = i;
			break;
		} else {
			unit = i + 1;
		}
	}

	const double quot = static_cast<double> (size) / scale [unit].divisor;

	return QString ("%1 %2")
		.arg (quot, 0, 'g', precision)
		.arg (scale [unit].suffix);
}
}

///////////////////////////////////////////////////////////////////////////////
void InstallThread::run ()
{
	auto installer = parent_->GetSetupContext ()->installer;

	installer->SetProgressCallback (installer, [](const struct KylaProgress* progress,
		void* context) -> void {
		emit static_cast<InstallThread*> (context)->ProgressChanged (static_cast<int> (progress->totalProgress * 100.0f),
			progress->action, progress->detailMessage);
	}, this);

	KylaTargetRepository targetRepository = nullptr;
	auto targetDirectory = parent_->GetTargetDirectory ();
	
	kylaAction action = kylaAction_Configure;
	// We try to open - if that fails, we create a new one
	installer->OpenTargetRepository (installer,
		targetDirectory.toUtf8 ().data (),
		0, &targetRepository);

	if (! targetRepository) {
		installer->OpenTargetRepository (installer,
			targetDirectory.toUtf8 ().data (),
			kylaRepositoryOption_Create, &targetRepository);
		action = kylaAction_Install;
	}

	if (!targetRepository) {
		emit InstallationFinished (false);
		// Something went seriously wrong
		return;
	}

	KylaDesiredState desiredState;

	std::vector<uint8_t> idStore;

	int itemCount = 0;
	for (const auto& id : parent_->GetSelectedFilesets ()) {
		idStore.insert (idStore.end (),
			id.bytes, id.bytes + sizeof (id.bytes));
		++itemCount;
	}

	std::vector<uint8_t*> idPointers;
	for (int i = 0; i < itemCount; ++i) {
		idPointers.push_back (idStore.data () + idStore.size () / itemCount * i);
	}

	desiredState.filesetCount = itemCount;
	desiredState.filesetIds = idPointers.data ();

	auto executeResult = installer->Execute (installer, action,
		targetRepository, 
		parent_->GetSetupContext ()->sourceRepository, &desiredState);

	installer->CloseRepository (installer, targetRepository);

	emit InstallationFinished (executeResult == kylaResult_Ok);
}

///////////////////////////////////////////////////////////////////////////////
SetupDialog::SetupDialog(SetupContext* context, QWidget *parent) 
	: QDialog(parent)
	, ui(new Ui::SetupDialog)
	, context_ (context)
{
	ui->setupUi(this);

	connect(ui->selectDirectoryButton, &QPushButton::clicked,
		[=]() -> void {
		QFileDialog fd{this};
		fd.setOption(QFileDialog::ShowDirsOnly);
		fd.setFileMode(QFileDialog::Directory);
		if (fd.exec()) {
			this->ui->targetDirectoryEdit->setText(fd.selectedFiles().first());
		}
	});

	connect(ui->targetDirectoryEdit, &QLineEdit::textChanged,
			[=] () -> void {
			ui->startInstallationButton->setEnabled(
				! ui->targetDirectoryEdit->text ().isEmpty());
	});

	auto installer = context_->installer;
	auto sourceRepository = context_->sourceRepository;

	connect (ui->featureSelection, &QListWidget::itemChanged,
		[=]() -> void {
		std::int64_t totalSize = 0;

		for (int i = 0; i < ui->featureSelection->count (); ++i) {
			auto item = static_cast<FilesetListItem*> (ui->featureSelection->item (i));

			if (item->checkState () == Qt::Checked) {
				totalSize += item->GetSize ();
			}
		}

		if (totalSize > 0) {
			ui->requiredDiskSpaceValue->setText (tr("Required disk space: %1")
				.arg (FormatMemorySize (totalSize, 3, 0.1f)));
		} else {
			ui->requiredDiskSpaceValue->setText (tr ("No features selected"));
		}
	});

	std::size_t resultSize = 0;
	installer->QueryRepository (installer, sourceRepository,
		kylaRepositoryProperty_AvailableFilesets, &resultSize, nullptr);

	std::vector<KylaUuid> filesets;
	filesets.resize (resultSize / sizeof (KylaUuid));
	installer->QueryRepository (installer, sourceRepository,
		kylaRepositoryProperty_AvailableFilesets, &resultSize, filesets.data ());

	for (const auto& fs : filesets) {
		std::size_t length = 0;
		installer->QueryFileset (installer, sourceRepository,
			fs, kylaFilesetProperty_Name,
			&length, nullptr);
		std::vector<char> name;
		name.resize (length);
		installer->QueryFileset (installer, sourceRepository,
			fs, kylaFilesetProperty_Name,
			&length, name.data ());

		std::int64_t filesetSize = 0;
		std::size_t filesetResultSize = sizeof (filesetSize);

		installer->QueryFileset (installer, sourceRepository,
			fs, kylaFilesetProperty_Size,
			&filesetResultSize, &filesetSize);

		auto item = new FilesetListItem (fs,
			filesetSize, name.data ());
		ui->featureSelection->addItem (item);
		item->setCheckState (Qt::Checked);
	}

	connect (ui->startInstallationButton, &QPushButton::clicked,
		[=] () -> void {
		ui->startInstallationButton->setEnabled (false);
		installThread_ = new InstallThread (this);
		connect (installThread_, &InstallThread::ProgressChanged,
			this, &SetupDialog::UpdateProgress);
		connect (installThread_, &InstallThread::InstallationFinished,
			this, &SetupDialog::InstallationFinished);
		installThread_->start ();
	});
}

///////////////////////////////////////////////////////////////////////////////
void SetupDialog::UpdateProgress (const int progress, const char* message,
	const char* detail)
{
	ui->installationProgressLabel->setText (QString ("%1: %2").arg (message).arg (detail));
	ui->progressBar->setValue (progress);
}

///////////////////////////////////////////////////////////////////////////////
void SetupDialog::InstallationFinished (const bool success)
{
	if (success) {
		QMessageBox::information (this,
			"Installation finished",
			"The installation finished successfully");
	} else {
		QMessageBox::critical (this,
			"Installation error",
			"An error occured during the installation");
	}

	close ();
}

///////////////////////////////////////////////////////////////////////////////
SetupDialog::~SetupDialog()
{
	if (installThread_) {
		installThread_->wait ();
	}
	delete ui;
}

///////////////////////////////////////////////////////////////////////////////
std::vector<KylaUuid> SetupDialog::GetSelectedFilesets () const
{
	std::vector<KylaUuid> result;

	for (int i = 0; i < ui->featureSelection->count (); ++i) {
		auto item = static_cast<FilesetListItem*> (ui->featureSelection->item (i));

		if (item->checkState () == Qt::Checked) {
			result.push_back (item->GetId ());
		}
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////////
QString SetupDialog::GetTargetDirectory () const
{
	return ui->targetDirectoryEdit->text ();
}

///////////////////////////////////////////////////////////////////////////////
SetupContext* SetupDialog::GetSetupContext () const
{
	return context_;
}

///////////////////////////////////////////////////////////////////////////////
InstallThread::InstallThread (const SetupDialog * dialog)
	: parent_ (dialog)
{
}
