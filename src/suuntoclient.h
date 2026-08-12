// SPDX-License-Identifier: GPL-2.0
// AI-generated (Claude)
#ifndef SUUNTOCLIENT_H
#define SUUNTOCLIENT_H

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QString>
#include <QVector>

#include <stdexcept>

class QNetworkAccessManager;

// This talks to an UNDOCUMENTED, UNOFFICIAL Suunto/Sports-Tracker cloud API
// (api.sports-tracker.com). Using this against your own Suunto account may
// violate Suunto's Terms of Service -- read them before using this. Use
// only with your own account and your own data.
//
// C++ port of suunto-export/export_suunto_dives.py's SuuntoClient. Methods
// below block the calling thread (each does a synchronous request via a
// nested QEventLoop) -- callers that must not block their thread (e.g. a
// GUI's main thread) should run a SuuntoClient from a worker thread.

class SuuntoAPIError : public std::runtime_error {
public:
	explicit SuuntoAPIError(const std::string &what) : std::runtime_error(what) {}
};

struct SuuntoSession {
	QString email;
	QString sessionKey;
	bool valid = false;
};

class SuuntoClient : public QObject {
	Q_OBJECT
public:
	explicit SuuntoClient(QObject *parent = nullptr);

	QString sessionKey() const { return sessionKey_; }
	void setSessionKey(const QString &key) { sessionKey_ = key; }
	bool hasSession() const { return !sessionKey_.isEmpty(); }

	// Throws SuuntoAPIError on failure. On success, sessionKey() is set and
	// the raw login response is returned.
	QJsonObject login(const QString &email, const QString &password);

	// Cheap probe to check the current session key is still accepted by the
	// server, without pulling any real data.
	bool verifySession();

	// Chronological (oldest first) list of dive-activity workouts only
	// (scuba diving / freediving), paginating through the API as needed.
	QVector<QJsonObject> listDives(qint64 sinceMs = 0);

	QByteArray fetchSmlJson(const QString &key);
	QByteArray fetchFit(const QString &key);

	static QString defaultSessionPath();
	static SuuntoSession loadSession(const QString &path);
	// Throws std::runtime_error if the cache file can't be written.
	static void saveSession(const QString &path, const QString &email, const QString &sessionKey);
	// Simple "key=value" lines file (like a .env file); throws on I/O error.
	static QMap<QString, QString> loadCredentialsFile(const QString &path);

private:
	QJsonArray listWorkouts(qint64 sinceMs, int limit, int offset);
	QByteArray request(const QString &method, const QString &path, const QByteArray &data = QByteArray(),
			    const QMap<QString, QString> &headers = {});
	QNetworkAccessManager &nam();

	QNetworkAccessManager *nam_ = nullptr;
	QString sessionKey_;
};

#endif
