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
using FeatureSelectionChangedCallback = std::function<void (const SetupDialog::FeatureTreeNode* const item)>;

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
class SetupDialog::FeatureTreeNode
{
public:
	FeatureTreeNode (const std::vector<KylaUuid>& ids,
		const std::int64_t size,
		const char* name, const char* description,
		FeatureSelectionChangedCallback featureSelectionCallback)
		: ids_ (ids)
		, size_ (size)
		, name_ (name)
		, description_ (description)
		, featureSelectionCallback_ (featureSelectionCallback)
	{
	}

	QWidget* CreateWidget ()
	{
		auto widget = new QWidget;
		auto layout = new QGridLayout;

		installCheckBox_ = new QCheckBox;
		layout->addWidget (installCheckBox_, 0, 0, Qt::AlignHCenter);
		installCheckBox_->setChecked (selected_);

		connect (installCheckBox_, &QCheckBox::stateChanged,
			[&](int state) -> void {
			selected_ = (state == Qt::Checked);
			featureSelectionCallback_ (this);
		});

		layout->addWidget (CreateMainLabel (name_), 0, 1);
		layout->addWidget (CreateDescriptionLabel (description_), 1, 1);
		
		// If we got children, they'll be part of the "detail area"
		if (!children_.empty ()) {
			auto treeWidget = new QTreeWidget;
			treeWidget->setHeaderHidden (true);
			treeWidget->setRootIsDecorated (false);
			treeWidget->setFrameShape (QFrame::NoFrame);
			treeWidget->setSizeAdjustPolicy (QAbstractScrollArea::SizeAdjustPolicy::AdjustToContentsOnFirstShow);

			int totalHeight = 0;
			for (auto& child : children_) {
				auto item = child->CreateTreeWidgetItem ();
				treeWidget->addTopLevelItem (item);
			}

			selectedSubfeatures_ = static_cast<int> (children_.size ());
			totalSubfeatures_ = CountChildren (this);

			connect (treeWidget, &QTreeWidget::itemChanged,
				[&](QTreeWidgetItem* item, int) -> void {
				auto featureTreeNode = reinterpret_cast<FeatureTreeNode*> (
					item->data (0, Qt::UserRole).value<std::intptr_t> ());

				featureTreeNode->selected_ = (item->checkState (0) == Qt::Checked);
				featureSelectionCallback_ (featureTreeNode);

				if (item->checkState (0) == Qt::Checked) {
					++selectedSubfeatures_;
				} else {
					--selectedSubfeatures_;
				}

				UpdateSelectedSubfeaturesLabel ();
			});
			
			selectedSubfeaturesLabel_ = new QLabel;
			UpdateSelectedSubfeaturesLabel ();
			selectedSubfeaturesLabel_->setSizePolicy (QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);

			layout->addWidget (selectedSubfeaturesLabel_, 2, 1);
			layout->addWidget (treeWidget, 3, 1);

			treeWidget_ = treeWidget;
		}

		layout->setColumnStretch (1, 1);
		widget->setLayout (layout);

		return widget;
	}

private:
	static QLabel* CreateMainLabel (const QString& text)
	{
		auto mainLabel = new QLabel;
		auto font = mainLabel->font ();
		font.setPointSize (font.pointSize () * 1.4);
		font.setBold (true);
		mainLabel->setFont (font);
		mainLabel->setText (text);
		mainLabel->setAlignment (Qt::AlignLeft);

		return mainLabel;
	}

	static QLabel* CreateDescriptionLabel (const QString& text)
	{
		auto descriptionLabel = new QLabel;
		descriptionLabel->setText (text);
		descriptionLabel->setWordWrap (true);
		descriptionLabel->setAlignment (Qt::AlignLeft);
		descriptionLabel->setSizePolicy (QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);

		return descriptionLabel;
	}

	void UpdateSelectedSubfeaturesLabel ()
	{
		selectedSubfeaturesLabel_->setText (QString ("%1 of %2 subfeature(s) selected")
			.arg (selectedSubfeatures_).arg (totalSubfeatures_));
	}

	int CountChildren (FeatureTreeNode* node)
	{
		int result = static_cast<int> (node->children_.size ());
		for (auto& child : node->children_) {
			result += CountChildren (child);
		}
		return result;
	}

