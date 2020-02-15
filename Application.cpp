#include "Application.h"

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
}

void Application::Loop()
{
	while (!glfwWindowShouldClose(m_window)) {
		glfwPollEvents();
	}
}

void Application::CleanUp()
{
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
