/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#include "SplashDialog.h"
#include <QApplication>

///////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	SetupContext setupContext;

	SplashDialog w (&setupContext, "kyla");
	w.show();

	return a.exec();
}
