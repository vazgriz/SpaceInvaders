#include "Renderer.h"

#include <vector>
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

Renderer::Renderer() {
	glfwInit();
	CreateWindow();
	CreateInstance();
	CreateSurface();

	glfwShowWindow(window);
}

Renderer::~Renderer() {
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