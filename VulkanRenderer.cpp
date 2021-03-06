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
		createRenderPass();
		createDescriptorSetLayout();
		createPushConstantRange();
		createGraphicsPipeline();
		createColorBufferImage();
		createDepthBufferImage();
		createFramebuffers();
		createCommandPool();
		createCommandBuffers();
		createTextureSampler();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
		createInputDescriptorSets();
		createSynchronization();

		// Data used to create objects on a scene

		// Set up Matrices for camera
		uboViewProjection.projection = glm::perspective(glm::radians(45.0f),
			(float)swapchainExtent.width / (float)swapchainExtent.height,
			0.1f, 100.0f);
		uboViewProjection.view = glm::lookAt(glm::vec3(0, 0.0f, 10.0f), glm::vec3(0.0f, 0.0f, -2.0f), glm::vec3(0.0f, 1.0f, 0.0f));

		uboViewProjection.projection[1][1] *= -1;

		// Create default "no texture" texture
		createTexture("plain.png");
	}
	catch (const std::runtime_error &e)
	{
		printf("ERROR: %s \n", e.what());
		//return EXIT_FAILURE; //1
	}

	return EXIT_SUCCESS; //0
}

void VulkanRenderer::updateModel(int modelId, glm::mat4 newModel)
{
	if (modelId >= models.size()) return;

	models[modelId].setModel(newModel);
}

void VulkanRenderer::draw()
{
	//Wait for given fence to signal (open) from last draw before continuing
	vkWaitForFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
	//Manually reset (unsignal) closed fences
	vkResetFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame]);

	// - GET NEXT IMAGE -- //
	//Get index of next image to be drawn to, and signal semaphore when ready to be drawn to
	uint32_t imageIndex;
	vkAcquireNextImageKHR(mainDevice.logicalDevice, swapchain, std::numeric_limits<uint64_t>::max(), imageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);
	
	//Re-record commands
	recordCommands(imageIndex);
	// Update Uniform Values
	updateUniformBuffers(imageIndex);
	
	// -- SUBMIT COMMAND BUFFER TO THE QUEUE -- //
	//queue submittion info
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;							//Number of semaphores to wait on
	submitInfo.pWaitSemaphores = &imageAvailable[currentFrame];				//List of semaphores to wait on

	VkPipelineStageFlags waitStages[] = 
	{
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};													
	submitInfo.pWaitDstStageMask = waitStages;					//Stages on which command buffers should check for semaphores to signal, so can proceed into the execution
	submitInfo.commandBufferCount = 1;							//Number of command buffers to submit simultaneosly
	submitInfo.pCommandBuffers = &commandBuffers[imageIndex];	//Command buffer to submit (threat like individual frame)
	
	submitInfo.signalSemaphoreCount = 1;						//Number of semaphores to signal when command buffer finishes
	submitInfo.pSignalSemaphores = &renderFinished[currentFrame];				//Semaphores to signal when command buffer finishes

	VkResult result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, drawFences[currentFrame]);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to submit commandbuffer(s) info the queue");
	}
	// -- PRESENT RENDERED IMAGE TO SCREEN -- //
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;						//Number of semaphores we shall wait before presenting the image
	presentInfo.pWaitSemaphores = &renderFinished[currentFrame];			//Semaphores need to signal so we can present the image
	presentInfo.swapchainCount = 1;							//Number of swapchains to present to
	presentInfo.pSwapchains = &swapchain;					//Swapchains we'll be presenting images to
	presentInfo.pImageIndices = &imageIndex;				//Index of image inside of swapchains to presenting

	result = vkQueuePresentKHR(graphicsQueue, &presentInfo);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to present to the image");
	}
	//Get next frame (use % MAX_FRAME_DRAWS to keep value below MAX_FRAME_DRAWS)
	currentFrame = (currentFrame + 1) % MAX_FRAME_DRAWS;
}

