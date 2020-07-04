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
	std::vector<VkImage> swapChainImages;
	std::vector<VkFramebuffer> frameBuffers;
	std::vector<VkImageView> swapChainImageViews;
	uint32_t width, height;
};

struct ImageLayout {
	VkAccessFlags AccessMask;
	VkImageLayout Layout;
};

struct LavaGpuBuffer {
	VkBuffer buffer;
	VkDeviceMemory memory;
	void* data;
	size_t size;
};


struct Vec3 {
	float x, y, z;
};

struct Vertex {
	Vec3 Position;
	Vec3 Normal;
};

struct Mesh {
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
};

class LavaRenderer {
public:
	LavaRenderer();
	void InitVulkan();
	void DestroyVulkan();

private:
	void CreateSwapchain();
	void DestroySwapchain();
private:
	VkPhysicalDevice PickPhysicalDevice(VkPhysicalDevice* devices, uint32_t deviceCount);
	void CreateInstance();
	void RegisterDebugCallback();
	void CreateDevice();
	void CreateSurface();
	void CreateSemaphore();
	void CreateQueue();
	void CreateCommandPool();
	void CreateRenderPass();
	void CreateGraphicsPipeline();
	VkFramebuffer CreateFrameBuffer(VkImageView imageView);
	VkImageView CreateImageView(VkImage image);
	VkImageMemoryBarrier PipelineBarrierImage(VkImage image,ImageLayout sourceLayout,ImageLayout targetLayout);
	uint32_t SelectBufferMemoryTypeIndex(uint32_t requiredMemoryTypeBits, VkMemoryPropertyFlags requiredFlags);

private:
	void CreateBuffer(LavaGpuBuffer& buffer, size_t size,VkBufferUsageFlags usageFlags);
	void DestroyBuffer(const LavaGpuBuffer& buffer);
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
	VkShaderModule vertShader;
	VkShaderModule fragShader;
	VkPipeline trianglePipeline;
	VkPipelineLayout trianglePipelineLayout;
	VkDebugReportCallbackEXT callback = 0;
	VkPhysicalDeviceMemoryProperties memoryProperties;

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