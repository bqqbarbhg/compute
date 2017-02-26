#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <math.h>

#define ArrayCount(arr) (sizeof(arr)/sizeof(*(arr)))

struct Device
{
	VkInstance Instance;
	VkPhysicalDevice PhysicalDevice;
	VkDevice Device;
	VkQueue Queue;
	VkSurfaceKHR Surface;
	VkSwapchainKHR Swapchain;
	uint32_t NumSwapchainImages;

	VkRenderPass RenderPass;

	Texture *BackbufferTextures[8];
	Framebuffer *BackbufferFramebuffers[8];

	VkCommandPool CommandPool;
	CommandBuffer CommandBuffers[8];

	VkSemaphore SemaImageAvailable;
	VkSemaphore SemaRenderFinished;

	uint32_t FrameIndex;

	uint32_t Width, Height;

	VkFence PresentFence[8];

	VkDebugReportCallbackEXT DebugCallback;

	std::unordered_map<uint32_t, VkRenderPass> RenderPasses;
};

struct Texture
{
	VkImage Image;
	VkImageView ImageView;
	VkDeviceMemory Memory;
};

struct Framebuffer
{
	std::unordered_map<uint32_t, VkFramebuffer> Framebuffers;
	std::vector<Texture*> Color;
	Texture *DepthStencil;

	uint32_t Width, Height;
};

enum RenderPassFlag
{
	RPF_ClearColor = 1 << 0,
	RPF_ClearDepth = 1 << 1,
	RPF_ClearStencil = 1 << 2,
	RPF_Present = 1 << 3,
	RPF_LoadFramebuffer = 1 << 4,
	RPF_HasDepthStencil = 1 << 5,

	RPF_NumColorShift = 16,
};

struct CommandBuffer
{
	Device *Dev;

	VkCommandBuffer CmdBuf;
	bool InRenderPass;

	Framebuffer *NextFramebuffer;
	ClearInfo NextClear;
	bool NextLoadFramebuffer;
};

VKAPI_ATTR VkBool32 VKAPI_CALL vulkanCallback(
	VkDebugReportFlagsEXT                       flags,
	VkDebugReportObjectTypeEXT                  objectType,
	uint64_t                                    object,
	size_t                                      location,
	int32_t                                     messageCode,
	const char*                                 pLayerPrefix,
	const char*                                 pMessage,
	void*                                       pUserData)
{
	fprintf(stderr, "VK error (%s): %s\n", pLayerPrefix, pMessage);
	__debugbreak();
	return VK_FALSE;
}

