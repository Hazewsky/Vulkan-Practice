#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

#include "Utilities.h"

class Mesh
{
public:
	Mesh();
	Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newLogicalDevice, std::vector<Vertex> * vertices);

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

	void createVertexBuffer(std::vector<Vertex> * vertices);

	void DestroyVertexBuffer();

	uint32_t findMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags flags);
};

