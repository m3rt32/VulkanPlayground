#include "LavaRenderer.h"


#define VK_CHECK(call) \
		do{ \
			VkResult result = call; \
			assert(result==VK_SUCCESS); \
		} while(0)

#define LAVA_ASSERT(x) assert(x)
#define LAVA_PRINT(s) std::cout<<s<<std::endl

LavaRenderer::LavaRenderer()
{
	int windowInit = glfwInit();
	assert(windowInit);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	window = glfwCreateWindow(1024, 768, "Lava", 0, 0);
	assert(window);

	int s_width = 0, s_height = 0;
	glfwGetWindowSize(window, &s_width, &s_height);
	frameBufferWidth = s_width;
	frameBufferHeight = s_height;

	InitVulkan();

	VkCommandBufferAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = commandPool;
	allocateInfo.commandBufferCount = 1;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	VkCommandBuffer commandBuffer;
	LAVA_ASSERT(vkAllocateCommandBuffers(activeDevice, &allocateInfo, &commandBuffer) == VK_SUCCESS);

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		uint32_t imageIndex = 0;
		LAVA_ASSERT(vkAcquireNextImageKHR(activeDevice, swapChain, UINT64_MAX, acquireSemaphore, nullptr, &imageIndex) == VK_SUCCESS);

		LAVA_ASSERT(vkResetCommandPool(activeDevice, commandPool, 0) == VK_SUCCESS);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		LAVA_ASSERT(vkBeginCommandBuffer(commandBuffer, &beginInfo)==VK_SUCCESS);

		VkClearValue color = { .3f,0,0,1 };

		//Dont clear the screen with a command, clear it via renderpass. No need for additional operation.
		VkRenderPassBeginInfo beginPassInfo = {};
		beginPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		beginPassInfo.renderPass = renderPass;
		beginPassInfo.framebuffer = frameBuffers[imageIndex];
		beginPassInfo.renderArea.extent.width = frameBufferWidth; //Useful for tiled rendering, specifying helps to performance for them
		beginPassInfo.renderArea.extent.height = frameBufferHeight;
		beginPassInfo.pClearValues = &color;
		beginPassInfo.clearValueCount = 1;

		vkCmdBeginRenderPass(commandBuffer, &beginPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewPort = {};
		viewPort.width = float(frameBufferWidth);
		viewPort.height = float(frameBufferHeight);
		viewPort.x = 0.f;
		viewPort.y = 0.f;
		viewPort.minDepth = 0.f;
		viewPort.maxDepth = 1.f;

		VkRect2D scissors = {};
		scissors.extent.width = frameBufferWidth;
		scissors.extent.height = frameBufferHeight;
		scissors.offset.x = 0;
		scissors.offset.y = 0;

		vkCmdSetViewport(commandBuffer, 0, 1, &viewPort);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissors);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);
		vkCmdEndRenderPass(commandBuffer);

		vkEndCommandBuffer(commandBuffer);

		VkPipelineStageFlags submitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &acquireSemaphore;
		submitInfo.pWaitDstStageMask = &submitStageMask;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &releaseSemaphore;

		LAVA_ASSERT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) == VK_SUCCESS);

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapChain;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &releaseSemaphore;

		LAVA_ASSERT(vkQueuePresentKHR(queue, &presentInfo) == VK_SUCCESS);

		LAVA_ASSERT(vkDeviceWaitIdle(activeDevice) == VK_SUCCESS);
	}

	glfwDestroyWindow(window);
	DestroyVulkan();
}

void LavaRenderer::InitVulkan()
{
	CreateInstance();
	CreateSurface();
	CreateDevice();
	GetSwapchainSupportData();
	CreateSwapchain();
	CreateSemaphore();
	CreateQueue();
	CreateRenderPass();
	CreateGraphicsPipeline();
	CreateCommandPool();
}

void LavaRenderer::DestroyVulkan()
{
	vkDestroySurfaceKHR(instance, surface, 0);
	vkDestroyDevice(activeDevice, 0);
	vkDestroyInstance(instance, 0);
}

