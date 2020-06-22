#include "Application.h"
static std::vector<char> readFile(const std::string& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	if (!file.is_open())
		std::cout << "Error opening file '" << filename << "'";
	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();
	return buffer;
}

void Application::Run()
{
	InitWindow();
	InitVulkan();
	Loop();
	CleanUp();
}

void Application::InitWindow()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	m_window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan App", nullptr, nullptr);
}

void Application::InitVulkan()
{
	CreateVulkanInstance();
	CreateSurface(); //Necessary to be created before physical device chk for swapchain checks
	SelectPhysicalDevice();
	CreateLogicalDevice();
	CreateSwapChain();
	CreateImageView();
	CreateRenderPass();
	CreateGraphicsPipeline();
	CreateFrameBuffer();
	CreateCommandPool();
	CreateCommandBuffers();
	CreateSyncronizers();
}

void Application::Loop()
{
	while (!glfwWindowShouldClose(m_window)) {
		glfwPollEvents();
		DrawFrame();
	}

	vkDeviceWaitIdle(m_VkLogicalDevice);
}

void Application::CleanUp()
{
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(m_VkLogicalDevice, imageAvailableSemaphore[i], nullptr);
		vkDestroySemaphore(m_VkLogicalDevice, renderFinishedSemaphore[i], nullptr);
		vkDestroyFence(m_VkLogicalDevice, inFlightFences[i], nullptr);
	}
	vkDestroyCommandPool(m_VkLogicalDevice, m_VkCommandPool, nullptr);

	for (auto frameBuffer : swapChainFrameBuffers)
	{
		vkDestroyFramebuffer(m_VkLogicalDevice, frameBuffer, nullptr);
	}
	vkDestroyPipeline(m_VkLogicalDevice, m_VkPipeline, nullptr);
	vkDestroyPipelineLayout(m_VkLogicalDevice, m_VkPipelineLayout, nullptr);
	vkDestroyRenderPass(m_VkLogicalDevice, m_VkRenderPass, nullptr);
	for (auto imageView : swapChainImageViews)
	{
		vkDestroyImageView(m_VkLogicalDevice, imageView, nullptr);
	}

	vkDestroySwapchainKHR(m_VkLogicalDevice, m_VkSwapChain, nullptr);
	vkDestroySurfaceKHR(m_Vkinstance, m_surface, nullptr);

	vkDestroyDevice(m_VkLogicalDevice, nullptr);

	//Destory other vulkan stuff before this!
	vkDestroyInstance(m_Vkinstance, nullptr);

	glfwDestroyWindow(m_window);
	glfwTerminate();
}

void Application::CreateVulkanInstance()
{
	//App info is optional
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Lava App";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "Lava Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_2;

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	uint32_t supportedExtensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionCount, nullptr);
	std::vector<VkExtensionProperties> extensionProperties(supportedExtensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionCount, extensionProperties.data());

#pragma region Optional-Compare supported and required extensions
	bool allRequiredExtensionsSupported = true;
	for (int i = 0; i < glfwExtensionCount; i++) {
		bool currentExtensionExist = false;
		for (int j = 0; j < extensionProperties.size(); j++) {
			if (strcmp(extensionProperties[j].extensionName, glfwExtensions[i]) == 0) {
				currentExtensionExist = true;
				break;
			}
		}
		if (!currentExtensionExist)
		{
			allRequiredExtensionsSupported = false;
			break;
		}
	}

	if (!allRequiredExtensionsSupported)
		std::cout << "Required Extensions are not supported by this machine." << std::endl;
#pragma endregion

#pragma region Check validation layers
	std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	if (!CheckValidationLayers(validationLayers))
		std::cout << "Validation Layers not supported on current platform";
#pragma endregion


	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.ppEnabledExtensionNames = glfwExtensions;
	createInfo.enabledExtensionCount = glfwExtensionCount;
	createInfo.enabledLayerCount = validationLayers.size();
	createInfo.ppEnabledLayerNames = validationLayers.data();

	if (vkCreateInstance(&createInfo, nullptr, &m_Vkinstance) != VK_SUCCESS) {
		std::cout << "Error creating vulkan instance!" << std::endl;
	}
}

void Application::CreateSurface()
{
	if (glfwCreateWindowSurface(m_Vkinstance, m_window, nullptr, &m_surface) != VK_SUCCESS) {
		std::cout << "Failed to display surfaces" << std::endl;
	}


}

