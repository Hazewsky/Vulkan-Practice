#include "VulkanRenderer.h"

VulkanRenderer::VulkanRenderer()
{

}

int VulkanRenderer::init(GLFWwindow* newWindow)
{
	window = newWindow;

	try
	{
		createInstance();
		createDebugMessenger();
		createSurface();
		getPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createGraphicsPipeline();
		
	}
	catch (const std::runtime_error &e)
	{
		printf("ERROR: %s \n", e.what());
		//return EXIT_FAILURE; //1
	}

	return EXIT_SUCCESS; //0
}

void VulkanRenderer::cleanup()
{
	vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline, nullptr);
	for (auto &imageView : swapchainImages)
	{
		vkDestroyImageView(mainDevice.logicalDevice, imageView.imageView, nullptr);
	}
	swapchainImages.clear();

	vkDestroySwapchainKHR(mainDevice.logicalDevice, swapchain, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	if (enableValidationLayers)
	{
		DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	}
	vkDestroyDevice(mainDevice.logicalDevice, nullptr);
	vkDestroyInstance(instance, nullptr);
	
	
}

VulkanRenderer::~VulkanRenderer()
{
}

void VulkanRenderer::createInstance()
{
	//info about the entire app
	//doesn't affect the program, for dev user
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	//DOESN'T affect th eprogram
	appInfo.pApplicationName = "Test Vulkan App";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0); //custom version of the app
	appInfo.pEngineName = "No Engine"; //custom engine name
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0); //custom engine version
	//DOES affect the program. 
	//Make sure you only use methods supported by your current Vulkan version or you'll experience undefined behavior 
	appInfo.apiVersion = VK_API_VERSION_1_1;

	//create a list to hold instance extensions
	auto instanceExtensions = std::vector <const char*>();
	//fill with glfw extensions to interface Vulkan windowing
	uint32_t glfwExtensionCount = 0;
	//array of pointers to C_Strings
	const char** extensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	if (extensions == NULL || glfwExtensionCount == 0)
	{
		//Vulkan is unavailable - error reported
		throw std::runtime_error("ERROR: Vulkan is unavailable on your system or an error occured");
	}

	//add extensions
	for (size_t i = 0; i < glfwExtensionCount; i++)
	{
		instanceExtensions.push_back(extensions[i]);
	}
	//add a debug messenger
	if (enableValidationLayers)
	{
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
	
	
	//create info for a vkInstance
	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
	createInfo.ppEnabledExtensionNames = instanceExtensions.data();

	//create debug messenger info for pNext to catch vkCreateInstance & vkDestroyInstance errors
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
		createInfo.ppEnabledLayerNames = validationLayers.data();
	
		populateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	}
	else
	{
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
	}

	//check VkInstance support
	if (enableValidationLayers && !checkInstanceExtensionSupport(&instanceExtensions))
	{
		//Trying to set up unsupported extension - error reported
		throw std::runtime_error("ERROR: Trying to set up at least one unsupported VkInstance extension");
	}
	//check for validation layers support
	if (!checkValidationLayerSupport(&validationLayers))
	{
		//Trying to set unsupported validation layers - error reported
		throw std::runtime_error("ERROR: Trying to set up at least one unsupported VkInstance Validation Layer");
	}
	//create instance
	VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
	if (result != VK_SUCCESS)
	{
		//Innstance creation failed - error reported
		throw std::runtime_error("ERROR: Failed to create a Vulkan Instance");
	}
	else
	{
		printf("SUCCESS: Vulkan Instance created successfully! \n");
	}
	
}

