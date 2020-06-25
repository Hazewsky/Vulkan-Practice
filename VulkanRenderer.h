#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept> //pass exceptions
#include <vector>
#include <algorithm>
#include <iostream>
#include <set>
#include <array>

#include "Utilities.h"

class VulkanRenderer
{
public:
	VulkanRenderer();

	int init(GLFWwindow * newWindow);
	void cleanup();

	~VulkanRenderer();

private:
	GLFWwindow* window;
	//Vulkan Components
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

	VkRenderPass renderPass;
	// -- Pipeline -- //
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;

	// -- Pools -- //
	VkCommandPool graphicsCommandPool;

	// -- Utility -- //
	VkFormat swapchainImageFormat;
	VkExtent2D swapchainExtent;
	//set up vlaidation layers
#ifdef NDEBUG
	const bool enableValidationLayers = false;
#else
	const bool enableValidationLayers = true;
#endif

	//vkFunctions
	// - Create Functions
	void createInstance();
	void createLogicalDevice();
	void createDebugMessenger();
	void createSurface();
	void createSwapChain();
	void createRenderPass();
	void createGraphicsPipeline();
	void createFramebuffers();
	void createCommandPool();

	// - Get Functions
	void getPhysicalDevice();

	// - Populate functions
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

	// - Support Functions
	// - Checker Functions
	bool checkInstanceExtensionSupport(std::vector<const char*>* checkExtensions);
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	bool checkDeviceSuitable(VkPhysicalDevice device);
	bool checkValidationLayerSupport(const std::vector<const char*>* checkLayers) const;

	// - Getter Functions
	QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device);
	SwapchainDetails getSwapChainDetails(VkPhysicalDevice device);

	// - Callback Functions
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType, 
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

	// - Wrappers to call extension funcs
	VkResult CreateDebugUtilsMessengerEXT(
		VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pDebugMessenger);

	void DestroyDebugUtilsMessengerEXT(
		VkInstance instance,
		VkDebugUtilsMessengerEXT pDebugMessenger,
		const VkAllocationCallbacks* pAllocator);

	// - Choose funcs
	VkSurfaceFormatKHR chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats);
	VkPresentModeKHR chooseBestSurfacePresentationMode(const std::vector<VkPresentModeKHR>& modes);
	//size of surface images
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);

	// - Create funcs
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
	VkShaderModule createShaderModule(const std::vector<char>& code);
};

