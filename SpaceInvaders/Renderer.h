#pragma once
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

class Renderer {
public:
	Renderer();
	~Renderer();

	GLFWwindow* GetWindow() const { return window; }

private:
	GLFWwindow* window;
	VkInstance instance;

	void CreateWindow();
	void CreateInstance();
};

