// SPDX-License-Identifier: GPL-2.0
// AI-generated (Claude)
#include "suuntoclient.h"
#include "suuntocrypto.h"

#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QUrl>
#include <QtAlgorithms>

#include <algorithm>

namespace {

const char *BASE_URL = "https://api.sports-tracker.com/apiserver/v1/";
const char *APP_VERSION_CODE = "6008013";
const char *PACKAGE_NAME = "com.stt.android.suunto";

// Activity IDs for diving in the Sports-Tracker ActivityType enum.
const int ACTIVITY_SCUBADIVING = 78;
const int ACTIVITY_FREEDIVING = 79;

QByteArray urlEncodeFormComponent(const QString &s)
{
	QByteArray out;
	const QByteArray utf8 = s.toUtf8();
	for (unsigned char c : utf8) {
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
			out += static_cast<char>(c);
		} else if (c == ' ') {
			out += '+';
		} else {
			out += '%';
			out += QByteArray(1, static_cast<char>(c)).toHex().toUpper();
		}
	}
	return out;
}

QByteArray urlEncodeForm(const QList<QPair<QString, QString>> &params)
{
	QByteArray out;
	for (int i = 0; i < params.size(); ++i) {
		if (i)
			out += '&';
		out += urlEncodeFormComponent(params[i].first);
		out += '=';
		out += urlEncodeFormComponent(params[i].second);
	}
	return out;
}

}

SuuntoClient::SuuntoClient(QObject *parent) : QObject(parent)
{
}

QNetworkAccessManager &SuuntoClient::nam()
{
	if (!nam_)
		nam_ = new QNetworkAccessManager(this);
	return *nam_;
}

QByteArray SuuntoClient::request(const QString &method, const QString &path, const QByteArray &data,
				  const QMap<QString, QString> &headers)
{
	QNetworkRequest req(QUrl(QString(BASE_URL) + path));
	req.setHeader(QNetworkRequest::UserAgentHeader,
		      QString("%1/%2").arg(PACKAGE_NAME, APP_VERSION_CODE));
	req.setRawHeader("Accept-Language", "en");
	req.setTransferTimeout(30000);
	if (!sessionKey_.isEmpty())
		req.setRawHeader("STTAuthorization", sessionKey_.toUtf8());
	for (auto it = headers.constBegin(); it != headers.constEnd(); ++it)
		req.setRawHeader(it.key().toUtf8(), it.value().toUtf8());

	QNetworkReply *reply;
	if (method == "GET")
		reply = nam().get(req);
	else if (method == "POST")
		reply = nam().post(req, data);
	else
		throw SuuntoAPIError("unsupported HTTP method: " + method.toStdString());

	QEventLoop loop;
	QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	QByteArray body = reply->readAll();
	int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	QNetworkReply::NetworkError netErr = reply->error();
	reply->deleteLater();

	if (status == 401)
		throw SuuntoAPIError("session rejected (401) -- login again");
	if (status >= 400) {
		throw SuuntoAPIError(QString("HTTP %1: %2").arg(status).arg(QString::fromUtf8(body.left(200)))
					     .toStdString());
	}
	if (status == 0 && netErr != QNetworkReply::NoError)
		throw SuuntoAPIError("request failed: " + reply->errorString().toStdString());

	return body;
}

