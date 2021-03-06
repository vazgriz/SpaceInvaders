#include "Renderer.h"

#include <set>
#include <algorithm>
#include <cmath>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

const std::vector<const char*> layers = {
	"VK_LAYER_LUNARG_standard_validation",
	//"VK_LAYER_LUNARG_api_dump"
};

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

struct Vertex {
	struct {
		float x, y;
	} pos;
	struct {
		float u, v;
	} tex;
};

std::vector<Vertex> vertices = {
	{ { -IMAGE_WIDTH / 2, -IMAGE_HEIGHT / 2 }, { 0, 0 } },
	{ {  IMAGE_WIDTH / 2, -IMAGE_HEIGHT / 2 }, { 1, 0 } },
	{ { -IMAGE_WIDTH / 2,  IMAGE_HEIGHT / 2 }, { 0, 1 } },
	{ {  IMAGE_WIDTH / 2, -IMAGE_HEIGHT / 2 }, { 1, 0 } },
	{ {  IMAGE_WIDTH / 2,  IMAGE_HEIGHT / 2 }, { 1, 1 } },
	{ { -IMAGE_WIDTH / 2,  IMAGE_HEIGHT / 2 }, { 0, 1 } }
};

Renderer::Renderer() {
	swapchain = VK_NULL_HANDLE;
	glfwInit();
	CreateWindow();
	CreateInstance();
	CreateSurface();
	PickPhysicalDevice();
	CreateDevice();
	CreateCommandPool();
	CreateVRAMBuffer();
	CreateVertexBuffer();
	CreateTexture();
	CreateImageView();
	CreateSampler();
	CreateDescriptorLayout();
	CreateDescriptorPool();
	CreateDescriptorSet();
	WriteDescriptorSet();
	RecreateSwapchain();
	CreateSemaphores();
	CreateCommandBuffers();

	glfwShowWindow(window);
}

Renderer::~Renderer() {
	vkDeviceWaitIdle(device);
	allocator.reset();
	vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
	vkDestroyCommandPool(device, commandPool, nullptr);
	vkDestroySemaphore(device, acquireImageSemaphore, nullptr);
	vkDestroySemaphore(device, renderDoneSemaphore, nullptr);
	CleanupSwapchain();
	vkDestroySampler(device, sampler, nullptr);
	vkDestroyImageView(device, imageView, nullptr);
	vkDestroyImage(device, texture, nullptr);
	vkDestroyBuffer(device, vramBuffer, nullptr);
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkDestroyDevice(device, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr);

	glfwDestroyWindow(window);
	glfwTerminate();
}

void Renderer::Render() {
	uint32_t index;
	vkAcquireNextImageKHR(device, swapchain, ~0ull, acquireImageSemaphore, VK_NULL_HANDLE, &index);
	vkWaitForFences(device, 1, &fences[index], true, ~0ull);
	vkResetFences(device, 1, &fences[index]);

	VkPipelineStageFlags waitMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &acquireImageSemaphore;
	submitInfo.pWaitDstStageMask = &waitMask;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderDoneSemaphore;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[index];

	vkQueueSubmit(graphicsQueue, 1, &submitInfo, fences[index]);

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.pImageIndices = &index;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderDoneSemaphore;

	vkQueuePresentKHR(presentQueue, &presentInfo);
}

void Renderer::CreateWindow() {
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_VISIBLE, false);
	window = glfwCreateWindow(width, height, "Space Invaders", nullptr, nullptr);
}

void Renderer::CreateInstance() {
	VkInstanceCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	info.enabledLayerCount = static_cast<uint32_t>(layers.size());
	info.ppEnabledLayerNames = layers.data();

	info.ppEnabledExtensionNames = glfwGetRequiredInstanceExtensions(&info.enabledExtensionCount);
	
	VK_CHECK(vkCreateInstance(&info, nullptr, &instance), "Failed to create instance");
}

void Renderer::CreateSurface() {
	VK_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &surface), "Failed to create surface");
}

void Renderer::PickPhysicalDevice() {
	physicalDevice = VK_NULL_HANDLE;
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0) {
		throw std::runtime_error("Failed to find GPUs with Vulkan support");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	for (const auto& device : devices) {
		if (IsDeviceSuitable(device)) {
			physicalDevice = device;
			break;
		}
	}

	if (physicalDevice == VK_NULL_HANDLE) {
		throw std::runtime_error("Failed to find a suitable GPU");
	}
}