Device *CreateDevice()
{
	Device *v = new Device();
	VkResult res;

	VkInstanceCreateInfo ci;
	ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ci.pNext = nullptr;
	ci.flags = 0;

	std::vector<const char*> instExt;
	instExt.push_back("VK_EXT_debug_report");

	uint32_t glfwNum;
	const char **glfwExt = glfwGetRequiredInstanceExtensions(&glfwNum);
	for (uint32_t i = 0; i < glfwNum; i++)
	{
		bool found = false;
		for (size_t j = 0; j < instExt.size(); j++)
		{
			if (!strcmp(instExt[j], glfwExt[i]))
			{
				found = true;
				break;
			}
		}

		if (!found)
			instExt.push_back(glfwExt[i]);
	}

	const char *deviceExt[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};

	const char *instLayer[] = {
		"VK_LAYER_LUNARG_core_validation",
		"VK_LAYER_LUNARG_swapchain",
		"VK_LAYER_LUNARG_parameter_validation",
		"VK_LAYER_LUNARG_object_tracker",
	};

	uint32_t numInstLayers;
	vkEnumerateInstanceLayerProperties(&numInstLayers, nullptr);
	VkLayerProperties *instLayers = new VkLayerProperties[numInstLayers];
	vkEnumerateInstanceLayerProperties(&numInstLayers, instLayers);

	{
		uint32_t numInstExtensions;
		vkEnumerateInstanceExtensionProperties(nullptr, &numInstExtensions, nullptr);
		VkExtensionProperties *instExtensions = new VkExtensionProperties[numInstExtensions];
		vkEnumerateInstanceExtensionProperties(nullptr, &numInstExtensions, instExtensions);

		for (uint32_t i = 0; i < numInstExtensions; i++)
		{
			printf("> %s\n", instExtensions[i].extensionName);
		}
	}

	for (uint32_t i = 0; i < numInstLayers; i++)
	{
		printf("Layer: %s (%s)\n", instLayers[i].layerName, instLayers[i].description);

		uint32_t numInstExtensions;
		vkEnumerateInstanceExtensionProperties(instLayers[i].layerName, &numInstExtensions, nullptr);
		VkExtensionProperties *instExtensions = new VkExtensionProperties[numInstExtensions];
		vkEnumerateInstanceExtensionProperties(instLayers[i].layerName, &numInstExtensions, instExtensions);

		for (uint32_t i = 0; i < numInstExtensions; i++)
		{
			printf("> %s\n", instExtensions[i].extensionName);
		}
	}

	ci.enabledExtensionCount = instExt.size();
	ci.enabledLayerCount = ArrayCount(instLayer);
	ci.ppEnabledExtensionNames = instExt.data();
	ci.ppEnabledLayerNames = instLayer;
	ci.pApplicationInfo = nullptr;

#if 0
	ci.enabledLayerCount = 0;
#endif

	res = vkCreateInstance(&ci, nullptr, &v->instance);

	auto pfn_vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(v->instance, "vkCreateDebugReportCallbackEXT");

	VkDebugReportCallbackCreateInfoEXT ei;
	ei.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	ei.pNext = nullptr;
	ei.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
		VK_DEBUG_REPORT_WARNING_BIT_EXT;

	ei.pfnCallback = &vulkanCallback;
	ei.pUserData = nullptr;
	res = pfn_vkCreateDebugReportCallbackEXT(v->instance, &ei, nullptr, &v->debugCallback);

	uint32_t family = ~0U;

	uint32_t numDevices;
	vkEnumeratePhysicalDevices(v->instance, &numDevices, nullptr);
	VkPhysicalDevice *devices = new VkPhysicalDevice[numDevices];
	vkEnumeratePhysicalDevices(v->instance, &numDevices, devices);

	for (uint32_t i = 0; i < numDevices; i++)
	{
		VkPhysicalDeviceProperties props;
		VkPhysicalDeviceFeatures feat;
		vkGetPhysicalDeviceProperties(devices[i], &props);
		vkGetPhysicalDeviceFeatures(devices[i], &feat);

		uint32_t numFamilies;
		vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &numFamilies, nullptr);
		VkQueueFamilyProperties *families = new VkQueueFamilyProperties[numFamilies];
		vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &numFamilies, families);

		for (uint32_t i = 0; i < numFamilies; i++)
		{
			if (families[i].queueCount > 0 && families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				family = i;
				break;
			}
		}

		printf("> %s\n", props.deviceName);

		break;
	}

	v->physicalDevice = devices[0];

	float prio[1] = { 1.0f };

	VkDeviceQueueCreateInfo q[1];
	q[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	q[0].pNext = nullptr;
	q[0].flags = 0;
	q[0].queueCount = 1;
	q[0].queueFamilyIndex = family;
	q[0].pQueuePriorities = prio;

	VkDeviceCreateInfo di;
	di.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	di.pNext = nullptr;
	di.flags = 0;

	di.enabledExtensionCount = ArrayCount(deviceExt);
	di.enabledLayerCount = 0;
	di.ppEnabledExtensionNames = deviceExt;
	di.pEnabledFeatures = nullptr;
	di.queueCreateInfoCount = 1;
	di.pQueueCreateInfos = q;

	res = vkCreateDevice(devices[0], &di, nullptr, &v->device);

	vkGetDeviceQueue(v->device, family, 0, &v->queue);

	glfwCreateWindowSurface(v->Instance, (GLFWwindow*)arg, NULL, &v->Surface);

	uint32_t numFormats;
	vkGetPhysicalDeviceSurfaceFormatsKHR(v->PhysicalDevice, v->Surface, &numFormats, nullptr);
	VkSurfaceFormatKHR *formats = new VkSurfaceFormatKHR[numFormats];
	vkGetPhysicalDeviceSurfaceFormatsKHR(v->PhysicalDevice, v->Surface, &numFormats, formats);

	VkSurfaceCapabilitiesKHR surfaceCaps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(v->PhysicalDevice, v->Surface, &surfaceCaps);

	printf("Extent: %dx%d\n", surfaceCaps.currentExtent.width, surfaceCaps.currentExtent.height);

	v->Width = surfaceCaps.currentExtent.width;
	v->Height = surfaceCaps.currentExtent.height;

	VkSurfaceFormatKHR format = formats[0];

	uint32_t numPresentModes;
	vkGetPhysicalDeviceSurfacePresentModesKHR(v->PhysicalDevice, v->Surface, &numPresentModes, nullptr);
	VkPresentModeKHR *presentModes = new VkPresentModeKHR[numPresentModes];
	vkGetPhysicalDeviceSurfacePresentModesKHR(v->PhysicalDevice, v->Surface, &numPresentModes, presentModes);

	VkBool32 support;
	vkGetPhysicalDeviceSurfaceSupportKHR(v->PhysicalDevice, family, v->Surface, &support);

	VkSwapchainCreateInfoKHR si;
	si.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	si.pNext = nullptr;
	si.flags = 0;

	si.surface = v->Surface;
	si.clipped = VK_TRUE;
	si.imageExtent.width = v->Width;
	si.imageExtent.height = v->Height;
	si.minImageCount = 3;
	si.imageFormat = format.format;
	si.imageColorSpace = format.colorSpace;
	si.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	si.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	si.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	si.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	si.oldSwapchain = VK_NULL_HANDLE;
	si.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	si.imageArrayLayers = 1;
	si.queueFamilyIndexCount = 1;
	si.pQueueFamilyIndices = &family;

	res = vkCreateSwapchainKHR(v->Device, &si, nullptr, &v->Swapchain);

	VkSemaphoreCreateInfo mi;
	mi.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	mi.pNext = nullptr;
	mi.flags = 0;
	vkCreateSemaphore(v->Device, &mi, nullptr, &v->SemaImageAvailable);
	vkCreateSemaphore(v->Device, &mi, nullptr, &v->SemaRenderFinished);

	res = vkGetSwapchainImagesKHR(v->Device, v->Swapchain, &v->NumSwapchainImages, nullptr);
	res = vkGetSwapchainImagesKHR(v->Device, v->Swapchain, &v->NumSwapchainImages, v->SwapchainImages);

	for (uint32_t i = 0; i < v->NumSwapchainImages; i++)
	{
		VkImageViewCreateInfo vi;
		vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		vi.pNext = nullptr;
		vi.flags = 0;

		vi.image = v->SwapchainImages[i];
		vi.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		vi.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		vi.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		vi.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		vi.format = format.format;
		vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		vi.subresourceRange.baseArrayLayer = 0;
		vi.subresourceRange.baseMipLevel = 0;
		vi.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		vi.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;

		res = vkCreateImageView(v->Device, &vi, nullptr, &v->SwapchainViews[i]);

		VkFenceCreateInfo fi;
		fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fi.pNext = nullptr;
		fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		res = vkCreateFence(v->Device, &fi, nullptr, &v->PresentFence[i]);
	}

	Texture *depthStencil = CreateStaticTexture2D(v, nullptr, 1, v->Width, v->Height, TexDepth32);
	for (uint32_t i = 0; i < v->NumSwapchainImages; i++)
	{
	}

	VkCommandPoolCreateInfo oi;
	oi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	oi.pNext = nullptr;
	oi.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	oi.queueFamilyIndex = family;

	res = vkCreateCommandPool(v->Device, &oi, nullptr, &v->CommandPool);

	VkCommandBufferAllocateInfo ai;
	ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	ai.pNext = nullptr;

	ai.commandBufferCount = v->NumSwapchainImages;
	ai.commandPool = v->CommandPool;
	ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	vkAllocateCommandBuffers(v->Device, &ai, v->CommandBuffers);

	v->FrameIndex = 0;

	return v;
}

