#include "vk_engine.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_images.h>
#include <vk_types.h>
#include <vk_pipelines.h>

#include "VkBootstrap.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <chrono>
#include <thread>

constexpr bool bUseValidationLayers = true;

VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }

void VulkanEngine::init()
{
  // Ensure only one engine is created
  assert(loadedEngine == nullptr);
  loadedEngine = this;

  SDL_Init(SDL_INIT_VIDEO);

  SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

  _window = SDL_CreateWindow("Vulkan Engine",
                             SDL_WINDOWPOS_UNDEFINED,
                             SDL_WINDOWPOS_UNDEFINED,
                             _windowExtent.width,
                             _windowExtent.height,
                             window_flags);

  if (!_window) throw std::runtime_error(SDL_GetError());

  init_vulkan();

  init_swapchain();

  init_commands();

  init_sync_structures();

  init_descriptors();

  init_pipelines();

  init_imgui();

  _isInitialized = true;
}

void VulkanEngine::init_vulkan()
{
  // Instance
  vkb::InstanceBuilder builder;

  auto inst_ret = builder.set_app_name("Vulkan Application")
    .request_validation_layers(bUseValidationLayers)
    .use_default_debug_messenger()
    .require_api_version(1, 3, 0)
    .build();

  if (!inst_ret.has_value()) throw std::runtime_error("Failed to create vulkan instance");

  vkb::Instance vkb_inst = inst_ret.value();

  _instance = vkb_inst.instance;
  _debug_messenger = vkb_inst.debug_messenger;

  // Device
  SDL_Vulkan_CreateSurface(_window, _instance, &_surface);
  
	VkPhysicalDeviceVulkan13Features features13 = {};
  features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13.dynamicRendering = true;
	features13.synchronization2 = true;

	VkPhysicalDeviceVulkan12Features features12 = {};
  features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	features12.bufferDeviceAddress = true;
	features12.descriptorIndexing = true;

  vkb::PhysicalDeviceSelector selector{ vkb_inst };
  vkb::PhysicalDevice physicalDevice = selector
    .set_minimum_version(1, 3)
    .set_required_features_13(features13)
    .set_required_features_12(features12)
    .set_surface(_surface)
    .select()
    .value();

  vkb::DeviceBuilder deviceBuilder{ physicalDevice };
  auto dev_ret = deviceBuilder.build();
  if (!dev_ret.has_value()) throw std::runtime_error("Failed to build device");
  vkb::Device vkbDevice = dev_ret.value();

  _device = vkbDevice.device;
  _chosenGPU = physicalDevice.physical_device;

  // Graphics queue
  _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
  _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

  // Memory Allocator
  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.physicalDevice = _chosenGPU;
  allocatorInfo.device = _device;
  allocatorInfo.instance = _instance;
  allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  vmaCreateAllocator(&allocatorInfo, &_allocator);

  _mainDeletionQueue.push_function([&]() {
    vmaDestroyAllocator(_allocator);
  });
}