void VulkanRenderer::createLogicalDevice()
{
	//get the queue family indices for the chosen physical devices
	QueueFamilyIndices indices = getQueueFamilies(mainDevice.physicalDevice);
	//vecotr ffor queue creation infos
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	//set for queue family indices. it's only for unique ints, 
	//so we're sure there won't be duplicate queue family indices, which will result into app crash
	std::set<int> queueFamilyIndices = {indices.graphicsFamily, indices.presentationFamily};

	for (int queueFamilyIndex : queueFamilyIndices)
	{
		//specify what queues the lgical devicess needs to create and the info to do so
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueCount = 1; //num of queues to create
		queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
		float priority = 1.0f;
		queueCreateInfo.pQueuePriorities = &priority; //tell Vulkan how to handle multiple queues (1.0f - highest proprity)
		queueCreateInfos.push_back(queueCreateInfo);
	}
	

	VkPhysicalDeviceFeatures deviceFeatures = {};
	//TMP: no device features
	//vkGetPhysicalDeviceFeatures(mainDevice.physicalDevice, &deviceFeatures);

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
	deviceCreateInfo.enabledLayerCount = 0;
	deviceCreateInfo.ppEnabledLayerNames = nullptr;
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

	VkResult result = vkCreateDevice(mainDevice.physicalDevice, &deviceCreateInfo, nullptr, &mainDevice.logicalDevice);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: Failed to create a logical device");
	}
	else
	{
		printf("SUCCESS: Vulkan logical device created successfully! \n");
		//get access to queues. they're created at the same time as the device
		//so we want handle to queues
		//from given logical device, given queue family, given queue index, place reference in given VkQueue instance
		vkGetDeviceQueue(mainDevice.logicalDevice, indices.graphicsFamily, 0, &graphicsQueue);
		vkGetDeviceQueue(mainDevice.logicalDevice, indices.presentationFamily, 0, &presentationQueue);
	}
}

void VulkanRenderer::createDebugMessenger()
{
	if (!enableValidationLayers)
	{
		return;
	}

	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	populateDebugMessengerCreateInfo(createInfo);

	if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: Failed to create a debug messenger!");
	}
	else
	{
		printf("SUCCESS: Debug Messenger created successfully!\n");
	};
}

void VulkanRenderer::createSurface()
{
	//create a cross-platform surface
	VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Error: Failed to create surface");
	}
	else
	{
		printf("SUCCESS: Surface created successfully! \n");
	}
}

