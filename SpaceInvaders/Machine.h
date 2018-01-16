#pragma once

#include <thread>
#include "CPU.h"
#include "Display.h"
#include "Renderer.h"

class Machine {
public:
	Machine();
	~Machine();

	void Run();

private:
	CPU cpu;
	Display display;
	Renderer renderer;
	std::thread emuThread;
	bool running = true;

	void Emulate();
};