VkPhysicalDevice LavaRenderer::PickPhysicalDevice(VkPhysicalDevice* devices, uint32_t deviceCount)
{
	for (uint32_t i = 0; i < deviceCount; i++) {
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(devices[i], &props);

		if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			LAVA_PRINT("Discrete gpu " << props.deviceName << " selected");
			return devices[i];
		}
	}


	if (deviceCount > 0) {
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(devices[0], &props);
		LAVA_PRINT("Fallback gpu " << props.deviceName << " selected");
	}

	return devices[0];
}

void LavaRenderer::CreateInstance()
{
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.apiVersion = VK_API_VERSION_1_2;

	const char* validationLayers[] = {
		"VK_LAYER_LUNARG_standard_validation"
	};

	const char* extensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.ppEnabledLayerNames = validationLayers;
	createInfo.enabledLayerCount = 1;
	createInfo.ppEnabledExtensionNames = glfwExtensions;
	createInfo.enabledExtensionCount = glfwExtensionCount;

	instance = 0;
	LAVA_ASSERT(vkCreateInstance(&createInfo, 0, &instance) == VK_SUCCESS);
}

void LavaRenderer::CreateDevice()
{
	VkPhysicalDevice gpus[16];

	uint32_t deviceCount = sizeof(gpus) / sizeof(gpus[0]);
	LAVA_ASSERT(vkEnumeratePhysicalDevices(instance, &deviceCount, &(gpus[0])) == VK_SUCCESS);
	activePhysicalDevice = PickPhysicalDevice(gpus, deviceCount);
	LAVA_ASSERT(activePhysicalDevice);

	float queuePriorities = 1.;
	VkDeviceQueueCreateInfo queueInfo = {};
	queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.queueCount = 1;
	queueInfo.pQueuePriorities = &queuePriorities;
	queueInfo.queueFamilyIndex = queueFamilyIndex;

	const char* deviceExtensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &queueInfo;
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;
	deviceCreateInfo.enabledExtensionCount = sizeof(deviceExtensions) / sizeof(deviceExtensions[0]);

	LAVA_ASSERT(vkCreateDevice(activePhysicalDevice, &deviceCreateInfo, nullptr, &activeDevice) == VK_SUCCESS);


}

void LavaRenderer::CreateSurface()
{
	LAVA_ASSERT(glfwCreateWindowSurface(instance, window, nullptr, &surface) == VK_SUCCESS);
}

void LavaRenderer::CreateSwapchain()
{
	VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = surface;
	swapChainCreateInfo.minImageCount = 2;
	swapChainCreateInfo.imageFormat = swapChainData.format;
	swapChainCreateInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	swapChainCreateInfo.imageExtent.width = frameBufferWidth;
	swapChainCreateInfo.imageExtent.height = frameBufferHeight;
	swapChainCreateInfo.imageArrayLayers = 1;//Unless Stereo rendering
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapChainCreateInfo.queueFamilyIndexCount = 1;
	swapChainCreateInfo.pQueueFamilyIndices = &queueFamilyIndex;
	swapChainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; //swapChainData.presentMode;
	swapChainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	LAVA_ASSERT(vkCreateSwapchainKHR(activeDevice, &swapChainCreateInfo, 0, &swapChain) == VK_SUCCESS);
}

void LavaRenderer::CreateSemaphore()
{
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	LAVA_ASSERT(vkCreateSemaphore(activeDevice, &semaphoreCreateInfo, nullptr, &acquireSemaphore) == VK_SUCCESS);

	LAVA_ASSERT(vkCreateSemaphore(activeDevice, &semaphoreCreateInfo, nullptr, &releaseSemaphore)==VK_SUCCESS);
}

void LavaRenderer::CreateQueue()
{
	vkGetDeviceQueue(activeDevice, queueFamilyIndex, 0, &queue);
}