void VulkanRenderer::createSwapChain()
{
	//get swap chain details so we can peek best settings

	SwapchainDetails details = getSwapChainDetails(mainDevice.physicalDevice);

	//1. CHOOSE BEST SURFACE FORMAT
	VkSurfaceFormatKHR surfaceFormat = chooseBestSurfaceFormat(details.surfaceFormats);
	//2. CHOOSE BEST PRESENTATION MODE
	VkPresentModeKHR presentationMode = chooseBestSurfacePresentationMode(details.presentationModes);
	//3. CHOOSE SWAP CHAIN IMAGE RESOLUTION
	VkExtent2D imageResolution = chooseSwapExtent(details.surfaceCapabilities);

	//How manu images are in the swap chain? Get 1 more than the minimum to allow triple buffering
	uint32_t imageCount = details.surfaceCapabilities.minImageCount + 1;
	//if maxImgCount is 0 -> it's limitless, no need in this check
	if (details.surfaceCapabilities.maxImageCount > 0 
		&& details.surfaceCapabilities.maxImageCount < imageCount) 
	{
		imageCount = details.surfaceCapabilities.maxImageCount;
	}
	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;
	createInfo.imageFormat = surfaceFormat.format;								//swapchain image format
	createInfo.imageColorSpace = surfaceFormat.colorSpace;						//swapchain color space
	createInfo.presentMode = presentationMode;									//swapchain presentation mode
	createInfo.imageExtent = imageResolution;									//swapchain image extents
	createInfo.minImageCount = imageCount;										//minimum images in swapchain
	createInfo.imageArrayLayers = 1;											//number of layers for each image in chain
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;				//What attachments images will be used as (there - only color)
	createInfo.preTransform = details.surfaceCapabilities.currentTransform;		//transform to perform on each of swapchain images
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;				//how to handle blending images with external graphics (e.g. windows)
	createInfo.clipped = VK_TRUE;												//whether to clip parts of image not in view (e.g. behind another window, off screen, etc)
	//queues section
	//Get qeueu family indices
	QueueFamilyIndices indices = getQueueFamilies(mainDevice.physicalDevice);
	//if graphics & presentation families different, then swapchain must let images be shared between families
	if (indices.graphicsFamily != indices.presentationFamily)
	{
		uint32_t queueFamilyIndices[] = {(uint32_t)indices.graphicsFamily, (uint32_t)indices.presentationFamily};
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT; //image share handling between multiple queues
		createInfo.queueFamilyIndexCount = 2; //only graphics family & presentation family can share image
		createInfo.pQueueFamilyIndices = queueFamilyIndices; //array of queues to share between
	}
	//if they are equal - exclusive mode will be enough
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
	}
	//if old swapchain been destroyed and this one replaces it, then link old one to quickly hand over responsibilities
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	VkResult result = vkCreateSwapchainKHR(mainDevice.logicalDevice, &createInfo, nullptr, &swapchain);
	if (result != VK_SUCCESS)
	{
		throw new std::runtime_error("Error: Unable to create swapchain!");
	}
	else
	{
		printf("SUCCESS: Swapchain created successfully! \n");
		//store common used values for later reference
		swapchainImageFormat = surfaceFormat.format;
		swapchainExtent = imageResolution;
		//get the list of images in swapchain. 
		uint32_t swapchainImageCount;
		vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapchainImageCount, nullptr);

		std::vector<VkImage> images(swapchainImageCount);
		vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapchainImageCount, images.data());

		for (const auto& image : images)
		{
			//store image handle
			SwapchainImage swapchainImage = {};
			swapchainImage.image = image;
			//create image view & store it
			swapchainImage.imageView = createImageView(image, swapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

			swapchainImages.push_back(swapchainImage);
			
		}
	}
}

void VulkanRenderer::createGraphicsPipeline()
{
	//read the SPIR-V code of shaders
	auto vertexShaderCode = readFile("Shaders/vert.spv");
	auto fragmentShaderCode = readFile("Shaders/frag.spv");

	//Build Shader Modules to link to Graphics Pipeline
	VkShaderModule vertexShaderModule = createShaderModule(vertexShaderCode);
	VkShaderModule fragmentShaderModule = createShaderModule(fragmentShaderCode);

	// -- SHADER STATE CREATION INFORMATION --
	//Vertex stage creation information
	VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
	vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;		//the type
	vertexShaderCreateInfo.module = vertexShaderModule;										//shader module for this particular stage
	vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;								//what the actual stage is -> vertex, fragment, tesselation, etc
	vertexShaderCreateInfo.pName = "main";													//pointer to the name of the func that's going to run at the start
	
	//Fragment stage creation information
	VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo = {};
	fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderCreateInfo.module = fragmentShaderModule;
	fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderCreateInfo.pName = "main";
	VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };
	//create pipeline
	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.pStages = shaderStages;

	//vkCreateGraphicsPipelines(mainDevice.logicalDevice, nullptr, 1, &graphicsPipelineCreateInfo, nullptr, )
	//we don't need shader modules anymore after pipeline creation -> delete 'em
	vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);
	
}

//best format is subjective but let's use 
//Format: VK_FORMAT_R8G8B8A8_UNORM (8bit RGBA unsigned normalized) Format  VK_FORMAT_B8G8R8A8_UNORM as backup
//Color Space: VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
VkSurfaceFormatKHR VulkanRenderer::chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
	//VK_FORMAT_UNDEFINES means that ALL formats are available and supported
	if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
	{
		return { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}

	//try to get mst common & optimal format
	for (const auto& format : formats)
	{
		if ((format.format == VK_FORMAT_R8G8B8A8_UNORM ||
			format.format == VK_FORMAT_B8G8R8A8_UNORM) &&
			format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return format;
		}
	}
	//return available format
	return formats[0];
}

