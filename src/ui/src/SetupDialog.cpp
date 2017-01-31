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

namespace {
///////////////////////////////////////////////////////////////////////////////
class FeatureTreeItem : public QTreeWidgetItem
{
public:
	FeatureTreeItem (const std::vector<KylaUuid>& ids, 
		const std::int64_t size, const char* description)
		: QTreeWidgetItem (QStringList{ description })
	, ids_ (ids)
	, size_ (size)
	{
		this->setFlags (Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
		this->setCheckState (0, Qt::Unchecked);
	}

	std::int64_t GetSize () const
	{
		return size_;
	}

	const std::vector<KylaUuid>& GetIds () const
	{
		return ids_;
	}

private:
	std::vector<KylaUuid> ids_;
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

	int isEncrypted = 0;
	size_t isEncryptedSize = sizeof (isEncrypted);
	installer->GetRepositoryProperty (installer, sourceRepository,
		kylaRepositoryProperty_IsEncrypted, &isEncryptedSize, &isEncrypted);

	if (! isEncrypted) {
		ui->passwordEdit->hide ();
		ui->passwordLabel->hide ();
		ui->passwordSplitter->hide ();
	}
	
	connect (ui->passwordEdit, &QLineEdit::textChanged,
		[=]() -> void {
		std::string key = ui->passwordEdit->text ().toStdString ();

		/*
		///@TODO(minor) Handle encryption
		*/
	});

	connect (ui->featureSelection, &QTreeWidget::itemChanged,
		[=]() -> void {
		std::int64_t totalSize = 0;
		for (auto& treeItem : featureTreeItems_) {
			auto item = static_cast<FeatureTreeItem*> (treeItem);

			if (item->checkState (0) == Qt::Checked) {
				totalSize += item->GetSize ();
			}
		}

		if (totalSize > 0) {
			ui->requiredDiskSpaceValue->setText (tr("Required disk space: %1")
				.arg (FormatMemorySize (totalSize, 4, 0.1f)));
		} else {
			ui->requiredDiskSpaceValue->setText (tr ("No features selected"));
		}
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
	// a list mapping of KylaFeatureTreeNode* -> internal Node* pointers

	featureTreeItems_.clear ();

	std::unordered_map<const KylaFeatureTreeNode*, FeatureTreeItem*> treeToItem;

	for (const auto& node : nodes) {
		std::vector<KylaUuid> featureIds;
		{
			size_t resultSize;
			installer->GetFeatureTreeProperty (installer, sourceRepository,
				kylaFeatureTreeProperty_NodeFeatures, node,
				&resultSize, nullptr);
			featureIds.resize (resultSize / sizeof (KylaUuid));
			installer->GetFeatureTreeProperty (installer, sourceRepository,
				kylaFeatureTreeProperty_NodeFeatures, node,
				&resultSize, featureIds.data ());
		}

		int64_t size = 0;

		for (const auto& featureId : featureIds) {
			size_t resultSize = sizeof (std::int64_t);
			std::int64_t featureSize = 0;

			installer->GetFeatureProperty (installer, sourceRepository,
				featureId, kylaFeatureProperty_Size, &resultSize, &featureSize);

			size += featureSize;
		}

		auto item = new FeatureTreeItem (featureIds, size, node->name);
		featureTreeItems_.push_back (item);
		treeToItem[node] = item;
	}

	QList<QTreeWidgetItem*> roots;

	for (const auto& node : nodes) {
		if (node->parent) {
			treeToItem.find (node->parent)->second->addChild (
				treeToItem.find (node)->second
			);
		} else {
			roots.push_back (treeToItem.find (node)->second);
		}
	}

	for (auto& root : roots) {
		ui->featureSelection->addTopLevelItems (roots);
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
std::vector<KylaUuid> SetupDialog::GetSelectedFeatures () const
{
	std::vector<KylaUuid> result;

	for (auto treeItem : featureTreeItems_) {
		auto item = static_cast<FeatureTreeItem*> (treeItem);

		if (item->checkState (0) == Qt::Checked) {
			result.insert (result.end (), 
				item->GetIds ().begin (), item->GetIds().end());
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
