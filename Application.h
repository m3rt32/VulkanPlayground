#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <optional>
class Application {
public:
	void Run();
private:
	void InitWindow();
	void InitVulkan();
	void Loop();
	void CleanUp();

public:
	struct QueueFamilyIndices {
		std::optional<uint32_t> graphicsFamily;
		bool IsComplete() { return graphicsFamily.has_value(); }
	};

private:
	void CreateVulkanInstance();
	void CreateSurface();
	void SelectPhysicalDevice();
	void CreateLogicalDevice();

private:
	bool CheckValidationLayers(const std::vector<const char*>& validationLayers);
	bool IsDeviceSuitable(VkPhysicalDevice targetDevice);
	QueueFamilyIndices FindQueueFamilyIndices(VkPhysicalDevice targetDevice);

private:
	GLFWwindow* m_window;
	VkInstance m_Vkinstance;
	VkPhysicalDevice m_VkPhysicalDevice = VK_NULL_HANDLE; //Vulkan manages malloc for this. Don't destroy
	VkDevice m_VkLogicalDevice;
	VkQueue m_graphicsQueue; //Vulkan managed malloc, gets cleaned when device destroyed
	VkSurfaceKHR m_surface;
};

