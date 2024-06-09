#include "qjsonsafe.h"

namespace QJsonSafe {

QJsonObject safeObject(const QJsonValue &json) {
	if (!json.isObject())
		throw JsonParseError("Object expected!");
	return json.toObject();
}

QJsonObject safeObject(const QJsonObject &parent, const QString &key) {
	if (!parent.contains(key))
		throw JsonParseError(key + " not found in parent object!");

	if (!parent[key].isObject())
		throw JsonParseError(key + ": object expected!");
	return parent[key].toObject();
}

QJsonArray safeArray(const QJsonValue &json, qsizetype size) {
	if (!json.isArray())
		throw JsonParseError("Array expected!");
	const QJsonArray array = json.toArray();
	if (array.size() != size)
		throw JsonParseError("size != "+QString::number(size));
	return array;
}

QJsonArray safeArray(const QJsonObject &parent, const QString &key) {
	if (!parent.contains(key))
		throw JsonParseError(key + " not found in parent object!");

	if (!parent[key].isArray())
		throw JsonParseError(key + ": array expected!");
	return parent[key].toArray();
}

QJsonArray safeArray(const QJsonObject &parent, const QString &key, qsizetype size) {
	if (!parent.contains(key))
		throw JsonParseError(key + " not found in parent object!");

	if (!parent[key].isArray())
		throw JsonParseError(key + ": array expected!");
	const QJsonArray array = parent[key].toArray();
	if (array.size() != size)
		throw JsonParseError(key + ": size != "+QString::number(size));
	return array;
}

unsigned int safeUInt(const QJsonValue &json) {
	if (!json.isDouble())
		throw JsonParseError("UInt expected!");
	int value = json.toInt(-1);
	if (value < 0)
		throw JsonParseError("UInt expected!");
	return static_cast<unsigned int>(value);
}

unsigned int safeUInt(const QJsonObject &parent, const QString &key) {
	if (!parent.contains(key))
		throw JsonParseError(key + " not found in parent object!");

	if (!parent[key].isDouble())
		throw JsonParseError(key + ": uint expected!");
	int value = parent[key].toInt(-1);
	if (value < 0)
		throw JsonParseError(key + ": uint expected!");
	return static_cast<unsigned int>(value);
}

int safeDouble(const QJsonValue &json) {
	if (!json.isDouble())
		throw JsonParseError("Double expected!");
	return json.toDouble();
}

int safeDouble(const QJsonObject &parent, const QString &key) {
	if (!parent.contains(key))
		throw JsonParseError(key + " not found in parent object!");

	if (!parent[key].isDouble())
		throw JsonParseError(key + ": double expected!");
	return parent[key].toDouble();
}

bool safeBool(const QJsonValue &json) {
	if (!json.isBool())
		throw JsonParseError("Bool expected!");
	return json.toBool();
}

bool safeBool(const QJsonObject &parent, const QString &key) {
	if (!parent.contains(key))
		throw JsonParseError(key + " not found in parent object!");

	if (!parent[key].isBool())
		throw JsonParseError(key + ": bool expected!");
	return parent[key].toBool();
}

QString safeString(const QJsonValue &json) {
	if (!json.isString())
		throw JsonParseError("String expected!");
	return json.toString();
}

QString safeString(const QJsonObject &parent, const QString &key) {
	if (!parent.contains(key))
		throw JsonParseError(key + " not found in parent object!");

	if (!parent[key].isString())
		throw JsonParseError("String expected!");
	return parent[key].toString();
}

} // namespace QJsonSafe
