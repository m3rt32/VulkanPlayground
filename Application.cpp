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
	m_window = glfwCreateWindow(800, 600, "Vulkan App", nullptr, nullptr);
}

void Application::InitVulkan()
{
	CreateVulkanInstance();
	Create
	SelectPhysicalDevice();
	CreateLogicalDevice();
}

void Application::Loop()
{
	while (!glfwWindowShouldClose(m_window)) {
		glfwPollEvents();
	}
}

void Application::CleanUp()
{
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
			if (strcmp(extensionProperties[j].extensionName,glfwExtensions[i])==0) {
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

	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
	queueCreateInfo.queueCount = 1;
	float queuePriority = 1.0f;
	queueCreateInfo.pQueuePriorities = &queuePriority;

	VkPhysicalDeviceFeatures deviceFeatures = {};

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

	if (vkCreateDevice(m_VkPhysicalDevice, &deviceCreateInfo, nullptr, &m_VkLogicalDevice) != VK_SUCCESS) {
		std::cout << "Logical device creation failed!" << std::endl;
	}

	vkGetDeviceQueue(m_VkLogicalDevice, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
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
	return FindQueueFamilyIndices(targetDevice).IsComplete();
}

Application::QueueFamilyIndices Application::FindQueueFamilyIndices(VkPhysicalDevice targetDevice)
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

		if (indices.IsComplete())
			break;

		i++;
	}

	return indices;
}
