#include <vulkan/vulkan.h>
#include <stdexcept>

#define VK_CHECK(exp, msg)                         \
{                                                  \
	VkResult result = exp;                         \
	if (result < 0) throw std::runtime_error(msg); \
}