	QTreeWidgetItem* CreateTreeWidgetItem ()
	{
		auto result = new QTreeWidgetItem{ QStringList{ name_ } };
		result->setFlags (Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
		result->setCheckState (0, Qt::Checked);
		QVariant value;
		value.setValue (reinterpret_cast<std::intptr_t> (this));
		result->setData (0, Qt::UserRole, value);

		for (auto& child : children_) {
			result->addChild (child->CreateTreeWidgetItem ());
		}

		return result;
	}

public:
	void AddChild (FeatureTreeNode* child)
	{
		children_.push_back (child);
		child->parent_ = this;
	}

	std::int64_t GetSize () const
	{
		return size_;
	}

	const std::vector<KylaUuid>& GetIds () const
	{
		return ids_;
	}

	const QString& GetDescription () const
	{
		return description_;
	}

	bool IsChecked () const
	{
		return selected_;
	}

	void FixupWidgetSize ()
	{
		if (treeWidget_) {
			treeWidget_->setMaximumHeight (treeWidget_->height ());
			treeWidget_->setSizeAdjustPolicy (QAbstractScrollArea::SizeAdjustPolicy::AdjustIgnored);
		}
	}
	
private:
	std::vector<KylaUuid> ids_;
	std::int64_t size_;
	QString name_, description_;
	QCheckBox* installCheckBox_;
	FeatureTreeNode* parent_ = nullptr;
	std::vector<FeatureTreeNode*> children_;
	bool selected_ = true;
	FeatureSelectionChangedCallback featureSelectionCallback_;
	int selectedSubfeatures_ = -1;
	int totalSubfeatures_ = -1;
	QLabel* selectedSubfeaturesLabel_ = nullptr;

	QTreeWidget* treeWidget_ = nullptr;
};

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
	// a list mapping of KylaFeatureTreeNode* -> internal Node* pointers

	featureTreeNodes_.clear ();

	std::unordered_map<const KylaFeatureTreeNode*, FeatureTreeNode*> treeToItem;

	FeatureSelectionChangedCallback callback = [&](const FeatureTreeNode* item) -> void {
		if (item->IsChecked ()) {
			requiredDiskSpace_ += item->GetSize ();
		} else {
			requiredDiskSpace_ -= item->GetSize ();
		}

		UpdateRequiredDiskSpace ();

		bool anyEnabled = false;
		for (auto& featureTreeNode : featureTreeNodes_) {
			if (featureTreeNode->IsChecked ()) {
				anyEnabled = true;
				break;
			}
		}

		ui->startInstallationButton->setEnabled (anyEnabled);
	};

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

		requiredDiskSpace_ += size;

		auto ftNode = std::make_unique<FeatureTreeNode> (featureIds, size, node->name, node->description,
			callback);
		treeToItem[node] = ftNode.get ();
		featureTreeNodes_.emplace_back (std::move (ftNode));
	}

	UpdateRequiredDiskSpace ();

	QList<FeatureTreeNode*> roots;

	for (const auto& node : nodes) {
		if (node->parent) {
			treeToItem.find (node->parent)->second->AddChild (
				treeToItem.find (node)->second);
		} else {
			roots.push_back (treeToItem.find (node)->second);
		}
	}

	auto featuresLayout = new QVBoxLayout;
	featuresLayout->setMargin (0);
	featuresLayout->setSpacing (0);

	for (auto& root : roots) {
		featuresLayout->addWidget (root->CreateWidget ());
	}

	featuresLayout->addSpacerItem (new QSpacerItem{ 0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding });
	
	ui->featuresAreaContent->setLayout (featuresLayout);

	this->show ();

	for (auto& root : roots) {
		root->FixupWidgetSize ();
	}
	
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
		ui->featuresAreaContent->setEnabled (false);
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

///////////////////////////////////////////////////////////////////////////////
void SetupDialog::showEvent (QShowEvent *)
{
#if KYLA_PLATFORM_WINDOWS
	taskbarButton_ = new QWinTaskbarButton (this);
	taskbarButton_->setWindow (this->windowHandle ());

	taskbarProgress_ = taskbarButton_->progress ();
#endif
}

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
		if (featureNode->IsChecked ()) {
			result.insert (result.end (), 
				featureNode->GetIds ().begin (), featureNode->GetIds().end());
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
