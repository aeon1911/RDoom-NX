/*
** RDoom Vulkan smoke test (vktest.nro)
**
** Proves the Vulkan path RDoom uses, in isolation, before the engine:
**   1. instance with VK_KHR_surface + VK_NN_vi_surface (loaderless Mesa NVK)
**   2. physical device enumeration and identification
**   3. vkCreateViSurfaceNN on the default NWindow
**   4. queue family with present support, device, swapchain (FIFO)
**   5. render pass + graphics pipeline from SPIR-V (runs NAK, the Rust
**      shader compiler inside libvulkan.a, so it also checks the TLS bridge)
**   6. N presented frames with a clear colour and a rotating triangle
**   7. clean teardown in reverse order
**
** Every step is written to sdmc:/switch/rdoom/logs/vktest.log and stdout.
** The program exits on its own after RDOOM_VKTEST_FRAMES frames, or on +.
*/

#define VK_USE_PLATFORM_VI_NN 1
#include <switch.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_vi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

#include "shaders_spv.h"
#include "nx_tls_bridge.h"
#include "nx_nvcompat.h"

#define RDOOM_VKTEST_FRAMES 600
#define WIDTH 1280
#define HEIGHT 720
#define MAX_IMAGES 8

static FILE *logf_;

void vk_logmsg(const char *fmt, ...);
#define logmsg vk_logmsg
void vk_logmsg(const char *fmt, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	u64 ms = armTicksToNs(armGetSystemTick()) / 1000000ULL;
	printf("[%6llu ms] %s\n", (unsigned long long)ms, buf);
	if (logf_)
	{
		fprintf(logf_, "[%6llu ms] %s\n", (unsigned long long)ms, buf);
		fflush(logf_);
	}
}

#define VKCHECK(call) do { VkResult r_ = (call); if (r_ != VK_SUCCESS) { logmsg("FAIL %s -> VkResult %d", #call, (int)r_); goto fail; } else logmsg("ok   %s", #call); } while (0)

void userAppInit(void)
{
	NX_TlsBridgeInit();
}