void LavaRenderer::CreateCommandPool()
{
	uint32_t swapChainImageCount = 0;
	LAVA_ASSERT(vkGetSwapchainImagesKHR(activeDevice, swapChain, &swapChainImageCount, 0) == VK_SUCCESS);
	swapChainImages = new VkImage[swapChainImageCount];

	LAVA_ASSERT(vkGetSwapchainImagesKHR(activeDevice, swapChain, &swapChainImageCount, swapChainImages) == VK_SUCCESS);
	//LAVA_ASSERT(vkGetSwapchainImagesKHR(activeDevice, swapChain, &swapChainImageCount, 0) == VK_SUCCESS);

	for (uint32_t i = 0; i < swapChainImageCount; i++) {
		swapChainImageViews[i] = CreateImageView(swapChainImages[i]);
	}

	for (uint32_t i = 0; i < swapChainImageCount; i++) {
		frameBuffers[i] = CreateFrameBuffer(swapChainImageViews[i]);
	}

	VkCommandPoolCreateInfo commandPoolCreateInfo = {};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;

	LAVA_ASSERT(vkCreateCommandPool(activeDevice, &commandPoolCreateInfo, nullptr, &commandPool)==VK_SUCCESS);
}

void LavaRenderer::CreateRenderPass()
{
	//Create attachment
	VkAttachmentDescription attachments[1] = {};

	attachments[0].format = swapChainData.format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;

	//These allow us to clear the screen and render in a single pass.
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachments;
	colorAttachments.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachments.attachment = 0; //attachment index at the top(which is 0)

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.pColorAttachments = &colorAttachments;
	subpass.colorAttachmentCount = 1;


	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = sizeof(attachments) / sizeof(attachments[0]);
	renderPassCreateInfo.pAttachments = attachments;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;

	LAVA_ASSERT(vkCreateRenderPass(activeDevice, &renderPassCreateInfo, nullptr, &renderPass)==VK_SUCCESS);
}

void LavaRenderer::CreateGraphicsPipeline()
{
	vertShader = LoadShader("shaders/vert.spv");
	fragShader = LoadShader("shaders/frag.spv");
	VkPipelineCache pipelineCache = 0;//critical for performance, fill later, don't leave zero inited

	VkPipelineShaderStageCreateInfo stages[2] = {};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vertShader;
	stages[0].pName = "main";

	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = fragShader;
	stages[1].pName = "main";

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {}; //Probably fill this when switch to VBOs
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
	inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkViewport viewPort = {};
	viewPort.width = frameBufferWidth;
	viewPort.height = frameBufferHeight;
	viewPort.x = 0.f;
	viewPort.y = 0.f;
	viewPort.minDepth = 0.f;
	viewPort.maxDepth = 1.f;

	VkRect2D scissors = {};
	scissors.extent.width = frameBufferWidth;
	scissors.extent.height = frameBufferHeight;
	scissors.offset.x = 0;
	scissors.offset.y = 0;

	VkPipelineViewportStateCreateInfo viewPortCreateInfo = {};
	viewPortCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	//viewPortCreateInfo.pViewports = &viewPort;
	viewPortCreateInfo.viewportCount = 1;
	//viewPortCreateInfo.pScissors = &scissors;
	viewPortCreateInfo.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterCreateInfo = {};
	rasterCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterCreateInfo.lineWidth = 1.f;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo stencilCreateInfo = {};
	stencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

	VkPipelineColorBlendAttachmentState colorAttachments = {};
	colorAttachments.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorAttachments.blendEnable = VK_FALSE;
	colorAttachments.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorAttachments.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorAttachments.colorBlendOp = VK_BLEND_OP_ADD;
	colorAttachments.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorAttachments.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorAttachments.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo = {};
	colorBlendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendCreateInfo.pAttachments = &colorAttachments;
	colorBlendCreateInfo.attachmentCount = 1;
	colorBlendCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendCreateInfo.blendConstants[0] = 0.f;
	colorBlendCreateInfo.blendConstants[1] = 0.f;
	colorBlendCreateInfo.blendConstants[2] = 0.f;
	colorBlendCreateInfo.blendConstants[3] = 0.f;

	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR };

	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {}; //For scalable viewport
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pDynamicStates = dynamicStates;
	dynamicStateCreateInfo.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);

	//Define input resources, like textures etc. Thats where you specify it.
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	LAVA_ASSERT(vkCreatePipelineLayout(activeDevice, &pipelineLayoutCreateInfo, nullptr, &trianglePipelineLayout) == VK_SUCCESS);

	VkGraphicsPipelineCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	createInfo.stageCount = 2;
	createInfo.pStages = stages;
	createInfo.pVertexInputState = &vertexInputStateCreateInfo;
	createInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	createInfo.pRasterizationState = &rasterCreateInfo;
	createInfo.pViewportState = &viewPortCreateInfo;
	createInfo.pMultisampleState = &multisampleStateCreateInfo;
	createInfo.pDepthStencilState = &stencilCreateInfo;
	createInfo.pColorBlendState = &colorBlendCreateInfo;
	createInfo.pDynamicState = &dynamicStateCreateInfo;
	createInfo.renderPass = renderPass;
	createInfo.layout = trianglePipelineLayout;

	LAVA_ASSERT(vkCreateGraphicsPipelines(activeDevice, pipelineCache, 1, &createInfo, nullptr, &trianglePipeline) == VK_SUCCESS);
}

