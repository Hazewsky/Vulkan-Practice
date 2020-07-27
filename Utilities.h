#pragma once

#include <fstream>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

const int MAX_FRAME_DRAWS = 3;
const int MAX_OBJECTS = 2;

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
	glm::vec3 pos;		// Vertex Position (x, y, z)
	glm::vec4 col;		// Vertex Color (r, g, b, a)
	glm::vec3 normal;	// Vertex Normals (x, y, z)
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


static VkResult createBuffer(VkPhysicalDevice physicalDevice, VkDevice device, 
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
		throw std::runtime_error("Failed to create a Buffer");
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
		throw std::runtime_error("Failed to allocate memory for buffer!");
	}

	//Allocate(Bind) memory to given vertex buffer
	vkBindBufferMemory(device, *buffer, *bufferMemory, 0);

	return result;
}
static VkCommandBuffer beginCommandBuffer(VkDevice device, VkCommandPool commandPool)
{
	//Command buffer to hold transfer command
	VkCommandBuffer commandBuffer;

	//Command buffer details
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	//Allocate command buffer from pool
	vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

	// Information to begin the command buffer record
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; //We've only using the command buffer once, so set up for one time submit

	//Begin recording transfer commands
	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

static void endAndSubmitCommandBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer)
{
	// End commands
	vkEndCommandBuffer(commandBuffer);

	// Queue submission information
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	// Submit transfer command to transfer queue (graphics queue) and wait until it finishes
	vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(queue);

	// Free temp command buffer back to pool
	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

static void copyImageBuffer(
	VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, 
	VkBuffer srcBuffer, VkImage image, uint32_t width, uint32_t height)
{
	VkCommandBuffer transferCommandBuffer = beginCommandBuffer(device, transferCommandPool);
	// Region of data to copy from and to

	VkBufferImageCopy imageRegion = {};
	imageRegion.bufferOffset = 0;											// Offset into data
	imageRegion.bufferRowLength = 0;										// Row length of data to calculate data spacing
	imageRegion.bufferImageHeight = 0;										// Image Height to calculate data spacing
	imageRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;	// Which aspect of image to copy
	imageRegion.imageSubresource.mipLevel = 0;								// Mipmap level to copy
	imageRegion.imageSubresource.baseArrayLayer = 1;						// Starting array layer (if array)
	imageRegion.imageSubresource.layerCount = 1;							// Number of layers to copy starting at baseArrayLayer

	imageRegion.imageExtent = { width, height, 1 };							// Size of data to copy as (x, y, z) values
	imageRegion.imageOffset = {0, 0, 0};									// Offset into image (as opposed to raw data in bufferOffset)
	
	// Command to copy buffer to given image
	vkCmdCopyBufferToImage(transferCommandBuffer, srcBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageRegion);

	// End & submit
	endAndSubmitCommandBuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}
static void copyBuffer(
	VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
	VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize)
{
	
	VkCommandBuffer transferCommandBuffer = beginCommandBuffer(device, transferCommandPool);
	// Region of data to copy from and to
	VkBufferCopy bufferCopyRegion = {};
	bufferCopyRegion.srcOffset = 0;
	bufferCopyRegion.dstOffset = 0;
	bufferCopyRegion.size = bufferSize;

	// Command to copy src buffer to dst buffer
	vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

	// End & submit
	endAndSubmitCommandBuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}

static void transitionImageLayout(VkDevice device, VkQueue queue, VkCommandPool commandPool,
	VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout)
{
	VkCommandBuffer commandBuffer = beginCommandBuffer(device, commandPool);

	VkImageMemoryBarrier imageMemoryBarrier = {};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.oldLayout = currentLayout;								// Layout to transition from
	imageMemoryBarrier.newLayout = newLayout;									// Layout to transition to
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;			// Queue family to transition from
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;			// Queue family to transition to
	imageMemoryBarrier.image = image;											// Image being accessed and modified as part of barrier
	imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;	// Aspect of image begin altered
	imageMemoryBarrier.subresourceRange.baseMipLevel = 0;						// First mipmap layer to start alteration on
	imageMemoryBarrier.subresourceRange.levelCount = 1;							// Number of mipmap levels to alter stating from baseMipLevel
	imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;						// First array layer to start alterations on
	imageMemoryBarrier.subresourceRange.layerCount = 1;							// Number of layers to alter starting from baseArrayLayer
	
	VkPipelineStageFlags srcStage;
	VkPipelineStageFlags dstStage;

	// If transitioning from new image to image ready to receive data...
	if (currentLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		imageMemoryBarrier.srcAccessMask = 0;							// Memory access stage transition must happen after...
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; // Memory access stage transition must happen before...

		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	// If transitioning from transfer destination to shader readable
	else if (currentLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	vkCmdPipelineBarrier(
		commandBuffer,
		srcStage, dstStage,				// Pipeline stage (match to src and dst AccessMasks)
		0,					// Dependency flags
		0, nullptr,			// Global Memory Barrier count + data
		0, nullptr,			// Buffer Memory Barrier count + data
		1, &imageMemoryBarrier	// Image Memory Barrier count + data
	);

	endAndSubmitCommandBuffer(device, commandPool, queue, commandBuffer);
}