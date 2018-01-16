#include "Renderer.h"

#include <set>
#include <stdexcept>

#define VK_CHECK(exp, msg)                         \
{                                                  \
	VkResult result = exp;                         \
	if (result < 0) throw std::runtime_error(msg); \
}

const std::vector<const char*> layers = {
	"VK_LAYER_LUNARG_standard_validation",
	//"VK_LAYER_LUNARG_api_dump"
};

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

Renderer::Renderer() {
	glfwInit();
	CreateWindow();
	CreateInstance();
	CreateSurface();
	PickPhysicalDevice();
	CreateDevice();

	glfwShowWindow(window);
}

Renderer::~Renderer() {
	vkDestroyDevice(device, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr);

	glfwDestroyWindow(window);
	glfwTerminate();
}

void Renderer::CreateWindow() {
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_VISIBLE, false);
	window = glfwCreateWindow(800, 600, "Space Invaders", nullptr, nullptr);
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

	vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
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

	bool suitable = queueInfo.graphicsQueue != ~0u && queueInfo.presentQueue != ~0u && extensionsSupported && surfaceAdequate;
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
		if (info.graphicsQueue == ~0u && queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			info.graphicsQueue = i;
		}

		VkBool32 presentSupport;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

		if (info.presentQueue == ~0u && queueFamily.queueCount > 0 && presentSupport) {
			info.presentQueue = i;
		}
	}

	return info;
}

void Renderer::CreateDevice() {
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { queueInfo.graphicsQueue, queueInfo.presentQueue };

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

	vkGetDeviceQueue(device, queueInfo.graphicsQueue, 0, &graphicsQueue);
	vkGetDeviceQueue(device, queueInfo.presentQueue, 0, &presentQueue);
}