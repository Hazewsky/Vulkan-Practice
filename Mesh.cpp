#include "Mesh.h"



Mesh::Mesh()
{
}

Mesh::Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newLogicalDevice, 
	VkQueue transferQueue, VkCommandPool transferCommandPool, 
	std::vector<Vertex>* vertices, std::vector<uint32_t> * indices)
{
	vertexCount = static_cast<uint32_t>(vertices->size());
	indexCount = static_cast<uint32_t>(indices->size());
	physicalDevice = newPhysicalDevice;
	device = newLogicalDevice;
	createVertexBuffer(transferQueue, transferCommandPool, vertices);
	createIndexBuffer(transferQueue, transferCommandPool, indices);
}


uint32_t Mesh::getVertexCount()
{
	return vertexCount;
}

uint32_t Mesh::getIndexCount()
{
	return indexCount;
}

VkBuffer Mesh::getVertexBuffer()
{
	return vertexBuffer;
}

VkBuffer Mesh::getIndexBuffer()
{
	return indexBuffer;
}

void Mesh::cleanup()
{
	destroyIndexBuffer();
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

	//Copy staging buffer to vertex buffer on GPU
	copyBuffer(device, transferQueue, transferCommandPool, stagingBuffer, vertexBuffer, bufferSize);

	//Clean up stagin buffer parts
	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Mesh::createIndexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<uint32_t>* indices)
{
	//Get size of buffer needed for indics
	VkDeviceSize bufferSize = sizeof(uint32_t) * indices->size();


	//Temporary buffer to "stage" index data before transferring to GPU
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	//CREATE STAGING BUFFER AND ALLOCATE MEMORY TO IT
	createBuffer(physicalDevice, device, bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer, &stagingBufferMemory);

	//MAP MEMORY TO INDEX BUFFER
	void * data;																		
	VkResult result = vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to map memory for vertex buffer!");
	}
	memcpy(data, indices->data(), (size_t)bufferSize);									
	vkUnmapMemory(device, stagingBufferMemory);											

	//Create buffer for index data on GPU access only area
	createBuffer(physicalDevice, device, bufferSize, 
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&indexBuffer, &indexBufferMemory);

	//Copy stagin buffer to index buffer on GPU
	copyBuffer(device, transferQueue, transferCommandPool, stagingBuffer, indexBuffer, bufferSize);

	//Clean up staging buffer parts
	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBuffer, nullptr);
}

void Mesh::DestroyVertexBuffer()
{
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMemory, nullptr);
}

void Mesh::destroyIndexBuffer()
{
	vkDestroyBuffer(device, indexBuffer, nullptr);
	vkFreeMemory(device, indexBufferMemory, nullptr);
}

