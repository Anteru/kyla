#include "startdialog.h"
#include <ui_startdialog.h>

#include <QFileDialog>

#include <vector>

namespace {
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
}

StartDialog::StartDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::StartDialog)
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

	kylaLib_ = new QLibrary ("kyla", this);
	createInstaller_ = reinterpret_cast<kylaCreateInstallerFunction> (kylaLib_->resolve ("kylaCreateInstaller"));
	destroyInstaller_ = reinterpret_cast<kylaDestroyInstallerFunction> (kylaLib_->resolve ("kylaDestroyInstaller"));

	createInstaller_ (KYLA_API_VERSION_1_0, &installer_);

	installer_->OpenSourceRepository (installer_,
		"http://work.anteru.net/kyla/glfw/",
		kylaRepositoryOption_ReadOnly,
		&sourceRepository_);

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
		ui->requiredDiskSpaceValue->setText (QString (tr ("%1 byte(s)"))
			.arg (totalSize));
		} else {
			ui->requiredDiskSpaceValue->setText (tr ("No features selected"));
		}
	});

	installer_->SetProgressCallback (installer_, [] (const struct KylaProgress* progress,
		void* context) -> void
	{
		QProgressBar* progressBar = static_cast<QProgressBar*> (context);
		progressBar->setValue (progress->totalProgress * 100);
	}, ui->progressBar);

	std::size_t resultSize = 0;
	installer_->QueryRepository (installer_, sourceRepository_,
		kylaRepositoryProperty_AvailableFilesets, &resultSize, nullptr);

	std::vector<KylaUuid> filesets;
	filesets.resize (resultSize / sizeof (KylaUuid));
	installer_->QueryRepository (installer_, sourceRepository_,
		kylaRepositoryProperty_AvailableFilesets, &resultSize, filesets.data ());

	for (const auto& fs : filesets) {
		std::size_t length = 0;
		installer_->QueryFileset (installer_, sourceRepository_,
			fs, kylaFilesetProperty_Name,
			&length, nullptr);
		std::vector<char> name;
		name.resize (length);
		installer_->QueryFileset (installer_, sourceRepository_,
			fs, kylaFilesetProperty_Name,
			&length, name.data ());

		std::int64_t filesetSize = 0;
		std::size_t filesetResultSize = sizeof (filesetSize);

		installer_->QueryFileset (installer_, sourceRepository_,
			fs, kylaFilesetProperty_Size,
			&filesetResultSize, &filesetSize);

		auto item = new FilesetListItem (fs,
			filesetSize, name.data ());
		ui->featureSelection->addItem (item);
		item->setCheckState (Qt::Checked);
	}

	connect (ui->startInstallationButton, &QPushButton::clicked,
		[=] () -> void {
		KylaTargetRepository targetRepository;
		installer_->OpenTargetRepository (installer_,
			ui->targetDirectoryEdit->text ().toUtf8 ().data (),
			kylaRepositoryOption_Create, &targetRepository);

		KylaDesiredState desiredState;
		
		std::vector<uint8_t> idStore;

		int itemCount = 0;
		for (int i = 0; i < ui->featureSelection->count (); ++i) {
			auto item = static_cast<FilesetListItem*> (ui->featureSelection->item (i));

			if (item->checkState () == Qt::Checked) {
				idStore.insert (idStore.end (),
					item->GetId ().bytes,
					item->GetId ().bytes + sizeof (item->GetId ().bytes));
				++itemCount;
			}
		}

		std::vector<uint8_t*> idPointers;
		for (int i = 0; i < itemCount; ++i) {
			idPointers.push_back (idStore.data () + idStore.size () / itemCount * i);
		}

		desiredState.filesetCount = itemCount;
		desiredState.filesetIds = idPointers.data ();

		installer_->Execute (installer_, kylaAction_Install,
			targetRepository, sourceRepository_, &desiredState);

		installer_->CloseRepository (installer_, targetRepository);
	});
}

StartDialog::~StartDialog()
{
	delete ui;

	installer_->CloseRepository (installer_, sourceRepository_);
	destroyInstaller_ (installer_);

	delete kylaLib_;
}
