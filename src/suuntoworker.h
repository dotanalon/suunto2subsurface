// SPDX-License-Identifier: GPL-2.0
// AI-generated (Claude)
#ifndef SUUNTOWORKER_H
#define SUUNTOWORKER_H

#include <QJsonObject>
#include <QObject>
#include <QVector>

#include "suuntoclient.h"

// Owns the SuuntoClient and does all network I/O. Lives in a dedicated
// QThread (see MainWindow) so login/list/export calls -- which block their
// calling thread, see suuntoclient.h -- never freeze the GUI. Invoke its
// slots via QMetaObject::invokeMethod(..., Qt::QueuedConnection) from the
// GUI thread; results come back via the signals below (Qt auto-queues
// these across threads since the receiver lives in the GUI thread).
class SuuntoWorker : public QObject {
	Q_OBJECT
public:
	explicit SuuntoWorker(QObject *parent = nullptr);

public slots:
	// Tries the cached session at sessionPath first; if that's missing or
	// rejected, logs in with email/password (both required in that case).
	void login(const QString &sessionPath, const QString &email, const QString &password);
	void fetchDiveList();
	// indices are into the QVector last reported via diveListReady().
	// outDir must exist. If alsoFit, also write the raw unmodified .fit
	// alongside each converted .json.
	void exportDives(const QVector<int> &indices, const QString &outDir, bool alsoFit);

	// Fetches+converts the selected dives, then merges them directly into
	// the existing Subsurface dive log at logPath (a .ssrf/.xml file or a
	// git-backed dive log directory) via DiveImporter.
	void importDirectly(const QVector<int> &indices, const QString &logPath);

signals:
	void loginSucceeded(const QString &email, bool usedCachedSession);
	void loginFailed(const QString &error);
	void diveListReady(const QVector<QJsonObject> &dives);
	void diveListFailed(const QString &error);
	void exportProgress(int current, int total, const QString &message);
	void exportFailed(int index, const QString &error);
	void exportFinished(int succeeded, int total);
	// error is empty on success.
	void directImportFinished(int importedCount, const QString &error);

private:
	SuuntoClient client_;
	QVector<QJsonObject> dives_;
};

#endif
