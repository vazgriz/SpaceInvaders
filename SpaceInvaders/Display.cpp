#include "Display.h"

Display::Display(CPU& cpu) : cpu(cpu) {
	vram = reinterpret_cast<uint8_t*>(cpu.GetRAM(0x2400));
	image.resize(IMAGE_WIDTH * IMAGE_HEIGHT);
}

void Display::ConvertImage() {
	for (size_t i = 0; i < VRAM_SIZE; i++) {
		ConvertByte(vram[i], &image[i * 8]);
	}
}

void Display::ConvertByte(uint8_t source, Color4* dest) {
	for (size_t i = 0; i < 8; i++) {
		if (source & 1) {
			dest[i] = { 255, 255, 255, 255 };
		} else {
			dest[i] = {};
		}

		source >>= 1;
	}
}