void VulkanEngine::create_swapchain(uint32_t width, uint32_t height)
{
  vkb::SwapchainBuilder swapchainBuilder{ _chosenGPU, _device, _surface };

  _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

  vkb::Swapchain vkbSwapchain = swapchainBuilder
    .set_desired_format(VkSurfaceFormatKHR{.format = _swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
    .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
    .set_desired_extent(width, height)
    .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
    .build()
    .value();

  _swapchainExtent = vkbSwapchain.extent;

  _swapchain = vkbSwapchain.swapchain;
  _swapchainImages = vkbSwapchain.get_images().value();
  _swapchainImageViews = vkbSwapchain.get_image_views().value();
}

void VulkanEngine::init_swapchain()
{
  create_swapchain(_windowExtent.width, _windowExtent.height);

  // Draw image size that will fit window
  VkExtent3D drawImageExtent = {
    _windowExtent.width,
    _windowExtent.height,
    1
  };

  // Hardcode the draw format to 32 bit float
  _drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
  _drawImage.imageExtent = drawImageExtent;

  VkImageUsageFlags drawImageUsages{};
  drawImageUsages += VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  drawImageUsages += VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  drawImageUsages += VK_IMAGE_USAGE_STORAGE_BIT;
  drawImageUsages += VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  VkImageCreateInfo rimg_info = vkinit::image_create_info(_drawImage.imageFormat, drawImageUsages, drawImageExtent);

  // Allocate for the draw image from GPU local memory
  VmaAllocationCreateInfo rimg_allocinfo = {};
  rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Allocate and create the image
  vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image, &_drawImage.allocation, nullptr);

  // Build an imageview for the draw image to use during rendering
  VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(_drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

  VK_CHECK(vkCreateImageView(_device, &rview_info, nullptr, &_drawImage.imageView));

  // add to deletion queues
  _mainDeletionQueue.push_function([this]() {
    vkDestroyImageView(_device, _drawImage.imageView, nullptr);
    vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
  });
}

void VulkanEngine::init_commands()
{
  VkCommandPoolCreateInfo CommandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily,
                                                                             VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  for (unsigned int i = 0; i < FRAME_OVERLAP; i++) {
    VK_CHECK(vkCreateCommandPool(_device, &CommandPoolInfo, nullptr, &_frames[i]._commandPool));

    // allocate the default command buffer that will be used for rendering
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));
  }

  // imgui command pool
  VK_CHECK(vkCreateCommandPool(_device, &CommandPoolInfo, nullptr, &_immCommandPool));

  // Allocate the command buffer for immediate submits
  VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_immCommandPool, 1);

  VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immCommandBuffer));

  _mainDeletionQueue.push_function([this]() {
    vkDestroyCommandPool(_device, _immCommandPool, nullptr);
  });
}

void VulkanEngine::init_sync_structures()
{
  // the Fence controls when the GPU has finished rendering the frame
  // It's created with the SIGNALED flag so we wait on it from the first frame
  VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
  // two Semaphores synchronize rendering with the swapchain
  VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

  for (unsigned int i = 0; i < FRAME_OVERLAP; i++) {
    VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

    VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore));
    VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore));
  }

  VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));
  _mainDeletionQueue.push_function([this]() { vkDestroyFence(_device, _immFence, nullptr); });
}

void VulkanEngine::init_descriptors()
{
  // Create a descriptor pool that will hold 10 sets with 1 image each
  std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
  {
    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
  };

  globalDescriptorAllocator.init_pool(_device, 10, sizes);

  // Make the descriptor set layout for the compute draw
  {
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    _drawImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
  }

  // Allocate a descriptor set for our draw image
  _drawImageDescriptors = globalDescriptorAllocator.allocate(_device, _drawImageDescriptorLayout);

  VkDescriptorImageInfo imgInfo{};
  imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  imgInfo.imageView = _drawImage.imageView;

  VkWriteDescriptorSet drawImageWrite = {};
  drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  drawImageWrite.pNext = nullptr;

  drawImageWrite.dstBinding = 0;
  drawImageWrite.dstSet = _drawImageDescriptors;
  drawImageWrite.descriptorCount = 1;
  drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  drawImageWrite.pImageInfo = &imgInfo;

  vkUpdateDescriptorSets(_device, 1, &drawImageWrite, 0, nullptr);

  // Make sure both the descriptor allocator and the new layout get cleaned up properly
  _mainDeletionQueue.push_function([&]() {
    globalDescriptorAllocator.destroy_pool(_device);

    vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
  });
}

