/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#include "SetupDialog.h"
#include <ui_SetupDialog.h>

#include <QFileDialog>
#include <QMessageBox>

#include <vector>
#include <unordered_map>

#include <QGridLayout>
#include <QCheckBox>

#include <functional>

#include <QTreeView>
#include <QTreeWidget>

#include <QDebug>

namespace {
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
	for (const auto& id : parent_->GetSelectedFeatures ()) {
		idStore.insert (idStore.end (),
			id.bytes, id.bytes + sizeof (id.bytes));
		++itemCount;
	}

	std::vector<uint8_t*> idPointers;
	for (int i = 0; i < itemCount; ++i) {
		idPointers.push_back (idStore.data () + idStore.size () / itemCount * i);
	}

	desiredState.featureCount = itemCount;
	desiredState.featureIds = idPointers.data ();

	auto executeResult = installer->Execute (installer, action,
		targetRepository, 
		parent_->GetSetupContext ()->sourceRepository, &desiredState);

	installer->CloseRepository (installer, targetRepository);

	emit InstallationFinished (executeResult == kylaResult_Ok);
}

namespace {
struct UuidHash
{
	size_t operator ()(const KylaUuid& uuid) const
	{
		return qHashBits (uuid.bytes, sizeof (uuid.bytes));
	}
};

struct UuidEqual
{
	bool operator ()(const KylaUuid& left, const KylaUuid& right) const
	{
		return ::memcmp (left.bytes, right.bytes, sizeof (left)) == 0;
	}
};

class KylaFeature
{
private:
	KylaUuid uuid_;

	QString title_;
	QString description_;

	KylaFeature* parent_ = nullptr;

public:
	KylaFeature (KylaInstaller* installer, KylaSourceRepository repository,
		KylaUuid uuid)
	: uuid_ (uuid)
	{
		std::vector<char> buffer;

		size_t size = 0;
		installer->GetFeatureProperty (installer, repository,
			uuid, kylaFeatureProperty_Title, &size, nullptr);

		if (size != 0) {
			buffer.resize (size);
			installer->GetFeatureProperty (installer, repository,
				uuid, kylaFeatureProperty_Title, &size, buffer.data ());
			title_ = QLatin1String (buffer.data ());
		}

		installer->GetFeatureProperty (installer, repository,
			uuid, kylaFeatureProperty_Description, &size, nullptr);

		if (size != 0) {
			buffer.resize (size);
			installer->GetFeatureProperty (installer, repository,
				uuid, kylaFeatureProperty_Description, &size, buffer.data ());
			description_ = QLatin1String (buffer.data ());
		}
	}

	const KylaUuid& GetId () const
	{
		return uuid_;
	}

	void SetParent (KylaFeature* parent)
	{
		parent_ = parent;
	}

	KylaFeature* GetParent () const
	{
		return parent_;
	}

	QString GetTitle () const
	{
		return title_;
	}

	QString GetDescription () const
	{
		return description_;
	}
};

///////////////////////////////////////////////////////////////////////////////
std::vector<KylaUuid> GetSubfeatureIds (KylaInstaller* installer,
	KylaSourceRepository sourceRepository,
	const KylaUuid& feature)
{
	std::vector<KylaUuid> result;

	size_t resultSize;
	installer->GetFeatureProperty (installer, sourceRepository,
		feature, kylaFeatureProperty_SubfeatureIds,
		&resultSize, nullptr);
	result.resize (resultSize / sizeof (KylaUuid));
	installer->GetFeatureProperty (installer, sourceRepository,
		feature, kylaFeatureProperty_SubfeatureIds,
		&resultSize, result.data ());

	return result;
}
}

///////////////////////////////////////////////////////////////////////////////
void SetupDialog::OnFeatureSelectionItemChanged (QTreeWidgetItem* item, int)
{
	auto size = item->data (0, FeatureSizeRole).toLongLong ();
	const auto isEnabled = item->checkState (0) == Qt::Checked;

	if (isEnabled) {
		requiredDiskSpace_ += size;

		if (item->parent ()) {
			item->parent ()->setCheckState (0, Qt::Checked);
		}
	} else {
		requiredDiskSpace_ -= size;

		for (int i = 0; i < item->childCount (); ++i) {
			item->child (i)->setCheckState (0, Qt::Unchecked);
		}
	}

	UpdateRequiredDiskSpace ();
	UpdateInstallButtonState ();
}

