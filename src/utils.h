#ifndef _UTILS_H_
#define _UTILS_H_

#include <QMap>
#include <vector>
#include <QString>

// transform std::unordered_map of one type to another with a given transform function
template<typename K, typename V>
QMap<V, K> invertQMap(const QMap<K, V>& inMap) {
	QMap<V, K> outMap;
	std::for_each(inMap.keyValueBegin(), inMap.keyValueEnd(),
		[&outMap] (const std::pair<K, V> &p) {
			if (outMap.contains(p.second))
				throw std::range_error((QString("Duplicate values in input map: ")+p.second+"!").toStdString());
			outMap.insert(p.second, p.first);
		}
	);
	return outMap;
}

template<typename T>
T pack(std::vector<uint8_t> data) {
	// Uses little-endian (data[3] == most significant byte)
	if (data.size() != sizeof(T))
		throw std::invalid_argument("data.size() != 4");
	T result = 0;
	for (int i = sizeof(T); i >= 0; i--) {
		result <<= 8;
		result |= data[i];
	}
	return result;
}

template<typename T>
T pack_reverse(std::vector<uint8_t> data) {
	// Uses big-endian (data[0] == most significant byte)
	if (data.size() != sizeof(T))
		throw std::invalid_argument("data.size() != 4");
	T result = 0;
	for (unsigned i = 0; i < sizeof(T); i++) {
		result <<= 8;
		result |= data[i];
	}
	return result;
}

#endif