void VulkanRenderer::cleanup()
{
	
	//wait till the device become idle to safely destroy semaphores & pools
	vkDeviceWaitIdle(mainDevice.logicalDevice);
	//same for queue
	//vkQueueWaitIdle(graphicsQueue);

	//free dynamic buffer allocated memory
	//_aligned_free(modelTransferSpace);

	for (size_t i = 0; i < models.size(); i++)
	{
		models[i].destroyMeshModel();
	}
	vkDestroySampler(mainDevice.logicalDevice, textureSampler, nullptr);

	// Cleanup textures
	for (size_t i = 0; i < textureImages.size(); i++)
	{
		vkDestroyImageView(mainDevice.logicalDevice, textureImageViews[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice, textureImages[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, textureImageMemory[i], nullptr);
	}

	// Cleanup Depth Buffer
	for (size_t i = 0; i < depthBufferImage.size(); i++)
	{
		vkDestroyImageView(mainDevice.logicalDevice, depthBufferImageView[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice, depthBufferImage[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, depthBufferImageMemory[i], nullptr);
	}
	
	// Cleanup Color Buffer
	for (size_t i = 0; i < colorBufferImage.size(); i++)
	{
		vkDestroyImageView(mainDevice.logicalDevice, colorBufferImageView[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice, colorBufferImage[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, colorBufferImageMemory[i], nullptr);
	}

	//don't need to free Descriptor Sets because they're be automatically released after Descriptor Pool destroy
	// Input Descriptor Sets
	vkDestroyDescriptorPool(mainDevice.logicalDevice, inputDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, inputDescriptorSetLayout, nullptr);
	//Sampler Descriptor Sets
	vkDestroyDescriptorPool(mainDevice.logicalDevice, samplerDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, samplerDescriptorSetLayout, nullptr);
	//Uniform Descriptor Sets
	vkDestroyDescriptorPool(mainDevice.logicalDevice, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, descriptorSetLayout, nullptr);

	for (size_t i = 0; i < swapchainImages.size(); i++)
	{
		//DYNAMIC UNIFORM BUFFER: TEMPORARY NOT IN USE
		/*vkDestroyBuffer(mainDevice.logicalDevice, modelUniformBufferDynamic[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, modelUniformBufferMemoryDynamic[i], nullptr);*/

		vkDestroyBuffer(mainDevice.logicalDevice, vpUniformBuffer[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, vpUniformBufferMemory[i], nullptr);
	}

	for (size_t i = 0; i < MAX_FRAME_DRAWS; i++)
	{
		vkDestroyFence(mainDevice.logicalDevice, drawFences[i], nullptr);
		vkDestroySemaphore(mainDevice.logicalDevice, renderFinished[i], nullptr);
		vkDestroySemaphore(mainDevice.logicalDevice, imageAvailable[i], nullptr);
	}
	

	//destroy command pool handles this
	/*vkFreeCommandBuffers(mainDevice.logicalDevice, graphicsCommandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
	commandBuffers.clear();*/

	vkDestroyCommandPool(mainDevice.logicalDevice, graphicsCommandPool, nullptr);

	for (auto &frameBuffer : swapchainFramebuffers)
	{
		vkDestroyFramebuffer(mainDevice.logicalDevice, frameBuffer, nullptr);
	}
	swapchainFramebuffers.clear();

	vkDestroyPipeline(mainDevice.logicalDevice, secondPipeline, nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice, secondPipelineLayout, nullptr);

	vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice, pipelineLayout, nullptr);

	vkDestroyRenderPass(mainDevice.logicalDevice, renderPass, nullptr);
	
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
	//store queue family indices for a later use
	queueFamilyIndices = indices;
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
	deviceFeatures.samplerAnisotropy = VK_TRUE; //Enable Anisotropy

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
	//QueueFamilyIndices indices = getQueueFamilies(mainDevice.physicalDevice);
	
	//if graphics & presentation families different, then swapchain must let images be shared between families
	if (queueFamilyIndices.graphicsFamily != queueFamilyIndices.presentationFamily)
	{
		uint32_t indices[] = {(uint32_t)queueFamilyIndices.graphicsFamily, (uint32_t)queueFamilyIndices.presentationFamily};
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT; //image share handling between multiple queues
		createInfo.queueFamilyIndexCount = 2; //only graphics family & presentation family can share image
		createInfo.pQueueFamilyIndices = indices; //array of queues to share between
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

void VulkanRenderer::createRenderPass()
{
	// Array of subpasses
	std::array<VkSubpassDescription, 2> subpasses{};

	// ATTACHEMENTS
	// SUBPASS 1 ATTACHMENTS + REFS ( INPUT ATTACHMENTS PASSED TO SUBPASS 2 )

	// Color attachment (Input)
	// Get supported format for color attachment
	colorImageFormat = chooseSupportedFormat(
		{ VK_FORMAT_R8G8B8A8_UNORM },
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	);

	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = colorImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // it op performs after render pass was finished (after 2 subpasses), but we use it in sub pass 1, so we really don't care
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Depth Attachment (Input)
	depthImageFormat = chooseSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = depthImageFormat;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// Color Attachment (Input) Reference
	VkAttachmentReference colorAttachmentReference = {};
	colorAttachmentReference.attachment = 1;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Depth Attachment ( Input) Reference
	VkAttachmentReference depthAttachmentReference = {};
	depthAttachmentReference.attachment = 2;
	depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	
	// Set up Subpass 1
	// Information about a particular subpass the Render Pass is using
	subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Pipeline type subpass is to be bound to
	subpasses[0].colorAttachmentCount = 1;
	subpasses[0].pColorAttachments = &colorAttachmentReference;
	subpasses[0].pDepthStencilAttachment = &depthAttachmentReference;

	//////////////////////////////////////////////////////////////////////////////////////

	// SUBPASS 2 ATTACHMENTS + REFERENCES (To pass to swapchain)

	// Swapchain Color attachment of render pass
	VkAttachmentDescription swapchainColorAttachment = {};
	swapchainColorAttachment.format = swapchainImageFormat;						// Format to use for attachment
	swapchainColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;					// Number of samples to write for multisampling
	swapchainColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;				// Describes what to do with attachment before rendering
	swapchainColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;		// Describes what to do with attachment after rendering
	swapchainColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	// Describes what to do with stencil before rendering
	swapchainColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;	// Describes what to do with stencil after rendering

	// Framebuffer data will be stored as an image, but images can be given different data layouts
	// to give optimal use for certain operations so define appropriate formats for different 
	swapchainColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;		// image data layout before render pass starts
	swapchainColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;					// src of presentation. Image data layout after render pass (to change to)

	// REFERENCES

	// Attachment reference uses an attachment index  that refers to index in the attachment list passed to renderPassCreateInfo
	VkAttachmentReference swapchainColorAttachmentReference = {};
	swapchainColorAttachmentReference.attachment = 0;
	swapchainColorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// References to attachments that subpass will take input from
	std::array<VkAttachmentReference, 2> inputReferences;
	inputReferences[0].attachment = 1;
	inputReferences[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	inputReferences[1].attachment = 2;
	inputReferences[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	// Set up Subpass 2
	// Information about a particular subpass the Render Pass is using
	subpasses[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Pipeline type subpass is to be bound to
	subpasses[1].colorAttachmentCount = 1;
	subpasses[1].pColorAttachments = &swapchainColorAttachmentReference;
	subpasses[1].inputAttachmentCount = static_cast<uint32_t>(inputReferences.size());
	subpasses[1].pInputAttachments = inputReferences.data();
	////////////////////////////////////////////////////////////////////////////////////////////////////////
	// SUBPASS DEPENDENCIES

	// Need to determine when layout transitions occur using subpass dependencies
	std::array<VkSubpassDependency, 3> subpassDeps;

	// 1. Conversion from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	// Must happen after the END of external subpass
	subpassDeps[0].srcSubpass = VK_SUBPASS_EXTERNAL; // Subpass index (VK_SUBPASS_EXTERNAL = Special value meaning outside of renderpass)
	subpassDeps[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; // Pipeline stage (VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT = end of pipeline)
	subpassDeps[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT; // Stage memory mask. (VK_ACCESS_MEMORY_READ_BIT = memory stage must happen before conversion to be able to read it)
	// BUT must happen before the color output of our main subpass (at 0)
	subpassDeps[0].dstSubpass = 0;
	subpassDeps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDeps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; //and before when can read\write from this thing
	subpassDeps[0].dependencyFlags = 0;

	// 2. Subpass 1 layout (color/depth) to Subpass 2 (Shader read)
	subpassDeps[1].srcSubpass = 0;
	subpassDeps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDeps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDeps[1].dstSubpass = 1;
	subpassDeps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	subpassDeps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	subpassDeps[1].dependencyFlags = 0;

	// 3. Conversion from VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	// Must happen after the END of external subpass
	subpassDeps[2].srcSubpass = 1;																				// Subpass index (0 = main subpass)
	subpassDeps[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;								// Pipeline stage (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT = after draw operation)
	subpassDeps[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;	// Stage memory mask. (VK_ACCESS_MEMORY_READ_BIT = memory stage must happen before conversion to be able to read it)
	// BUT must happen before the color output of our main subpass (at 0)
	subpassDeps[2].dstSubpass = VK_SUBPASS_EXTERNAL;
	subpassDeps[2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDeps[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT; // and before when can read\write from this thing
	subpassDeps[2].dependencyFlags = 0;

	std::vector<VkAttachmentDescription> attachments = {swapchainColorAttachment, colorAttachment, depthAttachment};
	// Create Info for Render Pass
	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassCreateInfo.pAttachments = attachments.data();
	renderPassCreateInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
	renderPassCreateInfo.pSubpasses = subpasses.data();
	renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDeps.size());
	renderPassCreateInfo.pDependencies = subpassDeps.data();

	VkResult result = vkCreateRenderPass(mainDevice.logicalDevice, &renderPassCreateInfo, nullptr, &renderPass);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Unable to create render pass!");
	}
}

void VulkanRenderer::createDescriptorSetLayout()
{
	// UNIFORM DESCRIPTOR SET LAYOUT
	// View-Projection Binding info
	VkDescriptorSetLayoutBinding viewProjectionLayoutBinding = {};
	viewProjectionLayoutBinding.binding = 0;											//Binding point in shader (designated by binding number in shader)
	viewProjectionLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;		//Type of descriptor(uniform, dynamic uniform, image samples)
	viewProjectionLayoutBinding.descriptorCount = 1;									//Number of descriptor for binding
	viewProjectionLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;				//Shader stage to bind to (vertex, geometry, fragment, etc)
	viewProjectionLayoutBinding.pImmutableSamplers = nullptr;							//For Texture: can make sampler data immutable by specifying in layout. Only the SAMPLER. 
																						//The ImageView it samples from can still be changed
	
	
	// DYNAMIC UNIFORM BUFFER: TEMPORARY NOT IN USE
	// Binding for Dynamic Uniform for Model Data
	/*VkDescriptorSetLayoutBinding modelLayoutBinding = {};
	modelLayoutBinding.binding = 1;
	modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	modelLayoutBinding.descriptorCount = 1;
	modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	modelLayoutBinding.pImmutableSamplers = nullptr;*/

	std::vector<VkDescriptorSetLayoutBinding> layoutBindings = {viewProjectionLayoutBinding/*, modelLayoutBinding*/};
	// Create descriptor set layout with given bindings
	VkDescriptorSetLayoutCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	createInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());	//Number of binding infos
	createInfo.pBindings = layoutBindings.data();							//Array of binding infos

	// Create descriptor set layout
	VkResult result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &createInfo, nullptr, &descriptorSetLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create uniform descriptor set layout!");
	}

	// SAMPLER DESCRIPTOR SET LAYOUT
	// Texture binding info
	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	// Texture create info
	VkDescriptorSetLayoutCreateInfo textureLayoutCreateInfo = {};
	textureLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	textureLayoutCreateInfo.bindingCount = 1;
	textureLayoutCreateInfo.pBindings = &samplerLayoutBinding;

	result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &textureLayoutCreateInfo, nullptr, &samplerDescriptorSetLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create sampler descriptor set layout!");
	}
	
	// CREATE INPUT ATTACHMENT IMAGE DESCRIPTOR SET LAYOUT
	// Color Input binding
	VkDescriptorSetLayoutBinding colorInputLayoutBinding = {};
	colorInputLayoutBinding.binding = 0;
	colorInputLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	colorInputLayoutBinding.descriptorCount = 1;
	colorInputLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// Depth input binding
	VkDescriptorSetLayoutBinding depthInputLayoutBinding = {};
	depthInputLayoutBinding.binding = 1;
	depthInputLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	depthInputLayoutBinding.descriptorCount = 1;
	depthInputLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// Array for input layout bindings
	std::vector<VkDescriptorSetLayoutBinding> inputBindings{ colorInputLayoutBinding, depthInputLayoutBinding };

	// INPUT DESCRIPTOR SET LAYOUT CREATE INFO
	VkDescriptorSetLayoutCreateInfo inputLayoutCreateInfo = {};
	inputLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	inputLayoutCreateInfo.bindingCount = static_cast<uint32_t>(inputBindings.size());
	inputLayoutCreateInfo.pBindings = inputBindings.data();

	result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &inputLayoutCreateInfo, nullptr, &inputDescriptorSetLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create input descriptor set layout!");
	}
}

void VulkanRenderer::createPushConstantRange()
{
	// Define push constant values (no 'create' needed)
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;	// Shader stage push constant will go to
	pushConstantRange.offset = 0;								// Offset into given data to pass to push constant
	pushConstantRange.size = sizeof(Model);						// Size of data being passed to push constant
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
	//Geometry stage creation information
	VkPipelineShaderStageCreateInfo geometryShaderCreateInfo = {};
	geometryShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	//geometryShaderCreateInfo.module = null;
	geometryShaderCreateInfo.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
	geometryShaderCreateInfo.pName = "main";

	//How the data for a single vertex (including info such as pos, color, text coords, normals, etc) is as a whole
	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = 0;								//Can bind multiple streams of data, so define each one
	bindingDescription.stride = sizeof(Vertex);					//Size of a single vertex object
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;	//How to move between data after each vertex
																//VK_VERTEX_INPUT_RATE_VERTEX	: move on to the next vertex (draw object by object)
																//VK_VERTEX_INPUT_RATE_INSTANCE : move on to a vertex of the next instance (draw all 1st vertices in all instances, then 2nd vertices and so on

	//How the data for an attribute is defined withing a vertex
	std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions;
	//Position attribute
	attributeDescriptions[0].binding = 0;							//Which binding the data is at (should be same as above, unless you have multiple streams of data)
	attributeDescriptions[0].location = 0;							//Location in shader where data will be read from
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;	//Format the data will take (also helps define size of data)
	attributeDescriptions[0].offset = offsetof(Vertex, pos);		//Where this attribute is defined in the data for a single vertex
	//Color attribute
	attributeDescriptions[1].binding = 0;							
	attributeDescriptions[1].location = 1;							
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;		//VK_FORMAT_R8G8B8A8_SRGB
	attributeDescriptions[1].offset = offsetof(Vertex, col);
	//Normal attribute
	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[2].offset = offsetof(Vertex, normal);
	//UVs attribute
	attributeDescriptions[3].binding = 0;
	attributeDescriptions[3].location = 3;
	attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[3].offset = offsetof(Vertex, UVs);

	// -- 1. VERTEX INPUT (TODO: Put in vertex descriptions when resources created) --
	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;											//List of Vertex BInding Descriptions (data spacing/stride info)
	vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();								//List of Vertex Attribute Descriptions (data format, where to bind to\from)

	// -- 2. INPUT ASSEMBLY --
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
	inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; //primitive type to assemble vertcies
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE; //allow overriding strip topology to start new primitives

	// -- 3. VIEWPORT & SCISSOR --
	//create a viewport info struct
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapchainExtent.width;
	viewport.height = (float)swapchainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	//create a scissor info struct
	VkRect2D scissor = {};
	scissor.offset = { 0,0 };			//offsets to use region from
	scissor.extent = swapchainExtent;	//extent to describe region to use, starting at offset

	VkPipelineViewportStateCreateInfo viewportCreateInfo = {};
	viewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportCreateInfo.viewportCount = 1;
	viewportCreateInfo.pViewports = &viewport;
	viewportCreateInfo.scissorCount = 1;
	viewportCreateInfo.pScissors = &scissor;

	// -- 4. DYNAMIC STATES --
	//Dynamic states to enable
	//TODO::Implement dynamic states for pipeline
	//std::vector<VkDynamicState> dynamicStateEnables;
	//dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);	//Dynamic viewport : Can resize in command buffer with vkCmdSetViewport(commandbuffer, 0, 1, &viewport);
	//dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);	//Dynamic Scissor: Can resize in command buffer with vkCmdSetScissor(commandBuffer, 0, 0, &scissor)

	//VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	//dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	//dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
	//dynamicStateCreateInfo.pDynamicStates = dynamicStateEnables.data();

	// -- 5. TESSELATION --
	//VkPipelineTessellationStateCreateInfo tesselationCreateInfo = {};
	//tesselationCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
	

	// -- 6. RASTERIZATION --
	VkPipelineRasterizationStateCreateInfo rasterizationCreateInfo = {};
	rasterizationCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationCreateInfo.depthClampEnable = VK_FALSE;				// Change if fragments deyond near\far planes are clipped (default VK_TRUE) or clamped to plane(can mess realtime shadowing)
	rasterizationCreateInfo.rasterizerDiscardEnable = VK_FALSE;			// Whether to discard data and skip rasterizer. Never creates fragments, only suitable for pipeline without framebuffer output
	rasterizationCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;			// How to handle filling points between vertices. If other then FIll -> requires GPU feature
	rasterizationCreateInfo.lineWidth = 1;								// How thick lines should be when drawn. If value is other than 1 -> GPU feature required
	rasterizationCreateInfo.cullMode = /*VK_CULL_MODE_BACK_BIT*/VK_CULL_MODE_NONE;			// Which face of a triangle to cull
	rasterizationCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;// Winding to determine which size is front
	rasterizationCreateInfo.depthBiasEnable = VK_FALSE;					// Whether to add depth bias to fragments (good for stopping "shadow acne" in shadow mapping)

	// -- 7. MULTISAMPLING --
	VkPipelineMultisampleStateCreateInfo multisampleCreateInfo = {};
	multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleCreateInfo.sampleShadingEnable = VK_FALSE;				// enable multisample shading
	multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // number of samples you take from the surrounding area. This case - single sample per fragment

	// -- 8. COLOR BLENDING --
	// Blend Attachment State (how blending is handled)
	VkPipelineColorBlendAttachmentState colorState = {};
	colorState.colorWriteMask = 
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;		//	set the mask to define color to be applied in blend
	colorState.blendEnable = VK_TRUE;	//	enable blending
	//	Blending uses equation: (srcColorBlendFactor * newColor) colorBlendOp (dstColorBlendFactor * oldColor)
	//	blend for color
	colorState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; //mult newColor by Alpha value
	colorState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorState.colorBlendOp = VK_BLEND_OP_ADD;
	//	Summarized: (VK_BLEND_FACTOR_SRC_ALPHA * new color) + (VK_BLEND_FACTOR_SRC_ONE_MINUS_ALPHA * old color);
	//			  (new color alpha * new color) + ((1 - new color alpha) * old color)
	//	blend for alpha
	colorState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; //get rid of the old alpha and replace with a new one
	colorState.alphaBlendOp = VK_BLEND_OP_ADD;
	//	Summarized: (1 * new alpha) + (0 * old alpha) = new alpha

	VkPipelineColorBlendStateCreateInfo colorBlendingCreateInfo = {};
	colorBlendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendingCreateInfo.logicOpEnable = VK_FALSE; //alternative to calculations is to use logical operations
	//	colorBlendingCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendingCreateInfo.attachmentCount = 1;
	colorBlendingCreateInfo.pAttachments = &colorState; 
	//	colorBlendingCreateInfo.blendConstants[0] = //blend constants of size 4

	std::vector<VkDescriptorSetLayout> descriptorSetLayouts = { descriptorSetLayout, samplerDescriptorSetLayout };
	// -- 9. PIPELINE LAYOUT --
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

	VkResult result = vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Unable to create pipeline layout!");
	}
	// -- 10. DEPTH STENCIL TESTING
	// TODO: Set up depth stencil testing
	VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = {};
	depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilCreateInfo.depthTestEnable = VK_TRUE;			// Enable checking depth to determine fragment write
	depthStencilCreateInfo.depthWriteEnable = VK_TRUE;			// Enable writing to depth buffer (to replace old values)
	depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;	// Comparison operation that allows an overwrite (is in front)
	depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;	// Depth Bound Test: Does the depth value exists between two bounds
	depthStencilCreateInfo.stencilTestEnable = VK_FALSE;

	// -- GRAPHICS PIPELINE CREATION --
	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2;									//Number of shader stages
	pipelineCreateInfo.pStages = shaderStages;							//Shader stages
	pipelineCreateInfo.layout = pipelineLayout;							//Pipeline layout the pipeline should use
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;		//All the fixed function pipeline states
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportCreateInfo;
	pipelineCreateInfo.pDynamicState = /*&dynamicStateCreateInfo*/nullptr;
	pipelineCreateInfo.pTessellationState = nullptr;					//TODO: Implement tesselation state
	pipelineCreateInfo.pRasterizationState = &rasterizationCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendingCreateInfo;		// Color blending stage
	pipelineCreateInfo.pDepthStencilState = &depthStencilCreateInfo;	// Depth Stencil test stage
	pipelineCreateInfo.renderPass = renderPass;							//which render pass will use that pipeline and pipeline will be compatible with
	pipelineCreateInfo.subpass = 0;										//which subpass to use with pipeline. It's one subpass per pipeline
	//Pipeline derivatives : Can create multiple pipelines that derive from one another for optimization
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;				//Existing pipeline to derive from...
	pipelineCreateInfo.basePipelineIndex = -1;							//or index of base pipeline other pipelines should be derived from (in case creating multiple at once)

	result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Unable to create pipeline(-s)!");
	}

	//we don't need shader modules anymore after pipeline creation -> delete 'em
	vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);
	
	// CREATE SECOND PASS PIPELINE
	// Second pass shaders
	auto secondVertexShaderCode = readFile("Shaders/second_vert.spv");
	auto secondFragmentShaderCode = readFile("Shaders/second_frag.spv");

	// BUild shaders
	VkShaderModule secondVertexShaderModule = createShaderModule(secondVertexShaderCode);
	VkShaderModule secondFragmentShaderModule = createShaderModule(secondFragmentShaderCode);

	// Set new shaders
	vertexShaderCreateInfo.module = secondVertexShaderModule;
	fragmentShaderCreateInfo.module = secondFragmentShaderModule;

	VkPipelineShaderStageCreateInfo secondShaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };

	// No vertex data for second pass
	vertexInputCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputCreateInfo.pVertexAttributeDescriptions = nullptr;

	//Don't want to write to depth buffer
	depthStencilCreateInfo.depthWriteEnable = VK_FALSE;

	// Create new pipeline layout
	VkPipelineLayoutCreateInfo secondPipelineLayoutCreateInfo = {};
	secondPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	secondPipelineLayoutCreateInfo.setLayoutCount = 1;
	secondPipelineLayoutCreateInfo.pSetLayouts = &inputDescriptorSetLayout;
	secondPipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	secondPipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	result = vkCreatePipelineLayout(mainDevice.logicalDevice, &secondPipelineLayoutCreateInfo, nullptr, &secondPipelineLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create second pipeline layout!");
	}

	pipelineCreateInfo.pStages = secondShaderStages;	//	Update second shader stage list
	pipelineCreateInfo.layout = secondPipelineLayout;	//	Change pipeline layout for input attachment descriptor sets
	pipelineCreateInfo.subpass = 1;						//	Use second subpass

	// Create second pipeline
	result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &secondPipeline);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create second graphics pipeline");
	}

	// Destroy unused shader modules
	vkDestroyShaderModule(mainDevice.logicalDevice, secondVertexShaderModule, nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, secondFragmentShaderModule, nullptr);
}

void VulkanRenderer::createColorBufferImage()
{
	// Resize supported format for color attachment
	colorBufferImage.resize(swapchainImages.size());
	colorBufferImageMemory.resize(swapchainImages.size());
	colorBufferImageView.resize(swapchainImages.size());

	for (size_t i = 0; i < swapchainImages.size(); i++)
	{
		// Create Color Buffer Image
		colorBufferImage[i] = createImage(swapchainExtent.width, swapchainExtent.height, colorImageFormat,
			VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, 
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&colorBufferImageMemory[i]);

		// Create Color Buffer Image View
		colorBufferImageView[i] = createImageView(colorBufferImage[i], colorImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

void VulkanRenderer::createDepthBufferImage()
{
	depthBufferImage.resize(swapchainImages.size());
	depthBufferImageMemory.resize(swapchainImages.size());
	depthBufferImageView.resize(swapchainImages.size());
	for (size_t i = 0; i < swapchainImages.size(); i++)
	{
		// Create Depth Buffer Image
		depthBufferImage[i] = createImage(swapchainExtent.width, swapchainExtent.height, depthImageFormat,
			VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&depthBufferImageMemory[i]);

		// Create Depth Buffer Image View
		depthBufferImageView[i] = createImageView(depthBufferImage[i], depthImageFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
	}
	
}

void VulkanRenderer::createFramebuffers()
{
	// resize framebuffer count to == swapchain image count
	swapchainFramebuffers.resize(swapchainImages.size());
	// create for each swap chain image
	for (size_t i = 0; i < swapchainFramebuffers.size(); i++)
	{
		std::array<VkImageView, 3> attachments = 
		{ swapchainImages[i].imageView,
			colorBufferImageView[i],
			depthBufferImageView[i]
		};

		VkFramebufferCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.renderPass = renderPass;										// Render pass layout the framebuffer will be used with
		createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		createInfo.pAttachments = attachments.data();							// List of attachments (1:1 with render pass)
		createInfo.width = swapchainExtent.width;								// Framebuffer width
		createInfo.height = swapchainExtent.height;								// Framebuffer height
		createInfo.layers = 1;													// Framebuffer layers

		VkResult result = vkCreateFramebuffer(mainDevice.logicalDevice, &createInfo, nullptr, &swapchainFramebuffers[i]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Unable to create framebuffer!");
		}
	}
}

void VulkanRenderer::createCommandPool()
{
	VkCommandPoolCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; //The command buffer can not be reset explicitly or implicitly using vkBeginCommandBuffer
	createInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;	//Queue family type that buffers from this command will use
	
	VkResult result = vkCreateCommandPool(mainDevice.logicalDevice, &createInfo, nullptr, &graphicsCommandPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Unable to create graphics command pool");
	}
}

void VulkanRenderer::createCommandBuffers()
{
	//resize command buffer count, 1 per each framebuffer
	commandBuffers.resize(swapchainFramebuffers.size());
	//we don't create command buffer, we allocate its, cause the memory region for it already exists due to CommandPool
	VkCommandBufferAllocateInfo cbAllocInfo = {};

	cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbAllocInfo.commandPool = graphicsCommandPool; //which pool we allocate from. 
	//In this case, command buffer is now allocated with graphics queue only
	cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;	//VK_COMMAND_BUFFER_LEVEL_PRIMARY -> buffer is submitted straight to the queue, run on the queue; can't be called from another command buffer
															//VK_COMMAND_BUFFER_LEVEL_SECONDARY -> can't be parsed to a queue, but can be called by another command buffer (via vbCmdExecuteCommands(buffer))
	cbAllocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size()); //amount of buffers to allocate
	//allocate bufffers & place handles into array of buffers
	VkResult result = vkAllocateCommandBuffers(mainDevice.logicalDevice, &cbAllocInfo, commandBuffers.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Can't allocate command buffers for graphics pipeline!");
	}
}

void VulkanRenderer::createSynchronization()
{
	imageAvailable.resize(MAX_FRAME_DRAWS);
	renderFinished.resize(MAX_FRAME_DRAWS);
	drawFences.resize(MAX_FRAME_DRAWS);
	//Semaphore creation info
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	//Fence creation information
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAME_DRAWS; i++)
	{
		if (vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &imageAvailable[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create Image Available semaphore");
		}
		if (vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &renderFinished[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create Render Finished semaphore");
		}
		if (vkCreateFence(mainDevice.logicalDevice, &fenceCreateInfo, nullptr, &drawFences[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create Draw Fence");
		}
	}
	
}

void VulkanRenderer::createTextureSampler()
{
	// Sapmler Create Info
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;			// How to render when image is magnified on screen
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;						// How to render when image is mnified on screen
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;	// How to handle texture wrap in U (x) direction
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;	// How to handle texture wrap in V (y) direction
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;	// How to handle texture wrap in W (z) direction
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;	// Border beyond texture (only works for border clamp)
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;				// Whether coords should be normalized between 0 and 1
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;		// Mipmap interpolation mode
	samplerCreateInfo.mipLodBias = 0.0f;								// Level of Details bias for mip level
	samplerCreateInfo.minLod = 0;										// Minimum Level of Detail to pick mip level
	samplerCreateInfo.maxLod = 0;										// Maximum Level of Detail to pick mip level
	samplerCreateInfo.anisotropyEnable = VK_TRUE;						// Enable Anisotropy
	samplerCreateInfo.maxAnisotropy = 16;								// Anisotropy sample level

	VkResult result = vkCreateSampler(mainDevice.logicalDevice, &samplerCreateInfo, nullptr, &textureSampler);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create sampler!");
	}

}

void VulkanRenderer::createUniformBuffers()
{
	// Buffer size will size of all view-projection variables (will offset to access)
	VkDeviceSize vpBufferSize = sizeof(UboViewProjection);
	//DYNAMIC UNIFORM BUFFER: TEMPORARY NOT IN USE
	// Model data buffer size
	/*VkDeviceSize modelBufferSize = modelUniformAlignment * MAX_OBJECTS;*/
	// One uniform buffer for each image (and by extension, command buffer)
	vpUniformBuffer.resize(swapchainImages.size());
	vpUniformBufferMemory.resize(swapchainImages.size());
	//DYNAMIC UNIFORM BUFFER: TEMPORARY NOT IN USE
	// One dynamic uniform buffer for each image that handles ALL model data of ALL objects located on the scene (but less then MAX_OBJECTS)
	/*modelUniformBufferDynamic.resize(swapchainImages.size());
	modelUniformBufferMemoryDynamic.resize(swapchainImages.size());*/

	// Create Uniform Buffers
	for (size_t i = 0; i < swapchainImages.size(); i++)
	{
		createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, vpBufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&vpUniformBuffer[i], &vpUniformBufferMemory[i]);
		//DYNAMIC UNIFORM BUFFER: TEMPORARY NOT IN USE
		/*createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, modelBufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&modelUniformBufferDynamic[i], &modelUniformBufferMemoryDynamic[i]);*/
	}
	
}

void VulkanRenderer::createDescriptorPool()
{
	// CREATE UNIIFORM DESCRIPTOR POOL //
	// Type of descriptors and how many DESCRIPTORS, not discriptor sets (combined makes the pool size, a part of the pool)
	VkDescriptorPoolSize vpPoolSize = {};
	vpPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vpPoolSize.descriptorCount = static_cast<uint32_t>(vpUniformBuffer.size());

	// DYNAMIC UNIFORM BUFFER: TEMPORARY NOT IN USE
	// Model Pool (DYNAMIC)
	/*VkDescriptorPoolSize modelPoolSize = {};
	modelPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	modelPoolSize.descriptorCount = static_cast<uint32_t>(modelUniformBufferDynamic.size());*/

	// List of pool sizes
	std::vector<VkDescriptorPoolSize> poolSizes = {vpPoolSize/*, modelPoolSize*/};

	// Data to create Descriptor Pool
	VkDescriptorPoolCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	createInfo.maxSets = static_cast<uint32_t>(swapchainImages.size());			//Max number of Descriptor sSets that can be created from pool
	createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());			//Amount of Pool Sizes (parts) being passed
	createInfo.pPoolSizes = poolSizes.data();									//Pool Sizes (parts) to create pool with

	VkResult result = vkCreateDescriptorPool(mainDevice.logicalDevice, &createInfo, nullptr, &descriptorPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create uniform descriptor pool!");
	}
	
	// CREATE SAMPLER DESCRIPTOR POOL //
	// Texture sampler pool
	VkDescriptorPoolSize samplerPoolSize = {};
	samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerPoolSize.descriptorCount = MAX_OBJECTS; // Totally messes up the logic if we want to 
	//swap textures for object or use multipler textures 

	VkDescriptorPoolCreateInfo samplerPoolCreateInfo = {};
	samplerPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	samplerPoolCreateInfo.maxSets = MAX_OBJECTS;
	samplerPoolCreateInfo.poolSizeCount = 1;
	samplerPoolCreateInfo.pPoolSizes = &samplerPoolSize;

	result = vkCreateDescriptorPool(mainDevice.logicalDevice, &samplerPoolCreateInfo, nullptr, &samplerDescriptorPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create sampler descriptor pool!");
	}

	// CREATE INPUT ATTACHMENT DESCRIPTOR POOL //
	VkDescriptorPoolSize colorInputPoolSize = {};
	colorInputPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	colorInputPoolSize.descriptorCount = static_cast<uint32_t>(colorBufferImageView.size());

	VkDescriptorPoolSize depthInputPoolSize = {};
	depthInputPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	depthInputPoolSize.descriptorCount = static_cast<uint32_t>(depthBufferImageView.size());

	std::array<VkDescriptorPoolSize, 2> inputPoolSizes{ colorInputPoolSize, depthInputPoolSize };

	VkDescriptorPoolCreateInfo inputPoolCreateInfo = {};
	inputPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	inputPoolCreateInfo.maxSets = static_cast<uint32_t>(swapchainImages.size());
	inputPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(inputPoolSizes.size());
	inputPoolCreateInfo.pPoolSizes = inputPoolSizes.data();

	result = vkCreateDescriptorPool(mainDevice.logicalDevice, &inputPoolCreateInfo, nullptr, &inputDescriptorPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create input descriptor pool!");
	}
}

void VulkanRenderer::createDescriptorSets()
{
	// Resize Descriptor Set list so one for every buffer
	descriptorSets.resize(swapchainImages.size());

	std::vector<VkDescriptorSetLayout> setLayouts(swapchainImages.size(), descriptorSetLayout);

	// Data to allocate Descriptor Set
	VkDescriptorSetAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.descriptorPool = descriptorPool;										//Pool to allocate Descriptor Set from
	allocateInfo.descriptorSetCount = static_cast<uint32_t>(swapchainImages.size());	//Number of sets to allocate
	allocateInfo.pSetLayouts = setLayouts.data();										//Layouts to use to allocate set (1:1 relationship)
	
	// Allocae descriptor sets (multiple)
	VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &allocateInfo, descriptorSets.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate descriptor sets for buffer");
	}
	
	// Update all of descriptor set buffer bindings
	for (size_t i = 0; i < swapchainImages.size(); i++)
	{
		// Buffer info and data offset info for view-projection
		VkDescriptorBufferInfo vpBuffferInfo = {};
		vpBuffferInfo.buffer = vpUniformBuffer[i];			//Buffer to get data from
		vpBuffferInfo.offset = 0;							//Position of start of data
		vpBuffferInfo.range = sizeof(UboViewProjection);					//Size of data to be bound to the descriptor set

		//DYNAMIC UNIFORM BUFFER: TEMPORARY NOT IN USE
		//For model data
		/*VkDescriptorBufferInfo modelBufferInfo = {};
		modelBufferInfo.buffer = modelUniformBufferDynamic[i];
		modelBufferInfo.offset = 0;
		modelBufferInfo.range = modelUniformAlignment;*/

		// Data about connection between binding and buffer
		VkWriteDescriptorSet vpSetWrite = {};
		vpSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		vpSetWrite.dstSet = descriptorSets[i];							// Descriptor Set to update
		vpSetWrite.dstBinding = 0;										// Binding to update (matches with binding on layout/shader) 
		vpSetWrite.dstArrayElement = 0;									// Index in array to update
		vpSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;	// Type of descriptor
		vpSetWrite.descriptorCount = 1;									// Amount to update
		vpSetWrite.pBufferInfo = &vpBuffferInfo;						// Info about buffer data to bind

		//DYNAMIC UNIFORM BUFFER: TEMPORARY NOT IN USE
		//same for model data
		/*VkWriteDescriptorSet modelSetWrite = {};
		modelSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		modelSetWrite.dstSet = descriptorSets[i];
		modelSetWrite.dstBinding = 1;
		modelSetWrite.dstArrayElement = 0;
		modelSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		modelSetWrite.descriptorCount = 1;
		modelSetWrite.pBufferInfo = &modelBufferInfo;*/

		std::vector<VkWriteDescriptorSet> setWrites = { vpSetWrite/*, modelSetWrite */};

		// Update the descriptor sets with new buffer/binding info
		vkUpdateDescriptorSets(mainDevice.logicalDevice, static_cast<uint32_t>(setWrites.size()), setWrites.data(), 0, nullptr);
	}
	 
}

void VulkanRenderer::createInputDescriptorSets()
{
	// Resize array to hold desriptor set for each swapchain image
	inputDescriptorSets.resize(swapchainImages.size());

	// Fill array of layouts ready for set creation
	std::vector<VkDescriptorSetLayout> setLayouts(swapchainImages.size(), inputDescriptorSetLayout);

	// Input Attachment Descriptor Set Allocation Info
	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = inputDescriptorPool;
	setAllocInfo.descriptorSetCount = static_cast<uint32_t>(swapchainImages.size());
	setAllocInfo.pSetLayouts = setLayouts.data();

	// Allocate Descriptor Sets
	VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocInfo, inputDescriptorSets.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate Input Attachment Descriptor Sets!");
	}

	// Update each descriptor set with input attachment
	for (size_t i = 0; i < swapchainImages.size(); i++)
	{
		// Color Attachment Descriptor
		VkDescriptorImageInfo colorAttachmentDescriptor = {};
		colorAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		colorAttachmentDescriptor.imageView = colorBufferImageView[i];
		colorAttachmentDescriptor.sampler = VK_NULL_HANDLE; //Sampler doesn't actually work with subpasses + input attachments 
		//because sampler requires negihboring fragment but input attachments only have access to their current processed fragment

		// Color Attachment Descriptor Write
		VkWriteDescriptorSet colorWrite = {};
		colorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		colorWrite.dstSet = inputDescriptorSets[i];
		colorWrite.dstBinding = 0;
		colorWrite.dstArrayElement = 0;
		colorWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		colorWrite.descriptorCount = 1;
		colorWrite.pImageInfo = &colorAttachmentDescriptor;

		// Depth Attachment Descriptor
		VkDescriptorImageInfo depthAttachmentDescriptor = {};
		depthAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		depthAttachmentDescriptor.imageView = depthBufferImageView[i];
		depthAttachmentDescriptor.sampler = VK_NULL_HANDLE; //Sampler doesn't actually work with subpasses + input attachments 
		//because sampler requires negihboring fragment but input attachments only have access to their current processed fragment

		// Depth Attachment Descriptor Write
		VkWriteDescriptorSet depthWrite = {};
		depthWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		depthWrite.dstSet = inputDescriptorSets[i];
		depthWrite.dstBinding = 1;
		depthWrite.dstArrayElement = 0;
		depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		depthWrite.descriptorCount = 1;
		depthWrite.pImageInfo = &depthAttachmentDescriptor;

		// List of input descriptor set writes
		std::vector<VkWriteDescriptorSet> setWrites = { colorWrite, depthWrite };

		//Update descriptor sets
		vkUpdateDescriptorSets(mainDevice.logicalDevice, static_cast<uint32_t>(setWrites.size()), setWrites.data(), 0, nullptr);

	}
}

void VulkanRenderer::updateUniformBuffers(uint32_t imageIndex)
{
	// Copy VP Data
	void* data;
	VkResult result = vkMapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex], 0, sizeof(UboViewProjection), 0, &data);
	if (result == VK_SUCCESS)
	{
		memcpy(data, &uboViewProjection, sizeof(UboViewProjection));
		vkUnmapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex]);
	}
	//DYNAMIC UNIFORM BUFFER: TEMPORARY NOT IN USE
	// Copy Model Data
	/*for (size_t i = 0; i < meshes.size(); i++)
	{
		Model* thisModel = (Model*)((uint64_t)modelTransferSpace + (i * modelUniformAlignment));
		*thisModel = meshes[i].getModel();
	}
	// Map the lst of model data
	result = vkMapMemory(mainDevice.logicalDevice, modelUniformBufferMemoryDynamic[imageIndex], 0, modelUniformAlignment * meshes.size(), 0, &data);
	if (result == VK_SUCCESS)
	{
		memcpy(data, modelTransferSpace, sizeof(UboViewProjection));
		vkUnmapMemory(mainDevice.logicalDevice, modelUniformBufferMemoryDynamic[imageIndex]);
	}*/

}

void VulkanRenderer::recordCommands(uint32_t currentImage)
{
	//Information about how to begin each command buffer
	VkCommandBufferBeginInfo bufferBeginInfo = {};
	bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	//fences prevent us from command avalanche, so we don't need this flag anymore
	//bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; //buffer can be reused while it's still in use and waiting for execution

	//info about how to begin render pass (only needed for graphical apps)
	VkRenderPassBeginInfo renderBeginInfo = {};
	renderBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderBeginInfo.renderPass = renderPass; //Render Pass to begin
	renderBeginInfo.renderArea.offset = {0,0}; //start point of render pass (pixels)
	renderBeginInfo.renderArea.extent = swapchainExtent; //size of region to render pass on (starting at offset)
	//set clear values (color + depth)
	std::array<VkClearValue, 3> clearValues = {};
	clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };	//for swapchainImage
	clearValues[1].color = { 0.6f, 0.65f, 0.4f, 1.0f };
	clearValues[2].depthStencil.depth = 1.0f;
	
	renderBeginInfo.pClearValues = clearValues.data(); //List of clear values
	renderBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());

	renderBeginInfo.framebuffer = swapchainFramebuffers[currentImage];

	//start recording commands to command buffer
	VkResult result =  vkBeginCommandBuffer(commandBuffers[currentImage], &bufferBeginInfo);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to start recording a Command Buufer");
	}
		
	//Begin render pass
	vkCmdBeginRenderPass(commandBuffers[currentImage], &renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		//Bind pipeline to be used in render pass
		vkCmdBindPipeline(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		//record drawing all meshes
		for (size_t j = 0; j < models.size(); j++)
		{
			MeshModel thisModel = models[j];
			// Push constants are the same for each model
			vkCmdPushConstants(
				commandBuffers[currentImage],
				pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT,	// State to push constants to
				0,							//	Offset of push constant to update
				sizeof(Model),				//	Size if data being pushed
				&thisModel.getModel());		//	Actual data to push (can be array)
			for (size_t k = 0; k < thisModel.getMeshCount(); k++)
			{
			// Use vertex buffer
			VkBuffer vertexBuffers[] = { thisModel.getMesh(k)->getVertexBuffer() };									//Buffers to bind
			VkDeviceSize vertexOffsets[] = { 0 };														//Offsets into buffers being bound
			vkCmdBindVertexBuffers(commandBuffers[currentImage], 0, 1, vertexBuffers, vertexOffsets);	//Command to bind vertex buffer before drawing

			// Use index buffer
			//VkBuffer indexBuffers[] = { thisModel.getMesh(k)->getIndexBuffer() };
			//VkDeviceSize indexOffsets[] = { 0 };
			vkCmdBindIndexBuffer(commandBuffers[currentImage], thisModel.getMesh(k)->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

			//DYNAMIC UNIFORM BUFFER: TEMPORARY NOT IN USE
			//Dynamic offset amount
			/*uint32_t dynamicOffset = static_cast<uint32_t>(modelUniformAlignment) * j;*/
			// "Push" constants to given stage directly (no buffer)
		
			// Bind Descriptor Sets
			std::array<VkDescriptorSet, 2> descriptorSetGroup = 
			{ 
				descriptorSets[currentImage], 
				samplerDescriptorSets[thisModel.getMesh(k)->getTextureId()]
			};

			vkCmdBindDescriptorSets(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
				0, static_cast<uint32_t>(descriptorSetGroup.size()), descriptorSetGroup.data(), /*1*/0, /*&dynamicOffset*/nullptr);

			// Execute Pipeline
			// Without index buffers
			// vkCmdDraw(commandBuffers[i], static_cast<uint32_t>(firstMesh.getVertexCount()), 1, 0, 0);
			// With index buffers
			vkCmdDrawIndexed(commandBuffers[currentImage], thisModel.getMesh(k)->getIndexCount(), 1, 0, 0, 0);
			}
		}
		// Start second subpass
		vkCmdNextSubpass(commandBuffers[currentImage], VK_SUBPASS_CONTENTS_INLINE);

		// BInd new subpass related to the next subpass
		vkCmdBindPipeline(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, secondPipeline);
		vkCmdBindDescriptorSets(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, secondPipelineLayout,
			0, 1, &inputDescriptorSets[currentImage],
			0, nullptr);
		vkCmdDraw(commandBuffers[currentImage], 3, 1, 0, 0);

	//End render pass
	vkCmdEndRenderPass(commandBuffers[currentImage]);
	//stop recording to command buffer
	result = vkEndCommandBuffer(commandBuffers[currentImage]);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to stop recording a Command Buufer");
	}
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

VkFormat VulkanRenderer::chooseSupportedFormat(const std::vector<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags featureFlags)
{
	// Loop throught options and pick up compatible one
	for (VkFormat format : formats)
	{
		// Get properties for given format on this device
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(mainDevice.physicalDevice, format, &properties);

		// Depending of tiling choice, need to check for different bit flag
		if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & featureFlags) == featureFlags)
		{
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & featureFlags) == featureFlags)
		{
			return format;
		}
	}

	throw std::runtime_error("Failed to find a matching format!");
}