void VulkanEngine::init_background_pipelines()
{
  VkPipelineLayoutCreateInfo computeLayout{};
  computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  computeLayout.pNext = nullptr;
  computeLayout.pSetLayouts = &_drawImageDescriptorLayout;
  computeLayout.setLayoutCount = 1;

  VkPushConstantRange pushConstant{};
  pushConstant.offset = 0;
  pushConstant.size = sizeof(ComputePushConstants);
  pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  computeLayout.pPushConstantRanges = &pushConstant;
  computeLayout.pushConstantRangeCount = 1;

  VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &_gradientPipelineLayout));

  VkShaderModule gradientShader;
  if (!vkutil::load_shader_module("build/shaders/gradient_color.comp.spv", _device, &gradientShader))
    fmt::print("Error when building the gradient compute shader\n");

  VkShaderModule skyShader;
  if (!vkutil::load_shader_module("build/shaders/sky.comp.spv", _device, &skyShader))
    fmt::print("Error when building the sky compute shader\n");

  VkPipelineShaderStageCreateInfo stageInfo{};
  stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stageInfo.pNext = nullptr;
  stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stageInfo.module = gradientShader;
  stageInfo.pName = "main";

  VkComputePipelineCreateInfo computePipelineCreateInfo{};
  computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  computePipelineCreateInfo.pNext = nullptr;
  computePipelineCreateInfo.layout = _gradientPipelineLayout;
  computePipelineCreateInfo.stage = stageInfo;

  ComputeEffect gradient;
  gradient.layout = _gradientPipelineLayout;
  gradient.name = "gradient";
  gradient.data = {};

  gradient.data.data1 = glm::vec4{1, 0, 0, 1};
  gradient.data.data2 = glm::vec4{0, 0, 1, 1};

  VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradient.pipeline));

  // Change the shader module to create the sky shader
  computePipelineCreateInfo.stage.module = skyShader;

  ComputeEffect sky;
  sky.layout = _gradientPipelineLayout;
  sky.name = "sky";
  sky.data = {};
  sky.data.data1 = glm::vec4{0.1, 0.2, 0.4, 0.97};

  VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky.pipeline));

  // Add these effects into the array
  backgroundEffects.push_back(gradient);
  backgroundEffects.push_back(sky);

  // Now that the pipeline is created, the module isn't needed anymore
  vkDestroyShaderModule(_device, gradientShader, nullptr);
  vkDestroyShaderModule(_device, skyShader, nullptr);

  _mainDeletionQueue.push_function([this, sky, gradient]() {
    vkDestroyPipelineLayout(_device, _gradientPipelineLayout, nullptr);
    vkDestroyPipeline(_device, sky.pipeline, nullptr);
    vkDestroyPipeline(_device, gradient.pipeline, nullptr);
  });
}

void VulkanEngine::init_triangle_pipeline()
{
  VkShaderModule triangleFragShader;
  if (!vkutil::load_shader_module("build/shaders/colored_triangle.frag.spv", _device, &triangleFragShader))
    fmt::print("Error when building the triangle fragment shader module");

  VkShaderModule triangleVertexShader;
  if (!vkutil::load_shader_module("build/shaders/colored_triangle.vert.spv", _device, &triangleVertexShader))
    fmt::print("Error when building the triangle vertex shader module");

  // Build the pipeline layout that controls the inputs/outputs of the shader
  VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
  VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_trianglePipelineLayout));

  PipelineBuilder pipelineBuilder;

  // Use the triangle layout
  pipelineBuilder._pipelineLayout = _trianglePipelineLayout;
  // Connect the vertex and pixel shaders
  pipelineBuilder.set_shaders(triangleVertexShader, triangleFragShader);
  // Draw triangles
  pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  // Fill the triangles
  pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
  // No backface culling
  pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
  // No multisampling
  pipelineBuilder.set_multisampling_none();
  // No blending
  pipelineBuilder.disable_blending();
  // No depth testing
  pipelineBuilder.disable_depthtest();

  // Connect the image format we will draw into from draw image
  pipelineBuilder.set_color_attachment_format(_drawImage.imageFormat);
  pipelineBuilder.set_depth_format(VK_FORMAT_UNDEFINED);

  _trianglePipeline = pipelineBuilder.build_pipeline(_device);

  // Clean structures
  vkDestroyShaderModule(_device, triangleFragShader, nullptr);
  vkDestroyShaderModule(_device, triangleVertexShader, nullptr);

  _mainDeletionQueue.push_function([&]() {
    vkDestroyPipelineLayout(_device, _trianglePipelineLayout, nullptr);
    vkDestroyPipeline(_device, _trianglePipeline, nullptr);
  });
}

void VulkanEngine::init_pipelines()
{
  init_background_pipelines();
  init_triangle_pipeline();
}

