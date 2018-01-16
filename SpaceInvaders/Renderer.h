#pragma once
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>

class Renderer {
public:
	Renderer();
	~Renderer();

	GLFWwindow* GetWindow() const { return window; }

private:
	struct QueueInfo {
		uint32_t graphicsFamily;
		uint32_t presentFamily;
	};

	struct SurfaceInfo {
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	GLFWwindow* window;
	uint32_t width = 800;
	uint32_t height = 600;
	VkInstance instance;
	VkSurfaceKHR surface;
	VkPhysicalDeviceProperties deviceProperties;
	QueueInfo queueInfo;
	SurfaceInfo surfaceInfo;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	VkSwapchainKHR swapchain;
	std::vector<VkImage> swapchainImages;
	VkFormat swapchainFormat;
	VkExtent2D swapchainExtent;
	std::vector<VkImageView> swapchainImageViews;
	std::vector<VkFence> fences;

	void CreateWindow();
	void CreateInstance();
	void CreateSurface();
	void PickPhysicalDevice();
	bool IsDeviceSuitable(VkPhysicalDevice device);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
	SurfaceInfo GetSurfaceInfo(VkPhysicalDevice device);
	QueueInfo GetQueueInfo(VkPhysicalDevice device);
	void CreateDevice();
	VkSurfaceFormatKHR ChooseSwapchainFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkExtent2D ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	void RecreateSwapchain();
	void CleanupSwapchain();
	void CreateSwapchain();
	void CreateImageViews();
	void CreateFences();
};

