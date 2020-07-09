#include "Mesh.h"



Mesh::Mesh()
{
}

Mesh::Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newLogicalDevice, 
	VkQueue transferQueue, VkCommandPool transferCommandPool, 
	std::vector<Vertex>* vertices)
{
	vertexCount = vertices->size();
	physicalDevice = newPhysicalDevice;
	device = newLogicalDevice;
	createVertexBuffer(transferQueue, transferCommandPool, vertices);
}


int Mesh::getVertexCount()
{
	return vertexCount;
}

VkBuffer Mesh::getVertexBuffer()
{
	return vertexBuffer;
}

void Mesh::cleanup()
{
	DestroyVertexBuffer();
}


Mesh::~Mesh()
{
}

void Mesh::createVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<Vertex>* vertices)
{
	//Get size of buffer needed for vertices
	VkDeviceSize bufferSize = sizeof(Vertex) * vertices->size();

	//Temporary buffer to "stage" vertex data before transferring to GPU
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	//CREATE STAGING BUFFER AND ALLOCATE MEMORY TO IT
	createBuffer(physicalDevice, device, bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
		&stagingBuffer, &stagingBufferMemory);
	//MAP MEMORY TO VERTEX BUFFER
	void * data;																		// 1. Create pointer to a point in normal memory
	VkResult result = vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);	// 2. "Map" the vertex buffer memory to that pointer
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to map memory for vertex buffer!");
	}
	memcpy(data, vertices->data(), (size_t)bufferSize);									// 3. Copy the data
	vkUnmapMemory(device, stagingBufferMemory);											// 4. Unmap the VertexBUfferMemory

	//Create buffer with TRANSFERR_DST_BIT to mark as recipient of transfer data (also VERTEX_BUFFER)
	//Buffer memory is to be DEVICE_LOCAL_BIT meaning memory is on the GPU and only  accessible by it and not CPU (host)
	createBuffer(physicalDevice, device, bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&vertexBuffer, &vertexBufferMemory);
}

void Mesh::DestroyVertexBuffer()
{
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMemory, nullptr);
}

