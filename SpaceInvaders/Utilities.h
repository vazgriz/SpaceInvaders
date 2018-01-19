#include <vulkan/vulkan.h>
#include <stdexcept>
#include <vector>
#include <fstream>

#define VK_CHECK(exp, msg)                         \
{                                                  \
	VkResult result = exp;                         \
	if (result < 0) throw std::runtime_error(msg); \
}

std::vector<char> LoadFile(const std::string& fileName);