void Application::SelectPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_Vkinstance, &deviceCount, nullptr);
	if (deviceCount == 0)
		std::cout << "There system does not have any Vulkan supported GPU" << std::endl;

	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(m_Vkinstance, &deviceCount, physicalDevices.data());

	for (auto& device : physicalDevices) {
		if (IsDeviceSuitable(device)) {
			m_VkPhysicalDevice = device;
			break;
		}
	}

	if (m_VkPhysicalDevice == VK_NULL_HANDLE)
		std::cout << "None of the GPUs are suitable for Vulkan." << std::endl;
}

void Application::CreateLogicalDevice()
{
	QueueFamilyIndices indices = FindQueueFamilyIndices(m_VkPhysicalDevice);


	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	//std::set only keeps unique keys. No duplication,if those families are in the same queuefamily, there will be
	//just a single value.
	std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(),indices.presentationFamily.value() };

	float queuePriority = 1.0f;
	for (auto queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo); //Only adds unique create infos this way.
	}

	VkPhysicalDeviceFeatures deviceFeatures = {};

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

	if (vkCreateDevice(m_VkPhysicalDevice, &deviceCreateInfo, nullptr, &m_VkLogicalDevice) != VK_SUCCESS) {
		std::cout << "Logical device creation failed!" << std::endl;
	}

	vkGetDeviceQueue(m_VkLogicalDevice, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
	vkGetDeviceQueue(m_VkLogicalDevice, indices.presentationFamily.value(), 0, &m_presentationQueue);
}

void Application::CreateSwapChain()
{
	auto swapChainSupportDetails = QuerySwapChainSupport(m_VkPhysicalDevice);
	VkSurfaceFormatKHR surfaceFormat = PickSwapSurfaceFormat(swapChainSupportDetails.formats);
	VkPresentModeKHR presentationMode = PickSwapPresentMode(swapChainSupportDetails.presentModes);
	VkExtent2D swapExtent = PickSwapExtent(swapChainSupportDetails.capabilities);

	uint32_t swapChainImageCount = swapChainSupportDetails.capabilities.minImageCount + 1;
	if (swapChainSupportDetails.capabilities.maxImageCount > 0 && swapChainImageCount > swapChainSupportDetails.capabilities.maxImageCount) {
		swapChainImageCount = swapChainSupportDetails.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_surface;
	createInfo.minImageCount = swapChainImageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = swapExtent;
	createInfo.imageArrayLayers = 1;//Unless Stereo rendering
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	auto indices = FindQueueFamilyIndices(m_VkPhysicalDevice);
	uint32_t queueFamiltyIndices[] = { indices.graphicsFamily.value(),indices.presentationFamily.value() };
	if (indices.graphicsFamily != indices.presentationFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamiltyIndices;
	}
	else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
	}
	createInfo.preTransform = swapChainSupportDetails.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; //TO-DO: Change this to see what happens
	createInfo.presentMode = presentationMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(m_VkLogicalDevice, &createInfo, nullptr, &m_VkSwapChain) != VK_SUCCESS) {
		std::cout << "Failed to create swapchain" << std::endl;
	}

	vkGetSwapchainImagesKHR(m_VkLogicalDevice, m_VkSwapChain, &swapChainImageCount, nullptr);
	swapChainImages.resize(swapChainImageCount);
	vkGetSwapchainImagesKHR(m_VkLogicalDevice, m_VkSwapChain, &swapChainImageCount, swapChainImages.data());
	m_VkFormat = surfaceFormat.format;
	m_VkExtent = swapExtent;
}