//let's use Mailbox, we'll skip some frames but it should be faster to present
VkPresentModeKHR VulkanRenderer::chooseBestSurfacePresentationMode(const std::vector<VkPresentModeKHR>& modes)
{
	for (const auto& presentationMode : modes)
	{
		if (presentationMode == VK_PRESENT_MODE_MAILBOX_KHR)
			return presentationMode;
	}
	//FIFO_KHR should always be available because it's a part of default Vulkan spec
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities)
{
	//if currentExtent is at numeric limit - it can vary. Othersize -> it's already at perfect size (size of the window)
	if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return surfaceCapabilities.currentExtent;
	}
	else
	{
		//it value can vary, need to set it manually
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		//create new extent using window size
		VkExtent2D newExtent = { };
		newExtent.width = static_cast<uint32_t>(width);
		newExtent.height = static_cast<uint32_t>(width);

		//Surface also defines max and min, so make sure withing boundaries by clapming values
		newExtent.width = std::max(surfaceCapabilities.minImageExtent.width, std::min(surfaceCapabilities.maxImageExtent.width, newExtent.width));
		newExtent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height, newExtent.height));

		return newExtent;
	}
}

VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
	VkImageViewCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = image;										//image to create a view for
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;					//type (1D, 2D, 3D, Cube, etc)
	createInfo.format = format;										//Format of image data
	createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;		//Allows remapping or RGBA components to other RGBA values
	createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	//subresources allow the view to view only a part of an image
	createInfo.subresourceRange.aspectMask = aspectFlags;			//Which aspect of image to view e.g. COLOR_BIT for viewing color
	createInfo.subresourceRange.baseMipLevel = 0;					//Start mapmap level to view from
	createInfo.subresourceRange.levelCount = 1;						//number of mipmap levels to view
	createInfo.subresourceRange.baseArrayLayer = 0;					//start array layer to view
	createInfo.subresourceRange.layerCount = 1;						//number of array levels to view

	VkImageView imageView;
	VkResult result = vkCreateImageView(mainDevice.logicalDevice, &createInfo, nullptr, &imageView);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Error: Failed to create image view for image!");
	}
	else
	{
		printf("SUCCESS: Image view for image created successfully!\n");
		return imageView;
	}
}

VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& code)
{
	VkShaderModuleCreateInfo createInfo = { };
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;			//structure type
	createInfo.codeSize = code.size();										//length of data
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());		//pointer to the data
	
	VkShaderModule module;
	VkResult result = vkCreateShaderModule(mainDevice.logicalDevice, &createInfo, nullptr, &module);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Error: Unable to create shader module!");
	}
	return module;
}

void VulkanRenderer::getPhysicalDevice()
{
	//pick the most suitable GPU
	//enumarete physical device VkInstance can access
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	//check for available devices. no device - Vulkan is unsupported
	if (deviceCount == 0) {
		throw std::runtime_error("ERROR: Can't find any GPUs that support Vulkan Instance");
	}

	//get the list of physical devices
	std::vector<VkPhysicalDevice> deviceList(deviceCount);
		
	vkEnumeratePhysicalDevices(instance, &deviceCount, deviceList.data());
	//pick the best suitable device
	for (const auto& device : deviceList)
	{
		if (checkDeviceSuitable(device))
		{
			mainDevice.physicalDevice = deviceList[0];
			break;
		}
	}
	

}

void VulkanRenderer::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT /*| VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT*/;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;
	createInfo.pUserData = nullptr;
}

