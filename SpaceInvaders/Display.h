#pragma once
#include "CPU.h"
#include <vector>

#define VRAM_ADDR 0x2400
#define VRAM_SIZE (7 * 1024)
#define IMAGE_WIDTH 256
#define IMAGE_HEIGHT 224

struct Color4 {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
};

class Display {
public:
	Display(CPU& cpu);

	void ConvertImage();
	const std::vector<Color4>& GetImage() const { return image; }

private:
	CPU& cpu;
	uint8_t* vram;
	std::vector<Color4> image;
	void ConvertByte(uint8_t source, Color4* dest);
};

