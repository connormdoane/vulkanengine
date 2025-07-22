#pragma once

#include <vector>
#include <vk_types.h>
#include <deque>
#include <span>

struct descriptorLayoutBuilder {
  std::vector<VkDescriptorSetLayoutBinding> bindings;

  void add_binding(uint32_t binding, VkDescriptorType type);
  void clear();
  VkDescriptorSetLayout vuild(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};
