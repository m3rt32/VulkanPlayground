#include "LavaRenderer.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <unordered_map>
#define LAVA_ASSERT(call) \
			{ \
				VkResult result = call; \
				assert(result == VK_SUCCESS); \
			}

#define LAVA_PRINT(s) std::cout<<s<<std::endl
Mesh LoadMesh() {
	using tinyobj::shape_t;
	using tinyobj::material_t;
	using tinyobj::index_t;
	tinyobj::attrib_t attrib;
	std::vector<shape_t> shapes;
	std::vector<material_t> materials;
	std::string warn, err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "assets/armadillo.obj")) {
		throw std::runtime_error(warn + err);
	}

	Mesh outputMesh;
	for (const shape_t& shape : shapes) {
		uint32_t i = 0;
		for (const index_t& index : shape.mesh.indices) {
			Vertex vertex = {};
			vertex.Position = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};

			vertex.Normal = {
				attrib.normals[3 * index.normal_index + 0],
				attrib.normals[3 * index.normal_index + 1],
				attrib.normals[3 * index.normal_index + 2]
			};

			outputMesh.vertices.push_back(vertex);
			outputMesh.indices.push_back(i);
			i++;
		}
	}
	return outputMesh;
}

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
	LAVA_ASSERT(vkAllocateCommandBuffers(activeDevice, &allocateInfo, &commandBuffer));

	Mesh mesh = LoadMesh();

	LavaGpuBuffer vb = {};
	CreateBuffer(vb,128*1024*1024,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	LavaGpuBuffer ib = {};
	CreateBuffer(ib, 128 * 1024 * 1024, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

	memcpy(vb.data, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));
	memcpy(ib.data, mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		uint32_t imageIndex = 0;
		LAVA_ASSERT(vkAcquireNextImageKHR(activeDevice, swapChain, UINT64_MAX, acquireSemaphore, nullptr, &imageIndex));

		LAVA_ASSERT(vkResetCommandPool(activeDevice, commandPool, 0));

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		LAVA_ASSERT(vkBeginCommandBuffer(commandBuffer, &beginInfo));

		ImageLayout beginSrcLayout;
		beginSrcLayout.AccessMask = VK_ACCESS_MEMORY_READ_BIT;
		beginSrcLayout.Layout = VK_IMAGE_LAYOUT_UNDEFINED;

		ImageLayout beginDstLayout;
		beginDstLayout.AccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		beginDstLayout.Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


		VkImageMemoryBarrier renderBeginBarrier = PipelineBarrierImage(swapChainData.swapChainImages[imageIndex], beginSrcLayout, beginDstLayout);
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderBeginBarrier);

		VkClearValue color = { .5,0,0,1 };

		//Dont clear the screen with a command, clear it via renderpass. No need for additional operation.
		VkRenderPassBeginInfo beginPassInfo = {};
		beginPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		beginPassInfo.renderPass = renderPass;
		beginPassInfo.framebuffer = swapChainData.frameBuffers[imageIndex];
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

		VkDeviceSize offsets = 0;
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vb.buffer, &offsets);
		vkCmdBindIndexBuffer(commandBuffer, ib.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(commandBuffer, mesh.indices.size(), 1, 0, 0, 0);
		//vkCmdDraw(commandBuffer, 3, 1, 0, 0);
		vkCmdEndRenderPass(commandBuffer);

		ImageLayout endSrcLayout;
		endSrcLayout.AccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		endSrcLayout.Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		ImageLayout endDstLayout;
		endDstLayout.AccessMask = 0;
		endDstLayout.Layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkImageMemoryBarrier renderEndBarrier = PipelineBarrierImage(swapChainData.swapChainImages[imageIndex], endSrcLayout, endDstLayout);
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderEndBarrier);

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

		LAVA_ASSERT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapChain;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &releaseSemaphore;

		LAVA_ASSERT(vkQueuePresentKHR(queue, &presentInfo));

		LAVA_ASSERT(vkDeviceWaitIdle(activeDevice));
	}

	DestroyBuffer(vb);
	DestroyBuffer(ib);

	glfwDestroyWindow(window);
	DestroyVulkan();
}