VkImage VulkanRenderer::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags useFlags, VkMemoryPropertyFlags propFlags, VkDeviceMemory* imageMemory)
{
	// Create Image
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;				// Type of image (1D, 2D, 3D)
	imageCreateInfo.extent.width = width;						// Width of image extent
	imageCreateInfo.extent.height = height;						// Height of image extent
	imageCreateInfo.extent.depth = 1;							// Depth of image (just 1, no 3D aspect)
	imageCreateInfo.mipLevels = 1;								// Nubmer of mipmap levels
	imageCreateInfo.arrayLayers = 1;							// Number of levels in image array
	imageCreateInfo.format = format;							// Format type of image
	imageCreateInfo.tiling = tiling;							// How image data should be "tiled" (Arranged for optimal reading speed)
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;	// Layout of image data on creation
	imageCreateInfo.usage = useFlags;							// Bit flags defining what image will be used for
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;			// Number of samples for multisampling
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;	// Whether image can be shared between queues

	VkImage image;
	VkResult result = vkCreateImage(mainDevice.logicalDevice, &imageCreateInfo, nullptr, &image);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create an Image!");
	}
	// CREATE MEMORY FOR IMAGE
	// Get memory requirements for a type of image
	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(mainDevice.logicalDevice, image, &memoryRequirements);

	// Allocate memory using image requirements and user defined properties
	VkMemoryAllocateInfo memoryAllocateInfo = {};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = findMemoryTypeIndex(mainDevice.physicalDevice, memoryRequirements.memoryTypeBits, propFlags);

	result = vkAllocateMemory(mainDevice.logicalDevice, &memoryAllocateInfo, nullptr, imageMemory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate memory for an Image!");
	}

	//Bind the image to the memory
	vkBindImageMemory(mainDevice.logicalDevice, image, *imageMemory, 0);

	return image;
}

VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
	VkImageViewCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = image;										// image to create a view for
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;					// type (1D, 2D, 3D, Cube, etc)
	createInfo.format = format;										// Format of image data
	createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;		// Allows remapping or RGBA components to other RGBA values
	createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	// subresources allow the view to view only a part of an image
	createInfo.subresourceRange.aspectMask = aspectFlags;			// Which aspect of image to view e.g. COLOR_BIT for viewing color
	createInfo.subresourceRange.baseMipLevel = 0;					// Start mapmap level to view from
	createInfo.subresourceRange.levelCount = 1;						// number of mipmap levels to view
	createInfo.subresourceRange.baseArrayLayer = 0;					// start array layer to view
	createInfo.subresourceRange.layerCount = 1;						// number of array levels to view

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
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;			// structure type
	createInfo.codeSize = code.size();										// length of data
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());		// pointer to the data
	
	VkShaderModule module;
	VkResult result = vkCreateShaderModule(mainDevice.logicalDevice, &createInfo, nullptr, &module);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Error: Unable to create shader module!");
	}
	return module;
}

int VulkanRenderer::createTextureImage(std::string fileName)
{
	// Load image file
	int width, height;
	VkDeviceSize imageSize;
	stbi_uc * imageData = loadTexture(fileName, &width, &height, &imageSize);

	// Create staging buffer to hold loaded data, ready to copy to device
	VkBuffer imageStagingBuffer;
	VkDeviceMemory imageStagingBufferMemory;
	createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice,
		imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&imageStagingBuffer, &imageStagingBufferMemory);
	// Copy Image Data to staging buffer
	void *data;
	vkMapMemory(mainDevice.logicalDevice, imageStagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, imageData, static_cast<size_t>(imageSize));
	vkUnmapMemory(mainDevice.logicalDevice, imageStagingBufferMemory);

	//Free original iamge data
	stbi_image_free(imageData);

	// Create image to hold final texture
	VkImage texImage;
	VkDeviceMemory texImageMemory;
	texImage = createImage(width, height, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &texImageMemory);

	// TRANSITION IMAGE TO THE CORRECT STATE FOR WRITING TO THE IMAGE STAGING BUFFER
	transitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, 
		texImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// COPY DATA TO IMAGE
	// Copy image data
	copyImageBuffer(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, imageStagingBuffer, texImage, width, height);

	// TRANSITION IMAGE TO BE SHADER READABLE FOR SHADER USAGE
	transitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool,
		texImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// Add texture data to vector for reference
	textureImages.push_back(texImage);
	textureImageMemory.push_back(texImageMemory);

	// Destroy staging buffer
	vkDestroyBuffer(mainDevice.logicalDevice, imageStagingBuffer, nullptr);
	vkFreeMemory(mainDevice.logicalDevice, imageStagingBufferMemory, nullptr);

	//return index fo new texture image
	return textureImages.size() - 1;
}

