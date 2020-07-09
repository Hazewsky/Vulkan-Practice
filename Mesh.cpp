#include "Mesh.h"



Mesh::Mesh()
{
}

Mesh::Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newLogicalDevice, std::vector<Vertex>* vertices)
{
	vertexCount = vertices->size();
	physicalDevice = newPhysicalDevice;
	device = newLogicalDevice;
	createVertexBuffer(vertices);
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

void Mesh::createVertexBuffer(std::vector<Vertex>* vertices)
{
	//CREATE VERTEX BUFFER
	//Information to create a buffer (doesn't include assigning memory)
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(Vertex) * vertices->size();			//size of buffer (size of 1 vertex * number of vertices)
	bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;			//Multiple types of buffer possible, we want Vertex Buffer
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;				//Similar to Swap Chain images, can share vertex buffers

	VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &vertexBuffer);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Vertex Buffer");
	}

	//GET BUFFER REQUIREMENTS
	VkMemoryRequirements memRequirements = {};
	vkGetBufferMemoryRequirements(device, vertexBuffer, &memRequirements);

	//ALLOCATE MEMORY TO BUFFER
	VkMemoryAllocateInfo memoryAllocInfo = {};
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memRequirements.size;
	memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(memRequirements.memoryTypeBits,							//Index of memory type based on Physical Device that has required bit flags
										VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);//Vk_MEMORY_PROPERTY_HOST_VISIBLE_BIT: CPU can interact with memory
																													//VK_MEMORY_PROPERTY_HOST_COHERENT_BIT: Allows placement of data straight into buffer after mapping
																													//(otherwise we've have to specify manually with flushMemotyRanges\invalidateMemoryRanges
	//Allocate memory to VkDeviceMemory
	result = vkAllocateMemory(device, &memoryAllocInfo, nullptr, &vertexBufferMemory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate memory for vertex buffer!");
	}
	//Allocate(Bind) memory to given vertex buffer
	vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, 0);

	//MAP MEMORY TO VERTEX BUFFER
	void * data;															// 1. Create pointer to a point in normal memory
	vkMapMemory(device, vertexBufferMemory, 0, bufferInfo.size, 0, &data);	// 2. "Map" the vertex buffer memory to that pointer
	memcpy(data, vertices->data(), (size_t)bufferInfo.size);				// 3. Copy the data
	vkUnmapMemory(device, vertexBufferMemory);								// 4. Unmap the VertexBUfferMemory
}

void Mesh::DestroyVertexBuffer()
{
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMemory, nullptr);
}

uint32_t Mesh::findMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags properties)
{
	//Get properties of physical device memory
	VkPhysicalDeviceMemoryProperties memoryProps;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProps);

	for (uint32_t i = 0; i < memoryProps.memoryTypeCount; i++)
	{
		//use bit shifting to check the allowed type
		if ((allowedTypes & (1 << i))									//Index of memory type must match corresponding bit in allowedTypes
			&& (memoryProps.memoryTypes[i].propertyFlags & properties) == properties)	//check if ALL properties are presented at this memory index
		{
			//this memory type is valied, so return its index
		}
	}

	return uint32_t();
}