///////////////////////////////////////////////////////////////////////////////
void SetupDialog::UpdateInstallButtonState ()
{
	bool anyEnabled = false;
	for (auto& featureTreeNode : featureTreeNodes_) {
		if (featureTreeNode->checkState (0) == Qt::Checked) {
			anyEnabled = true;
			break;
		}
	}

	ui->startInstallationButton->setEnabled (anyEnabled && 
		!ui->targetDirectoryEdit->text ().isEmpty ());
}

///////////////////////////////////////////////////////////////////////////////
SetupDialog::SetupDialog (SetupContext* context)
	: ui (new Ui::SetupDialog)
	, context_ (context)
{
	ui->setupUi (this);

	ui->applicationName->setText (context->setupInfo["applicationName"].toString ());

	connect (ui->selectDirectoryButton, &QPushButton::clicked,
		[=]() -> void {
		QFileDialog fd{ this };
		fd.setOption (QFileDialog::ShowDirsOnly);
		fd.setFileMode (QFileDialog::Directory);
		if (fd.exec ()) {
			this->ui->targetDirectoryEdit->setText (fd.selectedFiles ().first ());
		}
	});

	connect (ui->targetDirectoryEdit, &QLineEdit::textChanged,
		[=]() -> void {
		UpdateInstallButtonState ();
	});

	auto installer = context_->installer;
	auto sourceRepository = context_->sourceRepository;

	int isEncrypted = 0;
	size_t isEncryptedSize = sizeof (isEncrypted);
	installer->GetRepositoryProperty (installer, sourceRepository,
		kylaRepositoryProperty_IsEncrypted, &isEncryptedSize, &isEncrypted);

	if (!isEncrypted) {
		ui->passwordEdit->hide ();
		ui->passwordLabel->hide ();
	}

	connect (ui->passwordEdit, &QLineEdit::textChanged,
		[=]() -> void {
		std::string key = ui->passwordEdit->text ().toStdString ();

		/*
		///@TODO(minor) Handle encryption
		*/
	});

	std::size_t resultSize = 0;
	installer->GetRepositoryProperty (installer, sourceRepository,
		kylaRepositoryProperty_AvailableFeatures, &resultSize, nullptr);

	std::unordered_map<KylaUuid, 
		std::unique_ptr<KylaFeature>, UuidHash, UuidEqual> features;
	{
		std::vector<KylaUuid> featureIds;
		size_t resultSize;
		installer->GetRepositoryProperty (installer, sourceRepository,
			kylaRepositoryProperty_AvailableFeatures,
			&resultSize, nullptr);
		featureIds.resize (resultSize / sizeof (KylaUuid));
		installer->GetRepositoryProperty (installer, sourceRepository,
			kylaRepositoryProperty_AvailableFeatures,
			&resultSize, featureIds.data ());

		for (const auto& id : featureIds) {
			features [id] = std::make_unique<KylaFeature> (
				installer, sourceRepository, id);
		}

		// We now link the features together - for each feature, we request
		// all children and set the parent
		for (const auto& id : featureIds) {
			const auto subfeatures = GetSubfeatureIds (installer, sourceRepository,
				id);

			for (const auto& subId : subfeatures) {
				features.find (subId)->second->SetParent (
					features.find (id)->second.get ());
			}
		}
	}

	featureTreeNodes_.clear ();

	connect (ui->featureSelection, &QTreeWidget::itemChanged, 
		this, &SetupDialog::OnFeatureSelectionItemChanged);
	
	// We create all tree items now, and will link them below
	std::unordered_map<KylaFeature*, QTreeWidgetItem*> featureToTreeItem;
	for (const auto& feature : features) {
		int64_t size = 0;

		installer->GetFeatureProperty (installer, sourceRepository,
			feature.second->GetId (), kylaFeatureProperty_Size, &resultSize, &size);

		auto item = new QTreeWidgetItem ();
		item->setData (0, FeatureSizeRole, QVariant{ static_cast<qlonglong> (size) });
		item->setData (0, FeatureFeatureIdsIndexRole, QVariant{ static_cast<qlonglong> (featureTreeFeatureIds_.size ()) });
		item->setText (0, feature.second->GetTitle ());
		item->setToolTip (0, QString ("%1\n%2").arg (feature.second->GetTitle ()).arg (feature.second->GetDescription ()));
		item->setCheckState (0, Qt::Unchecked);

		featureToTreeItem [feature.second.get ()] = item;
		featureTreeNodes_.emplace_back (item);
		featureTreeFeatureIds_.emplace_back (feature.first);
	}

	UpdateRequiredDiskSpace ();

	QList<QTreeWidgetItem*> roots;

	for (const auto& feature : features) {
		if (feature.second->GetParent ()) {
			featureToTreeItem.find (feature.second->GetParent ())->second->addChild (
				featureToTreeItem.find (feature.second.get ())->second);
		} else {
			roots.push_back (featureToTreeItem.find (feature.second.get ())->second);
		}
	}
	
	ui->featureSelection->addTopLevelItems (roots);
	
	this->show ();

	{
		auto policy = ui->installationProgressLabel->sizePolicy ();
		policy.setRetainSizeWhenHidden (true);
		ui->installationProgressLabel->setSizePolicy (policy);
		ui->installationProgressLabel->hide ();
	}

	{
		auto policy = ui->progressBar->sizePolicy ();
		policy.setRetainSizeWhenHidden (true);
		ui->progressBar->setSizePolicy (policy);
		ui->progressBar->hide ();
	}

	connect (ui->startInstallationButton, &QPushButton::clicked,
		[=] () -> void {
		ui->startInstallationButton->setEnabled (false);
		ui->featureSelection->setEnabled (false);
		ui->selectDirectoryButton->setEnabled (false);
		ui->targetDirectoryEdit->setReadOnly (true);

		installThread_ = new InstallThread (this);
		connect (installThread_, &InstallThread::ProgressChanged,
			this, &SetupDialog::UpdateProgress);
		connect (installThread_, &InstallThread::InstallationFinished,
			this, &SetupDialog::InstallationFinished);

		StartProgress ();

		installThread_->start ();
	});
}