bool VulkanRenderer::checkInstanceExtensionSupport(std::vector<const char*>* checkExtensions)
{
	//get the num of extesions to create an array of the correct size to hold supported extensions
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	//ccreate a ist of VkExtensionProperties using count
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());


	for (const auto& checkExtension : *checkExtensions)
	{
		bool hasExtension = false;
		for (const auto& extension : extensions)
		{
			if (strcmp(checkExtension, extension.extensionName) == 0)
			{
				hasExtension = true;
				break;
			}
				

		}
		if (!hasExtension)
		{
			return false;
		}
		
	}
	/*std::for_each(checkExtensions->begin(), checkExtensions->end(), 
		[&](auto elem) mutable
	{
		auto result = std::find(std::begin(extensions), std::end(extensions), elem);
		if (result == std::end(extensions))
		{
			allExtensionsSupported = false;
		}
	});*/

	return true;
}

bool VulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
	//get ext count
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
	//no extensions
	if (extensionCount == 0)
	{
		return false;
	}
	//populate list of extensions
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

	for (const auto& deviceExtension : deviceExtensions)
	{
		bool hasExtension = false;
		for (const auto& extension : extensions)
		{
			if (strcmp(deviceExtension, extension.extensionName) == 0)
			{
				hasExtension = true;
				break;
			}
		}
		if (!hasExtension)
		{
			return false;
		}
			
	}
	return true;
}

bool VulkanRenderer::checkDeviceSuitable(VkPhysicalDevice device)
{
	//Information about device itself (ID, name, type, vendor, etc...)
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);
	//Information about what device can do (geo shader, tess shader, wide lines, etc)
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	auto indices = getQueueFamilies(device);

	//check extension support
	bool extensionsSupported = checkDeviceExtensionSupport(device);
	
	//check swap chain support
	bool swapChainValid = false;
	if (extensionsSupported)
	{
		SwapchainDetails swapChainDeltails = getSwapChainDetails(device);
		swapChainValid = !swapChainDeltails.presentationModes.empty() && !swapChainDeltails.surfaceFormats.empty();
	}

	return indices.isValid() && extensionsSupported && swapChainValid;
}

bool VulkanRenderer::checkValidationLayerSupport(const std::vector<const char*>* checkLayers) const
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const auto& layerName : *checkLayers)
	{
		bool layerFound = false;
		for (const auto& layerProperties : availableLayers)
		{
			if (strcmp(layerName, layerProperties.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}
		if (!layerFound)
		{
			return false;
		}
	}

	return true;

}

QueueFamilyIndices VulkanRenderer::getQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;

	//get count
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
	//populate with the actual data
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());
	
	
	int i = 0;
	for (const auto& queueFamily : queueFamilyList)
	{
		//get through each queue family an check if it has at least one type of queue (it can have 0 queues in it).
		
		//queue can be multiple types due to bitField. use BITWISE_AND with VK_QUEUE_*_BIT to check its presence
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.graphicsFamily = i;
		}
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
		{
			indices.computeFamily = i;
		}
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
		{
			indices.transferFamily = i;
		}
		//check if queue family supports presentation
		VkBool32 presentationSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentationSupport);
		//check is queue if presentation type (can be both graphics & presentation thou)
		if (queueFamily.queueCount > 0 && presentationSupport)
		{
			indices.presentationFamily = i;
		}

		if (indices.isValid())
		{
			break;
		}

		i++;
	}

	return indices;
}

SwapchainDetails VulkanRenderer::getSwapChainDetails(VkPhysicalDevice device)
{
	SwapchainDetails details;
	//get surface capabilities for the given surface on the given physical device
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.surfaceCapabilities);

	uint32_t count;

	//get surface formats
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, nullptr);

	if (count != 0)
	{
		details.surfaceFormats.resize(count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, details.surfaceFormats.data());
	}

	//get surface presentation modes
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, nullptr);

	if (count != 0)
	{
		details.presentationModes.resize(count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, details.presentationModes.data());
	}

	return details;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderer::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
	std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

VkResult VulkanRenderer::CreateDebugUtilsMessengerEXT(
	VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void VulkanRenderer::DestroyDebugUtilsMessengerEXT(
	VkInstance instance,
	VkDebugUtilsMessengerEXT pDebugMessenger,
	const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		func(instance, pDebugMessenger, pAllocator);
	}
}


