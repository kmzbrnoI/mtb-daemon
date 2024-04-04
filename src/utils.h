#ifndef __UTILS_H_
#define __UTILS_H_

#include <set>
#include <algorithm>

// To be replaced with std::set::contains in C++20
template<typename T>
bool setContains(std::set<T> s, const T& value) {
	return std::find(s.begin(), s.end(), value) != s.end();
}

#endif
