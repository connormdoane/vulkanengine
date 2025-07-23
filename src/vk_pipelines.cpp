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
