/*
** RDoom vkmatrix.nro - Vulkan feature matrix for the Switch / emulators.
**
** Draws one quadrant per feature the UZDoom Vulkan renderer depends on:
**   top-left     vertex buffer, RGBA8 UNORM colour attribute (green expected)
**   top-right    uniform buffer from a descriptor set (blue expected)
**   bottom-left  sampled 64x64 texture uploaded with vkCmdCopyBufferToImage
**                (orange/white checkerboard expected)
**   bottom-right storage buffer read in the fragment shader (4 colour bars)
**   centre       unpackUnorm4x8 of a push constant (byte permute -> PRMT on
**                NAK) (magenta expected)
** Clear colour is dark grey: a quadrant left grey means that feature drew
** nothing. Results are logged to sdmc:/switch/rdoom/logs/vkmatrix.log.
*/

#define VK_USE_PLATFORM_VI_NN 1
#include <switch.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_vi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "matrix_spv.h"
#include "nx_tls_bridge.h"
#include "nx_nvcompat.h"

#define W 1280
#define H 720
#define FRAMES 360

static FILE *g_log;
static void L(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	char b[512];
	vsnprintf(b, sizeof(b), fmt, ap);
	va_end(ap);
	if (g_log) { fprintf(g_log, "%s\n", b); fflush(g_log); }
}
#define CK(x) do { VkResult r_ = (x); if (r_ != VK_SUCCESS) { L("FAIL %s = %d", #x, r_); goto out; } } while (0)

void userAppInit(void) { NX_TlsBridgeInit(); }

static VkDevice dev;
static VkPhysicalDeviceMemoryProperties mp;

static uint32_t memType(uint32_t bits, VkMemoryPropertyFlags want)
{
	for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
		if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want)
			return i;
	return 0;
}

static VkResult makeBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer *b, VkDeviceMemory *m, void **map)
{
	VkBufferCreateInfo bi = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bi.size = size;
	bi.usage = usage;
	VkResult r = vkCreateBuffer(dev, &bi, NULL, b);
	if (r) return r;
	VkMemoryRequirements req;
	vkGetBufferMemoryRequirements(dev, *b, &req);
	VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = memType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	if ((r = vkAllocateMemory(dev, &ai, NULL, m))) return r;
	if ((r = vkBindBufferMemory(dev, *b, *m, 0))) return r;
	return vkMapMemory(dev, *m, 0, size, 0, map);
}

static VkShaderModule mod(const uint32_t *code, size_t size)
{
	VkShaderModuleCreateInfo ci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	ci.codeSize = size;
	ci.pCode = code;
	VkShaderModule m = VK_NULL_HANDLE;
	vkCreateShaderModule(dev, &ci, NULL, &m);
	return m;
}

