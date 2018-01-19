#include "Utilities.h"

std::vector<char> LoadFile(const std::string& fileName) {
	std::ifstream file(fileName, std::ios::binary | std::ios::ate);
	size_t size = file.tellg();
	std::vector<char> buffer(size);
	file.seekg(0, std::ios::beg);
	file.read(buffer.data(), size);

	return buffer;
}