int VulkanRenderer::createTexture(std::string fileName)
{
	// Create Texture Image and get its location in array
	int textureImageLoc = createTextureImage(fileName);

	// Create ImageView and add it to the lsit
	VkImageView imageView = createImageView(textureImages[textureImageLoc], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
	textureImageViews.push_back(imageView);

	// Create Texture Descriptor Set and get its location in array
	int descriptorLoc = createTextureDescriptor(imageView);

	//Return location of set with texture
	return descriptorLoc;
}

int VulkanRenderer::createTextureDescriptor(VkImageView textureImage)
{
	VkDescriptorSet descriptorSet;

	// Descriptor Set Allocation Info
	VkDescriptorSetAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.descriptorPool = samplerDescriptorPool;
	allocateInfo.descriptorSetCount = 1;
	allocateInfo.pSetLayouts = &samplerDescriptorSetLayout;

	// Allocate 
	VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &allocateInfo, &descriptorSet);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate texture descriptor sets!");
	}

	// Texture Image Info
	VkDescriptorImageInfo imageInfo = {};
	imageInfo.sampler = textureSampler;									// Sampler to use for set
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;	// Image layout when in use
	imageInfo.imageView = textureImage;									// Image to bind to set
	// Descriptor Write Info
	VkWriteDescriptorSet descriptorWrite = {};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = descriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &imageInfo;

	// Update new descriptor set
	vkUpdateDescriptorSets(mainDevice.logicalDevice, 1, &descriptorWrite, 0, nullptr);

	// Add descriptor Set to list
	samplerDescriptorSets.push_back(descriptorSet);

	// Return descriptor set location
	return samplerDescriptorSets.size() - 1;
}

