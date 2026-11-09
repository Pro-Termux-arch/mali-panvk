/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Pro-Termux-arch
 * Direct-ICD or stock-loader, bounded compute and raster/readback smoke test.
 * Deliberately refuses software/non-G78 devices. No root or global selection.
 */
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shaders.h"

#define FUNCTIONS(X) \
 X(DestroyInstance) X(EnumeratePhysicalDevices) X(GetPhysicalDeviceProperties2) \
 X(GetPhysicalDeviceFeatures) X(EnumerateDeviceExtensionProperties) \
 X(GetPhysicalDeviceMemoryProperties) X(GetPhysicalDeviceQueueFamilyProperties) \
 X(CreateDevice) X(GetDeviceQueue) X(DestroyDevice) X(CreateBuffer) X(DestroyBuffer) \
 X(GetBufferMemoryRequirements) X(AllocateMemory) X(FreeMemory) X(BindBufferMemory) \
 X(MapMemory) X(UnmapMemory) X(CreateShaderModule) X(DestroyShaderModule) \
 X(CreateDescriptorSetLayout) X(DestroyDescriptorSetLayout) X(CreateDescriptorPool) \
 X(DestroyDescriptorPool) X(AllocateDescriptorSets) X(UpdateDescriptorSets) \
 X(CreatePipelineLayout) X(DestroyPipelineLayout) X(CreateComputePipelines) \
 X(CreateGraphicsPipelines) X(DestroyPipeline) X(CreateCommandPool) X(DestroyCommandPool) \
 X(AllocateCommandBuffers) X(BeginCommandBuffer) X(EndCommandBuffer) \
 X(CmdBindPipeline) X(CmdBindDescriptorSets) X(CmdDispatch) X(CmdPipelineBarrier) \
 X(CreateFence) X(DestroyFence) X(QueueSubmit) X(WaitForFences) \
 X(CreateImage) X(DestroyImage) X(GetImageMemoryRequirements) X(BindImageMemory) \
 X(CreateImageView) X(DestroyImageView) X(CreateRenderPass) X(DestroyRenderPass) \
 X(CreateFramebuffer) X(DestroyFramebuffer) X(CmdBeginRenderPass) X(CmdEndRenderPass) \
 X(CmdDraw) X(CmdCopyImageToBuffer)
#define DECL(n) static PFN_vk##n vk##n;
FUNCTIONS(DECL)
#define CHECK(call) do { VkResult r = (call); if (r != VK_SUCCESS) { \
 fprintf(stderr, "FAIL %s: VkResult=%d line=%d\n", #call, r, __LINE__); exit(1); } } while (0)
#define REQUIRE(cond, msg) do { if (!(cond)) { fprintf(stderr, "FAIL %s\n", msg); exit(1); } } while (0)

static uint32_t
memory_type(VkPhysicalDevice gpu, uint32_t bits, VkMemoryPropertyFlags flags)
{
   VkPhysicalDeviceMemoryProperties p;
   vkGetPhysicalDeviceMemoryProperties(gpu, &p);
   for (uint32_t i = 0; i < p.memoryTypeCount; i++)
      if ((bits & (1u << i)) && (p.memoryTypes[i].propertyFlags & flags) == flags)
         return i;
   REQUIRE(0, "no compatible memory type");
   return 0;
}

static VkDeviceMemory
allocate(VkDevice dev, VkPhysicalDevice gpu, VkMemoryRequirements req, VkMemoryPropertyFlags flags)
{
   VkMemoryAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = req.size, .memoryTypeIndex = memory_type(gpu, req.memoryTypeBits, flags)};
   VkDeviceMemory mem;
   CHECK(vkAllocateMemory(dev, &ai, NULL, &mem));
   return mem;
}

static VkShaderModule
module(VkDevice dev, const uint32_t *code, size_t size)
{
   VkShaderModuleCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = size, .pCode = code};
   VkShaderModule m;
   CHECK(vkCreateShaderModule(dev, &ci, NULL, &m));
   return m;
}

