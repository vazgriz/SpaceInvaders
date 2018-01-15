#include "Machine.h"
#include <fstream>

Machine::Machine() {
	std::ifstream rom("invaders.rom", std::ios::binary | std::ios::ate);
	size_t size = rom.tellg();
	std::vector<char> buffer(size);
	rom.seekg(0, std::ios::beg);
	rom.read(buffer.data(), size);

	cpu.LoadROM(size, buffer.data());

	emuThread = std::thread([=] {
		Emulate();
	});
}

Machine::~Machine() {
	running = false;
	emuThread.join();
}

void Machine::Emulate() {
	while (running) {
		cpu.Step();
	}
}