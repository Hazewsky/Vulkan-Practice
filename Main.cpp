#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>
#include <iostream>

#include "VulkanRenderer.h"


GLFWwindow* window;
VulkanRenderer vulkanRenderer;


void initWIndow(std::string wName = "Test Window", const int width = 800, const int height = 600)
{
	//init GLFW
	if (glfwInit() == GLFW_TRUE)
	{
		//set not work with OpenGL. GLFW_NO_API, GLFW_OPENGL_API, GLFW_OPENGL_ES_API (the default one)
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); //no resize, else should recreate everything on resize

		window = glfwCreateWindow(width, height, wName.c_str(), nullptr, nullptr);

	}
}

int main()
{
	//Crate window
	initWIndow("Vulkan Render", 1280, 720);

	if (vulkanRenderer.init(window) == EXIT_FAILURE)
	{
		return EXIT_FAILURE;
	}

	float angle = 0.0f;
	float deltaTime = 0.0f;
	float lastTime = 0.0f;

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		float now = glfwGetTime();
		deltaTime = now - lastTime;
		lastTime = now;

		angle += 10.0f * deltaTime;
		if (angle > 360.0f) { angle -= 360.0f; }
		vulkanRenderer.updateModel(glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(1.0f, 1.0f, 1.0f)));
		vulkanRenderer.draw();
	}
	//clean up renderer
	vulkanRenderer.cleanup();
	//Destroy window
	glfwDestroyWindow(window);
	//Terminate GLFW
	glfwTerminate();

	////how many extensions Vulkan can support
	////uint32_t is base for vulkan
	////vkXXXX - function, VkXXXX - type
	//uint32_t extensionCount = 0;
	//vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	system("pause");
	return EXIT_SUCCESS;
}