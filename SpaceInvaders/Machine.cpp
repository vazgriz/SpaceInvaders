#include "Machine.h"

Machine::Machine() : display(cpu) {
	std::vector<char> rom = LoadFile("invaders.rom");

	cpu.LoadROM(rom.size(), rom.data());

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
		memcpy(renderer.GetVRAMMapping(), display.GetImage().data(), IMAGE_WIDTH * IMAGE_HEIGHT * 4);
		renderer.Render();
		cpu.QueueInterrupt(2, 0);
		cpu.QueueInterrupt(2, 25000);
	}
}

void Machine::Emulate() {
	while (running) {
		cpu.Step();
	}
}