#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <optional>
#include <set>
#include <cstdint>
#include <fstream>
struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentationFamily;
	bool IsComplete() { return graphicsFamily.has_value() && presentationFamily.has_value(); }
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

class Application {
public:
	void Run();
private:
	void InitWindow();
	void InitVulkan();
	void Loop();
	void CleanUp();

private:
	void CreateVulkanInstance();
	void CreateSurface();
	void SelectPhysicalDevice();
	void CreateLogicalDevice();
	void CreateSwapChain();
	void CreateImageView();
	void CreateRenderPass();
	void CreateGraphicsPipeline();
	void CreateFrameBuffer();

private:
	bool CheckValidationLayers(const std::vector<const char*>& validationLayers);
	bool IsDeviceSuitable(VkPhysicalDevice targetDevice);
	QueueFamilyIndices FindQueueFamilyIndices(VkPhysicalDevice targetDevice);
	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice targetDevice);
	bool CheckExtensionSupport(VkPhysicalDevice targetDevice);
	VkSurfaceFormatKHR PickSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
	VkPresentModeKHR PickSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes);
	VkExtent2D PickSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	VkShaderModule CreateShaderModule(const std::vector<char>& code);
private:
	const float WIDTH = 800;
	const float HEIGHT = 600;
	GLFWwindow* m_window;
	VkInstance m_Vkinstance;
	VkPhysicalDevice m_VkPhysicalDevice = VK_NULL_HANDLE; //Vulkan manages malloc for this. Don't destroy
	VkDevice m_VkLogicalDevice;
	VkQueue m_graphicsQueue; //Vulkan managed malloc, gets cleaned when device destroyed
	VkSurfaceKHR m_surface;
	VkQueue m_presentationQueue;
	VkSwapchainKHR m_VkSwapChain;
	VkFormat m_VkFormat;
	VkExtent2D m_VkExtent;
	VkRenderPass m_VkRenderPass;
	VkPipelineLayout m_VkPipelineLayout;
	VkPipeline m_VkPipeline;

private:
	VkShaderModule vertexShaderModule;
	VkShaderModule fragShaderModule;
	
private:
	const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	std::vector<VkImage> swapChainImages;
	std::vector<VkImageView> swapChainImageViews;
	std::vector<VkFramebuffer> swapChainFrameBuffers;
};

