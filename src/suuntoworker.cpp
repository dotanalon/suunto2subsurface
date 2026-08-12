// SPDX-License-Identifier: GPL-2.0
// AI-generated (Claude)
#include "suuntoworker.h"
#include "diveimporter.h"
#include "suuntoconverter.h"

#include <QDateTime>
#include <QDir>
#include <QFile>

SuuntoWorker::SuuntoWorker(QObject *parent) : QObject(parent)
{
}

void SuuntoWorker::login(const QString &sessionPath, const QString &email, const QString &password)
{
	SuuntoSession cached = SuuntoClient::loadSession(sessionPath);
	if (cached.valid) {
		client_.setSessionKey(cached.sessionKey);
		if (client_.verifySession()) {
			emit loginSucceeded(cached.email, true);
			return;
		}
		client_.setSessionKey(QString());
	}

	if (email.isEmpty() || password.isEmpty()) {
		emit loginFailed("no valid cached session; email and password are required");
		return;
	}

	try {
		client_.login(email, password);
	} catch (const std::exception &e) {
		emit loginFailed(QString::fromUtf8(e.what()));
		return;
	}

	try {
		SuuntoClient::saveSession(sessionPath, email, client_.sessionKey());
	} catch (const std::exception &) {
		// Non-fatal: just won't have a cached session for next run.
	}

	emit loginSucceeded(email, false);
}

void SuuntoWorker::fetchDiveList()
{
	try {
		dives_ = client_.listDives();
	} catch (const std::exception &e) {
		emit diveListFailed(QString::fromUtf8(e.what()));
		return;
	}
	emit diveListReady(dives_);
}

void SuuntoWorker::exportDives(const QVector<int> &indices, const QString &outDir, bool alsoFit)
{
	int succeeded = 0;
	int total = indices.size();
	int current = 0;

	for (int idx : indices) {
		++current;
		if (idx < 0 || idx >= dives_.size()) {
			emit exportFailed(idx, "internal error: dive index out of range");
			continue;
		}
		const QJsonObject &dive = dives_[idx];
		QString key = dive["key"].toString();
		qint64 startMs = dive["startTime"].toVariant().toLongLong();
		// Qt::UTC (not QTimeZone::UTC, a Qt 6.5+ addition) for portability to
		// Ubuntu 22.04's apt-packaged Qt6 (6.2.4), used by CI's AppImage build.
		QString stamp = QDateTime::fromMSecsSinceEpoch(startMs, Qt::UTC).toString("yyyyMMdd'T'HHmmss");
		QString base = QDir(outDir).filePath(stamp + "_" + key);

		emit exportProgress(current, total, QString("fetching dive %1 of %2 (%3)...").arg(current).arg(total).arg(key));

		QByteArray smlJson;
		try {
			smlJson = client_.fetchSmlJson(key);
		} catch (const std::exception &e) {
			emit exportFailed(idx, QString::fromUtf8(e.what()));
			continue;
		}

		QByteArray converted;
		bool convertOk = true;
		try {
			converted = SuuntoConverter::convertBytes(smlJson);
		} catch (const std::exception &e) {
			emit exportFailed(idx, QString("could not convert JSON, writing raw response instead: %1")
						       .arg(e.what()));
			convertOk = false;
		}

		QFile jsonFile(base + ".json");
		if (!jsonFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			emit exportFailed(idx, "could not write " + jsonFile.fileName());
			continue;
		}
		jsonFile.write(convertOk ? converted : smlJson);
		jsonFile.close();

		if (alsoFit) {
			try {
				QByteArray fit = client_.fetchFit(key);
				QFile fitFile(base + ".fit");
				if (fitFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
					fitFile.write(fit);
					fitFile.close();
				} else {
					emit exportFailed(idx, "could not write " + fitFile.fileName());
					continue;
				}
			} catch (const std::exception &e) {
				emit exportFailed(idx, QString("could not fetch .fit: %1").arg(e.what()));
				continue;
			}
		}

		++succeeded;
	}

	emit exportFinished(succeeded, total);
}

void SuuntoWorker::importDirectly(const QVector<int> &indices, const QString &logPath)
{
	QVector<QByteArray> converted;
	int total = indices.size();
	int current = 0;

	for (int idx : indices) {
		++current;
		if (idx < 0 || idx >= dives_.size()) {
			emit exportFailed(idx, "internal error: dive index out of range");
			continue;
		}
		const QJsonObject &dive = dives_[idx];
		QString key = dive["key"].toString();

		emit exportProgress(current, total, QString("fetching dive %1 of %2 (%3)...").arg(current).arg(total).arg(key));

		QByteArray smlJson;
		try {
			smlJson = client_.fetchSmlJson(key);
		} catch (const std::exception &e) {
			emit exportFailed(idx, QString::fromUtf8(e.what()));
			continue;
		}

		try {
			converted.append(SuuntoConverter::convertBytes(smlJson));
		} catch (const std::exception &e) {
			emit exportFailed(idx, QString("could not convert JSON: %1").arg(e.what()));
			continue;
		}
	}

	if (converted.isEmpty()) {
		emit directImportFinished(0, "no dives were successfully fetched and converted");
		return;
	}

	try {
		DiveImporter::importJsonIntoLog(logPath, converted);
	} catch (const std::exception &e) {
		emit directImportFinished(0, QString::fromUtf8(e.what()));
		return;
	}

	emit directImportFinished(converted.size(), QString());
}