bool Renderer::IsDeviceSuitable(VkPhysicalDevice device) {
	QueueInfo queueInfo = GetQueueInfo(device);

	bool extensionsSupported = CheckDeviceExtensionSupport(device);

	bool surfaceAdequate = false;
	SurfaceInfo surfaceInfo;
	if (extensionsSupported) {
		surfaceInfo = GetSurfaceInfo(device);
		surfaceAdequate = !surfaceInfo.formats.empty() && !surfaceInfo.presentModes.empty();
	}

	bool suitable = queueInfo.graphicsFamily != ~0u && queueInfo.presentFamily != ~0u && extensionsSupported && surfaceAdequate;
	if (suitable) {
		this->queueInfo = queueInfo;
		this->surfaceInfo = surfaceInfo;
	}

	return suitable;
}

bool Renderer::CheckDeviceExtensionSupport(VkPhysicalDevice device) {
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

	for (const auto& extension : availableExtensions) {
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

Renderer::SurfaceInfo Renderer::GetSurfaceInfo(VkPhysicalDevice device) {
	SurfaceInfo info;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &info.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	if (formatCount != 0) {
		info.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, info.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

	if (presentModeCount != 0) {
		info.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, info.presentModes.data());
	}

	return info;
}

Renderer::QueueInfo Renderer::GetQueueInfo(VkPhysicalDevice device) {
	QueueInfo info = { ~0u, ~0u };

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	for (uint32_t i = 0; i < queueFamilies.size(); i++) {
		auto& queueFamily = queueFamilies[i];
		if (info.graphicsFamily == ~0u && queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			info.graphicsFamily = i;
		}

		VkBool32 presentSupport;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

		if (info.presentFamily == ~0u && queueFamily.queueCount > 0 && presentSupport) {
			info.presentFamily = i;
		}
	}

	return info;
}

void Renderer::CreateDevice() {
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { queueInfo.graphicsFamily, queueInfo.presentFamily };

	float queuePriority = 1.0f;
	for (int queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();

	VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device), "Failed to create device");

	vkGetDeviceQueue(device, queueInfo.graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(device, queueInfo.presentFamily, 0, &presentQueue);

	allocator = std::make_unique<Allocator>(physicalDevice, device);
}

void Renderer::CreateRenderPass() {
	VkAttachmentDescription attachment = {};
	attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	attachment.format = swapchainFormat;
	attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkAttachmentReference ref = {};
	ref.attachment = 0;
	ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &ref;

	VkSubpassDependency fromExternal = {};
	fromExternal.srcSubpass = VK_SUBPASS_EXTERNAL;
	fromExternal.dstSubpass = 0;
	fromExternal.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	fromExternal.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	fromExternal.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	fromExternal.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubpassDependency toExternal = {};
	toExternal.srcSubpass = 0;
	toExternal.dstSubpass = VK_SUBPASS_EXTERNAL;
	toExternal.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	toExternal.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	toExternal.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	toExternal.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubpassDependency dependencies[] = { fromExternal, toExternal };

	VkRenderPassCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	info.attachmentCount = 1;
	info.pAttachments = &attachment;
	info.subpassCount = 1;
	info.pSubpasses = &subpass;
	info.dependencyCount = 2;
	info.pDependencies = dependencies;

	VK_CHECK(vkCreateRenderPass(device, &info, nullptr, &renderPass), "Failed to create render pass");
}

void Renderer::RecreateSwapchain() {
	CreateSwapchain();
	CreateRenderPass();
	CreateImageViews();
	CreateFramebuffers();
	CreateFences();
	CreatePipeline();
}

void Renderer::CleanupSwapchain() {
	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyRenderPass(device, renderPass, nullptr);
	vkDestroySwapchainKHR(device, swapchain, nullptr);
	for (auto& iv : swapchainImageViews) vkDestroyImageView(device, iv, nullptr);
	for (auto& fb : framebuffers) vkDestroyFramebuffer(device, fb, nullptr);
	for (auto& fence : fences) vkDestroyFence(device, fence, nullptr);
}

VkSurfaceFormatKHR Renderer::ChooseSwapchainFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
	if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
		return { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}

	for (const auto& availableFormat : availableFormats) {
		if ((availableFormat.format == VK_FORMAT_R8G8B8A8_UNORM || availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM) && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormat;
		}
	}

	throw std::runtime_error("Failed to find suitable surface format");
}

VkExtent2D Renderer::ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	} else {
		VkExtent2D actualExtent = { width, height };

		actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}
}