static const VkFormat VFormats[] =
{
	VK_FORMAT_R8G8B8A8_UNORM,
	VK_FORMAT_D32_SFLOAT,
	VK_FORMAT_D24_UNORM_S8_UINT,
};

static const bool VDepthFormat[] =
{
	false,
	true,
	true,
};

Texture *CreateStaticTexture2D(Device *d, const void **data, uint32_t levels, uint32_t width, uint32_t height, TexFormat format, uint32_t flags)
{
	Texture *tex = new Texture();

	uint32_t usage = 0;

	if (!(flags & TexFlagsNoSample))
		usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

	if (flags & TexFlagsRenderTarget)
	{
		if (VDepthFormat[format])
			usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		else
			usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}

	VkImageCreateInfo ii;
	ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ii.pNext = nullptr;
	ii.flags = 0;
	ii.format = VFormats[format];
	ii.extent.width = width;
	ii.extent.height = height;
	ii.extent.depth = 1;
	ii.mipLevels = levels;
	ii.arrayLayers = 1;
	ii.samples = VK_SAMPLE_COUNT_1_BIT;
	ii.tiling = VK_IMAGE_TILING_OPTIMAL;
	ii.usage = usage;
	ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ii.queueFamilyIndexCount = 0;
	ii.pQueueFamilyIndices = nullptr;
	ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkResult res = vkCreateImage(d->Device, &ii, nullptr, &tex->Image);

	VkImageViewCreateInfo vi;
	vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vi.pNext = nullptr;
	vi.flags = 0;
	vi.image = tex->Image;
	vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vi.format = ii.format;
	vi.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	vi.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	vi.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	vi.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	if (VDepthFormat[format])
		vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	else
		vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vi.subresourceRange.baseMipLevel = 0;
	vi.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	vi.subresourceRange.baseArrayLayer = 0;
	vi.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	res = vkCreateImageView(d->Device, &vi, nullptr, &tex->ImageView);

	VkMemoryRequirements mem;
	vkGetImageMemoryRequirements(device, stagingImage, &mem);

	VkMemoryAllocateInfo ai;
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.pNext = nullptr;
	ai.allocationSize = mem.size;
	ai.memoryTypeIndex = 0;
	res = vkAllocateMemory(d->Device, &ai, nullptr, &tex->Memory);

	res = vkBindImageMemory(d->Device, tex->Image, tex->Memory, 0);

	return tex;
}

