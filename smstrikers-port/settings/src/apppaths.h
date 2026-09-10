// Where the archive is, as opposed to where this executable is.

#pragma once

#include <QString>
#include <QStringList>

namespace AppPaths {

// The archive root for a given executable directory.
QString archiveRootFor(const QString& applicationDirPath);

// archiveRootFor(QCoreApplication::applicationDirPath()).
QString archiveRoot();

// The strikers.ini the application edits when no path was given on the command line: strikers.ini
// in the archive root.
QString defaultIniPath();

// The strikers.ini.example to seed a new file from, or an empty string if there is none.
QString findExample(const QString& iniPath, const QString& archiveRoot);

// The game executable in the archive root, or an empty string with `reason` filled in.
QString findGame(const QString& archiveRoot, QString* reason);

} // namespace AppPaths
