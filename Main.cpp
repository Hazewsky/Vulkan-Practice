#define STB_IMAGE_IMPLEMENTATION
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>
#include <iostream>

#include "VulkanRenderer.h"


GLFWwindow* window;
VulkanRenderer vulkanRenderer;


void keyPressedCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS || action == GLFW_REPEAT)
	{
		switch (key)
		{
		case GLFW_KEY_E:
			printf("Key E is pressed!\n");
			break;
		case GLFW_KEY_Q:
			printf("Key Q is pressed!\n");
			break;
		case GLFW_KEY_W:
			printf("Key W is pressed!\n");
			break;
		case GLFW_KEY_S:
			printf("Key S is pressed!\n");
			break;
		case GLFW_KEY_A:
			printf("Key A is pressed!\n");
			break;
		case GLFW_KEY_D:
			printf("Key D is pressed!\n");
			break;
		}
	}
}

void initWIndow(std::string wName = "Test Window", const int width = 800, const int height = 600)
{
	//init GLFW
	if (glfwInit() == GLFW_TRUE)
	{
		//set not work with OpenGL. GLFW_NO_API, GLFW_OPENGL_API, GLFW_OPENGL_ES_API (the default one)
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); //no resize, else should recreate everything on resize

		window = glfwCreateWindow(width, height, wName.c_str(), nullptr, nullptr);

		//set key callback
		glfwSetKeyCallback(window, keyPressedCallback);
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
	
	int firstModelId = vulkanRenderer.createMeshModel("Models/Neck_Mech_Walker_by_3DHaupt-(Wavefront OBJ).obj");
	
	
	//int firstModelId = vulkanRenderer.createCube("TexturesCom_SignsNeon0046_S.jpg");
	/*glm::mat4 testMat = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0, 0.0f));
	testMat = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0, 0.0f));
	testMat = glm::translate(testMat, glm::vec3(0.0f, 0.0f, -25.0f));*/
	//vulkanRenderer.updateModel(firstModelId, testMat);
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		float now = glfwGetTime();
		deltaTime = now - lastTime;
		lastTime = now;

		angle += 10.0f * deltaTime;
		if (angle > 360.0f) { angle -= 360.0f; }
		
		glm::mat4 firstModel(1.0f);
		/*glm::mat4 secondModel(1.0f);*/

		firstModel = glm::translate(firstModel, glm::vec3(0.0f, -2.0f, -5.0f));
		firstModel = glm::rotate(firstModel, glm::radians(angle * 5), glm::vec3(0.0f, 1.0f, 0.0f));
		vulkanRenderer.updateModel(firstModelId, firstModel);

		/*secondModel = glm::translate(secondModel, glm::vec3(0.0f, 1.0f, -4.5f));
		secondModel = glm::rotate(secondModel, glm::radians(-angle * 5), glm::vec3(0.0f, 0.0f, 1.0f));
		vulkanRenderer.updateModel(1, secondModel);*/

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