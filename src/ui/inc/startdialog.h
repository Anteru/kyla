#ifndef STARTDIALOG_H
#define STARTDIALOG_H

#include <QDialog>

#include <QLibrary>
#include "Kyla.h"

namespace Ui {
class StartDialog;
}

class StartDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StartDialog(QWidget *parent = 0);
    ~StartDialog();

private:
    Ui::StartDialog *ui;
    QLibrary* kylaLib_ = nullptr;

    KylaInstaller* installer_ = nullptr;
    using kylaCreateInstallerFunction = int (*) (int kylaApiVersion, KylaInstaller** installer);
    using kylaDestroyInstallerFunction = int (*) (KylaInstaller* installer);

    kylaCreateInstallerFunction createInstaller_;
    kylaDestroyInstallerFunction destroyInstaller_;

    KylaSourceRepository sourceRepository_;
    KylaTargetRepository targetRepository_;
};

#endif // STARTDIALOG_H