VkFramebuffer LavaRenderer::CreateFrameBuffer(VkImageView imageView)
{
	VkFramebuffer frameBuffer = 0;
	VkFramebufferCreateInfo frameBufferCreateInfo = {};
	frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frameBufferCreateInfo.renderPass = renderPass;
	frameBufferCreateInfo.attachmentCount = 1;
	frameBufferCreateInfo.pAttachments = &imageView;
	frameBufferCreateInfo.width = frameBufferWidth;
	frameBufferCreateInfo.height = frameBufferHeight;
	frameBufferCreateInfo.layers = 1;

	LAVA_ASSERT(vkCreateFramebuffer(activeDevice, &frameBufferCreateInfo, nullptr, &frameBuffer) == VK_SUCCESS);
	return frameBuffer;
}

VkImageView LavaRenderer::CreateImageView(VkImage image)
{
	VkImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.image = image;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format = swapChainData.format;
	imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.layerCount = 1;

	VkImageView imageView = 0;
	LAVA_ASSERT(vkCreateImageView(activeDevice, &imageViewCreateInfo, nullptr, &imageView) == VK_SUCCESS);

	return imageView;
}

void LavaRenderer::GetSwapchainSupportData()
{
	swapChainData = {};
	VkSurfaceFormatKHR surfaceFormat[16];
	uint32_t surfaceFormatCount = sizeof(surfaceFormat) / sizeof(surfaceFormat[0]);
	LAVA_ASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(activePhysicalDevice, surface, &surfaceFormatCount, surfaceFormat) ==VK_SUCCESS);
	swapChainData.format = surfaceFormat->format;

	VkPresentModeKHR presentMode[16];
	uint32_t presentModeCount = sizeof(presentMode) / sizeof(presentMode[0]);
	LAVA_ASSERT(vkGetPhysicalDeviceSurfacePresentModesKHR(activePhysicalDevice, surface, &presentModeCount, presentMode) == VK_SUCCESS);
	
	swapChainData.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	for(VkPresentModeKHR present : presentMode) {
		if (present == VK_PRESENT_MODE_MAILBOX_KHR)
			swapChainData.presentMode = present;
	}
}

VkShaderModule LavaRenderer::LoadShader(const char* path)
{
	FILE* file = fopen(path, "rb");
	fseek(file, 0, SEEK_END);
	long length = ftell(file);
	fseek(file, 0, SEEK_SET);

	char* buffer = new char[length];
	size_t file_length = fread(buffer, 1, length, file);
	LAVA_ASSERT(file_length == size_t(length));
	fclose(file);

	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = length;
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(buffer);

	VkShaderModule shaderModule = 0;
	LAVA_ASSERT(vkCreateShaderModule(activeDevice, &shaderModuleCreateInfo, nullptr, &shaderModule) == VK_SUCCESS);

	return shaderModule;
}
