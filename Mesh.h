#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

#include "Utilities.h"

class Mesh
{
public:
	Mesh();
	Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newLogicalDevice, 
		VkQueue transferQueue, VkCommandPool transferCommandPool, 
		std::vector<Vertex> * vertices);

	int getVertexCount();
	VkBuffer getVertexBuffer();

	void cleanup();

	~Mesh();
private:
	int vertexCount;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;

	VkPhysicalDevice physicalDevice;
	VkDevice device;

	void createVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<Vertex>* vertices);

	void DestroyVertexBuffer();

};

