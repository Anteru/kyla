/**
[LICENSE BEGIN]
kyla Copyright (C) 2016 Matthäus G. Chajdas

This file is distributed under the BSD 2-clause license. See LICENSE for
details.
[LICENSE END]
*/

#include "SetupLogic.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>

SetupContext* g_setupContext = nullptr;

static QObject* SetupLogicSingletonProvider (QQmlEngine *engine, QJSEngine *scriptEngine)
{
	Q_UNUSED (engine)
	Q_UNUSED (scriptEngine)

	return new SetupLogic (g_setupContext);
}

///////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
	QGuiApplication a(argc, argv);

	SetupContext setupContext;
	g_setupContext = &setupContext;

	qmlRegisterSingletonType<SetupLogic> ("kyla", 1, 0, "SetupLogic",
		SetupLogicSingletonProvider);

	QQmlApplicationEngine engine;
	engine.load (QUrl (QStringLiteral ("qrc:/MainWindow.qml")));

	return a.exec();
}
