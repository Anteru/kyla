#include "SplashDialog.h"
#include <QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	SetupContext setupContext;

	SplashDialog w (&setupContext, "kyla");
	w.show();

	return a.exec();
}