void Application::CreateImageView()
{
	swapChainImageViews.resize(swapChainImages.size());
	for (int i = 0; i < swapChainImageViews.size(); ++i)
	{
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapChainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = m_VkFormat;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;
		if (vkCreateImageView(m_VkLogicalDevice, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS)
		{
			std::cout << "Error creating image views" << std::endl;
		}
	}
}

void Application::CreateRenderPass()
{
	//RenderPass
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = m_VkFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	//Subpass that will be attached to renderpass
	VkAttachmentReference attachmentReference = {};
	attachmentReference.attachment = 0;//index of attachmentDescription above.
	attachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDescription = {};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &attachmentReference;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL; //Last executed subpass.
	dependency.dstSubpass = 0; //Our subpass
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &colorAttachment;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpassDescription;
	renderPassCreateInfo.dependencyCount = 1;
	renderPassCreateInfo.pDependencies = &dependency;


	if (vkCreateRenderPass(m_VkLogicalDevice, &renderPassCreateInfo, nullptr, &m_VkRenderPass) != VK_SUCCESS)
	{
		std::cout << "ERROR CREATING RENDER PASS" << std::endl;
	}
}

void Application::CreateGraphicsPipeline()
{
	auto vertShaderCode = readFile("shaders/vert.spv");
	std::cout << "VERT SHADER SIZE:" << vertShaderCode.size() << std::endl;
	auto fragShaderCode = readFile("shaders/frag.spv");
	std::cout << "FRAG SHADER SIZE:" << fragShaderCode.size() << std::endl;

	vertexShaderModule = CreateShaderModule(vertShaderCode);
	fragShaderModule = CreateShaderModule(fragShaderCode);

	VkPipelineShaderStageCreateInfo vertStageCreateInfo = {};
	vertStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertStageCreateInfo.module = vertexShaderModule;
	vertStageCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragStageCreateInfo = {};
	fragStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragStageCreateInfo.module = fragShaderModule;
	fragStageCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[]{ vertStageCreateInfo,fragStageCreateInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
	inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.f;
	viewport.y = 0.f;
	viewport.width = (float)m_VkExtent.width;
	viewport.height = (float)m_VkExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissorRect = {};
	scissorRect.offset = { 0,0 };
	scissorRect.extent = m_VkExtent;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissorRect;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;

	VkPipelineRasterizationStateCreateInfo rasterizationInfo = {};
	rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationInfo.depthBiasClamp = VK_FALSE;
	rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationInfo.lineWidth = 1.0f;
	rasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizationInfo.depthBiasEnable = VK_FALSE;
	rasterizationInfo.depthBiasConstantFactor = 0.f;
	rasterizationInfo.depthBiasClamp = 0.f;
	rasterizationInfo.depthBiasSlopeFactor = 0.f;

	VkPipelineMultisampleStateCreateInfo multisampleInfo = {};
	multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleInfo.sampleShadingEnable = VK_FALSE;
	multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleInfo.minSampleShading = 1.0f;
	multisampleInfo.pSampleMask = nullptr;
	multisampleInfo.alphaToCoverageEnable = VK_FALSE;
	multisampleInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachmentInfo = {};
	colorBlendAttachmentInfo.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachmentInfo.blendEnable = VK_FALSE;
	colorBlendAttachmentInfo.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentInfo.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachmentInfo.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentInfo.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentInfo.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachmentInfo.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlendInfo = {};
	colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendInfo.logicOpEnable = VK_FALSE;
	colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendInfo.attachmentCount = 1;
	colorBlendInfo.pAttachments = &colorBlendAttachmentInfo;
	colorBlendInfo.blendConstants[0] = 0.f;
	colorBlendInfo.blendConstants[1] = 0.f;
	colorBlendInfo.blendConstants[2] = 0.f;
	colorBlendInfo.blendConstants[3] = 0.f;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0;
	pipelineLayoutInfo.pSetLayouts = nullptr;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(m_VkLogicalDevice, &pipelineLayoutInfo, nullptr, &m_VkPipelineLayout) != VK_SUCCESS)
	{
		std::cout << "ERROR CREATING PIPELINE LAYOUT" << std::endl;
	}

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2;
	pipelineCreateInfo.pStages = shaderStages;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pRasterizationState = &rasterizationInfo;
	pipelineCreateInfo.pMultisampleState = &multisampleInfo;
	pipelineCreateInfo.pDepthStencilState = nullptr;
	pipelineCreateInfo.pColorBlendState = &colorBlendInfo;
	pipelineCreateInfo.pDynamicState = nullptr;
	pipelineCreateInfo.layout = m_VkPipelineLayout;
	pipelineCreateInfo.renderPass = m_VkRenderPass;
	pipelineCreateInfo.subpass = 0; //index of subpass, not count!
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineCreateInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(m_VkLogicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_VkPipeline) != VK_SUCCESS)
	{
		std::cout << "FAILED TO CREATE GRAPHICS PIPELINE" << std::endl;
	}

	vkDestroyShaderModule(m_VkLogicalDevice, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_VkLogicalDevice, fragShaderModule, nullptr);
}