void VulkanEngine::init_imgui()
{
  // Adapted from imgui demos
  // 1: Create descriptor pool. The size is overkill, but shouldn't be too much of a worry
  VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
                        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
                        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
                        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
                        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
                        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
                        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
                        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
                        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
                        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
                        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 1000;
  pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
  pool_info.pPoolSizes = pool_sizes;

  VkDescriptorPool imguiPool;
  VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

  // 2: Initialize imgui library
  ImGui::CreateContext();

  ImGui_ImplSDL2_InitForVulkan(_window);

  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = _instance;
  init_info.PhysicalDevice = _chosenGPU;
  init_info.Device = _device;
  init_info.Queue = _graphicsQueue;
  init_info.DescriptorPool = imguiPool;
  init_info.MinImageCount = 3;
  init_info.ImageCount = 3;
  init_info.UseDynamicRendering = true;

  // Dynamic rendering parameters
  init_info.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
  init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_swapchainImageFormat;

  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  ImGui_ImplVulkan_Init(&init_info);

  ImGui_ImplVulkan_CreateFontsTexture();

  _mainDeletionQueue.push_function([this, imguiPool]() {
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(_device, imguiPool, nullptr);
  });
}

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function)
{
  // Send the commands similarly to how we execute GPU commands, but without synchronizing with the swapchain
  VK_CHECK(vkResetFences(_device, 1, &_immFence));
  VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

  VkCommandBuffer cmd = _immCommandBuffer;

  VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  function(cmd);

  VK_CHECK(vkEndCommandBuffer(cmd));

  VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);
  VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, nullptr, nullptr);

  // Submit command buffer to the queue and execute it
  // _renderFence will now block until the graphics commands finish execution
  VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, _immFence));

  VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 9999999999));
}

void VulkanEngine::destroy_swapchain()
{
  vkDestroySwapchainKHR(_device, _swapchain, nullptr);

  // destroy swapchain's resources
  for (long unsigned int i = 0; i < _swapchainImageViews.size(); i++) {
    vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
  }
}

void VulkanEngine::cleanup()
{
  if (!_isInitialized) return;

  // Ensure the GPU is done working
  vkDeviceWaitIdle(_device);

  for (unsigned int i = 0; i < FRAME_OVERLAP; i++) {
    vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);

    vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
    vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
    vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore, nullptr);

    _frames[i]._deletionQueue.flush();
  }

  _mainDeletionQueue.flush();

  destroy_swapchain();

  vkDestroySurfaceKHR(_instance, _surface, nullptr);

  vkDestroyDevice(_device, nullptr);

  vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
  
  vkDestroyInstance(_instance, nullptr);

  SDL_DestroyWindow(_window);

  _isInitialized = false;
  loadedEngine = nullptr;
}

void VulkanEngine::draw_background(VkCommandBuffer cmd)
{
  VkClearColorValue clearValue;
  float flash = std::abs(std::sin(_frameNumber / 120.f));
  clearValue = { {0.0f, 0.0f, flash, 1.0f} };

  VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

  ComputeEffect& effect = backgroundEffects[currentBackgroundEffect];

  // Bind the background compute pipeline
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

  // Bind the descriptor set containing the draw image for the compute pipeline
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout, 0, 1, &_drawImageDescriptors, 0, nullptr);

  ComputePushConstants pc;
  pc.data1 = glm::vec4(1, 0, 0, 1);
  pc.data2 = glm::vec4(0, 0, 1, 1);

  vkCmdPushConstants(cmd, _gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

  // Execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by that size
  vkCmdDispatch(cmd, std::ceil(_drawExtent.width / 16.0), std::ceil(_drawExtent.height / 16.0), 1);
}

void VulkanEngine::draw_geometry(VkCommandBuffer cmd)
{
  VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingInfo renderingInfo = vkinit::rendering_info(_drawExtent, &colorAttachment, nullptr);
  vkCmdBeginRendering(cmd, &renderingInfo);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _trianglePipeline);

  // Set dynamic viewport and scissor
  VkViewport viewport = {};
  viewport.x = 0;
  viewport.y = 0;
  viewport.width = _drawExtent.width;
  viewport.height = _drawExtent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = {};
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent.width = _drawExtent.width;
  scissor.extent.height = _drawExtent.height;

  vkCmdSetScissor(cmd, 0, 1, &scissor);

  // Launch the draw command to draw 3 vertices
  vkCmdDraw(cmd, 3, 1, 0, 0);

  vkCmdEndRendering(cmd);
}

void VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
  VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
  VkRenderingInfo renderInfo = vkinit::rendering_info(_swapchainExtent, &colorAttachment, nullptr);

  vkCmdBeginRendering(cmd, &renderInfo);

  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

  vkCmdEndRendering(cmd);
}

void VulkanEngine::draw()
{
  // Wait for the GPU to signal it's finished drawing the previous frame
  // Timeout of 1 second
  VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true, 1000000000));
  get_current_frame()._deletionQueue.flush();
  VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));

  // Request an image from the swapchain
  uint32_t swapchainImageIndex;
  VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 1000000000, get_current_frame()._swapchainSemaphore, nullptr, &swapchainImageIndex));

  // Commands are done executing, so we can reset it to record again
  VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;
  VK_CHECK(vkResetCommandBuffer(cmd, 0));

  // We'll only use this command buffer once, so set that flag and then begin recording
  VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  _drawExtent.width = _drawImage.imageExtent.width;
  _drawExtent.height = _drawImage.imageExtent.height;

  VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  // Transition our draw image into general layout so we can write into it from the pipeline
  vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

  draw_background(cmd);

  // Transition the draw image into the best format for geometry drawing
  vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  draw_geometry(cmd);

  // Convert the image drawn and the swapchain image into transferrable layouts
  vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  // Copy from the drawn image to the swapchain image
  vkutil::copy_image_to_image(cmd, _drawImage.image, _swapchainImages[swapchainImageIndex], _drawExtent, _swapchainExtent);

  // Set swapchain to Attachment Optimal so we can add to it
  vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  // draw imgui into the swapchain image
  draw_imgui(cmd, _swapchainImageViews[swapchainImageIndex]);

  // Set swapchain image layout to a presentable format
  vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  // Finalize the command buffer, preventing additional commands being added but allowing execution
  VK_CHECK(vkEndCommandBuffer(cmd));

  // Prepare the submission to the queue
  VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);

  // We have to wait on the _presentSemaphore, since that will be signaled when the swapchain is ready
  // We also signal the _renderSemaphore to signal that rendering has finished
  VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                                                 get_current_frame()._swapchainSemaphore);
  VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                                                                   get_current_frame()._renderSemaphore);

  VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, &signalInfo, &waitInfo);

  // Submit the command buffer to the queue and execute it
  // _renderFence will now block again until the graphics commands finish execution
  VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, get_current_frame()._renderFence));

  // Finally, we put the image just rendered into the window
  // We have to wait on the _renderSemaphore for this, since the image needs to be rendered before it can be displayed
  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.pNext = nullptr;
  presentInfo.pSwapchains = &_swapchain;
  presentInfo.swapchainCount = 1;
    
  presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
  presentInfo.waitSemaphoreCount = 1;

  presentInfo.pImageIndices = &swapchainImageIndex;
  
  VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

  // Increase the number of frames drawn
  _frameNumber++;
}

void VulkanEngine::run()
{
  SDL_Event e;
  bool bQuit = false;

  while (!bQuit) {
    // Handle events on queue
    while (SDL_PollEvent(&e) != 0) {
      if (e.type == SDL_QUIT) bQuit = true;
      if (e.type == SDL_WINDOWEVENT) {
        if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) stop_rendering = true;
        if (e.window.event == SDL_WINDOWEVENT_RESTORED) stop_rendering = false;
      }
      if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_ESCAPE) bQuit = true;
        if (e.key.keysym.sym == SDLK_q) bQuit = true;
      }

      // Send SDL event to imgui for handling
      ImGui_ImplSDL2_ProcessEvent(&e);
    }

    if (stop_rendering) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    // imgui new frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // Some imgui UI for selecting shader
    if (ImGui::Begin("background")) {
      ComputeEffect& selected = backgroundEffects[currentBackgroundEffect];

      ImGui::Text("Selected effect: %s", selected.name);

      ImGui::SliderInt("Effect Index", &currentBackgroundEffect, 0, backgroundEffects.size() - 1);

      ImGui::InputFloat4("data1", (float*)& selected.data.data1);
      ImGui::InputFloat4("data2", (float*)& selected.data.data2);
      ImGui::InputFloat4("data3", (float*)& selected.data.data3);
      ImGui::InputFloat4("data4", (float*)& selected.data.data4);
    }
    ImGui::End();

    ImGui::Render();

    draw();
  }
}
