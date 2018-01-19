#include "CPU.h"

#include <stdexcept>
#include <iostream>
#include <iomanip>

#include "Disassemble.h"

CPU::CPU() {
	state = {};
	state.memory.resize(16 * 1024);
	shiftRegister = 0;
}

void CPU::LoadROM(size_t size, void* data) {
	memcpy(state.memory.data(), data, size);
}

void CPU::UnrecognizedInstruction() {
	throw std::runtime_error("Unrecognized instruction");
}

uint16_t CPU::Combine(uint8_t low, uint8_t high) {
	uint16_t lo = low;
	uint16_t hi = high;
	return (hi << 8) | lo;
}

void CPU::Split(uint16_t value, uint8_t& low, uint8_t& high) {
	low = static_cast<uint8_t>(value & 0xFF);
	high = static_cast<uint8_t>((value >> 8) & 0xFF);
}

uint8_t CPU::CheckParity(uint8_t result) {
	int p = 0;
	for (int i = 0; i < 8; i++) {
		if (result & 1) p++;
		result >>= 1;
	}
	return !(p & 1);
}

void CPU::SetResultFlags(uint8_t result) {
	state.conditionCodes.z = result == 0;
	state.conditionCodes.s = static_cast<int8_t>(result) < 0;
	state.conditionCodes.p = CheckParity(result);
}

void CPU::SetCarryFlag(uint16_t result) {
	state.conditionCodes.cy = result > 255;
}

void CPU::SetCarryFlag(uint32_t result) {
	state.conditionCodes.cy = result > 65535;
}

void CPU::SetAuxCarryFlag(uint8_t a, uint8_t b) {
	a &= 0xF;
	b &= 0xF;
	state.conditionCodes.ac = (a + b) > 16;
}

void CPU::Push(uint16_t value) {
	Split(value, state.memory[state.sp - 2], state.memory[state.sp - 1]);
	state.sp -= 2;
}

uint16_t CPU::Pop() {
	uint16_t result = Combine(state.memory[state.sp], state.memory[state.sp + 1]);
	state.sp += 2;
	return result;
}

void CPU::SetInput(size_t index, uint8_t value) {
	inputs[index] = value;
}

uint8_t CPU::GetOutput(size_t index) {
	return outputs[index];
}

uint8_t CPU::ReadInput(uint8_t index) {
	if (index == 3) {
		return shiftRegister >> (8 - (outputs[2] & 0x7));
	} else {
		return inputs[index];
	}
}

void CPU::WriteOutput(uint8_t index, uint8_t value) {
	if (index == 4) {
		shiftRegister = (static_cast<uint16_t>(value) << 8) | (shiftRegister >> 8);
	} else {
		outputs[index] = value;
	}
}

void CPU::Interrupt(size_t value) {
	Push(state.pc);
	state.pc = static_cast<uint16_t>(value * 8);
}

void CPU::QueueInterrupt(size_t value, size_t intstructionDelay) {
	if (state.interruptEnable) {
		std::lock_guard<std::mutex> lock(mutex);
		queue.push(CPU::Interrupt_t{ value, instructionCount + intstructionDelay });
	}
}