void Application::CreateFrameBuffer()
{
	swapChainFrameBuffers.resize(swapChainImageViews.size());
	for (size_t i = 0; i < swapChainImageViews.size(); i++)
	{
		VkImageView attachments[] = {
			swapChainImageViews[i]
		};

		VkFramebufferCreateInfo frameBufferInfo = {};
		frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferInfo.renderPass = m_VkRenderPass;
		frameBufferInfo.attachmentCount = 1;
		frameBufferInfo.pAttachments = attachments;
		frameBufferInfo.width = m_VkExtent.width;
		frameBufferInfo.height = m_VkExtent.height;
		frameBufferInfo.layers = 1;
		if (vkCreateFramebuffer(m_VkLogicalDevice, &frameBufferInfo, nullptr, &swapChainFrameBuffers[i]) != VK_SUCCESS)
		{
			std::cout << "ERROR CREATING FRAME BUFFER!" << std::endl;
		}
	}
}

void Application::CreateCommandPool()
{
	QueueFamilyIndices queueFamilyIndices = FindQueueFamilyIndices(m_VkPhysicalDevice);

	VkCommandPoolCreateInfo commandPoolCreateInfo = {};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
	commandPoolCreateInfo.flags = 0;

	if (vkCreateCommandPool(m_VkLogicalDevice, &commandPoolCreateInfo, nullptr, &m_VkCommandPool) != VK_SUCCESS)
	{
		std::cout << "ERROR CREATING COMMAND POOL!" << std::endl;
	}
}

void Application::CreateCommandBuffers()
{
	commandBuffers.resize(swapChainFrameBuffers.size());
	VkCommandBufferAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = m_VkCommandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = (uint32_t)commandBuffers.size();

	if (vkAllocateCommandBuffers(m_VkLogicalDevice, &allocateInfo, commandBuffers.data()) != VK_SUCCESS)
	{
		std::cout << "FAILED ALLOCATION OF COMMAND BUFFERS" << std::endl;
	}

	for (size_t i = 0; i < commandBuffers.size(); i++)
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.pInheritanceInfo = nullptr; //Those two for multiple command buffers
		beginInfo.flags = 0; //Those two for multiple command buffers

		if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS)
		{
			std::cout << "FAILED TO BEGIN COMMAND BUFFER" << std::endl;
		}

		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = m_VkRenderPass;
		renderPassBeginInfo.framebuffer = swapChainFrameBuffers[i];
		renderPassBeginInfo.renderArea.offset = { 0,0 };
		renderPassBeginInfo.renderArea.extent = m_VkExtent;

		VkClearValue clearColor = { 0.f,0.f,0.f,1.f };
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = &clearColor;
		vkCmdBeginRenderPass(commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_VkPipeline);
		vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);
		vkCmdEndRenderPass(commandBuffers[i]);

		if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS)
		{
			std::cout << "FAILED TO RECORD COMMAND BUFFER" << std::endl;
		}
	}
}

void Application::CreateSyncronizers()
{
	imageAvailableSemaphore.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphore.resize(MAX_FRAMES_IN_FLIGHT);
	inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
	imagesFlightFences.resize(swapChainImages.size(),VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; //To start the first drawframe loop, to avoid 4ever waiting.

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (vkCreateSemaphore(m_VkLogicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphore[i]) != VK_SUCCESS ||
			vkCreateSemaphore(m_VkLogicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphore[i]) != VK_SUCCESS ||
			vkCreateFence(m_VkLogicalDevice, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
		{
			std::cout << "ERROR CREATING SYNC POINTS FOR FRAME " << i << std::endl;
		}
	}
}

bool Application::CheckValidationLayers(const std::vector<const char*>& validationLayers)
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr); //If nullptr, we just gather the length
	//Then we pass it again to populate our collection with the second parameter as the collection:
	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (auto& validationLayer : validationLayers) {
		bool layerExists = false;
		for (auto& availableLayer : availableLayers) {
			if (strcmp(availableLayer.layerName, validationLayer) == 0)
			{
				layerExists = true;
				break;
			}
		}
		if (!layerExists)
			return false;
	}
	return true;
}

bool Application::IsDeviceSuitable(VkPhysicalDevice targetDevice)
{
	auto familyIndices = FindQueueFamilyIndices(targetDevice);
	bool extensionSupported = CheckExtensionSupport(targetDevice);
	bool swapChainSupportAcquired = false;
	if (extensionSupported) {
		auto supportDetails = QuerySwapChainSupport(targetDevice);
		swapChainSupportAcquired = !supportDetails.formats.empty() && !supportDetails.presentModes.empty();
	}

	return familyIndices.IsComplete() && extensionSupported && swapChainSupportAcquired;
}

