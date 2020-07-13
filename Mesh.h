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
		std::vector<Vertex> * vertices, std::vector<uint32_t> * indices);

	uint32_t getVertexCount();
	uint32_t getIndexCount();
	VkBuffer getVertexBuffer();
	VkBuffer getIndexBuffer();

	void cleanup();

	~Mesh();
private:
	//vertex buffer
	int vertexCount;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	//index buffer
	int indexCount;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;


	VkPhysicalDevice physicalDevice;
	VkDevice device;

	void createVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<Vertex>* vertices);
	void createIndexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<uint32_t> * indices);

	void DestroyVertexBuffer();
	void destroyIndexBuffer();

};

