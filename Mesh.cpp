#include "Mesh.h"



Mesh::Mesh()
{
}

Mesh::Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newLogicalDevice, 
	VkQueue transferQueue, VkCommandPool transferCommandPool, 
	std::vector<Vertex>* vertices, std::vector<uint32_t> * indices, int newTextureId)
{
	vertexCount = static_cast<uint32_t>(vertices->size());
	indexCount = static_cast<uint32_t>(indices->size());
	physicalDevice = newPhysicalDevice;
	device = newLogicalDevice;
	createVertexBuffer(transferQueue, transferCommandPool, vertices);
	createIndexBuffer(transferQueue, transferCommandPool, indices);

	model.modelMatrix = glm::mat4(1.0f);

	textureId = newTextureId;
	// Check for vertex normals. It we have a normal vec3::zero (0.0f, 0.0f, 0.0f) - calculate normals manually
	//dut to the plane that we use, we can only calculate normals for 3+ vertices
	//if (vertices->size() >= 3)
	//{
	//	for (size_t i = 0; i < vertices->size(); i++)
	//	{
	//		Vertex v = vertices->data()[i];
	//		
	//		if (v.normal == glm::vec3(0))
	//		{
	//			glm::vec3 normal;
	//			glm::vec3 u;
	//			glm::vec3 v;
	//			// Calculate normals. Since the vertex itself is just a single point with no surface, we've use surrounding vertices to build 
	//			//1st vertex
	//			if (i == 0 && i + 2 < vertices->size())
	//			{
	//				u = (vertices->begin() + 1)->pos - vertices->begin()->pos;
	//				v = (vertices->begin() + 2)->pos - vertices->begin()->pos;
	//				(vertices->begin() + i)->normal = glm::cross(u, v);
	//			}
	//			//middle vertices
	//			else if (i > 0 && i + 1 < vertices->size())
	//			{
	//				u = (vertices->begin() + i - 1)->pos - (vertices->begin() + i)->pos;
	//				v = (vertices->begin() + i + 1)->pos - vertices->begin()->pos;
	//				(vertices->begin() + i)->normal = glm::cross(u, v);
	//			}
	//			//last vertex
	//			else if (i == vertices->size() - 1 && i - 2 >= 0)
	//			{
	//				u = (vertices->begin() + i - 1)->pos - (vertices->begin() + i)->pos;
	//				v = (vertices->begin() + i - 2)->pos - vertices->begin()->pos;
	//				(vertices->begin() + i)->normal = glm::cross(u, v);
	//			}
	//		}
	//		
	//	}
	//}
	//else
	//{
	//	//we don't have enough points to calculate normals. Set arbitrary values

	//}
	
}

void Mesh::setModel(glm::mat4 newModel)
{
	model.modelMatrix = newModel;
}

Model Mesh::getModel()
{
	return model;
}

int Mesh::getTextureId()
{
	return textureId;
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
	//Index buffer destroy
	vkDestroyBuffer(device, indexBuffer, nullptr);
	vkFreeMemory(device, indexBufferMemory, nullptr);

	//Vertex buffer destroy
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMemory, nullptr);
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

	//Copy from staging buffer to GPU access buffer
	copyBuffer(device, transferQueue, transferCommandPool, stagingBuffer, indexBuffer, bufferSize);

	//Clean up staging buffer parts
	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}