static void
smoke(const char *library, const char *expect, const char *output)
{
   void *lib = dlopen(library, RTLD_NOW | RTLD_LOCAL);
   if (!lib) { fprintf(stderr, "FAIL dlopen: %s\n", dlerror()); exit(1); }
   PFN_vkGetInstanceProcAddr gipa = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vk_icdGetInstanceProcAddr");
   if (!gipa) gipa = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vkGetInstanceProcAddr");
   REQUIRE(gipa, "missing Vulkan entry point");
   PFN_vkEnumerateInstanceVersion version = (PFN_vkEnumerateInstanceVersion)gipa(NULL, "vkEnumerateInstanceVersion");
   uint32_t api = VK_API_VERSION_1_0;
   if (version) CHECK(version(&api));
   printf("entrypoint_api=%u.%u.%u (NOT physical device API)\n", VK_API_VERSION_MAJOR(api),
          VK_API_VERSION_MINOR(api), VK_API_VERSION_PATCH(api));
   PFN_vkCreateInstance create = (PFN_vkCreateInstance)gipa(NULL, "vkCreateInstance");
   REQUIRE(create, "missing vkCreateInstance");
   VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "Exynos2100 isolation smoke", .apiVersion = VK_API_VERSION_1_1};
   VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app};
   VkInstance inst;
   CHECK(create(&ici, NULL, &inst));
