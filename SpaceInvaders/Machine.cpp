#include "Machine.h"
#include <fstream>

Machine::Machine() : display(cpu) {
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

void Machine::Run() {
	while (!glfwWindowShouldClose(renderer.GetWindow())) {
		glfwPollEvents();
		display.ConvertImage();
		renderer.Render();
	}
}

void Machine::Emulate() {
	while (running) {
		cpu.Step();
	}
}