void BeginFrame(Devie *d)
{
	vkAcquireNextImageKHR(d->Device, d->Swapchain, 1000ULL*1000ULL*1000ULL, d->SemaImageAvailable, VK_NULL_HANDLE, &d->ImageIndex);

	vkWaitForFences(d->Device, 1, &d->PresentFence[d->ImageIndex], VK_TRUE, 1000ULL*1000ULL*1000ULL);
	vkResetFences(d->Device, 1, &v->PresentFence[d->ImageIndex]);

	VkCommandBufferBeginInfo bi;
	bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bi.pNext = nullptr;
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	bi.pInheritanceInfo = nullptr;

	CommandBuffers *cb = GetFrameCommandBuffer(d);
	res = vkBeginCommandBuffer(cb.CmdBuf, &bi);
	cb->InRenderPass = false;
	cb->NextFramebuffer = NULL;
	cb->NextClear.ClearColor = false;
	cb->NextClear.ClearDepth = false;
	cb->NextClear.ClearStencil = false;
	cb->NextLoadFramebuffer = false;
}

CommandBuffer *GetFrameCommandBuffer(Device *d)
{
	return d->CommandBuffers[d->FrameIndex];
}

void Present(Device *d)
{
	CommandBuffers *cb = GetFrameCommandBuffer(d);
	TryBeginRenderPass(cb);

	if (cb->InRenderPass)
	{
		vkEndRenderPass(cb->CmdBuf);
		cb->InRenderPass = false;
	}
	VkResult res = vkEndCommandBuffer(cb->CmdBuf);

	VkPipelineStageFlags waitPipe[1];
	waitPipe[0] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo si;
	si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	si.pNext = nullptr;
	si.pWaitSemaphores = &d->SemaImageAvailable;
	si.waitSemaphoreCount = 1;
	si.pSignalSemaphores = &d->SemaRenderFinished;
	si.signalSemaphoreCount = 1;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cmdBuf;
	si.pWaitDstStageMask = waitPipe;

	vkQueueSubmit(d->Queue, 1, &si, d->PresentFence[d->ImageIndex]);

	VkPresentInfoKHR pi;
	pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	pi.pNext = nullptr;
	pi.swapchainCount = 1;
	pi.waitSemaphoreCount = 1;
	pi.pWaitSemaphores = &d->SemaRenderFinished;
	pi.pResults = &res;
	pi.pSwapchains = &d->Swapchain;
	pi.pImageIndices = &d->ImageIndex;

	vkQueuePresentKHR(d->Queue, &pi);

	d->FrameIndex++;
}

