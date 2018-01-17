#pragma once
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>

#include "Utilities.h"
#include "Display.h"

class Renderer {
public:
	Renderer();
	~Renderer();

	GLFWwindow* GetWindow() const { return window; }

	void Render();

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
	QueueInfo queueInfo;
	SurfaceInfo surfaceInfo;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	VkRenderPass renderPass;
	VkSwapchainKHR swapchain;
	std::vector<VkImage> swapchainImages;
	VkFormat swapchainFormat;
	VkExtent2D swapchainExtent;
	std::vector<VkImageView> swapchainImageViews;
	std::vector<VkFramebuffer> framebuffers;
	std::vector<VkFence> fences;
	VkSemaphore acquireImageSemaphore;
	VkSemaphore renderDoneSemaphore;
	VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;

	void CreateWindow();
	void CreateInstance();
	void CreateSurface();
	void PickPhysicalDevice();
	bool IsDeviceSuitable(VkPhysicalDevice device);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
	SurfaceInfo GetSurfaceInfo(VkPhysicalDevice device);
	QueueInfo GetQueueInfo(VkPhysicalDevice device);
	void CreateDevice();
	void CreateRenderPass();
	VkSurfaceFormatKHR ChooseSwapchainFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkExtent2D ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	void RecreateSwapchain();
	void CleanupSwapchain();
	void CreateSwapchain();
	void CreateImageViews();
	void CreateFramebuffers();
	void CreateFences();
	void CreateSemaphores();
	void CreateCommandPool();
	void CreateCommandBuffers();
	void RecordCommandBuffer(uint32_t index);
};