///////////////////////////////////////////////////////////////////////////////
void SetupDialog::UpdateRequiredDiskSpace ()
{
	if (requiredDiskSpace_ > 0) {
		ui->requiredDiskSpaceValue->setText (tr ("Required disk space: %1")
			.arg (FormatMemorySize (requiredDiskSpace_, 4, 0.1f)));
	} else {
		ui->requiredDiskSpaceValue->setText (tr ("No features selected"));
	}
}

void SetupDialog::StartProgress ()
{
	ui->progressBar->show ();
	ui->installationProgressLabel->show ();

#if KYLA_PLATFORM_WINDOWS
	if (taskbarProgress_) {
		taskbarProgress_->show ();
	}
#endif
}

#if KYLA_PLATFORM_WINDOWS
///////////////////////////////////////////////////////////////////////////////
void SetupDialog::showEvent (QShowEvent *)
{
	taskbarButton_ = new QWinTaskbarButton (this);
	taskbarButton_->setWindow (this->windowHandle ());

	taskbarProgress_ = taskbarButton_->progress ();
}
#endif

///////////////////////////////////////////////////////////////////////////////
void SetupDialog::UpdateProgress (const int progress, const char* message,
	const char* detail)
{
	ui->progressBar->setValue (progress);

#if KYLA_PLATFORM_WINDOWS
	if (taskbarProgress_) {
		taskbarProgress_->setValue (progress);
	}
#endif
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
std::vector<KylaUuid> SetupDialog::GetSelectedFeatures () const
{
	std::vector<KylaUuid> result;

	for (auto& featureNode : featureTreeNodes_) {
		if (featureNode->checkState (0) != Qt::Unchecked) {
			result.push_back (featureTreeFeatureIds_ [featureNode->data (0, FeatureFeatureIdsIndexRole).toInt ()]);
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
