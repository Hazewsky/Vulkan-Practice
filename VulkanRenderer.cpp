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
		createGraphicsPipeline();
		createFramebuffers();
		createCommandPool();

		//Set up Matrices
		uboViewProjection.projection = glm::perspective(glm::radians(45.0f), 
			(float) swapchainExtent.width / (float) swapchainExtent.height,
			0.1f, 100.0f);
		uboViewProjection.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

		uboViewProjection.projection[1][1] *= -1;
		//Create a mesh 
		//vertex data
		std::vector<Vertex> meshVertices =
		{
			{{-0.4, 0.4, 0.0}, {1.0f, 0.0f, 0.0f}},		// 0
			{{-0.4, -0.4, 0.0}, {0.0f, 1.0f, 0.0f}},		// 1
			{{0.4, -0.4, 0.0}, {0.0f, 0.0f, 1.0f}},		// 2
			{{0.4, 0.4, 0.0}, {0.57f, 0.35f, 0.85f}},	// 3
		};

		std::vector<Vertex> meshVertices2 =
		{
			{{-0.25, 0.6, 0.0}, {0.75f, 0.23f, 0.11f}},		// 0
			{{-0.25, -0.6, 0.0}, {1.0f, 0.2f, 0.5f}},		// 1
			{{0.25, -0.6, 0.0}, {0.0f, 1.0f, 1.0f}},		// 2
			{{0.25, 0.6, 0.0}, {0.37f, 0.2f, 0.93f}},	// 3
			
		};

		//index data
		std::vector<uint32_t> meshIndices = 
		{
			0, 1, 2,	//1st triangle
			2, 3, 0		//2nd triangle
		};

		meshes.push_back(Mesh(mainDevice.physicalDevice, mainDevice.logicalDevice,
			graphicsQueue, graphicsCommandPool, &meshVertices, &meshIndices));
		meshes.push_back(Mesh(mainDevice.physicalDevice, mainDevice.logicalDevice,
			graphicsQueue, graphicsCommandPool, &meshVertices2, &meshIndices));

		createCommandBuffers();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();

		recordCommands();
		createSynchronization();
		
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
	if (modelId >= meshes.size()) return;

	meshes[modelId].setModel(newModel);
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
	_aligned_free(modelTransferSpace);

	

	//don't need to free Descriptor Sets because they're be automatically released after Descriptor Pool destroy
	vkDestroyDescriptorPool(mainDevice.logicalDevice, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, descriptorSetLayout, nullptr);

	for (size_t i = 0; i < swapchainImages.size(); i++)
	{
		vkDestroyBuffer(mainDevice.logicalDevice, modelUniformBufferDynamic[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, modelUniformBufferMemoryDynamic[i], nullptr);

		vkDestroyBuffer(mainDevice.logicalDevice, vpUniformBuffer[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, vpUniformBufferMemory[i], nullptr);
	}

	for (auto & mesh : meshes)
	{
		mesh.cleanup();
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
	//Color attachment of render pass
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = swapchainImageFormat;						//Format to use for attachment
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;					//Number of samples to write for multisampling
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;				//Describes what to do with attachment before rendering
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;				//Describes what to do with attachment after rendering
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	//Describes what to do with stencil before rendering
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;	//Describes what to do with stencil after rendering

	//Framebuffer data will be stored as an image, but images can be given difference data layouts
	//to give optimal use for certain operations so define appropriate formats for different 
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;		//image data layout before render pass starts
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;	//src of presentation. Image data layout after render pass (to change to)

	// Attachment reference uses an attachment index  that refers to index in the attachment list passed to renderPassCreateInfo
	VkAttachmentReference colorAttachmentReference = {};
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	//Information about a particular subpass the Render Pass is using
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; //piptline type subpass is to be bound to
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentReference;

	//Need to determine when layout transitions occur using subpass dependencies
	std::array<VkSubpassDependency, 2> subpassDeps;
	//1. Conversion from VK_IMAGE_LAAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	//Must happen after the END of external subpass
	subpassDeps[0].srcSubpass = VK_SUBPASS_EXTERNAL; //Subpass index (VK_SUBPASS_EXTERNAL = Special value meaning outside of renderpass)
	subpassDeps[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; //Pipeline stage (VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT = end of pipeline)
	subpassDeps[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT; //Stage memory mask. (VK_ACCESS_MEMORY_READ_BIT = memory stage must happen before conversion to be able to read it)
	//BUT must happen before the color output of our main subpass (at 0)
	subpassDeps[0].dstSubpass = 0;
	subpassDeps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDeps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; //and before when can read\write from this thing
	subpassDeps[0].dependencyFlags = 0;

	//2. Conversion from VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	//Must happen after the END of external subpass
	subpassDeps[1].srcSubpass = 0;										//Subpass index (0 = main subpass)
	subpassDeps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; //Pipeline stage (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT = after draw operation)
	subpassDeps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;			//Stage memory mask. (VK_ACCESS_MEMORY_READ_BIT = memory stage must happen before conversion to be able to read it)
	//BUT must happen before the color output of our main subpass (at 0)
	subpassDeps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	subpassDeps[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDeps[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT; //and before when can read\write from this thing
	subpassDeps[1].dependencyFlags = 0;

	//Create Info for Render Pass
	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &colorAttachment;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
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
	//View-Projection Binding info
	VkDescriptorSetLayoutBinding viewProjectionLayoutBinding = {};
	viewProjectionLayoutBinding.binding = 0;											//Binding point in shader (designated by binding number in shader)
	viewProjectionLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;		//Type of descriptor(uniform, dynamic uniform, image samples)
	viewProjectionLayoutBinding.descriptorCount = 1;									//Number of descriptor for binding
	viewProjectionLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;				//Shader stage to bind to (vertex, geometry, fragment, etc)
	viewProjectionLayoutBinding.pImmutableSamplers = nullptr;							//For Texture: can make sampler data immutable by specifying in layout. Only the SAMPLER. 
																						//The ImageView it samples from can still be changed
	//Binding for Dynamic Uniform for Model Data
	VkDescriptorSetLayoutBinding modelLayoutBinding = {};
	modelLayoutBinding.binding = 1;
	modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	modelLayoutBinding.descriptorCount = 1;
	modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	modelLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 2> layoutBindings = {viewProjectionLayoutBinding, modelLayoutBinding};
	//Create descriptor set layout with given bindings
	VkDescriptorSetLayoutCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	createInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());	//Number of binding infos
	createInfo.pBindings = layoutBindings.data();							//Array of binding infos

	//Create descriptor set layout
	VkResult result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &createInfo, nullptr, &descriptorSetLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create descriptor set layout!");
	}
	else
	{
		printf("SUCCESS: Descriptor set layout created successfully!");
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
	std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions;
	//Position attribute
	attributeDescriptions[0].binding = 0;							//Which binding the data is at (should be same as above, unless you have multiple streams of data)
	attributeDescriptions[0].location = 0;							//Location in shader where data will be read from
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;	//Format the data will take (also helps define size of data)
	attributeDescriptions[0].offset = offsetof(Vertex, pos);		//Where this attribute is defined in the data for a single vertex
	//Color attribute
	attributeDescriptions[1].binding = 0;							
	attributeDescriptions[1].location = 1;							
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;		//VK_FORMAT_R8G8B8A8_SRGB
	attributeDescriptions[1].offset = offsetof(Vertex, col);		

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
	rasterizationCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;			// Which face of a triangle to cull
	rasterizationCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;// Winding to determine which size is front
	rasterizationCreateInfo.depthBiasEnable = VK_FALSE;					// Whether to add depth bias to fragments (good for stopping "shadow acne" in shadow mapping)

	// -- 7. MULTISAMPLING --
	VkPipelineMultisampleStateCreateInfo multisampleCreateInfo = {};
	multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleCreateInfo.sampleShadingEnable = VK_FALSE; //enable multisample shading
	multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; //number of samples you take from the surrounding area. This case - single sample per fragment

	// -- 8. COLOR BLENDING --
	//BLean Attachment State (how blending is handled)
	VkPipelineColorBlendAttachmentState colorState = {};
	colorState.colorWriteMask = 
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;		//set the mask to define color to be applied in blend
	colorState.blendEnable = VK_TRUE;	//enable blending
	//Blending uses equation: (srcColorBlendFactor * newColor) colorBlendOp (dstColorBlendFactor * oldColor)
	//blend for color
	colorState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; //mult newColor by Alpha value
	colorState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorState.colorBlendOp = VK_BLEND_OP_ADD;
	//Summarized: (VK_BLEND_FACTOR_SRC_ALPHA * new color) + (VK_BLEND_FACTOR_SRC_ONE_MINUS_ALPHA * old color);
	//			  (new color alpha * new color) + ((1 - new color alpha) * old color)
	//blend for alpha
	colorState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; //get rid of the old alpha and replace with a new one
	colorState.alphaBlendOp = VK_BLEND_OP_ADD;
	//Summarized: (1 * new alpha) + (0 * old alpha) = new alpha

	VkPipelineColorBlendStateCreateInfo colorBlendingCreateInfo = {};
	colorBlendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendingCreateInfo.logicOpEnable = VK_FALSE; //alternative to calculations is to use logical operations
	//colorBlendingCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendingCreateInfo.attachmentCount = 1;
	colorBlendingCreateInfo.pAttachments = &colorState; 
	//colorBlendingCreateInfo.blendConstants[0] = //blend constants of size 4

	// -- 9. PIPELINE LAYOUT --
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	VkResult result = vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Enable to create pipeline layout!");
	}
	// -- 10. DEPTH STENCIL TESTING
	// TODO: Set up depth stencil testing


	// -- GRAPHICS PIPELINE CREATION --
	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.stageCount = 2;									//Number of shader stages
	graphicsPipelineCreateInfo.pStages = shaderStages;							//Shader stages
	graphicsPipelineCreateInfo.layout = pipelineLayout;							//Pipeline layout the pipeline should use
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;		//All the fixed function pipeline states
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	graphicsPipelineCreateInfo.pViewportState = &viewportCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = /*&dynamicStateCreateInfo*/nullptr;
	graphicsPipelineCreateInfo.pTessellationState = nullptr;					//TODO: Implement tesselation state
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizationCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlendingCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = nullptr;					//TODO: Implement depth stencil state
	graphicsPipelineCreateInfo.renderPass = renderPass;							//which render pass will use that pipeline and pipeline will be compatible with
	graphicsPipelineCreateInfo.subpass = 0;										//which subpass to use with pipeline. It's one subpass per pipeline
	//Pipeline derivatives : Can create multiple pipelines that derive from one another for optimization
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;				//Existing pipeline to derive from...
	graphicsPipelineCreateInfo.basePipelineIndex = -1;							//or index of base pipeline other pipelines should be derived from (in case creating multiple at once)

	result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &graphicsPipeline);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Enable to create pipeline(-s)!");
	}

	//we don't need shader modules anymore after pipeline creation -> delete 'em
	vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);
	
}

void VulkanRenderer::createFramebuffers()
{
	//resize framebuffer count to == swapchain image count
	swapchainFramebuffers.resize(swapchainImages.size());
	//create for each swap chain image
	for (size_t i = 0; i < swapchainFramebuffers.size(); i++)
	{
		std::array<VkImageView, 1> attachments = { swapchainImages[i].imageView };

		VkFramebufferCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.renderPass = renderPass;										//Render pass layout the framebuffer will be used with
		createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		createInfo.pAttachments = attachments.data();							//List of attachments (1:1 with render pass)
		createInfo.width = swapchainExtent.width;								//Framebuffer width
		createInfo.height = swapchainExtent.height;								//Framebuffer height
		createInfo.layers = 1;													//Framebuffer layers

		VkResult result = vkCreateFramebuffer(mainDevice.logicalDevice, &createInfo, nullptr, &swapchainFramebuffers[i]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Enable to create framebuffer!");
		}
	}
}

void VulkanRenderer::createCommandPool()
{
	VkCommandPoolCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily; //Queue family type that buffers from this command will use
	
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

void VulkanRenderer::createUniformBuffers()
{
	// Buffer size will size of all view-projection variables (will offset to access)
	VkDeviceSize vpBufferSize = sizeof(UboViewProjection);
	// Model data buffer size
	VkDeviceSize modelBufferSize = modelUniformAlignment * MAX_OBJECTS;
	// One uniform buffer for each image (and by extension, command buffer)
	vpUniformBuffer.resize(swapchainImages.size());
	vpUniformBufferMemory.resize(swapchainImages.size());
	// One dynamic uniform buffer for each image that handles ALL model data of ALL objects located on the scene (but less then MAX_OBJECTS)
	modelUniformBufferDynamic.resize(swapchainImages.size());
	modelUniformBufferMemoryDynamic.resize(swapchainImages.size());

	// Create Uniform Buffers
	for (size_t i = 0; i < swapchainImages.size(); i++)
	{
		createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, vpBufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&vpUniformBuffer[i], &vpUniformBufferMemory[i]);

		createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, modelBufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&modelUniformBufferDynamic[i], &modelUniformBufferMemoryDynamic[i]);
	}
	
}