#define LOAD(n) vk##n = (PFN_vk##n)gipa(inst, "vk" #n); REQUIRE(vk##n, "missing vk" #n);
   FUNCTIONS(LOAD)
   uint32_t n = 0;
   CHECK(vkEnumeratePhysicalDevices(inst, &n, NULL));
   REQUIRE(n && n < 64, "invalid device count");
   VkPhysicalDevice *gpus = calloc(n, sizeof(*gpus));
   REQUIRE(gpus, "allocation");
   CHECK(vkEnumeratePhysicalDevices(inst, &n, gpus));
   VkPhysicalDevice gpu = VK_NULL_HANDLE;
   VkPhysicalDeviceProperties2 props = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
   for (uint32_t i = 0; i < n; i++) {
      vkGetPhysicalDeviceProperties2(gpus[i], &props);
      if (props.properties.vendorID == 0x13b5 &&
          props.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU &&
          strstr(props.properties.deviceName, "G78")) { gpu = gpus[i]; break; }
   }
   free(gpus);
   REQUIRE(gpu, "no Arm integrated G78; software renderer is not a pass");
   uint32_t ne = 0;
   CHECK(vkEnumerateDeviceExtensionProperties(gpu, NULL, &ne, NULL));
   VkExtensionProperties *exts = calloc(ne ? ne : 1, sizeof(*exts));
   REQUIRE(exts, "extension allocation");
   CHECK(vkEnumerateDeviceExtensionProperties(gpu, NULL, &ne, exts));
   int has_driver = props.properties.apiVersion >= VK_API_VERSION_1_2;
   for (uint32_t i = 0; i < ne; i++) {
      printf("extension=%s\n", exts[i].extensionName);
      has_driver |= !strcmp(exts[i].extensionName, VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME);
   }
   free(exts);
   VkPhysicalDeviceDriverProperties driver = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
   if (has_driver) { props.pNext = &driver; vkGetPhysicalDeviceProperties2(gpu, &props); }
   api = props.properties.apiVersion;
   printf("physical=%s vendor=%04x device=%08x api=%u.%u.%u driver=%s info=%s\n",
          props.properties.deviceName, props.properties.vendorID, props.properties.deviceID,
          VK_API_VERSION_MAJOR(api), VK_API_VERSION_MINOR(api), VK_API_VERSION_PATCH(api),
          driver.driverName, driver.driverInfo);
   if (!strcmp(expect, "panvk")) REQUIRE(driver.driverID == VK_DRIVER_ID_MESA_PANVK, "not PanVK");
   else REQUIRE(driver.driverID == VK_DRIVER_ID_ARM_PROPRIETARY, "not stock Arm driver");
   VkPhysicalDeviceFeatures feats;
   vkGetPhysicalDeviceFeatures(gpu, &feats);
   printf("features logicOp=%u alphaToOne=%u fillModeNonSolid=%u shaderClipDistance=%u geometryShader=%u\n",
          feats.logicOp, feats.alphaToOne, feats.fillModeNonSolid, feats.shaderClipDistance, feats.geometryShader);
   uint32_t nq = 0;
   vkGetPhysicalDeviceQueueFamilyProperties(gpu, &nq, NULL);
   VkQueueFamilyProperties *queues = calloc(nq, sizeof(*queues));
   REQUIRE(queues, "queue allocation");
   vkGetPhysicalDeviceQueueFamilyProperties(gpu, &nq, queues);
   uint32_t family = UINT32_MAX;
   for (uint32_t i = 0; i < nq; i++)
      if (queues[i].queueCount && (queues[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) ==
          (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) { family = i; break; }
   free(queues);
   REQUIRE(family != UINT32_MAX, "no graphics+compute queue");
   float priority = 1;
   VkDeviceQueueCreateInfo qci = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = family, .queueCount = 1, .pQueuePriorities = &priority};
   VkDeviceCreateInfo dci = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = 1, .pQueueCreateInfos = &qci};
   VkDevice dev;
   CHECK(vkCreateDevice(gpu, &dci, NULL, &dev));
   VkQueue queue;
   vkGetDeviceQueue(dev, family, 0, &queue);
   VkBufferCreateInfo bci = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = 64*64*4,
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT};
   VkBuffer buffer;
   CHECK(vkCreateBuffer(dev, &bci, NULL, &buffer));
   VkMemoryRequirements req;
   vkGetBufferMemoryRequirements(dev, buffer, &req);
   VkDeviceMemory bmem = allocate(dev, gpu, req, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
   CHECK(vkBindBufferMemory(dev, buffer, bmem, 0));
   void *map;
   CHECK(vkMapMemory(dev, bmem, 0, bci.size, 0, &map));
   memset(map, 0, bci.size);
   VkDescriptorSetLayoutBinding binding = {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT};
   VkDescriptorSetLayoutCreateInfo slci = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1, .pBindings = &binding};
   VkDescriptorSetLayout sl;
   CHECK(vkCreateDescriptorSetLayout(dev, &slci, NULL, &sl));
   VkDescriptorPoolSize ps = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
   VkDescriptorPoolCreateInfo dpci = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &ps};
   VkDescriptorPool dp;
   CHECK(vkCreateDescriptorPool(dev, &dpci, NULL, &dp));
   VkDescriptorSetAllocateInfo sai = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = dp, .descriptorSetCount = 1, .pSetLayouts = &sl};
   VkDescriptorSet set;
   CHECK(vkAllocateDescriptorSets(dev, &sai, &set));
   VkDescriptorBufferInfo dbi = {buffer, 0, 256*4};
   VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = set, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &dbi};
   vkUpdateDescriptorSets(dev, 1, &write, 0, NULL);
   VkPipelineLayoutCreateInfo plci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1, .pSetLayouts = &sl};
   VkPipelineLayout cl, gl;
   CHECK(vkCreatePipelineLayout(dev, &plci, NULL, &cl));
   plci.setLayoutCount = 0;
   CHECK(vkCreatePipelineLayout(dev, &plci, NULL, &gl));
   VkShaderModule cs = module(dev, compute_spv, sizeof(compute_spv));
   VkShaderModule vs = module(dev, vertex_spv, sizeof(vertex_spv));
   VkShaderModule fs = module(dev, fragment_spv, sizeof(fragment_spv));
   VkComputePipelineCreateInfo cpci = {.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = cs, .pName = "main"}, .layout = cl};
   VkPipeline compute, graphics;
   CHECK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpci, NULL, &compute));
   VkImageCreateInfo imci = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_UNORM, .extent = {64,64,1}, .mipLevels = 1, .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT};
   VkImage image;
   CHECK(vkCreateImage(dev, &imci, NULL, &image));
   vkGetImageMemoryRequirements(dev, image, &req);
   VkDeviceMemory imem = allocate(dev, gpu, req, 0);
   CHECK(vkBindImageMemory(dev, image, imem, 0));
   VkImageViewCreateInfo ivci = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = imci.format,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
   VkImageView view;
   CHECK(vkCreateImageView(dev, &ivci, NULL, &view));
   VkAttachmentDescription att = {.format = imci.format, .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, .finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL};
   VkAttachmentReference ref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
   VkSubpassDescription sub = {.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = 1, .pColorAttachments = &ref};
   VkRenderPassCreateInfo rpci = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = 1, .pAttachments = &att, .subpassCount = 1, .pSubpasses = &sub};
   VkRenderPass rp;
   CHECK(vkCreateRenderPass(dev, &rpci, NULL, &rp));
   VkFramebufferCreateInfo fbci = {.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = rp, .attachmentCount = 1, .pAttachments = &view, .width = 64, .height = 64, .layers = 1};
   VkFramebuffer fb;
   CHECK(vkCreateFramebuffer(dev, &fbci, NULL, &fb));
   VkPipelineShaderStageCreateInfo stages[2] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs, .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fs, .pName = "main"}};
   VkPipelineVertexInputStateCreateInfo vi = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
   VkPipelineInputAssemblyStateCreateInfo ia = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
   VkViewport vp = {0,0,64,64,0,1};
   VkRect2D sc = {{0,0},{64,64}};
   VkPipelineViewportStateCreateInfo vps = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .pViewports = &vp, .scissorCount = 1, .pScissors = &sc};
   VkPipelineRasterizationStateCreateInfo rs = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .polygonMode = VK_POLYGON_MODE_FILL, .lineWidth = 1};
   VkPipelineMultisampleStateCreateInfo ms = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
   VkPipelineColorBlendAttachmentState ba = {.colorWriteMask = 15};
   VkPipelineColorBlendStateCreateInfo cb = {.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1, .pAttachments = &ba};
   VkGraphicsPipelineCreateInfo gpci = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 2, .pStages = stages, .pVertexInputState = &vi, .pInputAssemblyState = &ia,
      .pViewportState = &vps, .pRasterizationState = &rs, .pMultisampleState = &ms,
      .pColorBlendState = &cb, .layout = gl, .renderPass = rp};
   CHECK(vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gpci, NULL, &graphics));
   VkCommandPoolCreateInfo poolci = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .queueFamilyIndex = family};
   VkCommandPool pool;
   CHECK(vkCreateCommandPool(dev, &poolci, NULL, &pool));
   VkCommandBufferAllocateInfo cai = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 2};
   VkCommandBuffer cmds[2];
   CHECK(vkAllocateCommandBuffers(dev, &cai, cmds));
   VkFenceCreateInfo fci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
   VkFence fences[2];
   for (unsigned i = 0; i < 2; i++) CHECK(vkCreateFence(dev, &fci, NULL, &fences[i]));
   VkCommandBufferBeginInfo begin = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
   CHECK(vkBeginCommandBuffer(cmds[0], &begin));
   vkCmdBindPipeline(cmds[0], VK_PIPELINE_BIND_POINT_COMPUTE, compute);
   vkCmdBindDescriptorSets(cmds[0], VK_PIPELINE_BIND_POINT_COMPUTE, cl, 0, 1, &set, 0, NULL);
   vkCmdDispatch(cmds[0], 4, 1, 1);
   VkMemoryBarrier host = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT, .dstAccessMask = VK_ACCESS_HOST_READ_BIT};
   vkCmdPipelineBarrier(cmds[0], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &host, 0, NULL, 0, NULL);
   CHECK(vkEndCommandBuffer(cmds[0]));
   VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmds[0]};
   CHECK(vkQueueSubmit(queue, 1, &submit, fences[0]));
   CHECK(vkWaitForFences(dev, 1, &fences[0], VK_TRUE, 5000000000ULL));
   for (uint32_t i = 0; i < 256; i++) REQUIRE(((uint32_t *)map)[i] == i*3+7, "compute readback mismatch");
   puts("PASS compute: 256/256 shader outputs, coherent allocation/map and fence");
   CHECK(vkBeginCommandBuffer(cmds[1], &begin));
   VkClearValue clear = {.color = {{0,0,1,1}}};
   VkRenderPassBeginInfo rpbi = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, .renderPass = rp,
      .framebuffer = fb, .renderArea = {{0,0},{64,64}}, .clearValueCount = 1, .pClearValues = &clear};
   vkCmdBeginRenderPass(cmds[1], &rpbi, VK_SUBPASS_CONTENTS_INLINE);
   vkCmdBindPipeline(cmds[1], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics);
   vkCmdDraw(cmds[1], 3, 1, 0, 0);
   vkCmdEndRenderPass(cmds[1]);
   VkImageMemoryBarrier ib = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image, .subresourceRange = ivci.subresourceRange};
   vkCmdPipelineBarrier(cmds[1], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &ib);
   VkBufferImageCopy copy = {.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}, .imageExtent = {64,64,1}};
   vkCmdCopyImageToBuffer(cmds[1], image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &copy);
   host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
   vkCmdPipelineBarrier(cmds[1], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &host, 0, NULL, 0, NULL);
   CHECK(vkEndCommandBuffer(cmds[1]));
   submit.pCommandBuffers = &cmds[1];
   CHECK(vkQueueSubmit(queue, 1, &submit, fences[1]));
   CHECK(vkWaitForFences(dev, 1, &fences[1], VK_TRUE, 5000000000ULL));
   uint8_t *px = map;
   unsigned red = 0, blue = 0;
   for (unsigned i = 0; i < 64*64; i++) {
      int r = px[i*4] == 255 && px[i*4+1] == 0 && px[i*4+2] == 0;
      int b = px[i*4] == 0 && px[i*4+1] == 0 && px[i*4+2] == 255;
      REQUIRE((r || b) && px[i*4+3] == 255, "invalid raster color"); red += r; blue += b;
   }
   REQUIRE(red == 1152 && blue == 2944, "triangle coverage mismatch");
   REQUIRE(px[(32*64+32)*4] == 255 && px[2] == 255, "triangle position mismatch");
   FILE *f = fopen(output, "wb");
   REQUIRE(f, "cannot write PPM");
   REQUIRE(fprintf(f, "P6\n64 64\n255\n") > 0, "PPM header write");
   for (unsigned i = 0; i < 4096; i++) REQUIRE(fwrite(px+i*4, 1, 3, f) == 3, "PPM write");
   REQUIRE(fclose(f) == 0, "PPM close");
   printf("PASS vertex/fragment raster: red=%u blue=%u output=%s\n", red, blue, output);
   vkUnmapMemory(dev, bmem);
   for (unsigned i = 0; i < 2; i++) vkDestroyFence(dev, fences[i], NULL);
   vkDestroyCommandPool(dev, pool, NULL);
   vkDestroyPipeline(dev, graphics, NULL); vkDestroyPipeline(dev, compute, NULL);
   vkDestroyShaderModule(dev, cs, NULL); vkDestroyShaderModule(dev, vs, NULL); vkDestroyShaderModule(dev, fs, NULL);
   vkDestroyFramebuffer(dev, fb, NULL); vkDestroyRenderPass(dev, rp, NULL);
   vkDestroyImageView(dev, view, NULL); vkDestroyImage(dev, image, NULL); vkFreeMemory(dev, imem, NULL);
   vkDestroyDescriptorPool(dev, dp, NULL); vkDestroyPipelineLayout(dev, cl, NULL); vkDestroyPipelineLayout(dev, gl, NULL);
   vkDestroyDescriptorSetLayout(dev, sl, NULL); vkDestroyBuffer(dev, buffer, NULL); vkFreeMemory(dev, bmem, NULL);
   vkDestroyDevice(dev, NULL); vkDestroyInstance(inst, NULL);
   REQUIRE(dlclose(lib) == 0, "dlclose");
   puts("PASS clean teardown and dlclose");
}

