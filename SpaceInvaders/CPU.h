#pragma once
#include <stdint.h>
#include <vector>
	
struct ConditionCodes {
	uint8_t z;
	uint8_t s;
	uint8_t p;
	uint8_t cy;
	uint8_t ac;
	uint8_t pad;
};

struct State {
	uint8_t a;
	uint8_t b;
	uint8_t c;
	uint8_t d;
	uint8_t e;
	uint8_t h;
	uint8_t l;
	uint16_t sp;
	uint16_t pc;
	std::vector<uint8_t> memory;
	ConditionCodes conditionCodes;
	uint8_t interruptEnable;
};

class CPU {
public:
	CPU();
	void LoadROM(size_t size, void* data);
	void Step();

private:
	State state;

	void UnrecognizedInstruction();
	uint16_t Combine(uint8_t low, uint8_t high);
	void Split(uint16_t value, uint8_t& low, uint8_t& high);
	uint8_t CheckParity(uint8_t result);
	void SetResultFlags(uint8_t result);
	void SetCarryFlag(uint16_t result);
	void SetCarryFlag(uint32_t result);
	void SetAuxCarryFlag(uint8_t a, uint8_t b);
	void Push(uint16_t value);
	uint16_t Pop();
};

