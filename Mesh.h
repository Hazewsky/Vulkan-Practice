#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

#include "Utilities.h"

struct Model
{
	glm::mat4 modelMatrix;
};

class Mesh
{
public:
	Mesh();
	Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newLogicalDevice, 
		VkQueue transferQueue, VkCommandPool transferCommandPool, 
		std::vector<Vertex> * vertices, std::vector<uint32_t> * indices);

	void setModel(glm::mat4 model);
	Model getModel();

	uint32_t getVertexCount();
	uint32_t getIndexCount();
	VkBuffer getVertexBuffer();
	VkBuffer getIndexBuffer();

	void cleanup();

	~Mesh();
private:
	Model model;
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