int main(int argc, char **argv)
{
	(void)argc; (void)argv;
	mkdir("sdmc:/switch/rdoom", 0777);
	mkdir("sdmc:/switch/rdoom/logs", 0777);
	g_log = fopen("sdmc:/switch/rdoom/logs/vkmatrix.log", "w");
	// NAK (Rust) prints compiled shaders to fd 2 with NAK_DEBUG=print
	if (freopen("sdmc:/switch/rdoom/logs/vkmatrix-nak.log", "w", stderr))
	{
		setvbuf(stderr, NULL, _IONBF, 0);
		if (fileno(stderr) != 2)
			dup2(fileno(stderr), 2);
	}
	setenv("NAK_DEBUG", "print", 1);
	NX_NvCompatInit();
	L("vkmatrix start; nv compat: %s", NX_NvCompatStatus());

	VkInstance inst = VK_NULL_HANDLE;
	VkSurfaceKHR surf = VK_NULL_HANDLE;
	VkSwapchainKHR sc = VK_NULL_HANDLE;
	int ok = 0;

	const char *ie[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_NN_VI_SURFACE_EXTENSION_NAME };
	VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app.apiVersion = VK_API_VERSION_1_1;
	VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	ici.pApplicationInfo = &app;
	ici.enabledExtensionCount = 2;
	ici.ppEnabledExtensionNames = ie;
	CK(vkCreateInstance(&ici, NULL, &inst));
	uint32_t n = 1;
	VkPhysicalDevice pd;
	vkEnumeratePhysicalDevices(inst, &n, &pd);
	vkGetPhysicalDeviceMemoryProperties(pd, &mp);
	VkViSurfaceCreateInfoNN si = { VK_STRUCTURE_TYPE_VI_SURFACE_CREATE_INFO_NN };
	si.window = nwindowGetDefault();
	CK(vkCreateViSurfaceNN(inst, &si, NULL, &surf));
	float prio = 1;
	VkDeviceQueueCreateInfo q = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	q.queueCount = 1;
	q.pQueuePriorities = &prio;
	const char *de[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	dci.queueCreateInfoCount = 1;
	dci.pQueueCreateInfos = &q;
	dci.enabledExtensionCount = 1;
	dci.ppEnabledExtensionNames = de;
	CK(vkCreateDevice(pd, &dci, NULL, &dev));
	VkQueue queue;
	vkGetDeviceQueue(dev, 0, 0, &queue);

	VkFormat fmt = VK_FORMAT_R8G8B8A8_UNORM;
	VkSwapchainCreateInfoKHR sci = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	sci.surface = surf;
	sci.minImageCount = 3;
	sci.imageFormat = fmt;
	sci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	sci.imageExtent = (VkExtent2D){ W, H };
	sci.imageArrayLayers = 1;
	sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	sci.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	sci.clipped = VK_TRUE;
	CK(vkCreateSwapchainKHR(dev, &sci, NULL, &sc));
	uint32_t ic = 0;
	vkGetSwapchainImagesKHR(dev, sc, &ic, NULL);
	if (ic > 4) ic = 4;
	VkImage imgs[4];
	vkGetSwapchainImagesKHR(dev, sc, &ic, imgs);

	// render pass + framebuffers
	VkAttachmentDescription att = { 0, fmt, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR };
	VkAttachmentReference ar = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkSubpassDescription sub = { 0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, NULL, 1, &ar };
	VkRenderPassCreateInfo rpci = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL, 0, 1, &att, 1, &sub };
	VkRenderPass rp;
	CK(vkCreateRenderPass(dev, &rpci, NULL, &rp));
	VkImageView views[4];
	VkFramebuffer fbs[4];
	for (uint32_t i = 0; i < ic; i++)
	{
		VkImageViewCreateInfo v = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		v.image = imgs[i];
		v.viewType = VK_IMAGE_VIEW_TYPE_2D;
		v.format = fmt;
		v.subresourceRange = (VkImageSubresourceRange){ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		CK(vkCreateImageView(dev, &v, NULL, &views[i]));
		VkFramebufferCreateInfo f = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL, 0, rp, 1, &views[i], W, H, 1 };
		CK(vkCreateFramebuffer(dev, &f, NULL, &fbs[i]));
	}

	// resources: vertex buffer, UBO, SSBO, texture
	typedef struct { float x, y; uint32_t rgba; } Vtx;
	VkBuffer vb, ub, sb, stage;
	VkDeviceMemory vbm, ubm, sbm, stm;
	void *vmap, *umap, *smap, *tmap;
	CK(makeBuffer(sizeof(Vtx) * 6, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &vb, &vbm, &vmap));
	CK(makeBuffer(256, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &ub, &ubm, &umap));
	CK(makeBuffer(256, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &sb, &sbm, &smap));
	CK(makeBuffer(64 * 64 * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &stage, &stm, &tmap));
	// one full-screen quad in clip space; viewports place it in each quadrant
	Vtx quad[6] = { { -1, -1, 0xff20e020 }, { 1, -1, 0xff20e020 }, { 1, 1, 0xff20e020 },
	                { -1, -1, 0xff20e020 }, { 1, 1, 0xff20e020 }, { -1, 1, 0xff20e020 } };
	memcpy(vmap, quad, sizeof(quad));
	float ucol[4] = { 0.1f, 0.3f, 1.0f, 1.0f };
	memcpy(umap, ucol, sizeof(ucol));
	float scol[16] = { 1, 0, 0, 1,  1, 1, 0, 1,  0, 1, 1, 1,  1, 1, 1, 1 };
	memcpy(smap, scol, sizeof(scol));
	uint32_t *tx = (uint32_t *)tmap;
	for (int y = 0; y < 64; y++)
		for (int x = 0; x < 64; x++)
			tx[y * 64 + x] = ((x / 8 + y / 8) & 1) ? 0xffffffffu : 0xff1060ffu;

	VkImageCreateInfo tci = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	tci.imageType = VK_IMAGE_TYPE_2D;
	tci.format = VK_FORMAT_R8G8B8A8_UNORM;
	tci.extent = (VkExtent3D){ 64, 64, 1 };
	tci.mipLevels = 1;
	tci.arrayLayers = 1;
	tci.samples = VK_SAMPLE_COUNT_1_BIT;
	tci.tiling = VK_IMAGE_TILING_OPTIMAL;
	tci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	VkImage timg;
	CK(vkCreateImage(dev, &tci, NULL, &timg));
	VkMemoryRequirements treq;
	vkGetImageMemoryRequirements(dev, timg, &treq);
	VkMemoryAllocateInfo tai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, treq.size, memType(treq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };
	VkDeviceMemory tmem;
	CK(vkAllocateMemory(dev, &tai, NULL, &tmem));
	CK(vkBindImageMemory(dev, timg, tmem, 0));
	VkImageViewCreateInfo tvi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	tvi.image = timg;
	tvi.viewType = VK_IMAGE_VIEW_TYPE_2D;
	tvi.format = VK_FORMAT_R8G8B8A8_UNORM;
	tvi.subresourceRange = (VkImageSubresourceRange){ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	VkImageView tview;
	CK(vkCreateImageView(dev, &tvi, NULL, &tview));
	VkImage cimg;
	tci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	CK(vkCreateImage(dev, &tci, NULL, &cimg));
	VkMemoryRequirements creq;
	vkGetImageMemoryRequirements(dev, cimg, &creq);
	VkMemoryAllocateInfo cai2 = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, creq.size, memType(creq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };
	VkDeviceMemory cmem;
	CK(vkAllocateMemory(dev, &cai2, NULL, &cmem));
	CK(vkBindImageMemory(dev, cimg, cmem, 0));
	VkImageViewCreateInfo cvi = tvi;
	cvi.image = cimg;
	VkImageView cview;
	CK(vkCreateImageView(dev, &cvi, NULL, &cview));
	VkImageCreateInfo lci = tci;
	lci.tiling = VK_IMAGE_TILING_LINEAR;
	lci.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	lci.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	VkImage limg;
	CK(vkCreateImage(dev, &lci, NULL, &limg));
	VkMemoryRequirements lreq;
	vkGetImageMemoryRequirements(dev, limg, &lreq);
	VkMemoryAllocateInfo lai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, lreq.size,
		memType(lreq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) };
	VkDeviceMemory lmem;
	CK(vkAllocateMemory(dev, &lai, NULL, &lmem));
	CK(vkBindImageMemory(dev, limg, lmem, 0));
	{
		VkImageSubresource sr = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
		VkSubresourceLayout lay;
		vkGetImageSubresourceLayout(dev, limg, &sr, &lay);
		uint8_t *lp;
		CK(vkMapMemory(dev, lmem, 0, VK_WHOLE_SIZE, 0, (void **)&lp));
		for (int y = 0; y < 64; y++)
			for (int x = 0; x < 64; x++)
				((uint32_t *)(lp + lay.offset + y * lay.rowPitch))[x] = ((x / 8 + y / 8) & 1) ? 0xff00ff00u : 0xffff8000u;
		L("linear image rowPitch %llu offset %llu", (unsigned long long)lay.rowPitch, (unsigned long long)lay.offset);
	}
	VkImage bimg;
	VkImageCreateInfo bci = tci;
	bci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	CK(vkCreateImage(dev, &bci, NULL, &bimg));
	VkMemoryRequirements breq;
	vkGetImageMemoryRequirements(dev, bimg, &breq);
	VkMemoryAllocateInfo bai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, breq.size, memType(breq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };
	VkDeviceMemory bmem;
	CK(vkAllocateMemory(dev, &bai, NULL, &bmem));
	CK(vkBindImageMemory(dev, bimg, bmem, 0));
	VkImageViewCreateInfo bvi = tvi;
	bvi.image = bimg;
	VkImageView bview;
	CK(vkCreateImageView(dev, &bvi, NULL, &bview));
	VkSamplerCreateInfo smi = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	smi.magFilter = smi.minFilter = VK_FILTER_NEAREST;
	smi.addressModeU = smi.addressModeV = smi.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	VkSampler samp;
	CK(vkCreateSampler(dev, &smi, NULL, &samp));

	// descriptors
	VkDescriptorSetLayoutBinding binds[5] = {
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL },
		{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL },
		{ 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL },
		{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL },
		{ 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL },
	};
	VkDescriptorSetLayoutCreateInfo dl = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, 5, binds };
	VkDescriptorSetLayout dsl;
	CK(vkCreateDescriptorSetLayout(dev, &dl, NULL, &dsl));
	VkDescriptorPoolSize ps[3] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }, { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 }, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 } };
	VkDescriptorPoolCreateInfo dp = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, NULL, 0, 1, 3, ps };
	VkDescriptorPool pool;
	CK(vkCreateDescriptorPool(dev, &dp, NULL, &pool));
	VkDescriptorSetAllocateInfo dsa = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, NULL, pool, 1, &dsl };
	VkDescriptorSet set;
	CK(vkAllocateDescriptorSets(dev, &dsa, &set));
	VkDescriptorBufferInfo ubi = { ub, 0, 64 }, sbi = { sb, 0, 64 };
	VkDescriptorImageInfo ii = { samp, tview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	VkDescriptorImageInfo ci = { samp, cview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	VkDescriptorImageInfo bi2 = { samp, bview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	VkWriteDescriptorSet wr[5] = {
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, set, 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NULL, &ubi, NULL },
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, set, 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &ii, NULL, NULL },
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, set, 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &sbi, NULL },
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, set, 3, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &ci, NULL, NULL },
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, set, 4, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &bi2, NULL, NULL },
	};
	vkUpdateDescriptorSets(dev, 5, wr, 0, NULL);
	VkPushConstantRange pcr = { VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4 };
	VkPipelineLayoutCreateInfo plci = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, NULL, 0, 1, &dsl, 1, &pcr };
	VkPipelineLayout pl;
	CK(vkCreatePipelineLayout(dev, &plci, NULL, &pl));

	// pipelines: one per fragment shader, shared vertex shader
	VkShaderModule vs = mod(m_vbuf_vert_spv, sizeof(m_vbuf_vert_spv));
