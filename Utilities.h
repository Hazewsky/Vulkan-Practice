#pragma once

#include <fstream>
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