Framebuffer *CreateFramebuffer(Device *d, Texture **color, uint32_t numColor, Texture *depthStencilTexture, DepthStencilCreateInfo *depthStencilCreate);

void BeginRenderPass(CommandBuffer *cb)
{
	uint32_t flags = 0;
	if (cb->NextClear.ClearColor) flags |= RPF_ClearColor;
	if (cb->NextClear.ClearDepth) flags |= RPF_ClearDepth;
	if (cb->NextClear.ClearStencil) flags |= RPF_ClearStencil;
	if (!cb->NextFramebuffer) flags |= RPF_Present;
	if (cb->NextLoadFramebuffer) flags |= RPF_LoadFramebuffer;

	Framebuffer *fb = cb->NextFramebuffer ? cb->NextFramebuffer : cb->Backbuffer;

	flags |= (uint32_t)fb->Color.size() << RPF_NumColorShift;

	auto it = cb->Dev->RenderPasses.find(flags);

	if (it == cb->Dev->RenderPasses.end())
	{
		VkRenderPass renderPass;

		VkAttachmentDescription attachments[16];
		VkAttachmentDescription *depthStencil = nullptr;
		uint32_t numAtc = 0;
		uint32_t numColor = 0;

		for (uint32_t i = 0; i < fb->Color.size(); i++)
		{
			VkAttachmentDescription *atc = &attachments[numAtc++];
			numColor++;
			atc->flags = 0;
			atc->format = fb->Color[i].Format;

			if (flags & RPF_ClearColor)
				atc->loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			else if (flags & RPF_LoadFramebuffer)
				atc->loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			else
				atc->loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

			atc->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			atc->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atc->finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			atc->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			atc->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			atc->samples = VK_SAMPLE_COUNT_1_BIT;
		}

		if (fb->DepthStencil)
		{
			VkAttachmentDescription *atc = &attachments[numAtc++];

			depthStencil = atc;
			atc->flags = 0;
			atc->format = fb->DepthStencil.Format;

			if (flags & RPF_ClearDepth)
				atc->loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			else if (flags & RPF_LoadFramebuffer)
				atc->loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			else
				atc->loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

			if (flags & RPF_ClearStencil)
				atc->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			else if (flags & RPF_LoadFramebuffer)
				atc->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			else
				atc->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

			atc->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			atc->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			atc->finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			atc->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			atc->samples = VK_SAMPLE_COUNT_1_BIT;
		}

		VkAttachmentReference atr[16];
		for (uint32_t i = 0; i < numColor; i++)
		{
			atr[i].attachment = 0;
			atr[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		VkSubpassDescription sp[1];
		sp[0].flags = 0;
		sp[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		sp[0].colorAttachmentCount = numColor;
		sp[0].pColorAttachments = atr;
		sp[0].inputAttachmentCount = 0;
		sp[0].preserveAttachmentCount = 0;
		sp[0].pResolveAttachments = nullptr;
		sp[0].pDepthStencilAttachment = depthStencil;

		VkSubpassDependency dp[1];
		dp[0].dependencyFlags = 0;
		dp[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dp[0].dstSubpass = 0;
		dp[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dp[0].srcAccessMask = 0;
		dp[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dp[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo pi;
		pi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		pi.pNext = nullptr;
		pi.flags = 0;

		pi.attachmentCount = numAtc;
		pi.pAttachments = atc;
		pi.dependencyCount = ArrayCount(dp);
		pi.pDependencies = dp;
		pi.subpassCount = ArrayCount(sp);
		pi.pSubpasses = sp;

		res = vkCreateRenderPass(cb->Dev->Device, &pi, nullptr, &renderPass);

		it = cb->Dev->RenderPasses.insert(std::make_pair(flags, renderPass));
	}

	VkRenderPass renderPass = it->second;

	auto jt = fb->Framebuffers.find(flags);
	if (jt == fb->Framebuffers.end())
	{
		VkFramebuffer framebuffer;

		VkImageView attachments[16];
		uint32_t numAtc = 0;

		for (uint32_t i = 0; i < fb->Color.size(); i++)
		{
			attachments[numAtc] = fb->Color[i]->ImageView;
		}

		if (fb->DepthStencil)
		{
			attachments[numAtc] = fb->DepthStencil->ImageView;
		}

		VkFramebufferCreateInfo fi;
		fi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fi.pNext = nullptr;
		fi.flags = 0;

		fi.width = fb->Width;
		fi.height = fb->Height;
		fi.layers = 1;

		fi.renderPass = renderPass;
		fi.attachmentCount = numAtc;
		fi.pAttachments = attachmnents;

		res = vkCreateFramebuffer(cb->Dev->Device, &fi, nullptr, &framebuffer);

		jt = fb->Framebuffers.insert(std::make_pair(flags, framebuffer));
	}

	VkFramebuffer framebuffer = jt->value;

	VkClearValue clearValues[16];
	uint32_t numClear = 0;

	if (cb->NextClear.ClearColor || cb->NextClear.ClearDepth || cb->NextClear.ClearStencil)
	{
		ClearInfo *ci = cb->NextClear;
		for (uint32_t i = 0; i < fb->Color.size(); i++)
		{
			VkClearValue *cv = &clearValues[numClear++];
			cv->color.float32[0] = ci->Color[0];
			cv->color.float32[1] = ci->Color[1];
			cv->color.float32[2] = ci->Color[2];
			cv->color.float32[3] = ci->Color[3];
		}

		if (fb->DepthStencil)
		{
			VkClearValue *cv = &clearValues[numClear++];
			cv->depthStencil.depth = ci->Depth;
			cv->depthStencil.stencil = ci->Stencil;
		}
	}

	VkRenderPassBeginInfo pi;
	pi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	pi.pNext = nullptr;

	pi.framebuffer = framebuffer;
	pi.renderPass = renderPass;
	pi.renderArea.offset.x = 0;
	pi.renderArea.offset.y = 0;
	pi.renderArea.extent.width = fb->Width;
	pi.renderArea.extent.height = fb->Height;
	pi.clearValueCount = numClear;
	pi.pClearValues = clearValues;

	vkCmdBeginRenderPass(cmdBuf, &pi, VK_SUBPASS_CONTENTS_INLINE);

	cb->InRenderPass = true;
}

inline void TryBeginRenderPass(CommandBuffer *cb)
{
	if (!cb->InRenderPass)
		BeginRenderPass(cb);
}

void Clear(CommandBuffer *cb, const ClearInfo *ci)
{
	if (cb->InRenderPass)
	{
		VkClearAttachment cla[2];
		uint32_t num = 0;
		if (ci->ClearColor)
		{
			cla[num].clearValue.color.float32[0] = ci->Color[0];
			cla[num].clearValue.color.float32[1] = ci->Color[1];
			cla[num].clearValue.color.float32[2] = ci->Color[2];
			cla[num].clearValue.color.float32[3] = ci->Color[3];
			cla[num].colorAttachment = 0;
			cla[num].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			num++;
		}
		if (ci->ClearDepth || ci->ClearStencil)
		{
			cla[num].clearValue.depthStencil.depth = ci->Depth;
			cla[num].clearValue.depthStencil.stencil = ci->Stencil;
			cla[num].colorAttachment = 0;
			cla[num].aspectMask = 0;
			if (ci->ClearDepth) cla[num].aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
			if (ci->ClearStencil) cla[num].aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
			num++;
		}

		VkClearRect rct[1];
		rct[0].rect.offset.x = 0;
		rct[0].rect.offset.y = 0;
		rct[0].rect.extent.width = v->Width;
		rct[0].rect.extent.height = v->Height;
		rct[0].layerCount = 1;
		rct[0].baseArrayLayer = 0;

		vkCmdClearAttachments(cmdBuf, ArrayCount(cla), cla, ArrayCount(rct), rct);
	}
	else
	{
		cb->NextClear = *ci;
	}
}

void SetFramebuffer(CommandBuffer *cb, Framebuffer *f, bool loadContents)
{
	if (cb->InRenderPass)
	{
		vkEndRenderPass(cb->CmdBuf);
		cb->NextFramebuffer = f;
		cb->NextClear.ClearColor = false;
		cb->NextClear.ClearDepth = false;
		cb->NextClear.ClearStencil = false;
		cb->InRenderPass = false;
		cb->NextLoadFramebuffer = loadContents;
	}
}