void Renderer::CreateSwapchain() {
	VkSurfaceFormatKHR surfaceFormat = ChooseSwapchainFormat(surfaceInfo.formats);
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	VkExtent2D extent = ChooseSwapchainExtent(surfaceInfo.capabilities);

	uint32_t imageCount = 2;

	if (surfaceInfo.capabilities.maxImageCount > 0 && imageCount > surfaceInfo.capabilities.maxImageCount) {
		imageCount = surfaceInfo.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	info.surface = surface;
	info.minImageCount = imageCount;
	info.imageFormat = surfaceFormat.format;
	info.imageColorSpace = surfaceFormat.colorSpace;
	info.imageExtent = extent;
	info.imageArrayLayers = 1;
	info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	uint32_t queueFamilyIndices[] = { queueInfo.graphicsFamily, queueInfo.presentFamily };

	if (queueInfo.graphicsFamily != queueInfo.presentFamily) {
		info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		info.queueFamilyIndexCount = 2;
		info.pQueueFamilyIndices = queueFamilyIndices;
	} else {
		info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.queueFamilyIndexCount = 0; // Optional
		info.pQueueFamilyIndices = nullptr; // Optional
	}

	info.preTransform = surfaceInfo.capabilities.currentTransform;
	info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	info.presentMode = presentMode;
	info.clipped = VK_TRUE;

	VkSwapchainKHR oldSwapchain = swapchain;
	info.oldSwapchain = oldSwapchain;

	VK_CHECK(vkCreateSwapchainKHR(device, &info, nullptr, &swapchain), "Failed to create swap chain");

	if (oldSwapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(device, oldSwapchain, nullptr);
	}

	vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
	swapchainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

	swapchainFormat = surfaceFormat.format;
	swapchainExtent = extent;
}

void Renderer::CreateImageViews() {
	swapchainImageViews.resize(swapchainImages.size());

	for (size_t i = 0; i < swapchainImages.size(); i++) {
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapchainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = swapchainFormat;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		VK_CHECK(vkCreateImageView(device, &createInfo, nullptr, &swapchainImageViews[i]), "Failed to create image views");
	}
}

void Renderer::CreateFramebuffers() {
	framebuffers.resize(swapchainImages.size());

	for (size_t i = 0; i < framebuffers.size(); i++) {
		VkFramebufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		info.attachmentCount = 1;
		info.pAttachments = &swapchainImageViews[i];
		info.width = width;
		info.height = height;
		info.layers = 1;
		info.renderPass = renderPass;

		VK_CHECK(vkCreateFramebuffer(device, &info, nullptr, &framebuffers[i]), "Failed to create framebuffer");
	}
}

void Renderer::CreateFences() {
	fences.resize(swapchainImages.size());

	for (size_t i = 0; i < swapchainImages.size(); i++) {
		VkFenceCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		VK_CHECK(vkCreateFence(device, &info, nullptr, &fences[i]), "Failed to create fences");
	}
}

void Renderer::CreateSemaphores() {
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &acquireImageSemaphore), "Failed to create semaphores");
	VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderDoneSemaphore), "Failed to create semaphores");
}