void VulkanRenderer::createDescriptorPool()
{
	//Type of descriptors and how many DESCRIPTORS, not discriptor sets (combined makes the pool size, a part of the pool)
	VkDescriptorPoolSize vpPoolSize = {};
	vpPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vpPoolSize.descriptorCount = static_cast<uint32_t>(vpUniformBuffer.size());

	//Model Pool (DYNAMIC)
	VkDescriptorPoolSize modelPoolSize = {};
	modelPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	modelPoolSize.descriptorCount = static_cast<uint32_t>(modelUniformBufferDynamic.size());

	//List of pool sizes
	std::array<VkDescriptorPoolSize, 2> poolSizes = {vpPoolSize, modelPoolSize};

	//Data to create Descriptor Pool
	VkDescriptorPoolCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	createInfo.maxSets = static_cast<uint32_t>(swapchainImages.size());			//Max number of Descriptor sSets that can be created from pool
	createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());			//Amount of Pool Sizes (parts) being passed
	createInfo.pPoolSizes = poolSizes.data();									//Pool Sizes (parts) to create pool with

	VkResult result = vkCreateDescriptorPool(mainDevice.logicalDevice, &createInfo, nullptr, &descriptorPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create descriptor pool!");
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

		//For model data
		VkDescriptorBufferInfo modelBufferInfo = {};
		modelBufferInfo.buffer = modelUniformBufferDynamic[i];
		modelBufferInfo.offset = 0;
		modelBufferInfo.range = modelUniformAlignment;

		// Data about connection between binding and buffer
		VkWriteDescriptorSet vpSetWrite = {};
		vpSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		vpSetWrite.dstSet = descriptorSets[i];							// Descriptor Set to update
		vpSetWrite.dstBinding = 0;										// Binding to update (matches with binding on layout/shader) 
		vpSetWrite.dstArrayElement = 0;									// Index in array to update
		vpSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;	// Type of descriptor
		vpSetWrite.descriptorCount = 1;									// Amount to update
		vpSetWrite.pBufferInfo = &vpBuffferInfo;						// Info about buffer data to bind

		//same for model data
		VkWriteDescriptorSet modelSetWrite = {};
		modelSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		modelSetWrite.dstSet = descriptorSets[i];
		modelSetWrite.dstBinding = 1;
		modelSetWrite.dstArrayElement = 0;
		modelSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		modelSetWrite.descriptorCount = 1;
		modelSetWrite.pBufferInfo = &modelBufferInfo;

		std::array<VkWriteDescriptorSet, 2> setWrites = { vpSetWrite, modelSetWrite };

		// Update the descriptor sets with new buffer/binding info
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
	// Copy Model Data
	for (size_t i = 0; i < meshes.size(); i++)
	{
		UboModel* thisModel = (UboModel*)((uint64_t)modelTransferSpace + (i * modelUniformAlignment));
		*thisModel = meshes[i].getModel();
	}
	// Map the lst of model data
	result = vkMapMemory(mainDevice.logicalDevice, modelUniformBufferMemoryDynamic[imageIndex], 0, modelUniformAlignment * meshes.size(), 0, &data);
	if (result == VK_SUCCESS)
	{
		memcpy(data, modelTransferSpace, sizeof(UboViewProjection));
		vkUnmapMemory(mainDevice.logicalDevice, modelUniformBufferMemoryDynamic[imageIndex]);
	}

}