void LavaRenderer::InitVulkan()
{
	CreateInstance();
	RegisterDebugCallback();
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
	LAVA_ASSERT(vkDeviceWaitIdle(activeDevice));

	vkDestroyCommandPool(activeDevice, commandPool, 0);

	DestroySwapchain();
	vkDestroySurfaceKHR(instance, surface, 0);
	vkDestroyDevice(activeDevice, 0);
	
	PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT =
		(PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");

	vkDestroyDebugReportCallbackEXT(instance, callback, 0);
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

	//const char* extensions[] = {
	//	VK_KHR_SWAPCHAIN_EXTENSION_NAME
	//};

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
	extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.ppEnabledLayerNames = validationLayers;
	createInfo.enabledLayerCount = 1;
	createInfo.ppEnabledExtensionNames = extensions.data();
	createInfo.enabledExtensionCount = extensions.size();

	instance = 0;
	LAVA_ASSERT(vkCreateInstance(&createInfo, 0, &instance));
}


//Cannot be an instance function somehow.
VKAPI_ATTR VkBool32 VKAPI_CALL MyDebugReportCallback(
	VkDebugReportFlagsEXT       flags,
	VkDebugReportObjectTypeEXT  objectType,
	uint64_t                    object,
	size_t                      location,
	int32_t                     messageCode,
	const char* pLayerPrefix,
	const char* pMessage,
	void* pUserData)
{
	const char* type = (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) ? "ERROR:" :
		(flags & (VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)) ? "WARNING:" :
		"INFO";
	LAVA_PRINT(type << pMessage);

	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
		assert(!"Validation error!!!");

	return VK_FALSE;
}

void LavaRenderer::RegisterDebugCallback()
{
	VkDebugReportCallbackCreateInfoEXT  debugReportCreateInfo = {};
	debugReportCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
	debugReportCreateInfo.flags =
		VK_DEBUG_REPORT_WARNING_BIT_EXT |
		VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
		VK_DEBUG_REPORT_ERROR_BIT_EXT;
	debugReportCreateInfo.pfnCallback = MyDebugReportCallback;

	PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT =
		(PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");

	LAVA_ASSERT(vkCreateDebugReportCallbackEXT(instance, &debugReportCreateInfo, nullptr, &callback));
}

void LavaRenderer::CreateDevice()
{
	VkPhysicalDevice gpus[16];

	uint32_t deviceCount = sizeof(gpus) / sizeof(gpus[0]);
	LAVA_ASSERT(vkEnumeratePhysicalDevices(instance, &deviceCount, &(gpus[0])));
	activePhysicalDevice = PickPhysicalDevice(gpus, deviceCount);
	assert(activePhysicalDevice);

	SetGraphicsQueueFamily();

	//VkBool32 presentationSupported = 0; //TODO: This is a HACK, fix later. We should actually check while device pick.
	//LAVA_ASSERT(vkGetPhysicalDeviceSurfaceSupportKHR(activePhysicalDevice, queueFamilyIndex, surface, &presentationSupported) == VK_SUCCESS);
	//LAVA_ASSERT(presentationSupported);

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

	LAVA_ASSERT(vkCreateDevice(activePhysicalDevice, &deviceCreateInfo, nullptr, &activeDevice));


}

void LavaRenderer::CreateSurface()
{
	LAVA_ASSERT(glfwCreateWindowSurface(instance, window, nullptr, &surface));
}


void LavaRenderer::CreateSwapchain()
{
	//Query surface support for swapchain specs
	VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
	LAVA_ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(activePhysicalDevice, surface, &surfaceCapabilities));

	VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	if (swapChain) {
		swapChainCreateInfo.oldSwapchain = swapChain;
	}
	swapChainCreateInfo.surface = surface;
	swapChainCreateInfo.minImageCount = std::max(2u, surfaceCapabilities.minImageCount);
	swapChainCreateInfo.imageFormat = swapChainData.format;
	swapChainCreateInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	swapChainCreateInfo.imageExtent.width = frameBufferWidth;
	swapChainCreateInfo.imageExtent.height = frameBufferHeight;
	swapChainCreateInfo.imageArrayLayers = 1;//Unless Stereo rendering
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapChainCreateInfo.queueFamilyIndexCount = 1;
	swapChainCreateInfo.pQueueFamilyIndices = &queueFamilyIndex;
	swapChainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; //swapChainData.presentMode;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; //OPAQUE NOT SUPPORTED ON ANDROID
	swapChainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

	LAVA_ASSERT(vkCreateSwapchainKHR(activeDevice, &swapChainCreateInfo, 0, &swapChain));
}

void LavaRenderer::DestroySwapchain()
{
	// Causes an assertion on Swapchain destroy, swapchain probably auto destroys this?
	//for (uint32_t i = 0; i < swapChainData.swapChainImages.size(); i++) {
	//	vkDestroyImage(activeDevice, swapChainData.swapChainImages[i], 0);
	//}

	for (uint32_t i = 0; i < swapChainData.frameBuffers.size(); i++) {
		vkDestroyFramebuffer(activeDevice, swapChainData.frameBuffers[i], 0);
	}

	for (uint32_t i = 0; i < swapChainData.swapChainImageViews.size(); i++) {
		vkDestroyImageView(activeDevice, swapChainData.swapChainImageViews[i], 0);
	}

	vkDestroyPipeline(activeDevice, trianglePipeline, 0);
	vkDestroyPipelineLayout(activeDevice, trianglePipelineLayout, 0);
	vkDestroyShaderModule(activeDevice, vertShader, 0);
	vkDestroyShaderModule(activeDevice, fragShader, 0);
	vkDestroyRenderPass(activeDevice, renderPass, 0);
	vkDestroySemaphore(activeDevice, acquireSemaphore, 0);
	vkDestroySemaphore(activeDevice, releaseSemaphore, 0);
	vkDestroySwapchainKHR(activeDevice, swapChain, 0);
}

void LavaRenderer::CreateSemaphore()
{
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	LAVA_ASSERT(vkCreateSemaphore(activeDevice, &semaphoreCreateInfo, nullptr, &acquireSemaphore));

	LAVA_ASSERT(vkCreateSemaphore(activeDevice, &semaphoreCreateInfo, nullptr, &releaseSemaphore));
}

void LavaRenderer::CreateQueue()
{
	vkGetDeviceQueue(activeDevice, queueFamilyIndex, 0, &queue);
}

void LavaRenderer::CreateCommandPool()
{
	uint32_t swapChainImageCount = 0;
	LAVA_ASSERT(vkGetSwapchainImagesKHR(activeDevice, swapChain, &swapChainImageCount, 0));
	swapChainData.swapChainImages = std::vector<VkImage>(swapChainImageCount);
	swapChainData.swapChainImageViews = std::vector<VkImageView>(swapChainImageCount);
	swapChainData.frameBuffers = std::vector<VkFramebuffer>(swapChainImageCount);

	LAVA_ASSERT(vkGetSwapchainImagesKHR(activeDevice, swapChain, &swapChainImageCount, swapChainData.swapChainImages.data()));
	//LAVA_ASSERT(vkGetSwapchainImagesKHR(activeDevice, swapChain, &swapChainImageCount, 0) == VK_SUCCESS);

	for (uint32_t i = 0; i < swapChainImageCount; i++) {
		swapChainData.swapChainImageViews[i] = CreateImageView(swapChainData.swapChainImages[i]);
	}

	for (uint32_t i = 0; i < swapChainImageCount; i++) {
		swapChainData.frameBuffers[i] = CreateFrameBuffer(swapChainData.swapChainImageViews[i]);
	}

	VkCommandPoolCreateInfo commandPoolCreateInfo = {};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;

	LAVA_ASSERT(vkCreateCommandPool(activeDevice, &commandPoolCreateInfo, nullptr, &commandPool));
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
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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

	LAVA_ASSERT(vkCreateRenderPass(activeDevice, &renderPassCreateInfo, nullptr, &renderPass));
}

void LavaRenderer::CreateGraphicsPipeline()
{
	vertShader = LoadShader("shaders/triangle.vert.spv");
	fragShader = LoadShader("shaders/triangle.frag.spv");
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

	VkVertexInputBindingDescription stream = {};
	stream.binding = 0;
	stream.stride = sizeof(Vertex);
	stream.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription attribs[3] = {};
	attribs[0].location = 0;
	attribs[0].format = VK_FORMAT_R32G32B32_SFLOAT; // 3 floats
	attribs[0].offset = 0;

	attribs[1].location = 1;
	attribs[1].format = VK_FORMAT_R32G32B32_SFLOAT; // 3 floats
	attribs[1].offset = 12;

	attribs[2].location = 2;
	attribs[2].format = VK_FORMAT_R32G32_SFLOAT; // 2 floats
	attribs[2].offset = 24;

	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1; //buffer count
	vertexInputStateCreateInfo.pVertexBindingDescriptions = &stream;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount=3;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = attribs;

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

	LAVA_ASSERT(vkCreatePipelineLayout(activeDevice, &pipelineLayoutCreateInfo, nullptr, &trianglePipelineLayout));

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

	LAVA_ASSERT(vkCreateGraphicsPipelines(activeDevice, pipelineCache, 1, &createInfo, nullptr, &trianglePipeline));
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

	LAVA_ASSERT(vkCreateFramebuffer(activeDevice, &frameBufferCreateInfo, nullptr, &frameBuffer));
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
	LAVA_ASSERT(vkCreateImageView(activeDevice, &imageViewCreateInfo, nullptr, &imageView));

	return imageView;
}

VkImageMemoryBarrier LavaRenderer::PipelineBarrierImage(VkImage image, ImageLayout sourceLayout, ImageLayout targetLayout)
{
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask = sourceLayout.AccessMask;
	barrier.dstAccessMask = targetLayout.AccessMask;
	barrier.oldLayout = sourceLayout.Layout;
	barrier.newLayout = targetLayout.Layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//BUG: Some android drivers don't support these constants
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;  
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	return barrier;
}

uint32_t LavaRenderer::SelectBufferMemoryTypeIndex(uint32_t requiredMemoryTypeBits, VkMemoryPropertyFlags requiredFlags)
{
	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
		if ((requiredMemoryTypeBits & (1 << i)) != 0 && (memoryProperties.memoryTypes[i].propertyFlags & requiredFlags))
			return i;
	}
	assert("Buffer requirements not supported by GPU");
	return ~0u;
}

