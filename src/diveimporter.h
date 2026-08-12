// SPDX-License-Identifier: GPL-2.0
// AI-generated (Claude)
#ifndef DIVEIMPORTER_H
#define DIVEIMPORTER_H

#include <QByteArray>
#include <QString>
#include <QVector>

// Merges dives converted from Suunto JSON directly into an existing
// Subsurface dive log -- a .ssrf/.xml file, or a directory that is a
// git-backed dive log -- using Subsurface's own load/merge/save code
// (core/file.h, core/divelog.h, core/dive.h): the exact functions
// Subsurface itself uses for File > Import and for saving, so the write
// path (git or XML) is guaranteed as safe/compatible as Subsurface's own.
namespace DiveImporter {

// logPath must already exist -- this only merges into an existing log, it
// never creates a new one. Throws std::runtime_error on failure (bad log
// path, unparsable JSON, or save failure).
void importJsonIntoLog(const QString &logPath, const QVector<QByteArray> &convertedJsons);

}

#endif
