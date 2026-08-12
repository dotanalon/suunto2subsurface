// SPDX-License-Identifier: GPL-2.0
// AI-generated (Claude)
//
// Throwaway dev tool: exercises SuuntoClient (login/session-cache/list/
// fetch) and SuuntoConverter against a real Suunto account, so the C++ port
// can be checked against suunto-export/converter.py's output for the same
// dive. Not part of the shipped product -- no GUI, minimal error handling.

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>

#include "suuntoclient.h"
#include "suuntoconverter.h"

#include <cstdio>

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);

	QCommandLineParser parser;
	parser.addOption({"credentials-file", "path to a file with email=/password= lines", "path"});
	parser.addOption({"dive", "1-based dive number to fetch+convert (default: just list dives)", "n"});
	parser.addOption({"session-file", "session cache path override", "path"});
	parser.addOption({"out", "output path for the converted JSON", "path"});
	parser.addHelpOption();
	parser.process(app);

	QString sessionPath = parser.value("session-file");
	if (sessionPath.isEmpty())
		sessionPath = SuuntoClient::defaultSessionPath();

	SuuntoClient client;
	SuuntoSession cached = SuuntoClient::loadSession(sessionPath);
	if (cached.valid) {
		client.setSessionKey(cached.sessionKey);
		if (client.verifySession())
			fprintf(stderr, "reusing cached session for %s\n", qPrintable(cached.email));
		else
			client.setSessionKey(QString());
	}

	if (!client.hasSession()) {
		if (!parser.isSet("credentials-file")) {
			fprintf(stderr, "no cached session and no --credentials-file given\n");
			return 1;
		}
		QMap<QString, QString> creds;
		try {
			creds = SuuntoClient::loadCredentialsFile(parser.value("credentials-file"));
		} catch (const std::exception &e) {
			fprintf(stderr, "%s\n", e.what());
			return 1;
		}
		QString email = creds.value("email");
		QString password = creds.value("password");
		if (email.isEmpty() || password.isEmpty()) {
			fprintf(stderr, "credentials file must have email= and password= lines\n");
			return 1;
		}
		try {
			client.login(email, password);
		} catch (const std::exception &e) {
			fprintf(stderr, "login failed: %s\n", e.what());
			return 1;
		}
		try {
			SuuntoClient::saveSession(sessionPath, email, client.sessionKey());
		} catch (const std::exception &e) {
			fprintf(stderr, "warning: %s\n", e.what());
		}
		fprintf(stderr, "logged in as %s, session cached to %s\n", qPrintable(email),
			qPrintable(sessionPath));
	}

	QVector<QJsonObject> dives;
	try {
		dives = client.listDives();
	} catch (const std::exception &e) {
		fprintf(stderr, "listDives failed: %s\n", e.what());
		return 1;
	}
	fprintf(stderr, "found %d dive(s)\n", dives.size());
	for (int i = 0; i < dives.size(); ++i) {
		const QJsonObject &d = dives[i];
		fprintf(stderr, "  #%d key=%s startTime=%lld\n", i + 1, qPrintable(d["key"].toString()),
			static_cast<long long>(d["startTime"].toVariant().toLongLong()));
	}

	if (!parser.isSet("dive"))
		return 0;

	int n = parser.value("dive").toInt();
	if (n < 1 || n > dives.size()) {
		fprintf(stderr, "--dive %d out of range (1..%d)\n", n, dives.size());
		return 1;
	}
	QString key = dives[n - 1]["key"].toString();

	QByteArray smlJson;
	try {
		smlJson = client.fetchSmlJson(key);
	} catch (const std::exception &e) {
		fprintf(stderr, "fetchSmlJson failed: %s\n", e.what());
		return 1;
	}

	QByteArray converted;
	try {
		converted = SuuntoConverter::convertBytes(smlJson);
	} catch (const std::exception &e) {
		fprintf(stderr, "convertBytes failed: %s\n", e.what());
		return 1;
	}

	QString outPath = parser.value("out");
	if (outPath.isEmpty())
		outPath = QString("dive_%1.json").arg(key);

	QFile out(outPath);
	if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		fprintf(stderr, "could not write %s\n", qPrintable(outPath));
		return 1;
	}
	out.write(converted);
	fprintf(stderr, "wrote %s (%lld bytes)\n", qPrintable(outPath), static_cast<long long>(converted.size()));

	QFile raw(outPath + ".raw");
	if (!raw.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		fprintf(stderr, "could not write %s\n", qPrintable(raw.fileName()));
		return 1;
	}
	raw.write(smlJson);
	fprintf(stderr, "wrote raw sml response to %s\n", qPrintable(raw.fileName()));

	return 0;
}
