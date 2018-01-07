#include <string>
#include <iostream>
#include <fstream>
#include <vector>

std::vector<char> ReadFile(const std::string& fileName) {
	std::ifstream file(fileName, std::ios::binary | std::ios::ate);
	if (!file) {
		std::cout << "Could not open \"" << fileName << "\"\n";
		return {};
	}

	size_t size = file.tellg();

	std::vector<char> buffer(size);

	file.seekg(0, std::ios::beg);
	file.read(buffer.data(), size);

	return buffer;
}

int main(int argc, char* args[]) {
	if (argc == 1) return EXIT_FAILURE;

	std::vector<char> buffer = ReadFile(args[1]);

	std::cout << buffer.size() << " bytes\n";

	return EXIT_SUCCESS;
}