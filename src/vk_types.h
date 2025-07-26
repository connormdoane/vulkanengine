#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

struct AllocatedImage {
  VkImage image;
  VkImageView imageView;
  VmaAllocation allocation;
  VkExtent3D imageExtent;
  VkFormat imageFormat;
};

struct AllocatedBuffer {
  VkBuffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo info;
};

// TODO: HIGHLY optimize, will be used a LOT
struct Vertex {
  glm::vec3 position;
  float uv_x;
  glm::vec3 normal;
  float uv_y;
  glm::vec4 color;
};

// Holds resources for a mesh
struct GPUMeshBuffers {
  AllocatedBuffer indexBuffer;
  AllocatedBuffer vertexBuffer;
  VkDeviceAddress vertexBufferAddress;
};

// Push constants for mest object draws
struct GPUDrawPushConstants {
  glm::mat4 worldMatrix;
  VkDeviceAddress vertexBuffer;
};

enum class MaterialPass :uint8_t {
  MainColor,
  Transparent,
  Other
};

struct MaterialPipeline {
  VkPipeline pipeline;
  VkPipelineLayout layout;
};

struct MaterialInstance {
  MaterialPipeline* pipeline;
  VkDescriptorSet materialSet;
  MaterialPass passType;
};

struct DrawContext;

class IRenderable {
  virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
};

// The scene node can hold children and will also keep a transform to propagate to them
struct Node : public IRenderable {
  std::weak_ptr<Node> parent;
  std::vector<std::shared_ptr<Node>> children;

  glm::mat4 localTransform;
  glm::mat4 worldTransform;

  void refreshTransform(const glm::mat4& parentMatrix)
  {
    worldTransform = parentMatrix * localTransform;
    for (auto c : children) {
      c->refreshTransform(worldTransform);
    }
  }

  virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx)
  {
    for (auto& c : children) {
      c->Draw(topMatrix, ctx);
    }
  }
};

#define VK_CHECK(x)                                                    \
  do {                                                                 \
    VkResult err = x;                                                  \
    if (err) {                                                         \
      fmt::println("Detected Vulkan Error: {}", string_VkResult(err)); \
      abort();                                                         \
    }                                                                  \
  } while (0)
