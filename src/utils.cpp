#include "utils.h"

uint32_t packToUint32(std::vector<uint8_t> data) {
	if (data.size() != 4)
		throw std::invalid_argument("data.size() != 4");
	uint32_t result = 0;
	for (unsigned i = 0; i < 4; i++) {
		result <<= 8;
		result |= data[i];
	}
	return result;
}