int VulkanRenderer::createMeshModel(std::string modelFile)
{
	// Import model "scene"
	Assimp::Importer importer;
	//In case shoulkd need normals
	//importer.SetPropertyFloat("PP_GSN_MAX_SMOOTHING_ANGLE", 90); // flag to respect "real" edges
	
	const aiScene* scene = importer.ReadFile(modelFile, 
		aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices /*| aiProcess_GenSmoothNormals*/);

	if (!scene)
	{
		throw std::runtime_error("Failed to load model! (" + modelFile + ")\n");
	}

	// Get vector of all materials with 1:1 ID placement
	std::vector<std::string> textureNames = MeshModel::LoadMaterials(scene);

	// Conversion from the materials list IDs to our Descriptor Array IDs
	std::vector<int> matToTex(textureNames.size());

	// Loop over textureNames and create textures for them
	for (size_t i = 0; i < textureNames.size(); i++)
	{
		// If material had no texture, set '0' to indicate no texture, texture 0 will be reserved for a default texture (e.g. Diffuse)
		if (textureNames[i].empty())
		{
			matToTex[i] = 0;
		}
		else
		{
			// Otherwise, create texture and set value to index of new texture
			matToTex[i] = createTexture(textureNames[i]);
		}
	}

	// Load in all out meshes
	std::vector<Mesh> modelMeshes = MeshModel::LoadNode(
		mainDevice.physicalDevice, mainDevice.logicalDevice, graphicsQueue,
		graphicsCommandPool, scene->mRootNode, scene, matToTex);

	// Create mesh model and add to list
	MeshModel meshModel = MeshModel(modelMeshes);
	models.push_back(meshModel);

	return models.size() - 1;
}