void Renderer::CreateVRAMBuffer() {
	VkBufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	info.size = IMAGE_WIDTH * IMAGE_HEIGHT * 4;

	VK_CHECK(vkCreateBuffer(device, &info, nullptr, &vramBuffer), "Failed to create buffer");

	VkMemoryRequirements requirements;
	vkGetBufferMemoryRequirements(device, vramBuffer, &requirements);

	Allocation alloc = allocator->Alloc(requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	vkBindBufferMemory(device, vramBuffer, alloc.memory, alloc.offset);
	vkMapMemory(device, alloc.memory, alloc.offset, IMAGE_WIDTH * IMAGE_HEIGHT * 4, 0, &vramMapping);
}

void Renderer::CreateVertexBuffer() {
	VkBufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	info.size = sizeof(Vertex) * vertices.size();

	VK_CHECK(vkCreateBuffer(device, &info, nullptr, &vertexBuffer), "Failed to create buffer");

	VkMemoryRequirements requirements;
	vkGetBufferMemoryRequirements(device, vertexBuffer, &requirements);

	Allocation alloc = allocator->Alloc(requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	vkBindBufferMemory(device, vertexBuffer, alloc.memory, alloc.offset);

	CopyStaging(sizeof(Vertex) * vertices.size(), vertices.data(), vertexBuffer);
}

void Renderer::CreateTexture() {
	VkImageCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.format = VK_FORMAT_R8G8B8A8_UNORM;
	info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	info.extent = { IMAGE_WIDTH, IMAGE_HEIGHT, 1 };
	info.arrayLayers = 1;
	info.mipLevels = 1;
	info.imageType = VK_IMAGE_TYPE_2D;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	info.samples = VK_SAMPLE_COUNT_1_BIT;

	VK_CHECK(vkCreateImage(device, &info, nullptr, &texture), "Failed to create texture");

	VkMemoryRequirements requirements;
	vkGetImageMemoryRequirements(device, texture, &requirements);

	Allocation alloc = allocator->Alloc(requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	vkBindImageMemory(device, texture, alloc.memory, alloc.offset);
}

void Renderer::CreateImageView() {
	VkImageViewCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.image = texture;
	info.format = VK_FORMAT_R8G8B8A8_UNORM;
	info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.layerCount = 1;
	info.subresourceRange.baseMipLevel = 0;
	info.subresourceRange.levelCount = 1;

	VK_CHECK(vkCreateImageView(device, &info, nullptr, &imageView), "Failed to create image view");
}

void Renderer::CreateSampler() {
	VkSamplerCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	info.minFilter = VK_FILTER_LINEAR;
	info.magFilter = VK_FILTER_LINEAR;
	info.maxAnisotropy = 1.0f;
	info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

	VK_CHECK(vkCreateSampler(device, &info, nullptr, &sampler), "Failed to create sampler");
}

void Renderer::CreateDescriptorLayout() {
	VkDescriptorSetLayoutBinding binding = {};
	binding.binding = 0;
	binding.descriptorCount = 1;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.bindingCount = 1;
	info.pBindings = &binding;

	VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &descriptorSetLayout), "Failed to create descriptor set layout");
}

void Renderer::CreateDescriptorPool() {
	VkDescriptorPoolSize size = {};
	size.descriptorCount = 1;
	size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	VkDescriptorPoolCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	info.maxSets = 1;
	info.poolSizeCount = 1;
	info.pPoolSizes = &size;
	VK_CHECK(vkCreateDescriptorPool(device, &info, nullptr, &descriptorPool), "Failed to create descriptor pool");
}

void Renderer::CreateDescriptorSet() {
	VkDescriptorSetAllocateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	info.descriptorPool = descriptorPool;
	info.descriptorSetCount = 1;
	info.pSetLayouts = &descriptorSetLayout;
	
	VK_CHECK(vkAllocateDescriptorSets(device, &info, &descriptorSet), "Failed to allocate descriptor sets");
}

void Renderer::WriteDescriptorSet() {
	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageView = imageView;
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.sampler = sampler;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = descriptorSet;
	write.dstBinding = 0;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.descriptorCount = 1;
	write.pImageInfo = &imageInfo;
	
	vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

VkShaderModule Renderer::CreateShader(const std::string& fileName) {
	std::vector<char> data = LoadFile(fileName);

	VkShaderModuleCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize = static_cast<uint32_t>(data.size());
	info.pCode = reinterpret_cast<uint32_t*>(data.data());

	VkShaderModule _module;
	VK_CHECK(vkCreateShaderModule(device, &info, nullptr, &_module), "Failed to create shader module");

	return _module;
}

void Renderer::CreatePipeline() {
	VkShaderModule vertShader = CreateShader("Shaders/invaders.vert.spv");
	VkShaderModule fragShader = CreateShader("Shaders/invaders.frag.spv");

	VkPipelineShaderStageCreateInfo vertStage = {};
	vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertStage.module = vertShader;
	vertStage.pName = "main";
	vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineShaderStageCreateInfo fragStage = {};
	fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragStage.module = fragShader;
	fragStage.pName = "main";
	fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkPipelineShaderStageCreateInfo stages[] = { vertStage, fragStage };

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkVertexInputBindingDescription bindings[] = {
		{ 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX },
	};

	VkVertexInputAttributeDescription attributes[] = {
		{ 0, 0, VK_FORMAT_R32G32_SFLOAT, 0 },
		{ 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, tex) },
	};

	VkPipelineVertexInputStateCreateInfo vertexInput = {};
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInput.vertexBindingDescriptionCount = 1;
	vertexInput.pVertexBindingDescriptions = bindings;
	vertexInput.vertexAttributeDescriptionCount = 2;
	vertexInput.pVertexAttributeDescriptions = attributes;

	VkPipelineRasterizationStateCreateInfo rasterization = {};
	rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization.lineWidth = 1.0f;
	rasterization.cullMode = VK_CULL_MODE_NONE;

	VkViewport viewport = {};
	viewport.width = static_cast<float>(width);
	viewport.height = static_cast<float>(height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D clipping = {};
	clipping.extent = swapchainExtent;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &clipping;

	VkPipelineColorBlendAttachmentState attachment = {};
	attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	attachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo blending = {};
	blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blending.attachmentCount = 1;
	blending.pAttachments = &attachment;

	VkPipelineMultisampleStateCreateInfo multisample = {};
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPushConstantRange range = {};
	range.size = 64;
	range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &range;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &descriptorSetLayout;
	
	VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout), "Failed to create pipeline layout");

	VkGraphicsPipelineCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	info.stageCount = 2;
	info.pStages = stages;
	info.pInputAssemblyState = &inputAssembly;
	info.pVertexInputState = &vertexInput;
	info.pRasterizationState = &rasterization;
	info.pViewportState = &viewportState;
	info.pColorBlendState = &blending;
	info.pMultisampleState = &multisample;
	info.renderPass = renderPass;
	info.subpass = 0;
	info.layout = pipelineLayout;
	
	VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline), "Failed to create pipeline");

	vkDestroyShaderModule(device, vertShader, nullptr);
	vkDestroyShaderModule(device, fragShader, nullptr);
}

