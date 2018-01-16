#include <iostream>
#include <fstream>
#include <vector>
#include "CPU.h"

int main() {
	std::ifstream rom("invaders.rom", std::ios::binary | std::ios::ate);
	size_t size = rom.tellg();
	std::vector<char> buffer(size);
	rom.seekg(0, std::ios::beg);
	rom.read(buffer.data(), size);
	CPU cpu;
	cpu.LoadROM(size, buffer.data());

	while (true) {
		cpu.Step();
	}
	return 0;
}