#include "Allocator.h"

Allocator::Allocator(VkPhysicalDevice physicalDevice, VkDevice device){
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
	this->device = device;
}

Allocator::~Allocator() {
	for (auto& page : pages) vkFreeMemory(device, page, nullptr);
}

Allocation Allocator::Alloc(VkMemoryRequirements requirements, VkMemoryPropertyFlags flags) {
	uint32_t type = FindType(requirements, flags);
	if (type != ~0) {
		VkMemoryAllocateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		info.allocationSize = requirements.size;
		info.memoryTypeIndex = type;
		
		VkDeviceMemory memory;
		VK_CHECK(vkAllocateMemory(device, &info, nullptr, &memory), "Failed to allocate memory");

		pages.push_back(memory);

		return { memory, 0, requirements.size };
	}

	return {};
}

uint32_t Allocator::FindType(VkMemoryRequirements requirements, VkMemoryPropertyFlags flags) {
	for (uint32_t i = 0; i < properties.memoryTypeCount; i++) {
		if (((requirements.memoryTypeBits >> i) & 1) == 0) continue;

		auto& type = properties.memoryTypes[i];
		if ((type.propertyFlags & flags) == flags) {
			return i;
		}
	}

	return ~0;
}