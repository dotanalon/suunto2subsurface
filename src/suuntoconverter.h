// SPDX-License-Identifier: GPL-2.0
// AI-generated (Claude)
#ifndef SUUNTOCONVERTER_H
#define SUUNTOCONVERTER_H

#include <QByteArray>

// Converts a Suunto *cloud* API dive export (the "sml" shape returned by
// GET workouts/{key}/sml) into the same DeviceLog.Header / DeviceLog.Samples
// JSON shape the Suunto app's own "export as JSON" share feature produces --
// the shape core/import-suunto-json.cpp actually parses. C++ port of
// suunto-export/converter.py; see that file's header comment for the
// cloud-vs-app envelope rationale.
namespace SuuntoConverter {

// Throws std::runtime_error if jsonData isn't a recognizable dive export
// (neither cloud 'sml' nor app 'DeviceLog' shape, or not a dive activity).
QByteArray convertBytes(const QByteArray &jsonData);

}

#endif