int main(int argc, char **argv)
{
	(void)argc; (void)argv;
	mkdir("sdmc:/switch", 0777);
	mkdir("sdmc:/switch/rdoom", 0777);
	mkdir("sdmc:/switch/rdoom/logs", 0777);
	logf_ = fopen("sdmc:/switch/rdoom/logs/vktest.log", "w");
	freopen("sdmc:/switch/rdoom/logs/vktest-stderr.log", "w", stderr);
	setvbuf(stderr, NULL, _IONBF, 0);

	logmsg("RDoom vktest start (Mesa NVK, loaderless libvulkan.a)");
	logmsg("tls bridge: TPIDR_EL0 %s libnx thread pointer", NX_TlsBridgeCheck() ? "matches" : "DOES NOT match");

	NX_NvCompatInit();
	logmsg("nv compat: %s", NX_NvCompatStatus());

	padConfigureInput(1, HidNpadStyleSet_NpadStandard);
	PadState pad;
	padInitializeDefault(&pad);

	VkInstance instance = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkRenderPass renderPass = VK_NULL_HANDLE;
	VkPipelineLayout pipeLayout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkShaderModule vs = VK_NULL_HANDLE, fs = VK_NULL_HANDLE;
	VkCommandPool cmdPool = VK_NULL_HANDLE;
	VkCommandBuffer cmd[MAX_IMAGES] = {0};
	VkImageView views[MAX_IMAGES] = {0};
	VkFramebuffer fbs[MAX_IMAGES] = {0};
	VkSemaphore acquireSem = VK_NULL_HANDLE, renderSem[MAX_IMAGES] = {0};
	VkFence fences[MAX_IMAGES] = {0};
	VkImage images[MAX_IMAGES];
	uint32_t imageCount = 0;
	int ok = 0;
	uint32_t framesPresented = 0;

	uint32_t apiVersion = 0;
	VKCHECK(vkEnumerateInstanceVersion(&apiVersion));
	logmsg("instance api version %u.%u.%u", VK_VERSION_MAJOR(apiVersion), VK_VERSION_MINOR(apiVersion), VK_VERSION_PATCH(apiVersion));

	uint32_t extCount = 0;
	vkEnumerateInstanceExtensionProperties(NULL, &extCount, NULL);
	VkExtensionProperties *exts = calloc(extCount, sizeof(*exts));
	vkEnumerateInstanceExtensionProperties(NULL, &extCount, exts);
	for (uint32_t i = 0; i < extCount; i++) logmsg("instance ext: %s", exts[i].extensionName);
	free(exts);

	const char *instExts[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_NN_VI_SURFACE_EXTENSION_NAME };
	VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app.pApplicationName = "RDoom vktest";
	app.pEngineName = "RDoom";
	app.apiVersion = VK_API_VERSION_1_1;
	VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	ici.pApplicationInfo = &app;
	ici.enabledExtensionCount = 2;
	ici.ppEnabledExtensionNames = instExts;
	VKCHECK(vkCreateInstance(&ici, NULL, &instance));

	uint32_t pdCount = 0;
	VKCHECK(vkEnumeratePhysicalDevices(instance, &pdCount, NULL));
	logmsg("physical devices: %u", pdCount);
	if (pdCount == 0) goto fail;
	VkPhysicalDevice pds[4];
	if (pdCount > 4) pdCount = 4;
	vkEnumeratePhysicalDevices(instance, &pdCount, pds);
	VkPhysicalDevice pd = pds[0];
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(pd, &props);
	logmsg("device: %s  vendor 0x%04x  device 0x%04x  api %u.%u.%u  driver 0x%08x  type %d",
		props.deviceName, props.vendorID, props.deviceID,
		VK_VERSION_MAJOR(props.apiVersion), VK_VERSION_MINOR(props.apiVersion), VK_VERSION_PATCH(props.apiVersion),
		props.driverVersion, (int)props.deviceType);
	VkPhysicalDeviceMemoryProperties memProps;
	vkGetPhysicalDeviceMemoryProperties(pd, &memProps);
	for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
	{
		VkMemoryPropertyFlags f = memProps.memoryTypes[i].propertyFlags;
		logmsg("memory type %u: heap %u flags 0x%x%s%s%s%s", i, memProps.memoryTypes[i].heapIndex, f,
			(f & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? " DEVICE_LOCAL" : "",
			(f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ? " HOST_VISIBLE" : "",
			(f & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? " HOST_COHERENT" : "",
			(f & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? " HOST_CACHED" : "");
	}
	for (uint32_t i = 0; i < memProps.memoryHeapCount; i++)
		logmsg("memory heap %u: %llu MiB flags 0x%x", i, (unsigned long long)(memProps.memoryHeaps[i].size >> 20), memProps.memoryHeaps[i].flags);

	VkViSurfaceCreateInfoNN sci = { VK_STRUCTURE_TYPE_VI_SURFACE_CREATE_INFO_NN };
	sci.window = nwindowGetDefault();
	VKCHECK(vkCreateViSurfaceNN(instance, &sci, NULL, &surface));

	uint32_t qfCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(pd, &qfCount, NULL);
	VkQueueFamilyProperties qfp[8];
	if (qfCount > 8) qfCount = 8;
	vkGetPhysicalDeviceQueueFamilyProperties(pd, &qfCount, qfp);
	int qf = -1;
	for (uint32_t i = 0; i < qfCount; i++)
	{
		VkBool32 present = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, surface, &present);
		logmsg("queue family %u: flags 0x%x count %u present %d", i, qfp[i].queueFlags, qfp[i].queueCount, present);
		if (qf < 0 && (qfp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) qf = (int)i;
	}
	if (qf < 0) { logmsg("FAIL no graphics queue with present support"); goto fail; }

	float prio = 1.0f;
	VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	qci.queueFamilyIndex = (uint32_t)qf;
	qci.queueCount = 1;
	qci.pQueuePriorities = &prio;
	const char *devExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	dci.queueCreateInfoCount = 1;
	dci.pQueueCreateInfos = &qci;
	dci.enabledExtensionCount = 1;
	dci.ppEnabledExtensionNames = devExts;
	VKCHECK(vkCreateDevice(pd, &dci, NULL, &device));
	VkQueue queue;
	vkGetDeviceQueue(device, (uint32_t)qf, 0, &queue);

	VkSurfaceCapabilitiesKHR caps;
	VKCHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surface, &caps));
	logmsg("surface caps: images %u..%u extent %ux%u", caps.minImageCount, caps.maxImageCount, caps.currentExtent.width, caps.currentExtent.height);
	uint32_t fmtCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface, &fmtCount, NULL);
	VkSurfaceFormatKHR fmts[16];
	if (fmtCount > 16) fmtCount = 16;
	vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface, &fmtCount, fmts);
	VkSurfaceFormatKHR fmt = fmts[0];
	for (uint32_t i = 0; i < fmtCount; i++)
	{
		logmsg("surface format %u: %d colorspace %d", i, fmts[i].format, fmts[i].colorSpace);
		if (fmts[i].format == VK_FORMAT_B8G8R8A8_UNORM || fmts[i].format == VK_FORMAT_R8G8B8A8_UNORM) fmt = fmts[i];
	}
	uint32_t pmCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(pd, surface, &pmCount, NULL);
	VkPresentModeKHR pms[8];
	if (pmCount > 8) pmCount = 8;
	vkGetPhysicalDeviceSurfacePresentModesKHR(pd, surface, &pmCount, pms);
	for (uint32_t i = 0; i < pmCount; i++) logmsg("present mode %u: %d", i, pms[i]);

	uint32_t wantImages = caps.minImageCount < 3 ? 3 : caps.minImageCount;
	if (caps.maxImageCount && wantImages > caps.maxImageCount) wantImages = caps.maxImageCount;
	VkSwapchainCreateInfoKHR swci = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	swci.surface = surface;
	swci.minImageCount = wantImages;
	swci.imageFormat = fmt.format;
	swci.imageColorSpace = fmt.colorSpace;
	swci.imageExtent.width = WIDTH;
	swci.imageExtent.height = HEIGHT;
	swci.imageArrayLayers = 1;
	swci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swci.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	swci.clipped = VK_TRUE;
	VKCHECK(vkCreateSwapchainKHR(device, &swci, NULL, &swapchain));
	VKCHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, NULL));
	if (imageCount > MAX_IMAGES) imageCount = MAX_IMAGES;
	VKCHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images));
	logmsg("swapchain images: %u format %d", imageCount, fmt.format);

	VkAttachmentDescription att = {0};
	att.format = fmt.format;
	att.samples = VK_SAMPLE_COUNT_1_BIT;
	att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	VkAttachmentReference ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkSubpassDescription sub = {0};
	sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sub.colorAttachmentCount = 1;
	sub.pColorAttachments = &ref;
	VkSubpassDependency dep = {0};
	dep.srcSubpass = VK_SUBPASS_EXTERNAL;
	dep.dstSubpass = 0;
	dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	VkRenderPassCreateInfo rpci = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	rpci.attachmentCount = 1;
	rpci.pAttachments = &att;
	rpci.subpassCount = 1;
	rpci.pSubpasses = &sub;
	rpci.dependencyCount = 1;
	rpci.pDependencies = &dep;
	VKCHECK(vkCreateRenderPass(device, &rpci, NULL, &renderPass));

	for (uint32_t i = 0; i < imageCount; i++)
	{
		VkImageViewCreateInfo ivci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		ivci.image = images[i];
		ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		ivci.format = fmt.format;
		ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		ivci.subresourceRange.levelCount = 1;
		ivci.subresourceRange.layerCount = 1;
		VKCHECK(vkCreateImageView(device, &ivci, NULL, &views[i]));
		VkFramebufferCreateInfo fbci = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		fbci.renderPass = renderPass;
		fbci.attachmentCount = 1;
		fbci.pAttachments = &views[i];
		fbci.width = WIDTH;
		fbci.height = HEIGHT;
		fbci.layers = 1;
		VKCHECK(vkCreateFramebuffer(device, &fbci, NULL, &fbs[i]));
	}

	VkShaderModuleCreateInfo smci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	smci.codeSize = sizeof(tri_vert_spv);
	smci.pCode = tri_vert_spv;
	VKCHECK(vkCreateShaderModule(device, &smci, NULL, &vs));
	smci.codeSize = sizeof(tri_frag_spv);
	smci.pCode = tri_frag_spv;
	VKCHECK(vkCreateShaderModule(device, &smci, NULL, &fs));

	VkPushConstantRange pcr = { VK_SHADER_STAGE_VERTEX_BIT, 0, 16 };
	VkPipelineLayoutCreateInfo plci = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &pcr;
	VKCHECK(vkCreatePipelineLayout(device, &plci, NULL, &pipeLayout));

	VkPipelineShaderStageCreateInfo stages[2] = {
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_VERTEX_BIT, vs, "main", NULL },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main", NULL },
	};
	VkPipelineVertexInputStateCreateInfo vi = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkViewport vp = { 0, 0, WIDTH, HEIGHT, 0, 1 };
	VkRect2D sc = { { 0, 0 }, { WIDTH, HEIGHT } };
	VkPipelineViewportStateCreateInfo vps = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	vps.viewportCount = 1; vps.pViewports = &vp;
	vps.scissorCount = 1; vps.pScissors = &sc;
	VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.lineWidth = 1.0f;
	VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineColorBlendAttachmentState cba = {0};
	cba.colorWriteMask = 0xf;
	VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	cb.attachmentCount = 1; cb.pAttachments = &cba;
	VkGraphicsPipelineCreateInfo gpci = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	gpci.stageCount = 2; gpci.pStages = stages;
	gpci.pVertexInputState = &vi;
	gpci.pInputAssemblyState = &ia;
	gpci.pViewportState = &vps;
	gpci.pRasterizationState = &rs;
	gpci.pMultisampleState = &ms;
	gpci.pColorBlendState = &cb;
	gpci.layout = pipeLayout;
	gpci.renderPass = renderPass;
	u64 t0 = armGetSystemTick();
	VKCHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpci, NULL, &pipeline));
	logmsg("pipeline (NAK shader compile) took %.2f ms", armTicksToNs(armGetSystemTick() - t0) / 1e6);

	VkCommandPoolCreateInfo cpci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	cpci.queueFamilyIndex = (uint32_t)qf;
	VKCHECK(vkCreateCommandPool(device, &cpci, NULL, &cmdPool));
	VkCommandBufferAllocateInfo cbai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cbai.commandPool = cmdPool;
	cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cbai.commandBufferCount = imageCount;
	VKCHECK(vkAllocateCommandBuffers(device, &cbai, cmd));

	VkSemaphoreCreateInfo semci = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	VkFenceCreateInfo fci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	VKCHECK(vkCreateSemaphore(device, &semci, NULL, &acquireSem));
	for (uint32_t i = 0; i < imageCount; i++)
	{
		VKCHECK(vkCreateSemaphore(device, &semci, NULL, &renderSem[i]));
		VKCHECK(vkCreateFence(device, &fci, NULL, &fences[i]));
	}

	logmsg("render loop: %d frames", RDOOM_VKTEST_FRAMES);
	u64 loopStart = armGetSystemTick(), last = loopStart;
	double minMs = 1e9, maxMs = 0;
	for (int frame = 0; frame < RDOOM_VKTEST_FRAMES && appletMainLoop(); frame++)
	{
		padUpdate(&pad);
		if (padGetButtonsDown(&pad) & HidNpadButton_Plus) { logmsg("+ pressed, leaving"); break; }

		uint32_t idx;
		VkResult ar = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, acquireSem, VK_NULL_HANDLE, &idx);
		if (ar != VK_SUCCESS && ar != VK_SUBOPTIMAL_KHR) { logmsg("FAIL vkAcquireNextImageKHR -> %d at frame %d", ar, frame); goto fail; }
		vkWaitForFences(device, 1, &fences[idx], VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &fences[idx]);

		VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		vkResetCommandBuffer(cmd[idx], 0);
		vkBeginCommandBuffer(cmd[idx], &bi);
		float t = frame / 60.0f;
		VkClearValue clear;
		clear.color.float32[0] = 0.08f + 0.06f * sinf(t);
		clear.color.float32[1] = 0.02f;
		clear.color.float32[2] = 0.02f;
		clear.color.float32[3] = 1.0f;
		VkRenderPassBeginInfo rpbi = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		rpbi.renderPass = renderPass;
		rpbi.framebuffer = fbs[idx];
		rpbi.renderArea.extent.width = WIDTH;
		rpbi.renderArea.extent.height = HEIGHT;
		rpbi.clearValueCount = 1;
		rpbi.pClearValues = &clear;
		vkCmdBeginRenderPass(cmd[idx], &rpbi, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(cmd[idx], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		float pc[4] = { t, 0, 0, 0 };
		vkCmdPushConstants(cmd[idx], pipeLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), pc);
		vkCmdDraw(cmd[idx], 3, 1, 0, 0);
		vkCmdEndRenderPass(cmd[idx]);
		vkEndCommandBuffer(cmd[idx]);

		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		si.waitSemaphoreCount = 1; si.pWaitSemaphores = &acquireSem; si.pWaitDstStageMask = &waitStage;
		si.commandBufferCount = 1; si.pCommandBuffers = &cmd[idx];
		si.signalSemaphoreCount = 1; si.pSignalSemaphores = &renderSem[idx];
		VkResult sr = vkQueueSubmit(queue, 1, &si, fences[idx]);
		if (sr != VK_SUCCESS) { logmsg("FAIL vkQueueSubmit -> %d at frame %d", sr, frame); goto fail; }

		VkPresentInfoKHR pi = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		pi.waitSemaphoreCount = 1; pi.pWaitSemaphores = &renderSem[idx];
		pi.swapchainCount = 1; pi.pSwapchains = &swapchain; pi.pImageIndices = &idx;
		VkResult pr = vkQueuePresentKHR(queue, &pi);
		if (pr != VK_SUCCESS && pr != VK_SUBOPTIMAL_KHR) { logmsg("FAIL vkQueuePresentKHR -> %d at frame %d", pr, frame); goto fail; }
		framesPresented++;

		// the acquire semaphore is reused: wait for this frame before the next acquire
		vkWaitForFences(device, 1, &fences[idx], VK_TRUE, UINT64_MAX);
		u64 now = armGetSystemTick();
		double ms = armTicksToNs(now - last) / 1e6;
		last = now;
		if (frame > 2) { if (ms < minMs) minMs = ms; if (ms > maxMs) maxMs = ms; }
		if (frame == 0 || (frame + 1) % 120 == 0) logmsg("frame %d presented (image %u, %.2f ms)", frame + 1, idx, ms);
	}
	double total = armTicksToNs(armGetSystemTick() - loopStart) / 1e9;
	logmsg("presented %u frames in %.2f s: avg %.2f fps, frame time min %.2f ms max %.2f ms",
		framesPresented, total, framesPresented / total, minMs, maxMs);
	ok = framesPresented > 0;