QueueFamilyIndices Application::FindQueueFamilyIndices(VkPhysicalDevice targetDevice)
{
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(targetDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(targetDevice, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const auto& queueFamily : queueFamilies) {
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = i;
		}
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(targetDevice, i, m_surface, &presentSupport);
		if (presentSupport)
		{
			indices.presentationFamily = i;
		}

		if (indices.IsComplete())
			break;

		i++;
	}

	return indices;
}

SwapChainSupportDetails Application::QuerySwapChainSupport(VkPhysicalDevice targetDevice)
{
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(targetDevice, m_surface, &details.capabilities);
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(targetDevice, m_surface, &formatCount, nullptr);
	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(targetDevice, m_surface, &formatCount, details.formats.data());
	}
	uint32_t presentationModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(targetDevice, m_surface, &presentationModeCount, nullptr);
	if (presentationModeCount != 0)
	{
		details.presentModes.resize(presentationModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(targetDevice, m_surface, &presentationModeCount,
			details.presentModes.data());
	}
	return details;
}

bool Application::CheckExtensionSupport(VkPhysicalDevice targetDevice)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(targetDevice, nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(targetDevice, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
	for (const auto& extension : availableExtensions) {
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

VkSurfaceFormatKHR Application::PickSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
	for (const auto& format : formats) {
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return format;
		}
	}
	return formats[0];
}

VkPresentModeKHR Application::PickSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes)
{
	for (const auto& presentMode : presentModes) {
		if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return presentMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Application::PickSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != UINT32_MAX) {
		return capabilities.currentExtent;
	}
	else {
		VkExtent2D extent = { WIDTH,HEIGHT };
		extent.width = std::max(capabilities.minImageExtent.width,
			std::min(capabilities.maxImageExtent.width, extent.width));
		extent.height = std::max(capabilities.minImageExtent.height,
			std::min(capabilities.maxImageExtent.height, extent.height));

		return  extent;
	}
}

VkShaderModule Application::CreateShaderModule(const std::vector<char>& code)
{
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
	VkShaderModule shaderModule;
	if (vkCreateShaderModule(m_VkLogicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		std::cout << "ERROR CREATING SHADER MODULE" << std::endl;
	}
	return shaderModule;
}

void Application::DrawFrame()
{
	vkWaitForFences(m_VkLogicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
	//After last command complete signal received by fence, we have to manually reset fence to non-signaled state.
	
	uint32_t imageIndex;
	//UINT64_MAX =>Disables timeout for acquisition.
	vkAcquireNextImageKHR(m_VkLogicalDevice, m_VkSwapChain, UINT64_MAX, imageAvailableSemaphore[currentFrame], VK_NULL_HANDLE,
		&imageIndex);

	//Check currently used image is belong to last frame or not.
	if(imagesFlightFences[imageIndex]!=VK_NULL_HANDLE)
	{
		//If so, wait until last frame's analysis complete.
		vkWaitForFences(m_VkLogicalDevice, 1, &imagesFlightFences[imageIndex], VK_TRUE, UINT64_MAX);
	}

	//After last frame processing complete, make current frame as active frame(which will used as last frame, next frame)
	imagesFlightFences[imageIndex] = inFlightFences[currentFrame];
	
	VkSemaphore waitSemaphores[] = { imageAvailableSemaphore[currentFrame] };
	VkSemaphore signalSemaphores[] = { renderFinishedSemaphore[currentFrame] };
	VkPipelineStageFlags waitFlags[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = waitSemaphores;
	submit_info.pWaitDstStageMask = waitFlags;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &commandBuffers[imageIndex];
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signalSemaphores;

	//Reset fence after all waiting completed
	vkResetFences(m_VkLogicalDevice, 1, &inFlightFences[currentFrame]);

	if (vkQueueSubmit(m_graphicsQueue, 1, &submit_info, inFlightFences[currentFrame]) != VK_SUCCESS)
	{
		std::cout << "FAILED TO SUBMIT DRAW COMMAND BUFFER" << std::endl;
	}

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapchains[] = { m_VkSwapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapchains;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;
	
	vkQueuePresentKHR(m_presentationQueue, &presentInfo);

	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

}
