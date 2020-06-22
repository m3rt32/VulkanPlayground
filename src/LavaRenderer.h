#pragma once
#define GLFW_INCLUDE_VULKAN
#define _CRT_SECURE_NO_WARNINGS

#include <assert.h>
#include <stdio.h>
#include <iostream>

#include <GLFW/glfw3.h>
//#include <vulkan/vulkan.h>

struct SwapChainData {
public:
	VkFormat format;
	VkPresentModeKHR presentMode;
};

class LavaRenderer {
public:
	LavaRenderer();
	void InitVulkan();
	void DestroyVulkan();

private:
	VkPhysicalDevice PickPhysicalDevice(VkPhysicalDevice* devices, uint32_t deviceCount);
	void CreateInstance();
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
	VkImage* swapChainImages;
	VkFramebuffer frameBuffers[16];
	VkImageView swapChainImageViews[16];
	VkShaderModule vertShader;
	VkShaderModule fragShader;
	VkPipeline trianglePipeline;
	VkPipelineLayout trianglePipelineLayout;

private:
	void GetSwapchainSupportData();
	VkShaderModule LoadShader(const char* path);


private:
	uint32_t queueFamilyIndex = 0; //TODO:Calculate with enumaration and checks
	uint32_t frameBufferWidth;
	uint32_t frameBufferHeight;
	SwapChainData swapChainData;
};