static unsigned
mali_mappings(void)
{
   FILE *f = fopen("/proc/self/maps", "r");
   REQUIRE(f, "cannot inspect mapping lifecycle");
   char line[1024];
   unsigned count = 0;
   while (fgets(line, sizeof(line), f)) count += strstr(line, "/dev/mali0") != NULL;
   REQUIRE(!ferror(f), "maps read error");
   fclose(f);
   return count;
}

int main(int argc, char **argv)
{
   setvbuf(stdout, NULL, _IONBF, 0);
   if ((argc != 4 && argc != 5) || (strcmp(argv[2], "panvk") && strcmp(argv[2], "stock"))) {
      fprintf(stderr, "usage: %s /absolute/library.so panvk|stock output.ppm [cycles=1..20]\n", argv[0]); return 2;
   }
   char *end = NULL;
   long cycles = argc == 5 ? strtol(argv[4], &end, 10) : 1;
   REQUIRE(cycles > 0 && cycles <= 20 && (!end || !*end), "cycles must be 1..20");
   unsigned before = mali_mappings();
   for (long i = 0; i < cycles; i++) {
      printf("cycle=%ld/%ld\n", i+1, cycles);
      smoke(argv[1], argv[2], argv[3]);
      printf("mali_mappings_after_cycle=%u initial=%u\n", mali_mappings(), before);
   }
   if (!strcmp(argv[2], "panvk"))
      REQUIRE(mali_mappings() == before, "Mali mappings leaked across driver unload");
   return 0;
}
