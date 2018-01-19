#include "CPU.h"

#include <stdexcept>
#include <iostream>
#include <iomanip>

#include "Disassemble.h"

CPU::CPU() {
	state = {};
	state.memory.resize(16 * 1024);
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

void CPU::QueueInterrupt(size_t value, size_t intstructionDelay) {
	std::lock_guard<std::mutex> lock(mutex);
	queue.push(CPU::Interrupt{ value, instructionCount + intstructionDelay });
}

void CPU::Step() {
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (queue.size() > 0) {
			auto interrupt = queue.front();
			if (instructionCount > interrupt.target) {
				queue.pop();
				Push(state.pc);
				state.pc = static_cast<uint16_t>(interrupt.value * 8);
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
		case 0x2a:	//LHLD addr
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
		case 0x56:	//MOV D, M
		{
			uint16_t addr = Combine(state.l, state.h);
			state.d = state.memory[addr];
			break;
		}
		case 0x5C:	//MOV E, H
			state.e = state.h;
			break;
		case 0x5E:	//MOV E, M
		{
			uint16_t addr = Combine(state.l, state.h);
			state.e = state.memory[addr];
			break;
		}
		case 0x66:	//MOV H, M
		{
			uint16_t addr = Combine(state.l, state.h);
			state.h = state.memory[addr];
			break;
		}
		case 0x6F:	//MOV L, A
			state.l = state.a;
			break;
		case 0x77:	//MOV M, A
		{
			uint16_t addr = Combine(state.l, state.h);
			state.memory[addr] = state.a;
			break;
		}
		case 0x7A:	//MOV A, D
			state.a = state.d;
			break;
		case 0x7B:	//MOV A, E
			state.a = state.e;
			break;
		case 0x7C:	//MOV A, H
			state.a = state.h;
			break;
		case 0x7E:	//MOV A, M
		{
			uint16_t addr = Combine(state.l, state.h);
			state.a = state.memory[addr];
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
		case 0xAF:	//XRA A
		{
			uint8_t temp = state.a ^ state.a;
			SetResultFlags(temp);
			SetCarryFlag(static_cast<uint16_t>(temp));
			state.a = temp;
			break;
		}
		case 0xC1:	//POP B
			Split(Pop(), state.c, state.b);
			break;
		case 0xC2:	//JNZ addr
			if (state.conditionCodes.z == 0) {
				uint16_t addr = Combine(inst[1], inst[2]);
				state.pc = addr;
			} else {
				state.pc += 2;
			}
			break;
		case 0xC3:	//JMP addr
		{
			uint16_t addr = Combine(inst[1], inst[2]);
			state.pc = addr;
			break;
		}
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
		case 0xC9:	//RET
		{
			state.pc = Pop();
			break;
		}
		case 0xCD:	//CALL addr
		{
			Push(state.pc + 2);
			state.pc = Combine(inst[1], inst[2]);
			break;
		}
		case 0xD1:	//POP D
			Split(Pop(), state.e, state.d);
			break;
		case 0xD3:	//OUT byte
			state.pc += 1;
			break;
		case 0xD5:	//PUSH D
			Push(Combine(state.e, state.d));
			break;
		case 0xE1:	//POP H
			Split(Pop(), state.l, state.h);
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
		case 0xF5:	//PUSH PSW
		{
			uint8_t psw = state.conditionCodes.s & 1;
			psw = (psw << 1) | (state.conditionCodes.z & 1);
			psw = (psw << 4) | (state.conditionCodes.p & 1);
			psw = (psw << 2) | (state.conditionCodes.cy & 1);
			Push(Combine(psw, state.a));
			break;
		}
		case 0xFB:	//EI
			state.interruptEnable = 1;
			break;
		case 0xFE:	//CPI byte
		{
			uint8_t temp = state.a - inst[1];
			SetCarryFlag(static_cast<uint16_t>(temp));
			SetResultFlags(temp);
			state.pc += 1;
			break;
		}
	}
}