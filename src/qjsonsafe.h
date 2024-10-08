/* QJSON safe access functions.
 * Safe functions check for criteria like type match & key presence.
 * If a check does not pass, JsonParseError is thrown.
 */

#ifndef _QJSONSAFE_H_
#define _QJSONSAFE_H_

#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>

struct JsonParseError : public std::logic_error {
	JsonParseError(const QString &str) : logic_error(str.toStdString()) {}
};

namespace QJsonSafe {

QJsonObject safeObject(const QJsonValue&);
QJsonObject safeObject(const QJsonObject &parent, const QString &key);

QJsonArray safeArray(const QJsonValue&, qsizetype size);
QJsonArray safeArray(const QJsonObject &parent, const QString &key);
QJsonArray safeArray(const QJsonObject &parent, const QString &key, qsizetype size);

unsigned int safeUInt(const QJsonValue&);
unsigned int safeUInt(const QJsonObject &parent, const QString &key);

double safeDouble(const QJsonValue&);
double safeDouble(const QJsonObject &parent, const QString &key);

bool safeBool(const QJsonValue&);
bool safeBool(const QJsonObject &parent, const QString &key);

QString safeString(const QJsonValue&);
QString safeString(const QJsonObject &parent, const QString &key);

}; // namespace QJsonSafe

#endif
