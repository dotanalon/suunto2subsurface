// SPDX-License-Identifier: GPL-2.0
// AI-generated (Claude)
#ifndef SUUNTOCRYPTO_H
#define SUUNTOCRYPTO_H

#include <QString>
#include <QList>
#include <QPair>

// Request-signing for the undocumented Suunto/Sports-Tracker cloud API
// (api.sports-tracker.com). This is a C++ port of the reverse-engineered
// scheme published in the MIT-licensed https://github.com/tajchert/suuntool
// (key material there is credited as extracted from APK
// com.stt.android.suunto v6.8.13). Using this against your own Suunto
// account may violate Suunto's Terms of Service.

namespace SuuntoCrypto {

QString deriveLoginSecret();
QString deriveTotpMasterSecret();

// path e.g. "login2"; params in request order, matching the form body.
QString signParams(const QString &path, const QList<QPair<QString, QString>> &params);

// salt is typically the account email. offsetMs allows tests to pin the
// 30-second time step deterministically.
QString generateTotp(const QString &salt, qint64 offsetMs = 0);

}

#endif