fail:
	if (device) vkDeviceWaitIdle(device);
	for (uint32_t i = 0; i < MAX_IMAGES; i++)
	{
		if (fences[i]) vkDestroyFence(device, fences[i], NULL);
		if (renderSem[i]) vkDestroySemaphore(device, renderSem[i], NULL);
		if (fbs[i]) vkDestroyFramebuffer(device, fbs[i], NULL);
		if (views[i]) vkDestroyImageView(device, views[i], NULL);
	}
	if (acquireSem) vkDestroySemaphore(device, acquireSem, NULL);
	if (cmdPool) vkDestroyCommandPool(device, cmdPool, NULL);
	if (pipeline) vkDestroyPipeline(device, pipeline, NULL);
	if (pipeLayout) vkDestroyPipelineLayout(device, pipeLayout, NULL);
	if (vs) vkDestroyShaderModule(device, vs, NULL);
	if (fs) vkDestroyShaderModule(device, fs, NULL);
	if (renderPass) vkDestroyRenderPass(device, renderPass, NULL);
	if (swapchain) vkDestroySwapchainKHR(device, swapchain, NULL);
	if (device) vkDestroyDevice(device, NULL);
	if (surface) vkDestroySurfaceKHR(instance, surface, NULL);
	if (instance) vkDestroyInstance(instance, NULL);
	logmsg("nv compat: %u fixed VA calls answered locally", NX_NvCompatEmulatedCalls());
	logmsg("teardown complete");
	logmsg("RESULT: %s", ok ? "PASS (Vulkan frames presented)" : "FAIL");
	if (logf_) fclose(logf_);
	return ok ? 0 : 1;
}
