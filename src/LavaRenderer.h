#pragma once
#define GLFW_INCLUDE_VULKAN
#define _CRT_SECURE_NO_WARNINGS

#include <assert.h>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <algorithm>

#include <GLFW/glfw3.h>
//#include <vulkan/vulkan.h>

struct SwapChainData {
public:
	VkFormat format;
	VkPresentModeKHR presentMode;
};

struct ImageLayout {
	VkAccessFlags AccessMask;
	VkImageLayout Layout;
};

class LavaRenderer {
public:
	LavaRenderer();
	void InitVulkan();
	void DestroyVulkan();

private:
	VkPhysicalDevice PickPhysicalDevice(VkPhysicalDevice* devices, uint32_t deviceCount);
	void CreateInstance();
	void RegisterDebugCallback();
	void CreateDevice();
	void CreateSurface();
	void CreateSwapchain();
	void CreateSemaphore();
	void CreateQueue();
	void CreateCommandPool();
	void CreateRenderPass();
	void CreateGraphicsPipeline();
	VkFramebuffer CreateFrameBuffer(VkImageView imageView);
	VkImageView CreateImageView(VkImage image);
	VkImageMemoryBarrier PipelineBarrierImage(VkImage image,ImageLayout sourceLayout,ImageLayout targetLayout);

private:
	GLFWwindow* window;
	VkInstance instance;
	VkPhysicalDevice activePhysicalDevice;
	VkDevice activeDevice;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapChain;
	VkSemaphore acquireSemaphore;
	VkSemaphore releaseSemaphore;
	VkQueue queue;
	VkCommandPool commandPool;
	VkRenderPass renderPass;
	std::vector<VkImage> swapChainImages;
	std::vector<VkFramebuffer> frameBuffers;
	std::vector<VkImageView> swapChainImageViews;
	VkShaderModule vertShader;
	VkShaderModule fragShader;
	VkPipeline trianglePipeline;
	VkPipelineLayout trianglePipelineLayout;
	VkDebugReportCallbackEXT callback = 0;

private:
	void GetSwapchainSupportData();
	void SetGraphicsQueueFamily();
	VkShaderModule LoadShader(const char* path);


private:
	uint32_t queueFamilyIndex = 0; //TODO:Calculate with enumaration and checks
	uint32_t frameBufferWidth;
	uint32_t frameBufferHeight;
	SwapChainData swapChainData;
};