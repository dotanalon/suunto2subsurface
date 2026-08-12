// SPDX-License-Identifier: GPL-2.0
// AI-generated (Claude)
#include "suuntocrypto.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QMessageAuthenticationCode>
#include <QPasswordDigestor>

namespace {

const char *PACKAGE_NAME = "com.stt.android.suunto";
const char *TOTP_OBFUSCATION_KEY = "Bh8nsTyCeC0Ql2drMen78awk84AE3ZxW";

// Mirrors Go's utf8.DecodeRune-based replacement of invalid UTF-8 sequences
// with U+FFFD: decode leniently, then re-encode. Qt's UTF-8 codec follows
// the same Unicode "maximal subpart" recommended practice as Python's
// str.decode(errors="replace"), so this round-trip matches byte-for-byte.
QByteArray utf8Replace(const QByteArray &data)
{
	return QString::fromUtf8(data).toUtf8();
}

QByteArray xorCycling(const QByteArray &data, const QByteArray &key)
{
	QByteArray out(data.size(), Qt::Uninitialized);
	for (int i = 0; i < data.size(); ++i)
		out[i] = static_cast<char>(data[i] ^ key[i % key.size()]);
	return out;
}

QString deriveObfuscatedSecret(const QByteArray &keyPartsJoined, const QByteArray &pkg)
{
	QByteArray raw = QByteArray::fromBase64(keyPartsJoined);
	QByteArray mid = utf8Replace(raw);
	QByteArray xored = xorCycling(mid, pkg);
	return QString::fromUtf8(utf8Replace(xored));
}

// Base64-encoded, XOR-obfuscated key material (see deriveObfuscatedSecret above).
// Source: github.com/tajchert/suuntool internal/auth/keys.go
const char *LOGIN_KEY_PARTS =
	"FBkubDYmN28bWVQLLTsWFxcmaRB"
	"fN2AqIBc/IRAoNgshbxgnOGUVGlU3LC0xL0AuXXXXMXY"
	"RWQ4zIi0PWz4hekc1QGNTPlciNhEKV1teYSIkDGYY";
const char *TOTP_KEY_PARTS =
	"FBkubDYmN28bWVQLLTsWWhI+NAtILCNlPQc5Y"
	"BgiMRYjKA99Jj4HHFIqLmomOFttBQchNzcZU0QrODcDWz4hekc1QGNTPlciNhEKGl5GPDkzFyVX";

}

namespace SuuntoCrypto {

QString deriveLoginSecret()
{
	return deriveObfuscatedSecret(LOGIN_KEY_PARTS, PACKAGE_NAME);
}

QString deriveTotpMasterSecret()
{
	return deriveObfuscatedSecret(TOTP_KEY_PARTS, TOTP_OBFUSCATION_KEY);
}

QString signParams(const QString &path, const QList<QPair<QString, QString>> &params)
{
	QString msg = "POST&" + path;
	for (const auto &[k, v] : params)
		msg += "&" + k + "=" + v;
	msg += "&secret=" + deriveLoginSecret();

	QByteArray digest = QCryptographicHash::hash(msg.toUtf8(), QCryptographicHash::Sha256);
	return QString::fromLatin1(digest.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString generateTotp(const QString &salt, qint64 offsetMs)
{
	QString master = deriveTotpMasterSecret();

	// Go ranges over the string as Unicode code points ("runes"), not raw
	// bytes -- iterating QString's UTF-16 units matches that here since the
	// master secret only ever contains BMP code points.
	QByteArray pwd(master.size(), Qt::Uninitialized);
	for (int i = 0; i < master.size(); ++i)
		pwd[i] = static_cast<char>(master[i].unicode() & 0xFF);

	QByteArray key = QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha1, pwd,
							     salt.toUtf8(), 100, 32);

	qint64 counter = (QDateTime::currentMSecsSinceEpoch() + offsetMs) / 30000;
	QByteArray msg(8, Qt::Uninitialized);
	for (int i = 7; i >= 0; --i) {
		msg[i] = static_cast<char>(counter & 0xFF);
		counter >>= 8;
	}

	QMessageAuthenticationCode mac(QCryptographicHash::Sha1);
	mac.setKey(key);
	mac.addData(msg);
	QByteArray digest = mac.result();

	int offset = digest[digest.size() - 1] & 0x0F;
	quint32 code = (static_cast<quint32>(digest[offset] & 0x7F) << 24) |
		       (static_cast<quint32>(static_cast<quint8>(digest[offset + 1])) << 16) |
		       (static_cast<quint32>(static_cast<quint8>(digest[offset + 2])) << 8) |
		       static_cast<quint32>(static_cast<quint8>(digest[offset + 3]));

	return QString("%1").arg(code % 1000000, 6, 10, QChar('0'));
}

}
