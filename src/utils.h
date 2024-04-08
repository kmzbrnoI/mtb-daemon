#ifndef __UTILS_H_
#define __UTILS_H_

#include <QMap>

// transform std::unordered_map of one type to another with a given transform function
template<typename K, typename V>
QMap<V, K> invertQMap(const QMap<K, V>& inMap)
{
	QMap<V, K> outMap;
	std::for_each(inMap.keyValueBegin(), inMap.keyValueEnd(),
		[&outMap] (const std::pair<K, V> &p) {
			if (outMap.contains(p.second))
				throw std::range_error("Duplicate values in input map!");
			outMap.insert(p.second, p.first);
		}
	);
	return outMap;
}

#endif
