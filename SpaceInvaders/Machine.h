#pragma once

#include <thread>
#include "CPU.h"
#include "Renderer.h"

class Machine {
public:
	Machine();
	~Machine();

private:
	CPU cpu;
	Renderer renderer;
	std::thread emuThread;
	bool running = true;

	void Emulate();
};

