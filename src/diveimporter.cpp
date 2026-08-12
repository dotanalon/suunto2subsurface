// SPDX-License-Identifier: GPL-2.0
// AI-generated (Claude)
#include "diveimporter.h"

#include "core/dive.h"
#include "core/divelog.h"
#include "core/file.h"

#include <stdexcept>

namespace DiveImporter {

void importJsonIntoLog(const QString &logPath, const QVector<QByteArray> &convertedJsons)
{
	std::string path = logPath.toStdString();

	// parse_file(..., &divelog) loads into the global divelog, since
	// save_dives_logic() below always saves *that* global, not whatever
	// divelog we might pass it.
	if (parse_file(path.c_str(), &divelog) < 0)
		throw std::runtime_error("could not open Subsurface dive log: " + logPath.toStdString());

	struct divelog importLog;
	for (const QByteArray &json : convertedJsons) {
		std::string buffer(json.constData(), static_cast<size_t>(json.size()));
		if (suunto_json_import(buffer, std::string(), &importLog) != 1)
			throw std::runtime_error("Suunto JSON: not recognized as a dive");
	}

	divelog.add_imported_dives(importLog, import_flags::is_downloaded);

	if (save_dives_logic(path.c_str(), false, false) != 0)
		throw std::runtime_error("could not save Subsurface dive log: " + logPath.toStdString());
}

}