void Renderer::CreateCommandPool() {
	VkCommandPoolCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.queueFamilyIndex = queueInfo.graphicsFamily;

	VK_CHECK(vkCreateCommandPool(device, &info, nullptr, &commandPool), "Failed to create command pool");
}

void Renderer::CreateCommandBuffers() {
	VkCommandBufferAllocateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.commandPool = commandPool;
	info.commandBufferCount = static_cast<uint32_t>(swapchainImageViews.size());

	commandBuffers.resize(swapchainImageViews.size());
	VK_CHECK(vkAllocateCommandBuffers(device, &info, commandBuffers.data()), "Failed to create command buffer");

	for (uint32_t i = 0; i < commandBuffers.size(); i++) {
		RecordCommandBuffer(i);
	}
}

void Renderer::RecordCommandBuffer(uint32_t index) {
	VkCommandBuffer& commandBuffer = commandBuffers[index];

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = texture;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
		0, nullptr,
		0, nullptr,
		1, &barrier);

	VkBufferImageCopy copy = {};
	copy.imageExtent = { IMAGE_WIDTH, IMAGE_HEIGHT, 1 };
	copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy.imageSubresource.baseArrayLayer = 0;
	copy.imageSubresource.layerCount = 1;

	vkCmdCopyBufferToImage(commandBuffer, vramBuffer, texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
		0, nullptr,
		0, nullptr,
		1, &barrier);

	VkClearValue clear = {};
	clear.color.float32[0] = 0.125f;
	clear.color.float32[1] = 0.125f;
	clear.color.float32[2] = 0.125f;
	clear.color.float32[3] = 1;

	VkRenderPassBeginInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass;
	renderPassInfo.framebuffer = framebuffers[index];
	renderPassInfo.renderArea.extent = swapchainExtent;
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clear;

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &offset);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	glm::mat4 matrix = glm::ortho(-(width / 2.0f), (width / 2.0f), -(height / 2.0f), (height / 2.0f), 0.0f, 1.0f);
	matrix = matrix * glm::rotate(glm::mat4(), -static_cast<float>(M_PI) / 2, glm::vec3(0, 0, 1));
	vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &matrix);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

	vkCmdDraw(commandBuffer, 6, 1, 0, 0);

	vkCmdEndRenderPass(commandBuffer);

	vkEndCommandBuffer(commandBuffer);
}

VkCommandBuffer Renderer::GetSingleUseCommandBuffer() {
	VkCommandBufferAllocateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.commandPool = commandPool;
	info.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	VK_CHECK(vkAllocateCommandBuffers(device, &info, &commandBuffer), "Failed to allocate command buffer");

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void Renderer::SubmitSingleUseCommandBuffer(VkCommandBuffer commandBuffer) {
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	info.commandBufferCount = 1;
	info.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(graphicsQueue, 1, &info, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphicsQueue);

	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void Renderer::CopyStaging(size_t size, void* srcMemory, VkBuffer dstBuffer) {
	VkBufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.size = size;
	info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	VkBuffer staging;
	VK_CHECK(vkCreateBuffer(device, &info, nullptr, &staging), "Failed to create staging buffer");

	VkMemoryRequirements requirements;
	vkGetBufferMemoryRequirements(device, staging, &requirements);

	Allocation alloc = allocator->Alloc(requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	vkBindBufferMemory(device, staging, alloc.memory, alloc.offset);

	void* mapping;
	vkMapMemory(device, alloc.memory, alloc.offset, size, 0, &mapping);

	memcpy(mapping, srcMemory, size);

	VkCommandBuffer commandBuffer = GetSingleUseCommandBuffer();

	VkBufferCopy copy = {};
	copy.size = size;

	vkCmdCopyBuffer(commandBuffer, staging, dstBuffer, 1, &copy);

	SubmitSingleUseCommandBuffer(commandBuffer);

	vkDestroyBuffer(device, staging, nullptr);
}