// SPDX-License-Identifier: GPL-2.0
// AI-generated (Claude)
#include "suuntoconverter.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <stdexcept>

namespace {

// Look up a Summary.Samples[].Attributes['suunto/sml'][typeName] block.
// See normalizeCloudJson() below for why this indirection exists.
QJsonValue summaryType(const QJsonObject &root, const QString &typeName)
{
	for (const QJsonValue &sv : root["Summary"].toObject()["Samples"].toArray()) {
		QJsonObject sml = sv.toObject()["Attributes"].toObject()["suunto/sml"].toObject();
		if (sml.contains(typeName))
			return sml[typeName];
	}
	return QJsonValue();
}

// Some dives report a gas's TankFillPressure/StartPressure/EndPressure
// already in bar instead of Pa -- seen on a real multi-gas dive where gas 0
// correctly had 23200000 (Pa, == 232 bar) but gas 1 (a tank with no paired
// pressure transmitter) had a bare 232. No real dive pressure is ever this
// small in Pa, so treat anything in that range as already-bar and rescale
// it, rather than importing it as a near-zero working pressure.
double fixPressureUnit(double value)
{
	if (value > 0 && value < 10000)
		return value * 100000;
	return value;
}

// The Suunto *cloud* API's 'sml' export -- what SuuntoClient downloads via
// GET workouts/{key}/sml -- uses a different envelope than the Suunto
// *app's* own manual "export as JSON" share feature (the shape
// core/import-suunto-json.cpp and the DeviceLog fallback below were
// originally written against). The cloud shape nests everything under
// Data/Summary .Samples[].Attributes['suunto/sml'][<Type>] instead of a flat
// DeviceLog.Header / DeviceLog.Samples[]:
//   Summary.Samples[]  with an Attributes['suunto/sml']['Header'] entry and
//                      an Attributes['suunto/sml']['DiveHeader'] entry
//                      (which holds the real Gases list, as plain
//                      percentages, not the 0.0-1.0 fractions the DeviceLog
//                      shape uses)
//   Data.Samples[]     each wraps the per-instant fields (Depth, Ceiling,
//                      NoDecTime, Cylinders, DiveEvents, Events,
//                      Temperature, ...) one level deeper, in
//                      Attributes['suunto/sml']['Sample'].
// This flattens that into the same (header, samples) shape the rest of this
// file expects. Returns false (header left null) if root isn't cloud-shaped
// (no Summary/Data keys).
bool normalizeCloudJson(const QJsonObject &root, QJsonObject *header, QJsonArray *samples)
{
	if (!root.contains("Summary") || !root.contains("Data"))
		return false;

	QJsonValue headerVal = summaryType(root, "Header");
	if (!headerVal.isObject())
		return false;
	*header = headerVal.toObject();

	QJsonObject diveHeader = summaryType(root, "DiveHeader").toObject();
	QJsonArray gases = diveHeader["Gases"].toArray();
	QJsonArray usedGases;
	for (const QJsonValue &g : gases) {
		if (g.toObject()["TankSize"].toDouble() > 0)
			usedGases.append(g);
	}

	QJsonObject diving;
	if (!usedGases.isEmpty()) {
		QJsonArray outGases;
		for (const QJsonValue &gv : usedGases) {
			QJsonObject g = gv.toObject();
			QJsonObject out;
			out["Oxygen"] = g["Oxygen"].toDouble() / 100.0;
			out["Helium"] = g["Helium"].toDouble() / 100.0;
			out["TankSize"] = g["TankSize"].toDouble();
			out["TankFillPressure"] = fixPressureUnit(g["TankFillPressure"].toDouble());
			out["StartPressure"] = fixPressureUnit(g["StartPressure"].toDouble());
			out["EndPressure"] = fixPressureUnit(g["EndPressure"].toDouble());
			outGases.append(out);
		}
		diving["Gases"] = outGases;
	}
	// GfLow/GfHigh aren't part of the app's own DeviceLog schema (there's no
	// existing convention to match), but core/import-suunto-json.cpp reads
	// them from here (parse_deco_settings()). This is what lets a single
	// converted JSON carry the gradient factors without a paired FIT.
	if (diveHeader.contains("LowGf") && diveHeader.contains("HighGf") &&
	    !diveHeader["LowGf"].isNull() && !diveHeader["HighGf"].isNull()) {
		diving["GfLow"] = diveHeader["LowGf"];
		diving["GfHigh"] = diveHeader["HighGf"];
	}
	if (!diving.isEmpty())
		(*header)["Diving"] = diving;

	for (const QJsonValue &iv : root["Data"].toObject()["Samples"].toArray()) {
		QJsonObject item = iv.toObject();
		QJsonObject sml = item["Attributes"].toObject()["suunto/sml"].toObject();
		if (!sml.contains("Sample"))
			continue;
		QJsonObject flat = sml["Sample"].toObject();
		flat["TimeISO8601"] = item["TimeISO8601"];
		samples->append(flat);
	}
	return true;
}

// jsonData may be the cloud 'sml' shape or an already-DeviceLog-shaped export.
void parseJsonDive(const QByteArray &jsonData, QJsonObject *header, QJsonArray *samples)
{
	QJsonParseError err;
	QJsonDocument doc = QJsonDocument::fromJson(jsonData, &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject())
		throw std::runtime_error("Suunto JSON: invalid JSON (" + err.errorString().toStdString() + ")");
	QJsonObject root = doc.object();

	if (!normalizeCloudJson(root, header, samples)) {
		QJsonObject deviceLog = root["DeviceLog"].toObject();
		*header = deviceLog["Header"].toObject();
		*samples = deviceLog["Samples"].toArray();
	}

	if (header->isEmpty())
		throw std::runtime_error("Suunto JSON: could not find dive header (checked both "
					  "cloud 'sml' and app-export 'DeviceLog' shapes)");
	if ((*header)["ActivityType"].toInt(0) != 51)
		throw std::runtime_error("Suunto JSON: not a dive activity (ActivityType=" +
					  QString::number((*header)["ActivityType"].toInt(0)).toStdString() + ")");
}

}

namespace SuuntoConverter {

QByteArray convertBytes(const QByteArray &jsonData)
{
	QJsonObject header;
	QJsonArray samples;
	parseJsonDive(jsonData, &header, &samples);

	QJsonObject deviceLog;
	deviceLog["Header"] = header;
	deviceLog["Samples"] = samples;
	QJsonObject out;
	out["DeviceLog"] = deviceLog;

	return QJsonDocument(out).toJson(QJsonDocument::Compact);
}

}
