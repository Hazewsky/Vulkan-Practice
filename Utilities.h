#pragma once

#include <fstream>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

const int MAX_FRAME_DRAWS = 3;

const std::vector<const char*> validationLayers =
{
	"VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME //define for swapchain extension
};

//Vertex data layout representation
struct Vertex
{
	glm::vec3 pos; //Vertex Position (x, y, z)
	glm::vec3 col; //Vertex Color (r, g, b)
};

//Indices (locations) of Queue Families (if they exist)
struct QueueFamilyIndices
{
	int graphicsFamily = -1; //location of the Graphics Queue Family
	int presentationFamily = -1; //location of presentation Queue Family. usully the same as graphics family
	int computeFamily = -1;
	int transferFamily = -1;
	//check if queue families are valid
	bool isValid()
	{
		return graphicsFamily >= 0 && presentationFamily >= 0;
	}
};

struct SwapchainDetails
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities; //surface properties, e.g. image size\extent, count, etc
	std::vector<VkSurfaceFormatKHR> surfaceFormats; //surface image formats, e.g. RGBA and size of each color (8/10, etc)
	std::vector<VkPresentModeKHR> presentationModes; //how images should be presented to screen 
};

struct SwapchainImage
{
	VkImage image;
	VkImageView imageView;
};

static std::vector<char> readFile(const std::string& filename)
{
	//read file as std::ios::binary -> Binary;
	//std::ios::ate -> Start reading from EOF. Put the pointer straight to the end (define file size)
	std::ifstream file(filename, std::ios::binary | std::ios::ate);

	//check if file strean successfully opened
	if (!file.is_open())
	{
		throw std::runtime_error("Failed to open a file: " + filename + "!");
	}

	//grab the file size and resize the vector of file buffer
	size_t fileSize = (size_t)file.tellg(); //tellg -> tertive the pos of the pointer
	std::vector<char> fileBuffer(fileSize);

	file.seekg(0); //reset the file, go to pos 0

	//read the file into the buffer stream to the very end
	file.read(fileBuffer.data(), fileSize);
	//clsoe the stream
	file.close();

	return fileBuffer;
}

static uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t allowedTypes, VkMemoryPropertyFlags properties)
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
			return i;
		}
	}

	return uint32_t();
}


static void createBuffer(VkPhysicalDevice physicalDevice, VkDevice device, 
	VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage,
	VkMemoryPropertyFlags bufferProperties, VkBuffer *buffer, VkDeviceMemory *bufferMemory)
{
	//CREATE VERTEX BUFFER
	//Information to create a buffer (doesn't include assigning memory)
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = bufferSize;							//size of buffer (size of 1 vertex * number of vertices)
	bufferInfo.usage = bufferUsage;							//Multiple types of buffer possible
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;		//Similar to Swap Chain images, can share vertex buffers

	VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, buffer);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Vertex Buffer");
	}

	//GET BUFFER REQUIREMENTS
	VkMemoryRequirements memRequirements = {};
	vkGetBufferMemoryRequirements(device, *buffer, &memRequirements);

	//ALLOCATE MEMORY TO BUFFER
	VkMemoryAllocateInfo memoryAllocInfo = {};
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memRequirements.size;
	memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(physicalDevice, 
		memRequirements.memoryTypeBits,													//Index of memory type based on Physical Device that has required bit flags
		bufferProperties);																//Vk_MEMORY_PROPERTY_HOST_VISIBLE_BIT: CPU can interact with memory
																						//VK_MEMORY_PROPERTY_HOST_COHERENT_BIT: Allows placement of data straight into buffer after mapping
																						//VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT: ONLY GPU can interact with memory\data
																						//(otherwise we've have to specify manually with flushMemotyRanges\invalidateMemoryRanges
//Allocate memory to VkDeviceMemory
	result = vkAllocateMemory(device, &memoryAllocInfo, nullptr, bufferMemory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate memory for vertex buffer!");
	}
	//Allocate(Bind) memory to given vertex buffer
	vkBindBufferMemory(device, *buffer, *bufferMemory, 0);
}

static void copyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
	VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize)
{

}
