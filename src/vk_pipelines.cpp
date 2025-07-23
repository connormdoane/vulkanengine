#include <vk_pipelines.h>
#include <fstream>
#include <vk_initializers.h>

bool vkutil::load_shader_module(const char* filePath, VkDevice device, VkShaderModule* outShaderModule)
{
  // Open the file with the cursor at the end
  std::ifstream file(filePath, std::ios::ate | std::ios::binary);

  if (!file.is_open()) return false;

  // Find the size of the file based on the current cursor location
  // Because it's at the end, its location will be equal to the size in bytes
  size_t fileSize = (size_t)file.tellg();

  // Spirv file expects the buffer to be on uint32, so make sure to reserve a vector large enough for the full file
  std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

  file.seekg(0);

  // Load the entire file into the buffer
  file.read((char*)buffer.data(), fileSize);
  file.close();

  // Create the shader module based on the file
  VkShaderModuleCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.pNext = nullptr;

  createInfo.codeSize = buffer.size() * sizeof(uint32_t);
  createInfo.pCode = buffer.data();

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) return false;
  *outShaderModule = shaderModule;
  return true;
}

void PipelineBuilder::clear()
{
  // Clear all of the structs we need back to 0
  _inputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };

  _rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };

  _colorBlendAttachment = {};

  _multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };

  _pipelineLayout = {};

  _depthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };

  _renderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };

  _shaderStages.clear();
}

void PipelineBuilder::set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader)
{
  _shaderStages.clear();

  _shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));
  _shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));
}

void PipelineBuilder::set_input_topology(VkPrimitiveTopology topology)
{
  _inputAssembly.topology = topology;
  // Primitive restart is used for triangle and line strips, which won't be implemented
  _inputAssembly.primitiveRestartEnable = VK_FALSE;
}

void PipelineBuilder::set_polygon_mode(VkPolygonMode mode)
{
  _rasterizer.polygonMode = mode;
  _rasterizer.lineWidth = 1.f;
}

void PipelineBuilder::set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace)
{
  _rasterizer.cullMode = cullMode;
  _rasterizer.frontFace = frontFace;
}

void PipelineBuilder::set_multisampling_none()
{
  _multisampling.sampleShadingEnable = VK_FALSE;

  // multisampling defaulted to no multisampling (1 sample per pixel)
  _multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  _multisampling.minSampleShading = 1.0f;
  _multisampling.pSampleMask = nullptr;

  // No alpha to coverage either
  _multisampling.alphaToCoverageEnable = VK_FALSE;
  _multisampling.alphaToOneEnable = VK_FALSE;
}

void PipelineBuilder::disable_blending()
{
  // Default write mask
  _colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  // no blending
  _colorBlendAttachment.blendEnable = VK_FALSE;
}

void PipelineBuilder::set_color_attachment_format(VkFormat format)
{
  _colorAttachmentformat = format;

  _renderInfo.colorAttachmentCount = 1;
  _renderInfo.pColorAttachmentFormats = &_colorAttachmentformat;
}

void PipelineBuilder::set_depth_format(VkFormat format)
{
  _renderInfo.depthAttachmentFormat = format;
}

void PipelineBuilder::disable_depthtest()
{
  _depthStencil.depthTestEnable = VK_FALSE;
  _depthStencil.depthWriteEnable = VK_FALSE;
  _depthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
  _depthStencil.depthBoundsTestEnable = VK_FALSE;
  _depthStencil.stencilTestEnable = VK_FALSE;
  _depthStencil.front = {};
  _depthStencil.back = {};
  _depthStencil.minDepthBounds = 0.f;
  _depthStencil.maxDepthBounds = 1.f;
}

VkPipeline PipelineBuilder::build_pipeline(VkDevice device)
{
  // Make viewport state from our stored viewport and scissor
  VkPipelineViewportStateCreateInfo viewportState = {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.pNext = nullptr;

  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  // Setup color blending. Right now, just set to "no blend"
  VkPipelineColorBlendStateCreateInfo colorBlending = {};
  colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.pNext = nullptr;

  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &_colorBlendAttachment;

  VkPipelineVertexInputStateCreateInfo _vertexInputInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

  // Use all of the info structs to build the actual pipeline
  VkGraphicsPipelineCreateInfo pipelineInfo = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
  pipelineInfo.pNext = &_renderInfo;

  pipelineInfo.stageCount = (uint32_t)_shaderStages.size();
  pipelineInfo.pStages = _shaderStages.data();
  pipelineInfo.pVertexInputState = &_vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &_inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &_rasterizer;
  pipelineInfo.pMultisampleState = &_multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDepthStencilState = &_depthStencil;
  pipelineInfo.layout = _pipelineLayout;

  VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

  VkPipelineDynamicStateCreateInfo dynamicInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
  dynamicInfo.pDynamicStates = &state[0];
  dynamicInfo.dynamicStateCount = 2;

  pipelineInfo.pDynamicState = &dynamicInfo;

  // Errors on the graphics pipeline creation can be complex, so it's handled better than the usual VK_CHECK
  VkPipeline newPipeline;

  if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
    fmt::println("Failed to create pipeline");
    return VK_NULL_HANDLE;
  } else {
    return newPipeline;
  }
}