void VulkanRenderer::recordCommands()
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
	VkClearValue clearValues[] = 
	{ 
		{0.6f, 0.65f, 0.4f, 1.0f}
	};
	renderBeginInfo.pClearValues = clearValues; //List of clear values (TODO: Depth Attachment Clear Value)
	renderBeginInfo.clearValueCount = 1;

	for (size_t i = 0; i < commandBuffers.size(); i++)
	{
		renderBeginInfo.framebuffer = swapchainFramebuffers[i];

		//start recording commands to command buffer
		VkResult result =  vkBeginCommandBuffer(commandBuffers[i], &bufferBeginInfo);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to start recording a Command Buufer");
		}
		
		//Begin render pass
		vkCmdBeginRenderPass(commandBuffers[i], &renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			//Bind pipeline to be used in render pass
			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

			//record drawing all meshes
			for (size_t j = 0; j < meshes.size(); j++)
			{
				// Use vertex buffer
				VkBuffer vertexBuffers[] = { meshes[j].getVertexBuffer() };							//Buffers to bind
				VkDeviceSize vertexOffsets[] = { 0 };												//Offsets into buffers being bound
				vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, vertexOffsets);		//Command to bind vertex buffer before drawing

				// Use index buffer
				VkBuffer indexBuffers[] = { meshes[j].getIndexBuffer() };
				VkDeviceSize indexOffsets[] = { 0 };
				vkCmdBindIndexBuffer(commandBuffers[i], meshes[j].getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

				//Dynamic offset amount
				uint32_t dynamicOffset = static_cast<uint32_t>(modelUniformAlignment) * j;
				// Bind Descriptor Sets
				// TODO: Implement Dynamic Offsets
				vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
					0, 1, &descriptorSets[i], 1, &dynamicOffset);

				// Execute Pipeline
				// Without index buffers
				// vkCmdDraw(commandBuffers[i], static_cast<uint32_t>(firstMesh.getVertexCount()), 1, 0, 0);
				// With index buffers
				vkCmdDrawIndexed(commandBuffers[i], meshes[j].getIndexCount(), 1, 0, 0, 0);
			}
			

		//End render pass
		vkCmdEndRenderPass(commandBuffers[i]);
		//stop recording to command buffer
		result = vkEndCommandBuffer(commandBuffers[i]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to stop recording a Command Buufer");
		}
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
	
	//Get properties of our device

	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(mainDevice.physicalDevice, &deviceProperties);
	
	minUniformBufferOffset = deviceProperties.limits.minUniformBufferOffsetAlignment;
	//calculate our actual offset for modelUniform
	allocateDynamicBufferTransferSpace();
}

void VulkanRenderer::allocateDynamicBufferTransferSpace()
{
	//Calculate alignment of model data
	modelUniformAlignment = (sizeof(UboModel) + minUniformBufferOffset - 1) & ~(minUniformBufferOffset - 1);

	// Create Space in memory to hold dynamic buffer that is aligned to our required alignment and holds MAX_OBJECTS
	modelTransferSpace = (UboModel*)_aligned_malloc(modelUniformAlignment * MAX_OBJECTS, modelUniformAlignment);
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
	//Enable Rasterization Depth clamp
	//deviceFeatures.depthClamp = VK_TRUE;

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


