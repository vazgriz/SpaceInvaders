#include "Renderer.h"

#include <vector>
#include <stdexcept>

Renderer::Renderer() {
	glfwInit();
	CreateWindow();

	glfwShowWindow(window);
}

Renderer::~Renderer() {
	glfwDestroyWindow(window);
	glfwTerminate();
}

void Renderer::CreateWindow() {
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_VISIBLE, false);
	window = glfwCreateWindow(800, 600, "Space Invaders", nullptr, nullptr);
}
