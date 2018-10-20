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
std::vector<KylaUuid> GetFeatureIdsForFeatureTreeNode (KylaInstaller* installer,
	KylaSourceRepository sourceRepository,
	const KylaFeatureTreeNode* featureTreeNode)
{
	std::vector<KylaUuid> result;

	size_t resultSize;
	installer->GetFeatureTreeProperty (installer, sourceRepository,
		kylaFeatureTreeProperty_NodeFeatures, featureTreeNode,
		&resultSize, nullptr);
	result.resize (resultSize / sizeof (KylaUuid));
	installer->GetFeatureTreeProperty (installer, sourceRepository,
		kylaFeatureTreeProperty_NodeFeatures, featureTreeNode,
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
	} else {
		requiredDiskSpace_ -= size;
	}

	UpdateRequiredDiskSpace ();

	bool anyEnabled = false;
	for (auto& featureTreeNode : featureTreeNodes_) {
		if (featureTreeNode->checkState (0) == Qt::Checked) {
			anyEnabled = true;
			break;
		}
	}

	ui->startInstallationButton->setEnabled (anyEnabled);
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
		ui->startInstallationButton->setEnabled (
			!ui->targetDirectoryEdit->text ().isEmpty ());
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

	std::vector<const KylaFeatureTreeNode*> nodes;
	{
		size_t resultSize;
		installer->GetFeatureTreeProperty (installer, sourceRepository,
			kylaFeatureTreeProperty_Nodes, nullptr,
			&resultSize, nullptr);
		nodes.resize (resultSize / sizeof (const KylaFeatureTreeNode*));
		installer->GetFeatureTreeProperty (installer, sourceRepository,
			kylaFeatureTreeProperty_Nodes, nullptr,
			&resultSize, nodes.data ());
	}

	// We convert the feature tree into tree nodes in two passes - first,
	// we create the nodes, then, we link them to their parents
	// Every node which ends up without a parent is then added to the widget
	// Adding works the reverse way (we add to the parent), so we maintain
	// a list mapping of KylaFeatureTreeNode* -> tree widget* pointers

	featureTreeNodes_.clear ();

	std::unordered_map<const KylaFeatureTreeNode*, QTreeWidgetItem*> treeToItem;

	connect (ui->featureSelection, &QTreeWidget::itemChanged, 
		this, &SetupDialog::OnFeatureSelectionItemChanged);

	connect (ui->featureSelection, &QTreeWidget::currentItemChanged, 
		[this] (QTreeWidgetItem* item, QTreeWidgetItem*) -> void
	{
		auto desc = item->data (0, FeatureDescriptionRole);

		if (desc.isValid ()) {
			ui->featureDescriptionLabel->setText (desc.toString ());
		}
	});

	for (const auto& node : nodes) {
		const auto featureIds = GetFeatureIdsForFeatureTreeNode (
			installer, sourceRepository, node);

		int64_t size = 0;

		for (const auto& featureId : featureIds) {
			size_t resultSize = sizeof (std::int64_t);
			std::int64_t featureSize = 0;

			installer->GetFeatureProperty (installer, sourceRepository,
				featureId, kylaFeatureProperty_Size, &resultSize, &featureSize);

			size += featureSize;
		}

		auto item = new QTreeWidgetItem ();
		item->setData (0, FeatureSizeRole, QVariant{ static_cast<qlonglong> (size) });
		item->setData (0, FeatureDescriptionRole, QVariant{ node->description });
		item->setData (0, FeatureFeatureIdsIndexRole, QVariant{ static_cast<qlonglong> (featureTreeFeatureIds_.size ()) });
		item->setText (0, node->name);
		item->setCheckState (0, Qt::Unchecked);

		treeToItem[node] = item;
		featureTreeNodes_.emplace_back (item);
		featureTreeFeatureIds_.emplace_back (std::move (featureIds));
	}

	UpdateRequiredDiskSpace ();

	QList<QTreeWidgetItem*> roots;

	for (const auto& node : nodes) {
		if (node->parent) {
			treeToItem.find (node->parent)->second->addChild (
				treeToItem.find (node)->second);
		} else {
			roots.push_back (treeToItem.find (node)->second);
		}
	}
	
	for (auto& root : roots) {
		ui->featureSelection->addTopLevelItems (roots);
	}
	
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
			const auto& ids = featureTreeFeatureIds_ [featureNode->data (0, FeatureFeatureIdsIndexRole).toInt ()];
			result.insert (result.end (), ids.begin (), ids.end());
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
