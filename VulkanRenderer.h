#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept> //pass exceptions
#include <vector>
#include <algorithm>
#include <iostream>
#include <set>
#include <array>
#include <string>

#include "stb_image.h"

#include "Mesh.h"
#include "Utilities.h"

class VulkanRenderer
{
public:
	VulkanRenderer();

	int init(GLFWwindow * newWindow);
	void updateModel(int modelId, glm::mat4 newModel);
	void draw();
	void cleanup();
	
	~VulkanRenderer();

private:
	GLFWwindow* window;
	//use to control the maximum number of images being drawn on a queue.
	int currentFrame = 0;

	// -- Scene Objects -- //
	std::vector<Mesh> meshes;

	// -- Scene Settings -- //
	struct UboViewProjection
	{
		glm::mat4 projection;
		glm::mat4 view;
	} uboViewProjection;
	// -- Vulkan Components -- //
	//instance
	//VkXXX - type, vkXXXX - function
	VkInstance instance;
	struct
	{
		VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;
	} mainDevice;
	// -- Queues -- //
	VkQueue graphicsQueue;
	VkQueue presentationQueue;
	QueueFamilyIndices queueFamilyIndices;

	VkDebugUtilsMessengerEXT debugMessenger;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;

	std::vector<SwapchainImage> swapchainImages;
	std::vector<VkFramebuffer> swapchainFramebuffers;
	std::vector<VkCommandBuffer> commandBuffers;

	// -- Depth Buffer -- //
	VkImage depthBufferImage;
	VkDeviceMemory depthBufferImageMemory;
	VkImageView depthBufferImageView;
	VkFormat depthImageFormat;

	VkRenderPass renderPass;

	// -- Descriptors -- //
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSetLayout samplerDescriptorSetLayout;

	//Push Constants
	VkPushConstantRange pushConstantRange;

	VkDescriptorPool descriptorPool;
	VkDescriptorPool samplerDescriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;
	std::vector<VkDescriptorSet> samplerDescriptorSets;

	std::vector<VkBuffer> vpUniformBuffer;
	std::vector<VkDeviceMemory> vpUniformBufferMemory;

	//DYNAMIC UNIFORM BUFFER: TEMPORARY NOT IN USE
	/*std::vector<VkBuffer> modelUniformBufferDynamic;
	std::vector<VkDeviceMemory> modelUniformBufferMemoryDynamic;*/

	//DYNAMIC UNIFORM BUFFER: TEMPORARY NOT IN USE
	/*VkDeviceSize minUniformBufferOffset;
	size_t modelUniformAlignment;
	//Model* modelTransferSpace;*/

	// -- Assets -- //
	VkSampler textureSampler;
	std::vector<VkImage> textureImages;
	std::vector<VkDeviceMemory> textureImageMemory;
	std::vector<VkImageView> textureImageViews;

	// -- Pipeline -- //
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;

	// -- Pools -- //
	VkCommandPool graphicsCommandPool;

	// -- Utility -- //
	VkFormat swapchainImageFormat;
	VkExtent2D swapchainExtent;

	// -- Synchronization -- //
	std::vector<VkSemaphore> imageAvailable; //for rendering
	std::vector<VkSemaphore> renderFinished;
	std::vector<VkFence> drawFences;

	// -- Validation Layers -- //
#ifdef NDEBUG
	const bool enableValidationLayers = false;
#else
	const bool enableValidationLayers = true;
#endif

	//vkFunctions
	// -- Create Functions -- //
	void createInstance();
	void createLogicalDevice();
	void createDebugMessenger();
	void createSurface();
	void createSwapChain();
	void createRenderPass();
	void createDescriptorSetLayout();
	void createPushConstantRange();
	void createGraphicsPipeline();
	void createDepthBufferImage();
	void createFramebuffers();
	void createCommandPool();
	void createCommandBuffers();
	void createTextureSampler();
	void createSynchronization();
	void createUniformBuffers();
	void createDescriptorPool();
	void createDescriptorSets();

	void updateUniformBuffers(uint32_t imageIndex);

	// -- Record Functions -- //
	void recordCommands(uint32_t currentImage);

	// -- Get Functions -- //
	void getPhysicalDevice();

	// -- Allocate Functions -- //
	//void allocateDynamicBufferTransferSpace();

	// -- Populate functions -- //
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

	// -- Support Functions -- //
	// -- Checker Functions -- //
	bool checkInstanceExtensionSupport(std::vector<const char*>* checkExtensions);
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	bool checkDeviceSuitable(VkPhysicalDevice device);
	bool checkValidationLayerSupport(const std::vector<const char*>* checkLayers) const;

	// -- Getter Functions -- //
	QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device);
	SwapchainDetails getSwapChainDetails(VkPhysicalDevice device);

	// -- Callback Functions -- //
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType, 
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

	// -- Wrappers to call extension funcs -- //
	VkResult CreateDebugUtilsMessengerEXT(
		VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pDebugMessenger);

	void DestroyDebugUtilsMessengerEXT(
		VkInstance instance,
		VkDebugUtilsMessengerEXT pDebugMessenger,
		const VkAllocationCallbacks* pAllocator);

	// -- Choose funcs -- //
	
	VkSurfaceFormatKHR chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats);
	VkPresentModeKHR chooseBestSurfacePresentationMode(const std::vector<VkPresentModeKHR>& modes);
	//size of surface images
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);
	VkFormat chooseSupportedFormat(const std::vector<VkFormat> &formats, VkImageTiling tiling, VkFormatFeatureFlags featureFlags);

	// -- Create funcs -- //
	VkImage createImage(
		uint32_t width, uint32_t height, VkFormat format, 
		VkImageTiling tiling, VkImageUsageFlags useFlags, VkMemoryPropertyFlags propFlags,
		VkDeviceMemory *imageMemory);
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
	VkShaderModule createShaderModule(const std::vector<char>& code);
	
	int createTextureImage(std::string fileName);
	int createTexture(std::string fileName);
	int createTextureDescriptor(VkImageView textureImage);
	// -- Loader Functions -- //
	stbi_uc * loadTexture(std::string fileName, int * width, int * height, VkDeviceSize * imageSize);
};