void LavaRenderer::CreateBuffer(LavaGpuBuffer& gpuBuffer, size_t size, VkBufferUsageFlags usageFlags)
{
	VkBufferCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	createInfo.size = size;
	createInfo.usage = usageFlags;

	VkBuffer buffer = {};
	LAVA_ASSERT(vkCreateBuffer(activeDevice, &createInfo, nullptr, &buffer));

	VkMemoryRequirements memoryRequirements = {};
 	vkGetBufferMemoryRequirements(activeDevice, buffer, &memoryRequirements);

	uint32_t memoryTypeIndex = SelectBufferMemoryTypeIndex(memoryRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	assert(memoryTypeIndex != ~0u);

	VkMemoryAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.allocationSize = memoryRequirements.size;
	allocateInfo.memoryTypeIndex = memoryTypeIndex;

	VkDeviceMemory memory = {};
	LAVA_ASSERT(vkAllocateMemory(activeDevice, &allocateInfo, nullptr, &memory));

	vkBindBufferMemory(activeDevice, buffer, memory, 0); // no need for offset

	void* data = 0;
	LAVA_ASSERT(vkMapMemory(activeDevice, memory, 0, size, 0, &data));

	gpuBuffer.buffer = buffer;
	gpuBuffer.memory = memory;
	gpuBuffer.data = data;
	gpuBuffer.size = size;
}

void LavaRenderer::DestroyBuffer(const LavaGpuBuffer& buffer)
{
	vkFreeMemory(activeDevice, buffer.memory, 0);
	vkDestroyBuffer(activeDevice, buffer.buffer, 0);
}

void LavaRenderer::GetSwapchainSupportData()
{
	swapChainData = {};
	uint32_t surfaceFormatCount = 0;
	LAVA_ASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(activePhysicalDevice, surface, &surfaceFormatCount, 0));
	assert(surfaceFormatCount > 0);

	std::vector<VkSurfaceFormatKHR> surfaceFormat(surfaceFormatCount);
	LAVA_ASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(activePhysicalDevice, surface, &surfaceFormatCount, surfaceFormat.data()));
	swapChainData.format = surfaceFormat[0].format;

	VkPresentModeKHR presentMode[16];
	uint32_t presentModeCount = sizeof(presentMode) / sizeof(presentMode[0]);
	LAVA_ASSERT(vkGetPhysicalDeviceSurfacePresentModesKHR(activePhysicalDevice, surface, &presentModeCount, presentMode));

	swapChainData.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (VkPresentModeKHR present : presentMode) {
		if (present == VK_PRESENT_MODE_MAILBOX_KHR)
			swapChainData.presentMode = present;
	}
}

