#include "Application.h"
static std::vector<char> readFile(const std::string& filename)
{
	std::ifstream file(filename, std::ios::ate| std::ios::binary);
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
	CreateGraphicsPipeline();
}

void Application::Loop()
{
	while (!glfwWindowShouldClose(m_window)) {
		glfwPollEvents();
	}
}

void Application::CleanUp()
{
	vkDestroyPipelineLayout(m_VkLogicalDevice, m_VkPipelineLayout, nullptr);
	for(auto imageView: swapChainImageViews)
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
		if(vkCreateImageView(m_VkLogicalDevice,&createInfo,nullptr,&swapChainImageViews[i])!=VK_SUCCESS)
		{
			std::cout << "Error creating image views" << std::endl;
		}
	}
}

void Application::CreateGraphicsPipeline()
{
	auto vertShaderCode = readFile("shaders/vert.spv");
	std::cout << "VERT SHADER SIZE:"<<vertShaderCode.size() << std::endl;
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

	if(vkCreatePipelineLayout(m_VkLogicalDevice,&pipelineLayoutInfo,nullptr,&m_VkPipelineLayout)!=VK_SUCCESS)
	{
		std::cout << "ERROR CREATING PIPELINE LAYOUT" << std::endl;
	}
	
	vkDestroyShaderModule(m_VkLogicalDevice, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_VkLogicalDevice,fragShaderModule,nullptr);
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
	if(vkCreateShaderModule(m_VkLogicalDevice,&createInfo,nullptr,&shaderModule)!=VK_SUCCESS)
	{
		std::cout << "ERROR CREATING SHADER MODULE" << std::endl;
	}
	return shaderModule;
}