void CPU::Step() {
	if (state.interruptEnable) {
		std::lock_guard<std::mutex> lock(mutex);
		if (queue.size() > 0) {
			auto interrupt = queue.front();
			if (instructionCount >= interrupt.target) {
				queue.pop();
				Interrupt(interrupt.value);
			}
		}
	}

	uint8_t* inst = &state.memory[state.pc];
	state.pc++;
	instructionCount++;

	switch (*inst) {
		default:
			std::cout << std::hex << std::setw(4) << state.pc << " ";
			Disassemble(inst);
			std::cout << "\n";
			UnrecognizedInstruction();
			break;
		case 0x00:	//NOP
			break;
		case 0x01:	//LXI B, word
			state.c = inst[1];
			state.b = inst[2];
			state.pc += 2;
			break;
		case 0x02:	//STAX A
			state.memory[Combine(state.c, state.b)] = state.a;
			break;
		case 0x03:	//INX B
		{
			uint16_t temp = Combine(state.c, state.b);
			temp++;
			Split(temp, state.c, state.b);
			break;
		}
		case 0x04:	//INR B
			SetResultFlags(state.b + 1);
			SetAuxCarryFlag(state.b, 1);
			state.b++;
			break;
		case 0x05:	//DCR B
			SetResultFlags(state.b - 1);
			SetAuxCarryFlag(state.b, -1);
			state.b--;
			break;
		case 0x06:	//MVI B, byte
			state.b = inst[1];
			state.pc += 1;
			break;
		case 0x07:	//RLC
			state.conditionCodes.cy = (state.a >> 7);
			state.a = (state.a << 1) | (state.a >> 7);
			break;
		case 0x09:	//DAD B
		{
			uint32_t temp = Combine(state.l, state.h) + Combine(state.c, state.b);
			SetCarryFlag(temp);
			Split(static_cast<uint16_t>(temp & 0xFFFF), state.l, state.h);
			break;
		}
		case 0x0A:	//LDAX B
			state.a = state.memory[Combine(state.c, state.b)];
			break;
		case 0x0B:	//DCX B
		{
			uint16_t temp = Combine(state.c, state.b);
			temp--;
			Split(temp, state.c, state.b);
			break;
		}
		case 0x0C:	//INR C
			SetResultFlags(state.c + 1);
			SetAuxCarryFlag(state.c, 1);
			state.c++;
			break;
		case 0x0D:	//DCR C
			SetResultFlags(state.c - 1);
			SetAuxCarryFlag(state.c, -1);
			state.c--;
			break;
		case 0x0E:	//MVI C, byte
			state.c = inst[1];
			state.pc += 1;
			break;
		case 0x0F:	//RRC
			state.conditionCodes.cy = (state.a & 1);
			state.a = (state.a >> 1) | (state.a << 7);
			break;
		case 0x11:	//LXI D, word
			state.e = inst[1];
			state.d = inst[2];
			state.pc += 2;
			break;
		case 0x12:	//STAX D
			state.memory[Combine(state.e, state.d)] = state.a;
			break;
		case 0x13:	//INX D
		{
			uint16_t temp = Combine(state.e, state.d);
			temp++;
			Split(temp, state.e, state.d);
			break;
		}
		case 0x14:	//INR B
			SetResultFlags(state.d + 1);
			SetAuxCarryFlag(state.d, 1);
			state.d++;
			break;
		case 0x15:	//DCR D
			SetResultFlags(state.d - 1);
			SetAuxCarryFlag(state.d, -1);
			state.d--;
			break;
		case 0x16:	//MVI D, byte
			state.d = inst[1];
			state.pc += 1;
			break;
		case 0x17:	//RAL
		{
			uint8_t temp = state.conditionCodes.cy & 1;
			state.conditionCodes.cy = (state.a >> 7);
			state.a = (state.a << 1) | temp;
			break;
		}
		case 0x19:	//DAD D
		{
			uint32_t temp = Combine(state.l, state.h) + Combine(state.e, state.d);
			SetCarryFlag(temp);
			Split(static_cast<uint16_t>(temp & 0xFFFF), state.l, state.h);
			break;
		}
		case 0x1A:	//LDAX D
			state.a = state.memory[Combine(state.e, state.d)];
			break;
		case 0x1B:	//DCX D
		{
			uint16_t temp = Combine(state.e, state.d);
			temp--;
			Split(temp, state.e, state.d);
			break;
		}
		case 0x1C:	//INR E
			SetResultFlags(state.e + 1);
			SetAuxCarryFlag(state.e, 1);
			state.e++;
			break;
		case 0x1D:	//DCR E
			SetResultFlags(state.e - 1);
			SetAuxCarryFlag(state.e, -1);
			state.e--;
			break;
		case 0x1E:	//MVI E, byte
			state.e = inst[1];
			state.pc += 1;
			break;
		case 0x1F:	//RAR
		{
			int8_t temp = static_cast<int8_t>(state.a);	//uses C++'s arithmetic shift on signed numbers
			temp = temp >> 1;
			state.a = static_cast<uint8_t>(temp);
			break;
		}
		case 0x21:	//LXI H, word
			state.l = inst[1];
			state.h = inst[2];
			state.pc += 2;
			break;
		case 0x22:	//SHLD addr
		{
			uint16_t addr = Combine(inst[1], inst[2]);
			state.memory[addr] = state.l;
			state.memory[addr + 1] = state.h;
			state.pc += 2;
			break;
		}
		case 0x23:	//INX H
		{
			uint16_t temp = Combine(state.l, state.h);
			temp++;
			Split(temp, state.l, state.h);
			break;
		}
		case 0x24:	//INR H
			SetResultFlags(state.h + 1);
			SetAuxCarryFlag(state.h, 1);
			state.h++;
			break;
		case 0x25:	//DCR H
			SetResultFlags(state.h - 1);
			SetAuxCarryFlag(state.h, -1);
			state.h--;
			break;
		case 0x26:	//MVI H, byte
			state.h = inst[1];
			state.pc += 1;
			break;
		case 0x29:	//DAD H
		{
			uint32_t temp = Combine(state.l, state.h) + Combine(state.l, state.h);
			SetCarryFlag(temp);
			Split(static_cast<uint16_t>(temp & 0xFFFF), state.l, state.h);
			break;
		}
		case 0x2A:	//LHLD addr
		{
			uint16_t addr = Combine(inst[1], inst[2]);
			state.l = state.memory[addr];
			state.h = state.memory[addr + 1];
			state.pc += 2;
			break;
		}
		case 0x2B:	//DCX H
		{
			uint16_t temp = Combine(state.l, state.h);
			temp--;
			Split(temp, state.l, state.h);
			break;
		}
		case 0x2C:	//INR L
			SetResultFlags(state.l + 1);
			SetAuxCarryFlag(state.l, 1);
			state.l++;
			break;
		case 0x2D:	//DCR L
			SetResultFlags(state.l - 1);
			SetAuxCarryFlag(state.l, -1);
			state.l--;
			break;
		case 0x2E:	//MVI L, byte
			state.l = inst[1];
			state.pc += 1;
			break;
		case 0x2F:	//CMA
			state.a = ~state.a;
			break;
		case 0x31:	//LXI SP, word
			state.sp = Combine(inst[1], inst[2]);
			state.pc += 2;
			break;
		case 0x32: //STA addr
		{
			uint16_t addr = Combine(inst[1], inst[2]);
			state.memory[addr] = state.a;
			state.pc += 2;
			break;
		}
		case 0x33:	//INX SP
			state.sp++;
			break;
		case 0x34:	//INR M
		{
			uint8_t& temp = state.memory[Combine(state.l, state.h)];
			SetResultFlags(temp + 1);
			SetAuxCarryFlag(temp, 1);
			temp++;
			break;
		}
		case 0x35:	//DCR M
		{
			uint8_t& temp = state.memory[Combine(state.l, state.h)];
			SetResultFlags(temp - 1);
			SetAuxCarryFlag(temp, -1);
			temp--;
			break;
		}
		case 0x36:	//MVI M, byte
		{
			uint8_t& temp = state.memory[Combine(state.l, state.h)];
			temp = inst[1];
			state.pc += 1;
			break;
		}
		case 0x37: //STC
			state.conditionCodes.cy = 1;
			break;
		case 0x39:	//DAD SP
		{
			uint32_t temp = Combine(state.l, state.h) + state.sp;
			SetCarryFlag(temp);
			Split(static_cast<uint16_t>(temp & 0xFFFF), state.l, state.h);
			break;
		}
		case 0x3A:	//LDA addr
		{
			uint16_t addr = Combine(inst[1], inst[2]);
			state.a = state.memory[addr];
			state.pc += 2;
			break;
		}
		case 0x3B:	//DCR SP
			state.sp--;
			break;
		case 0x3C:	//INR A
			SetResultFlags(state.a + 1);
			SetAuxCarryFlag(state.a, 1);
			state.a++;
			break;
		case 0x3D:	//DCR A
			SetResultFlags(state.a - 1);
			SetAuxCarryFlag(state.a, -1);
			state.a--;
			break;
		case 0x3E:	//MVI A, byte
			state.a = inst[1];
			state.pc += 1;
			break;
		case 0x3F:	//CMC
			state.conditionCodes.cy = !state.conditionCodes.cy;
			break;
		case 0x40:	//MOV B, B
			break;
		case 0x41:	//MOV B, C
			state.b = state.c;
			break;
		case 0x42:	//MOV B, D
			state.b = state.d;
			break;
		case 0x43:	//MOV B, E
			state.b = state.e;
			break;
		case 0x44:	//MOV B, H
			state.b = state.h;
			break;
		case 0x45:	//MOV B, L
			state.b = state.l;
			break;
		case 0x46:	//MOV B, M
		{
			uint16_t addr = Combine(state.l, state.h);
			state.b = state.memory[addr];
			break;
		}
		case 0x47:	//MOV B, A
			state.b = state.a;
			break;
		case 0x48:	//MOV C, B
			state.c = state.b;
			break;
		case 0x49:	//MOV C, C
			break;
		case 0x4A:	//MOV C, D
			state.c = state.d;
			break;
		case 0x4B:	//MOV C, E
			state.c = state.e;
			break;
		case 0x4C:	//MOV C, H
			state.c = state.h;
			break;
		case 0x4D:	//MOV C, L
			state.c = state.l;
			break;
		case 0x4E:	//MOV C, M
		{
			uint16_t addr = Combine(state.l, state.h);
			state.c = state.memory[addr];
			break;
		}
		case 0x4F:	//MOV C, A
			state.c = state.a;
			break;
		case 0x50:	//MOV D, B
			state.d = state.b;
			break;
		case 0x51:	//MOV D, C
			state.d = state.c;
			break;
		case 0x52:	//MOV D, D
			break;
		case 0x53:	//MOV D, E
			state.d = state.e;
			break;
		case 0x54:	//MOV D, H
			state.d = state.h;
			break;
		case 0x55:	//MOV D, L
			state.d = state.l;
			break;
		case 0x56:	//MOV D, M
		{
			uint16_t addr = Combine(state.l, state.h);
			state.d = state.memory[addr];
			break;
		}
		case 0x57:	//MOV D, A
			state.d = state.a;
			break;
		case 0x58:	//MOV E, B
			state.e = state.b;
			break;
		case 0x59:	//MOV E, C
			state.e = state.c;
			break;
		case 0x5A:	//MOV E, D
			state.e = state.d;
			break;
		case 0x5B:	//MOV E, E
			break;
		case 0x5C:	//MOV E, H
			state.e = state.h;
			break;
		case 0x5D:	//MOV E, L
			state.e = state.l;
			break;
		case 0x5E:	//MOV E, M
		{
			uint16_t addr = Combine(state.l, state.h);
			state.e = state.memory[addr];
			break;
		}
		case 0x5F:	//MOV E, A
			state.e = state.a;
			break;
		case 0x60:	//MOV H, B
			state.h = state.b;
			break;
		case 0x61:	//MOV H, C
			state.h = state.c;
			break;
		case 0x62:	//MOV H, D
			state.h = state.d;
			break;
		case 0x63:	//MOV H, E
			state.h = state.e;
			break;
		case 0x64:	//MOV H, H
			break;
		case 0x65:	//MOV H, L
			state.h = state.l;
			break;
		case 0x66:	//MOV H, M
		{
			uint16_t addr = Combine(state.l, state.h);
			state.h = state.memory[addr];
			break;
		}
		case 0x67:	//MOV H, A
			state.h = state.a;
			break;
		case 0x68:	//MOV L, B
			state.l = state.b;
			break;
		case 0x69:	//MOV L, C
			state.l = state.c;
			break;
		case 0x6A:	//MOV L, D
			state.l = state.d;
			break;
		case 0x6B:	//MOV L, E
			state.l = state.e;
			break;
		case 0x6C:	//MOV L, H
			state.l = state.h;
			break;
		case 0x6D:	//MOV L, L
			break;
		case 0x6E:	//MOV L, M
		{
			uint16_t addr = Combine(state.l, state.h);
			state.l = state.memory[addr];
			break;
		}
		case 0x6F:	//MOV L, A
			state.l = state.a;
			break;
		case 0x70:	//MOV M, B
		{
			uint16_t addr = Combine(state.l, state.h);
			state.memory[addr] = state.b;
			break;
		}
		case 0x71:	//MOV M, C
		{
			uint16_t addr = Combine(state.l, state.h);
			state.memory[addr] = state.c;
			break;
		}
		case 0x72:	//MOV M, D
		{
			uint16_t addr = Combine(state.l, state.h);
			state.memory[addr] = state.d;
			break;
		}
		case 0x73:	//MOV M, E
		{
			uint16_t addr = Combine(state.l, state.h);
			state.memory[addr] = state.e;
			break;
		}
		case 0x74:	//MOV M, H
		{
			uint16_t addr = Combine(state.l, state.h);
			state.memory[addr] = state.h;
			break;
		}
		case 0x75:	//MOV M, L
		{
			uint16_t addr = Combine(state.l, state.h);
			state.memory[addr] = state.l;
			break;
		}
		//0x76 HLT
		case 0x77:	//MOV M, A
		{
			uint16_t addr = Combine(state.l, state.h);
			state.memory[addr] = state.a;
			break;
		}
		case 0x78:	//MOV A, B
			state.a = state.b;
			break;
		case 0x79:	//MOV A, C
			state.a = state.c;
			break;
		case 0x7A:	//MOV A, D
			state.a = state.d;
			break;
		case 0x7B:	//MOV A, E
			state.a = state.e;
			break;
		case 0x7C:	//MOV A, H
			state.a = state.h;
			break;
		case 0x7D:	//MOV A, L
			state.a = state.l;
			break;
		case 0x7E:	//MOV A, M
		{
			uint16_t addr = Combine(state.l, state.h);
			state.a = state.memory[addr];
			break;
		}
		case 0x7F:	//MOV A, A
			break;
		case 0x80:	//ADD B
		{
			uint8_t temp = state.a + state.b;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) + static_cast<uint16_t>(state.b)));
			state.a = temp;
			break;
		}
		case 0x81:	//ADD C
		{
			uint8_t temp = state.a + state.c;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) + static_cast<uint16_t>(state.c)));
			state.a = temp;
			break;
		}
		case 0x82:	//ADD D
		{
			uint8_t temp = state.a + state.d;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) + static_cast<uint16_t>(state.d)));
			state.a = temp;
			break;
		}
		case 0x83:	//ADD E
		{
			uint8_t temp = state.a + state.e;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) + static_cast<uint16_t>(state.e)));
			state.a = temp;
			break;
		}
		case 0x84:	//ADD H
		{
			uint8_t temp = state.a + state.h;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) + static_cast<uint16_t>(state.h)));
			state.a = temp;
			break;
		}
		case 0x85:	//ADD L
		{
			uint8_t temp = state.a + state.l;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) + static_cast<uint16_t>(state.l)));
			state.a = temp;
			break;
		}
		case 0x86:	//ADD M
		{
			uint16_t addr = Combine(state.l, state.h);
			uint8_t value = state.memory[addr];
			uint8_t temp = state.a + value;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) + static_cast<uint16_t>(value)));
			state.a = temp;
			break;
		}
		case 0x87:	//ADD A
		{
			uint8_t temp = state.a + state.a;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) + static_cast<uint16_t>(state.a)));
			state.a = temp;
			break;
		}
		case 0x88:	//ADC B
		{
			uint8_t temp = state.a + state.b + state.conditionCodes.cy;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) + static_cast<uint16_t>(state.b + state.conditionCodes.cy)));
			state.a = temp;
			break;
		}
		case 0x89:	//ADC C
		{
			uint8_t temp = state.a + state.c + state.conditionCodes.cy;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) + static_cast<uint16_t>(state.c) + state.conditionCodes.cy));
			state.a = temp;
			break;
		}
		case 0x8A:	//ADC D
		{
			uint8_t temp = state.a + state.d + state.conditionCodes.cy;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) + static_cast<uint16_t>(state.d) + state.conditionCodes.cy));
			state.a = temp;
			break;
		}
		case 0x8B:	//ADC E
		{
			uint8_t temp = state.a + state.e + state.conditionCodes.cy;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) + static_cast<uint16_t>(state.e) + state.conditionCodes.cy));
			state.a = temp;
			break;
		}
		case 0x8C:	//ADC H
		{
			uint8_t temp = state.a + state.h + state.conditionCodes.cy;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) + static_cast<uint16_t>(state.h) + state.conditionCodes.cy));
			state.a = temp;
			break;
		}
		case 0x8D:	//ADC L
		{
			uint8_t temp = state.a + state.l + state.conditionCodes.cy;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) + static_cast<uint16_t>(state.l) + state.conditionCodes.cy));
			state.a = temp;
			break;
		}
		case 0x8E:	//ADC M
		{
			uint16_t addr = Combine(state.l, state.h);
			uint8_t value = state.memory[addr];
			uint8_t temp = state.a + value + state.conditionCodes.cy;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) + static_cast<uint16_t>(value) + state.conditionCodes.cy));
			state.a = temp;
			break;
		}
		case 0x8F:	//ADC A
		{
			uint8_t temp = state.a + state.a + state.conditionCodes.cy;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) + static_cast<uint16_t>(state.a) + state.conditionCodes.cy));
			state.a = temp;
			break;
		}
		case 0x90:	//SUB B
		{
			uint8_t temp = state.a - state.b;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) - static_cast<uint16_t>(state.b)));
			state.a = temp;
			break;
		}
		case 0x91:	//SUB C
		{
			uint8_t temp = state.a - state.c;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) - static_cast<uint16_t>(state.c)));
			state.a = temp;
			break;
		}
		case 0x92:	//SUB D
		{
			uint8_t temp = state.a - state.d;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) - static_cast<uint16_t>(state.d)));
			state.a = temp;
			break;
		}
		case 0x93:	//SUB E
		{
			uint8_t temp = state.a - state.e;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) - static_cast<uint16_t>(state.e)));
			state.a = temp;
			break;
		}
		case 0x94:	//SUB H
		{
			uint8_t temp = state.a - state.h;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) - static_cast<uint16_t>(state.h)));
			state.a = temp;
			break;
		}
		case 0x95:	//SUB L
		{
			uint8_t temp = state.a - state.l;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) - static_cast<uint16_t>(state.l)));
			state.a = temp;
			break;
		}
		case 0x96:	//SUB M
		{
			uint16_t addr = Combine(state.l, state.h);
			uint8_t value = state.memory[addr];
			uint8_t temp = state.a - value;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) - static_cast<uint16_t>(value)));
			state.a = temp;
			break;
		}
		case 0x97:	//SUB A
		{
			uint8_t temp = state.a - state.a;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) - static_cast<uint16_t>(state.a)));
			state.a = temp;
			break;
		}
		case 0x98:	//SBB B
		{
			uint8_t temp = state.a - state.b - state.conditionCodes.cy;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) - static_cast<uint16_t>(state.b - state.conditionCodes.cy)));
			state.a = temp;
			break;
		}
		case 0x99:	//SBB C
		{
			uint8_t temp = state.a - state.c - state.conditionCodes.cy;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) - static_cast<uint16_t>(state.c) - state.conditionCodes.cy));
			state.a = temp;
			break;
		}
		case 0x9A:	//SBB D
		{
			uint8_t temp = state.a - state.d - state.conditionCodes.cy;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) - static_cast<uint16_t>(state.d) - state.conditionCodes.cy));
			state.a = temp;
			break;
		}
		case 0x9B:	//SBB E
		{
			uint8_t temp = state.a - state.e - state.conditionCodes.cy;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) - static_cast<uint16_t>(state.e) - state.conditionCodes.cy));
			state.a = temp;
			break;
		}
		case 0x9C:	//SBB H
		{
			uint8_t temp = state.a - state.h - state.conditionCodes.cy;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) - static_cast<uint16_t>(state.h) - state.conditionCodes.cy));
			state.a = temp;
			break;
		}
		case 0x9D:	//SBB L
		{
			uint8_t temp = state.a - state.l - state.conditionCodes.cy;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) - static_cast<uint16_t>(state.l) - state.conditionCodes.cy));
			state.a = temp;
			break;
		}
		case 0x9E:	//SBB M
		{
			uint16_t addr = Combine(state.l, state.h);
			uint8_t value = state.memory[addr];
			uint8_t temp = state.a - value - state.conditionCodes.cy;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) - static_cast<uint16_t>(value) - state.conditionCodes.cy));
			state.a = temp;
			break;
		}
		case 0x9F:	//SBB A
		{
			uint8_t temp = state.a - state.a - state.conditionCodes.cy;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) - static_cast<uint16_t>(state.a) - state.conditionCodes.cy));
			state.a = temp;
			break;
		}
		case 0xA0:	//ANA B
		{
			uint8_t temp = state.a & state.b;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xA1:	//ANA C
		{
			uint8_t temp = state.a & state.c;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xA2:	//ANA D
		{
			uint8_t temp = state.a & state.d;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xA3:	//ANA E
		{
			uint8_t temp = state.a & state.e;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xA4:	//ANA H
		{
			uint8_t temp = state.a & state.h;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xA5:	//ANA L
		{
			uint8_t temp = state.a & state.l;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xA6:	//ANA M
		{
			uint16_t addr = Combine(state.l, state.h);
			uint8_t temp = state.a & state.memory[addr];
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xA7:	//ANA A
		{
			uint8_t temp = state.a & state.a;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xA8:	//XRA B
		{
			uint8_t temp = state.a ^ state.b;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xA9:	//XRA C
		{
			uint8_t temp = state.a ^ state.c;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xAA:	//XRA D
		{
			uint8_t temp = state.a ^ state.d;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xAB:	//XRA E
		{
			uint8_t temp = state.a ^ state.e;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xAC:	//XRA H
		{
			uint8_t temp = state.a ^ state.h;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xAD:	//XRA L
		{
			uint8_t temp = state.a ^ state.l;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xAE:	//XRA M
		{
			uint16_t addr = Combine(state.l, state.h);
			uint8_t temp = state.a ^ state.memory[addr];
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xAF:	//XRA A
		{
			uint8_t temp = state.a ^ state.a;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xB0:	//ORA B
		{
			uint8_t temp = state.a | state.b;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xB1:	//ORA C
		{
			uint8_t temp = state.a | state.c;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xB2:	//ORA D
		{
			uint8_t temp = state.a | state.d;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xB3:	//ORA E
		{
			uint8_t temp = state.a | state.e;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xB4:	//ORA H
		{
			uint8_t temp = state.a | state.h;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xB5:	//ORA L
		{
			uint8_t temp = state.a | state.l;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xB6:	//ORA M
		{
			uint16_t addr = Combine(state.l, state.h);
			uint8_t temp = state.a | state.memory[addr];
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xB7:	//ORA A
		{
			uint8_t temp = state.a | state.a;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xC0:	//RNZ
			if (!state.conditionCodes.z) {
				state.pc = Pop();
			}
			break;
		case 0xC1:	//POP B
			Split(Pop(), state.c, state.b);
			break;
		case 0xC2:	//JNZ addr
			if (state.conditionCodes.z == 0) {
				state.pc = Combine(inst[1], inst[2]);
			} else {
				state.pc += 2;
			}
			break;
		case 0xC3:	//JMP addr
		{
			state.pc = Combine(inst[1], inst[2]);
			break;
		}
		case 0xC4:	//CNZ
			if (!state.conditionCodes.z) {
				Push(state.pc);
				state.pc = Combine(inst[1], inst[2]);
			} else {
				state.pc += 2;
			}
			break;
		case 0xC5:	//PUSH B
			Push(Combine(state.c, state.b));
			break;
		case 0xC6:	//ADI byte
		{
			uint16_t temp = static_cast<uint16_t>(state.a) + static_cast<uint16_t>(inst[1]);
			SetResultFlags(static_cast<uint8_t>(temp));
			SetCarryFlag(temp);
			state.a = static_cast<uint8_t>(temp);
			state.pc += 1;
			break;
		}
		case 0xC7:	//RST 0
		{
			Interrupt(0);
			break;
		}
		case 0xC8:	//RZ
			if (state.conditionCodes.z) {
				state.pc = Pop();
			}
			break;
		case 0xC9:	//RET
		{
			state.pc = Pop();
			break;
		}
		case 0xCA:	//JZ
			if (state.conditionCodes.z) {
				state.pc = Combine(inst[1], inst[2]);
			} else {
				state.pc += 2;
			}
			break;
		case 0xCC:	//CZ addr
			if (state.conditionCodes.z) {
				Push(state.pc);
				state.pc = Combine(inst[1], inst[2]);
			} else {
				state.pc += 2;
			}
			break;
		case 0xCD:	//CALL addr
		{
			Push(state.pc + 2);
			state.pc = Combine(inst[1], inst[2]);
			break;
		}
		case 0xCE:	//ACI byte
		{
			uint8_t temp = state.a + inst[1] + state.conditionCodes.cy;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) + static_cast<uint16_t>(inst[1]) + state.conditionCodes.cy));
			state.a = temp;
			state.pc += 1;
			break;
		}
		case 0xCF:	//RST 1
		{
			Interrupt(1);
			break;
		}
		case 0xD0:	//RNC
			if (!state.conditionCodes.cy) {
				state.pc = Pop();
			}
			break;
		case 0xD1:	//POP D
			Split(Pop(), state.e, state.d);
			break;
		case 0xD2:	//JNC
			if (!state.conditionCodes.cy) {
				state.pc = Combine(inst[1], inst[2]);
			} else {
				state.pc += 2;
			}
			break;
		case 0xD3:	//OUT byte
			WriteOutput(inst[1], state.a);
			state.pc += 1;
			break;
		case 0xD4:	//CNC
			if (!state.conditionCodes.cy) {
				Push(state.pc);
				state.pc = Combine(inst[1], inst[2]);
			} else {
				state.pc += 2;
			}
			break;
		case 0xD5:	//PUSH D
			Push(Combine(state.e, state.d));
			break;
		case 0xD6:	//SUI byte
		{
			uint8_t temp = state.a - inst[1];
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) - static_cast<uint16_t>(inst[1])));
			state.a = temp;
			break;
		}
		case 0xD7:	//RST 2
		{
			Interrupt(2);
			break;
		}
		case 0xD8:	//RC
			if (state.conditionCodes.cy) {
				state.pc = Pop();
			}
			break;
		case 0xDA:	//JC addr
			if (state.conditionCodes.cy) {
				state.pc = Combine(inst[1], inst[2]);
			} else {
				state.pc += 2;
			}
			break;
		case 0xDB:	//IN byte
			state.a = ReadInput(inst[1]);
			state.pc += 1;
			break;
		case 0xDC:	//CC addr
			if (state.conditionCodes.cy) {
				Push(state.pc);
				state.pc = Combine(inst[1], inst[2]);
			} else {
				state.pc += 2;
			}
			break;
		case 0xDE:	//SBI byte
		{
			uint8_t temp = state.a - inst[1] - state.conditionCodes.cy;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(static_cast<uint16_t>(state.a) - static_cast<uint16_t>(inst[1]) - state.conditionCodes.cy));
			state.a = temp;
			state.pc += 1;
			break;
		}
		case 0xDF:	//RST 3
		{
			Interrupt(3);
			break;
		}
		case 0xE0:	//RPO
			if (!state.conditionCodes.p) {
				state.pc = Pop();
			}
			break;
		case 0xE1:	//POP H
			Split(Pop(), state.l, state.h);
			break;
		case 0xE2:	//JPO addr
			if (!state.conditionCodes.p) {
				state.pc = Combine(inst[1], inst[2]);
			} else {
				state.pc += 2;
			}
			break;
		case 0xE3:	//XTHL
		{
			uint16_t temp = Combine(state.l, state.h);
			Split(Pop(), state.l, state.h);
			Push(temp);
			break;
		}
		case 0xE4:	//CPO addr
			if (!state.conditionCodes.p) {
				Push(state.pc);
				state.pc = Combine(inst[1], inst[2]);
			} else {
				state.pc += 2;
			}
			break;
		case 0xE5:	//PUSH H
			Push(Combine(state.l, state.h));
			break;
		case 0xE6:	//ANI byte
		{
			uint8_t temp = state.a & inst[1];
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			state.pc += 1;
			break;
		}
		case 0xE7:	//RST 4
		{
			Interrupt(4);
			break;
		}
		case 0xE8:	//RPE
			if (state.conditionCodes.p) {
				state.pc = Pop();
			}
			break;
		case 0xE9:	//PCHL
			state.pc = Combine(state.l, state.h);
			break;
		case 0xEA:	//JPE addr
			if (state.conditionCodes.p) {
				state.pc = Combine(inst[1], inst[2]);
			} else {
				state.pc += 2;
			}
			break;
		case 0xEB:	//XCHG
		{
			uint8_t temp1 = state.l;
			uint8_t temp2 = state.h;
			state.l = state.e;
			state.h = state.d;
			state.e = temp1;
			state.d = temp2;
			break;
		}
		case 0xEC:	//CPE addr
			if (state.conditionCodes.p) {
				Push(state.pc);
				state.pc = Combine(inst[1], inst[2]);
			} else {
				state.pc += 2;
			}
			break;
		case 0xEE:	//XRI byte
		{
			uint8_t temp = state.a ^ inst[1];
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			state.pc += 1;
			break;
		}
		case 0xEF:	//RST 5
		{
			Interrupt(5);
			break;
		}
		case 0xF0:	//RPE
			if (state.conditionCodes.p) {
				state.pc = Pop();
			}
			break;
		case 0xF1:	//POP PSW
		{
			uint8_t psw;
			Split(Pop(), psw, state.a);
			state.conditionCodes.cy = psw & 1;
			state.conditionCodes.p = (psw >> 2) & 1;
			state.conditionCodes.z = (psw >> 6) & 1;
			state.conditionCodes.s = (psw >> 7) & 1;
			break;
		}
		case 0xF2:	//JPE addr
			if (state.conditionCodes.p) {
				state.pc = Combine(inst[1], inst[2]);
			} else {
				state.pc += 2;
			}
			break;
		case 0xF3:	//DI
			state.interruptEnable = 0;
			break;
		case 0xF4:	//CPE addr
			if (state.conditionCodes.p) {
				Push(state.pc);
				state.pc = Combine(inst[1], inst[2]);
			} else {
				state.pc += 2;
			}
			break;
		case 0xF5:	//PUSH PSW
		{
			uint8_t psw = state.conditionCodes.s & 1;
			psw = (psw << 1) | (state.conditionCodes.z & 1);
			psw = (psw << 4) | (state.conditionCodes.p & 1);
			psw = (psw << 2) | (state.conditionCodes.cy & 1);
			Push(Combine(psw, state.a));
			break;
		}
		case 0xF6:	//ORI
		{
			uint8_t temp = state.a | inst[1];
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.pc += 1;
			break;
		}
		case 0xF7:	//RST 6
		{
			Interrupt(6);
			break;
		}
		case 0xF8:	//RM
			if (state.conditionCodes.s) {
				state.pc = Pop();
			}
			break;
		case 0xFA:	//JM addr
			if (state.conditionCodes.s) {
				state.pc = Combine(inst[1], inst[2]);
			} else {
				state.pc += 2;
			}
			break;
		case 0xFB:	//EI
			state.interruptEnable = 1;
			break;
		case 0xFC:	//CM addr
			if (state.conditionCodes.s) {
				Push(state.pc);
				state.pc = Combine(inst[1], inst[2]);
			} else {
				state.pc += 2;
			}
			break;
		case 0xFE:	//CPI byte
		{
			uint8_t temp = state.a - inst[1];
			SetCarryFlag(static_cast<uint16_t>(temp));
			SetResultFlags(temp);
			state.pc += 1;
			break;
		}
		case 0xFF:	//RST 7
		{
			Interrupt(7);
			break;
		}
	}
}