void LavaRenderer::SetGraphicsQueueFamily()
{
	uint32_t queuePropertyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(activePhysicalDevice, &queuePropertyCount, 0);
	std::vector<VkQueueFamilyProperties> queueFamilyProperties(queuePropertyCount);

	vkGetPhysicalDeviceMemoryProperties(activePhysicalDevice, &memoryProperties); //For buffers

	vkGetPhysicalDeviceQueueFamilyProperties(activePhysicalDevice, &queuePropertyCount, queueFamilyProperties.data());

	for (uint32_t i = 0; i < queuePropertyCount; i++) {
		if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			VkBool32 presentationSupport = 0;
			vkGetPhysicalDeviceSurfaceSupportKHR(activePhysicalDevice, i, surface, &presentationSupport);
			if (presentationSupport) {
				queueFamilyIndex = i;
				return;
			}
		}
	}

	assert(!"No graphics queue supported GPU found");
	queueFamilyIndex = ~0u;
}

VkShaderModule LavaRenderer::LoadShader(const char* path)
{
	FILE* file = fopen(path, "rb");
	fseek(file, 0, SEEK_END);
	long length = ftell(file);
	fseek(file, 0, SEEK_SET);

	char* buffer = new char[length];
	size_t file_length = fread(buffer, 1, length, file);
	assert(file_length == size_t(length));
	fclose(file);

	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = length;
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(buffer);

	VkShaderModule shaderModule = 0;
	LAVA_ASSERT(vkCreateShaderModule(activeDevice, &shaderModuleCreateInfo, nullptr, &shaderModule));

	return shaderModule;
}
