#pragma once
#include <vulkan/vulkan.h>
#include <vector>

#include "Utilities.h"

struct Allocation {
	VkDeviceMemory memory;
	size_t offset;
	size_t size;
};

class Allocator {
public:
	Allocator(VkPhysicalDevice physicalDevice, VkDevice device);
	~Allocator();

	Allocation Alloc(VkMemoryRequirements requirements, VkMemoryPropertyFlags flags);

private:
	VkPhysicalDeviceMemoryProperties properties;
	VkDevice device;
	std::vector<VkDeviceMemory> pages;

	uint32_t FindType(VkMemoryRequirements requirements, VkMemoryPropertyFlags flags);
};