int VulkanRenderer::createCube(std::string texture)
{
	std::vector<Vertex> meshVertices =
	{
		// FRONT FACE
		{{-0.5, 0.5, 0.5f}, {0.1f, 0.3f, 0.5f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},	// 0
		{{-0.5, -0.5, 0.5f}, {0.5f, 0.3f, 0.1f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},	// 1
		{{0.5, -0.5, 0.5f}, {0.3f, 0.5f, 0.1f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},	// 2
		{{0.5, 0.5, 0.5f}, {0.3f, 0.1f, 0.5f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},		// 3
		//// BACK FACE
		{{-0.5, 0.5, -0.5f}, {0.1f, 0.3f, 0.5f, 1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},	// 4
		{{-0.5, -0.5, -0.5f}, {0.5f, 0.3f, 0.1f, 1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},	// 5
		{{0.5, -0.5, -0.5f},  {0.3f, 0.5f, 0.1f, 1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},	// 6
		{{0.5, 0.5, -0.5f}, {0.3f, 0.1f, 0.5f, 1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},	// 7
		//EXTRA FOR LIGHTING TEST
		//LEFT FACE
		{{-0.5, 0.5, -0.5f}, {0.1f, 0.3f, 0.5f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},	// 8
		{{-0.5, -0.5, -0.5f}, {0.5f, 0.3f, 0.1f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},	// 9
		{{-0.5, -0.5, 0.5f},  {0.3f, 0.5f, 0.1f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},	// 10
		{{-0.5, 0.5, 0.5f}, {0.3f, 0.1f, 0.5f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},	// 11
		//RIGHT FACE
		{{0.5, 0.5, 0.5f},{0.1f, 0.3f, 0.5f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},		// 12
		{{0.5, -0.5, 0.5f}, {0.5f, 0.3f, 0.1f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},	// 13
		{{0.5, -0.5, -0.5f}, {0.3f, 0.5f, 0.1f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},	// 14
		{{0.5, 0.5, -0.5f}, {0.3f, 0.1f, 0.5f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},	// 15
		//TOP FACE
		{{-0.5, 0.5, -0.5f}, {0.1f, 0.3f, 0.5f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},	// 16
		{{-0.5, 0.5, 0.5f}, {0.5f, 0.3f, 0.1f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},	// 17
		{{0.5, 0.5, 0.5f},  {0.3f, 0.5f, 0.1f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},	// 18
		{{0.5, 0.5, -0.5f},{0.3f, 0.1f, 0.5f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},		// 19
		//BOTTOM FACE
		{{-0.5, -0.5, -0.5f}, {0.1f, 0.3f, 0.5f, 1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},	// 20
		{{-0.5, -0.5, 0.5f}, {0.5f, 0.3f, 0.1f, 1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},	// 21
		{{0.5, -0.5, 0.5f},  {0.3f, 0.5f, 0.1f, 1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},	// 22
		{{0.5, -0.5, -0.5f},{0.3f, 0.1f, 0.5f, 1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},	// 23

	};
	std::vector<uint32_t> meshIndices1 =
	{
		//FRONT FACE
		0, 1, 2,	//1st triangle
		2, 3, 0,	//2nd triangle
		////BACK FACE
		4, 5, 6,
		6, 7, 4,
		////LEFT FACE
		1, 0, 4,
		4, 5, 1,
		////RIGHT FACE
		6, 7, 3,
		3, 2, 6,
		////TOP FACE
		3, 7, 4,
		4, 0, 3,
		////BOTTOM FACE
		2, 6, 5,
		5, 1, 2
	};
	Mesh newMesh(mainDevice.physicalDevice, mainDevice.logicalDevice,
		graphicsQueue, graphicsCommandPool, &meshVertices, &meshIndices1,
		createTexture(texture));
	models.push_back(MeshModel(std::vector<Mesh>{ newMesh }));
	return models.size() - 1;
}

stbi_uc * VulkanRenderer::loadTexture(std::string fileName, int * width, int * height, VkDeviceSize * imageSize)
{
	// Number of channels image uses
	int channels;
	
	// Load pixel data for image
	std::string fileLoc = "Textures/" + fileName;
	stbi_uc * image = stbi_load(fileLoc.c_str(), width, height, &channels, STBI_rgb_alpha);

	if (!image)
	{
		throw std::runtime_error("Failed to load a Texture file! (" + fileName + ")");
	}

	*imageSize = *width * *height * 4;

	return image;
}

void VulkanRenderer::getPhysicalDevice()
{
	// pick the most suitable GPU
	// enumarete physical device VkInstance can access
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	// check for available devices. no device - Vulkan is unsupported
	if (deviceCount == 0) {
		throw std::runtime_error("ERROR: Can't find any GPUs that support Vulkan Instance");
	}

	// get the list of physical devices
	std::vector<VkPhysicalDevice> deviceList(deviceCount);
		
	vkEnumeratePhysicalDevices(instance, &deviceCount, deviceList.data());
	// pick the best suitable device
	for (const auto& device : deviceList)
	{
		if (checkDeviceSuitable(device))
		{
			mainDevice.physicalDevice = deviceList[0];
			break;
		}
	}
	
	// Get properties of our device

	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(mainDevice.physicalDevice, &deviceProperties);
	// DYNAMIC UNIFORM BUFFER: TEMPORARY NOT IN USE
	// minUniformBufferOffset = deviceProperties.limits.minUniformBufferOffsetAlignment;
	// calculate our actual offset for modelUniform
	// allocateDynamicBufferTransferSpace();
}
/*
// DYNAMIC UNIFORM BUFFER: TEMPORARY NOT IN USE
void VulkanRenderer::allocateDynamicBufferTransferSpace()
{
	
	// Calculate alignment of model data
	modelUniformAlignment = (sizeof(Model) + minUniformBufferOffset - 1) & ~(minUniformBufferOffset - 1);

	// Create Space in memory to hold dynamic buffer that is aligned to our required alignment and holds MAX_OBJECTS
	modelTransferSpace = (Model*)_aligned_malloc(modelUniformAlignment * MAX_OBJECTS, modelUniformAlignment);
}
*/
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
	// get the num of extesions to create an array of the correct size to hold supported extensions
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	// create a list of VkExtensionProperties using count
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
	// Information about device itself (ID, name, type, vendor, etc...)
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	// Information about what device can do (geo shader, tess shader, wide lines, etc)
	VkPhysicalDeviceFeatures deviceFeatures;
	// Enable Rasterization Depth clamp
	// deviceFeatures.depthClamp = VK_TRUE;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	auto indices = getQueueFamilies(device);

	// check extension support
	bool extensionsSupported = checkDeviceExtensionSupport(device);
	
	// check swap chain support
	bool swapChainValid = false;
	if (extensionsSupported)
	{
		SwapchainDetails swapChainDeltails = getSwapChainDetails(device);
		swapChainValid = !swapChainDeltails.presentationModes.empty() && !swapChainDeltails.surfaceFormats.empty();
	}

	return indices.isValid() && extensionsSupported && swapChainValid && deviceFeatures.samplerAnisotropy;
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

	// get count
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
	// populate with the actual data
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());
	
	
	int i = 0;
	for (const auto& queueFamily : queueFamilyList)
	{
		// get through each queue family an check if it has at least one type of queue (it can have 0 queues in it).
		
		// queue can be multiple types due to bitField. use BITWISE_AND with VK_QUEUE_*_BIT to check its presence
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
		// check if queue family supports presentation
		VkBool32 presentationSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentationSupport);
		// check is queue if presentation type (can be both graphics & presentation thou)
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
	// get surface capabilities for the given surface on the given physical device
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.surfaceCapabilities);

	uint32_t count;

	// get surface formats
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, nullptr);

	if (count != 0)
	{
		details.surfaceFormats.resize(count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, details.surfaceFormats.data());
	}

	// get surface presentation modes
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


