#include "startdialog.h"
#include <ui_startdialog.h>

#include <QFileDialog>

#include <vector>

namespace {
class FilesetListItem : public QListWidgetItem
{
public:
	FilesetListItem (const KylaFilesetInfo& info, const char* description)
		: QListWidgetItem (description)
		, info_ (info)
	{
		this->setFlags (Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
		this->setCheckState (Qt::Unchecked);
	}

	const KylaFilesetInfo& GetInfo () const
	{
		return info_;
	}

private:
	KylaFilesetInfo info_;
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
		std::int64_t totalCount = 0;

		for (int i = 0; i < ui->featureSelection->count (); ++i) {
			auto item = static_cast<FilesetListItem*> (ui->featureSelection->item (i));

			if (item->checkState () == Qt::Checked) {
				totalSize += item->GetInfo ().fileSize;
				totalCount += item->GetInfo ().fileCount;
			}
		}

		if (totalCount > 0) {
		ui->requiredDiskSpaceValue->setText (QString (tr ("%1 file(s), %2 byte(s)"))
			.arg (totalCount).arg (totalSize));
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

	int filesetCount = 0;
	installer_->QueryFilesets (installer_, sourceRepository_,
		&filesetCount, nullptr);

	std::vector<KylaFilesetInfo> filesets;
	filesets.resize (filesetCount);
	installer_->QueryFilesets (installer_, sourceRepository_,
		&filesetCount, filesets.data ());

	for (const auto& fs : filesets) {
		int length = 0;
		installer_->QueryFilesetName (installer_, sourceRepository_,
			fs.id, &length, nullptr);
		std::vector<char> name;
		name.resize (length);
		installer_->QueryFilesetName (installer_, sourceRepository_,
			fs.id, &length, name.data ());

		ui->featureSelection->addItem (new FilesetListItem (fs, name.data ()));
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
					item->GetInfo ().id,
					item->GetInfo ().id + sizeof (item->GetInfo ().id));
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