QJsonObject SuuntoClient::login(const QString &email, const QString &password)
{
	QString totp = SuuntoCrypto::generateTotp(email);
	QList<QPair<QString, QString>> signedParams = {{"l", email}, {"p", password}, {"totp", totp}};
	QString signature = SuuntoCrypto::signParams("login2", signedParams);

	QByteArray saltRaw(16, '\0');
	for (int i = 0; i < saltRaw.size(); ++i)
		saltRaw[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
	QString salt = QString::fromLatin1(saltRaw.toBase64(QByteArray::Base64UrlEncoding |
							     QByteArray::OmitTrailingEquals));

	QList<QPair<QString, QString>> formParams = signedParams;
	formParams.append({"timestamp", QString::number(QDateTime::currentMSecsSinceEpoch())});
	formParams.append({"salt", salt});
	formParams.append({"signature", signature});

	QMap<QString, QString> headers = {
		{"Content-Type", "application/x-www-form-urlencoded;charset=UTF-8"},
		{"x-login-email-verification-enabled", "true"},
	};

	QByteArray body = request("POST", "login2", urlEncodeForm(formParams), headers);
	QJsonObject session = QJsonDocument::fromJson(body).object();
	sessionKey_ = session["sessionkey"].toString();
	if (sessionKey_.isEmpty())
		throw SuuntoAPIError("login failed: no sessionkey in response");
	return session;
}

bool SuuntoClient::verifySession()
{
	try {
		listWorkouts(Q_INT64_C(1) << 53, 1, 0);
		return true;
	} catch (const SuuntoAPIError &) {
		return false;
	}
}

QJsonArray SuuntoClient::listWorkouts(qint64 sinceMs, int limit, int offset)
{
	QString path = QString("workouts?since=%1&limit=%2&offset=%3").arg(sinceMs).arg(limit).arg(offset);
	QByteArray body = request("GET", path);
	QJsonObject envelope = QJsonDocument::fromJson(body).object();
	if (envelope.contains("error") && !envelope["error"].isNull()) {
		QJsonObject err = envelope["error"].toObject();
		throw SuuntoAPIError(QString("API error %1: %2")
					     .arg(err["code"].toVariant().toString(), err["description"].toString())
					     .toStdString());
	}
	return envelope["payload"].toArray();
}

QVector<QJsonObject> SuuntoClient::listDives(qint64 sinceMs)
{
	QVector<QJsonObject> dives;
	const int pageSize = 100;
	int offset = 0;
	while (true) {
		QJsonArray items = listWorkouts(sinceMs, pageSize, offset);
		if (items.isEmpty())
			break;
		for (const QJsonValue &v : items) {
			QJsonObject item = v.toObject();
			int activityId = item["activityId"].toInt(-1);
			if (activityId == ACTIVITY_SCUBADIVING || activityId == ACTIVITY_FREEDIVING)
				dives.append(item);
		}
		if (items.size() < pageSize)
			break;
		offset += pageSize;
	}
	std::sort(dives.begin(), dives.end(), [](const QJsonObject &a, const QJsonObject &b) {
		return a["startTime"].toVariant().toLongLong() < b["startTime"].toVariant().toLongLong();
	});
	return dives;
}

QByteArray SuuntoClient::fetchSmlJson(const QString &key)
{
	return request("GET", QString("workouts/%1/sml").arg(key));
}

QByteArray SuuntoClient::fetchFit(const QString &key)
{
	// Note the singular "workout/" segment -- deliberate API quirk.
	return request("GET", QString("workout/exportFit/%1").arg(key));
}

QString SuuntoClient::defaultSessionPath()
{
	QString configHome = qEnvironmentVariable("XDG_CONFIG_HOME");
	if (configHome.isEmpty())
		configHome = QDir::homePath() + "/.config";
	return configHome + "/suunto2subsurface/session.json";
}

SuuntoSession SuuntoClient::loadSession(const QString &path)
{
	SuuntoSession result;
	QFile f(path);
	if (!f.exists() || !f.open(QIODevice::ReadOnly))
		return result;
	QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
	result.email = obj["email"].toString();
	result.sessionKey = obj["sessionkey"].toString();
	result.valid = !result.sessionKey.isEmpty();
	return result;
}

void SuuntoClient::saveSession(const QString &path, const QString &email, const QString &sessionKey)
{
	QFileInfo info(path);
	QDir().mkpath(info.absolutePath());
	QFile::setPermissions(info.absolutePath(),
			       QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

	QJsonObject obj;
	obj["email"] = email;
	obj["sessionkey"] = sessionKey;

	QFile f(path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
		throw std::runtime_error(("could not write session cache: " + path).toStdString());
	f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
	f.close();
	QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

QMap<QString, QString> SuuntoClient::loadCredentialsFile(const QString &path)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly))
		throw std::runtime_error(("could not open credentials file: " + path).toStdString());

	QMap<QString, QString> creds;
	while (!f.atEnd()) {
		QString line = QString::fromUtf8(f.readLine()).trimmed();
		if (line.isEmpty() || line.startsWith('#'))
			continue;
		int eq = line.indexOf('=');
		if (eq < 0)
			continue;
		creds[line.left(eq).trimmed().toLower()] = line.mid(eq + 1).trimmed();
	}
	return creds;
}