#define NP 8
	const uint32_t *fcode[NP] = { m_color_frag_spv, m_ubo_frag_spv, m_tex_frag_spv, m_ssbo_frag_spv, m_unpack_frag_spv, m_texfetch_frag_spv, m_texclear_frag_spv, m_texblit_frag_spv };
	size_t fsize[NP] = { sizeof(m_color_frag_spv), sizeof(m_ubo_frag_spv), sizeof(m_tex_frag_spv), sizeof(m_ssbo_frag_spv), sizeof(m_unpack_frag_spv), sizeof(m_texfetch_frag_spv), sizeof(m_texclear_frag_spv), sizeof(m_texblit_frag_spv) };
	const char *names[NP] = { "vertex buffer + RGBA8 attribute", "UBO (descriptor set)", "uploaded texture, texture()", "SSBO in fragment shader", "unpackUnorm4x8 (PRMT)", "uploaded texture, texelFetch()", "cleared texture, texture()", "linear image + vkCmdBlitImage, texture()" };
	VkPipeline pipes[NP] = { 0 };
	VkVertexInputBindingDescription vbd = { 0, sizeof(Vtx), VK_VERTEX_INPUT_RATE_VERTEX };
	VkVertexInputAttributeDescription vad[2] = { { 0, 0, VK_FORMAT_R32G32_SFLOAT, 0 }, { 1, 0, VK_FORMAT_R8G8B8A8_UNORM, 8 } };
	VkPipelineVertexInputStateCreateInfo vi = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL, 0, 1, &vbd, 2, vad };
	VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, NULL, 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
	VkPipelineViewportStateCreateInfo vps = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, NULL, 0, 1, NULL, 1, NULL };
	VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rs.lineWidth = 1;
	VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineColorBlendAttachmentState cba = { 0 };
	cba.colorWriteMask = 0xf;
	VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	cb.attachmentCount = 1;
	cb.pAttachments = &cba;
	VkDynamicState dyn[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo ds = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, NULL, 0, 2, dyn };
	for (int i = 0; i < NP; i++)
	{
		L("--- compiling pipeline %d (%s); NAK output follows in vkmatrix-nak.log", i, names[i]);
		fprintf(stderr, "\n===== pipeline %d: %s =====\n", i, names[i]);
		VkShaderModule fs = mod(fcode[i], fsize[i]);
		VkPipelineShaderStageCreateInfo st[2] = {
			{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_VERTEX_BIT, vs, "main", NULL },
			{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main", NULL } };
		VkGraphicsPipelineCreateInfo g = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
		g.stageCount = 2; g.pStages = st; g.pVertexInputState = &vi; g.pInputAssemblyState = &ia;
		g.pViewportState = &vps; g.pRasterizationState = &rs; g.pMultisampleState = &ms; g.pColorBlendState = &cb;
		g.pDynamicState = &ds; g.layout = pl; g.renderPass = rp;
		VkResult r = vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &g, NULL, &pipes[i]);
		L("pipeline %d (%s): %d", i, names[i], r);
	}

	VkCommandPoolCreateInfo cpi = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, 0 };
	VkCommandPool cp;
	CK(vkCreateCommandPool(dev, &cpi, NULL, &cp));
	VkCommandBuffer cmd;
	VkCommandBufferAllocateInfo cai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL, cp, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
	CK(vkAllocateCommandBuffers(dev, &cai, &cmd));
	VkFence fence;
	VkFenceCreateInfo fci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	CK(vkCreateFence(dev, &fci, NULL, &fence));
	VkSemaphore acq, done;
	VkSemaphoreCreateInfo sci2 = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	vkCreateSemaphore(dev, &sci2, NULL, &acq);
	vkCreateSemaphore(dev, &sci2, NULL, &done);

	// texture upload
	{
		VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		vkBeginCommandBuffer(cmd, &bi);
		VkImageMemoryBarrier b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		b.image = timg;
		b.subresourceRange = tvi.subresourceRange;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &b);
		VkBufferImageCopy reg = { 0 };
		reg.imageSubresource = (VkImageSubresourceLayers){ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		reg.imageExtent = (VkExtent3D){ 64, 64, 1 };
		vkCmdCopyBufferToImage(cmd, stage, timg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &reg);
		VkImageMemoryBarrier cb2 = b;
		cb2.image = cimg;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &cb2);
		VkClearColorValue orange = { { 1.0f, 0.5f, 0.0f, 1.0f } };
		vkCmdClearColorImage(cmd, cimg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &orange, 1, &tvi.subresourceRange);
		cb2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		cb2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		cb2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		cb2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &cb2);
		// linear (CPU-written) -> optimal through the 3D engine
		VkImageMemoryBarrier lb[2] = { b, b };
		lb[0].image = limg; lb[0].srcAccessMask = VK_ACCESS_HOST_WRITE_BIT; lb[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		lb[0].oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED; lb[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		lb[1].image = bimg; lb[1].srcAccessMask = 0; lb[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		lb[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; lb[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 2, lb);
		VkImageBlit blit = { { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, { { 0, 0, 0 }, { 64, 64, 1 } },
		                     { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, { { 0, 0, 0 }, { 64, 64, 1 } } };
		vkCmdBlitImage(cmd, limg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, bimg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
		lb[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; lb[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		lb[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; lb[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &lb[1]);
		b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &b);
		vkEndCommandBuffer(cmd);
		VkSubmitInfo s = { VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL, 0, NULL, NULL, 1, &cmd };
		CK(vkQueueSubmit(queue, 1, &s, fence));
		vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX);
		vkResetFences(dev, 1, &fence);
		L("texture uploaded");
	}

	PadState pad;
	padConfigureInput(1, HidNpadStyleSet_NpadStandard);
	padInitializeDefault(&pad);
	uint32_t presented = 0;
	for (int f = 0; f < FRAMES && appletMainLoop(); f++)
	{
		padUpdate(&pad);
		if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
		uint32_t idx;
		if (vkAcquireNextImageKHR(dev, sc, UINT64_MAX, acq, VK_NULL_HANDLE, &idx) != VK_SUCCESS) { L("acquire failed"); break; }
		VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		vkResetCommandBuffer(cmd, 0);
		vkBeginCommandBuffer(cmd, &bi);
		VkClearValue clr = { .color = { { 0.12f, 0.12f, 0.12f, 1 } } };
		VkRenderPassBeginInfo rb = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL, rp, fbs[idx], { { 0, 0 }, { W, H } }, 1, &clr };
		vkCmdBeginRenderPass(cmd, &rb, VK_SUBPASS_CONTENTS_INLINE);
		VkDeviceSize off = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &off);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pl, 0, 1, &set, 0, NULL);
		uint32_t packed = 0xffff00ffu;  // RGBA bytes ff 00 ff ff -> magenta
		vkCmdPushConstants(cmd, pl, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4, &packed);
		// bottom-left quadrant split: uploaded+texture() | uploaded+texelFetch() | cleared+texture()
		const VkRect2D rects[NP] = { { { 16, 16 }, { 608, 328 } }, { { 656, 16 }, { 608, 328 } }, { { 16, 376 }, { 192, 328 } },
		                             { { 656, 376 }, { 608, 328 } }, { { 540, 300 }, { 200, 60 } }, { { 224, 376 }, { 192, 328 } },
		                             { { 432, 376 }, { 192, 328 } }, { { 1080, 40 }, { 160, 120 } } };
		for (int i = 0; i < NP; i++)
		{
			if (!pipes[i]) continue;
			VkViewport vp = { (float)rects[i].offset.x, (float)rects[i].offset.y, (float)rects[i].extent.width, (float)rects[i].extent.height, 0, 1 };
			vkCmdSetViewport(cmd, 0, 1, &vp);
			vkCmdSetScissor(cmd, 0, 1, &rects[i]);
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipes[i]);
			vkCmdDraw(cmd, 6, 1, 0, 0);
		}
		vkCmdEndRenderPass(cmd);
		vkEndCommandBuffer(cmd);
		VkPipelineStageFlags ws = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo s = { VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL, 1, &acq, &ws, 1, &cmd, 1, &done };
		if (vkQueueSubmit(queue, 1, &s, fence) != VK_SUCCESS) { L("submit failed"); break; }
		VkPresentInfoKHR pi = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, NULL, 1, &done, 1, &sc, &idx, NULL };
		vkQueuePresentKHR(queue, &pi);
		vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX);
		vkResetFences(dev, 1, &fence);
		presented++;
	}
	L("presented %u frames", presented);
	ok = presented > 0;
out:
	if (dev) vkDeviceWaitIdle(dev);
	L("RESULT: %s", ok ? "frames presented (check the screenshot for each quadrant)" : "FAIL");
	if (g_log) fclose(g_log);
	return 0;
}
