#pragma once

#include <string>
#include <vector>

void Disassemble(const uint8_t* inst);
void Disassemble(const std::vector<char>& buffer);