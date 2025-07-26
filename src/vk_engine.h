#pragma once

#include <vk_types.h>
#include <vk_descriptors.h>
#include <vk_pipelines.h>
#include <vk_loader.h>
#include <camera.h>

struct DeletionQueue {
  std::deque<std::function<void()>> deletors;

  void push_function(std::function<void()>&& function) {
    deletors.push_back(function);
  }

  void flush() {
    for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
      (*it)(); // call functors
    }
    deletors.clear();
  }
};

struct FrameData {
  VkCommandPool _commandPool;
  VkCommandBuffer _mainCommandBuffer;

  VkSemaphore _swapchainSemaphore, _renderSemaphore;
  VkFence _renderFence;

  DeletionQueue _deletionQueue;
  DescriptorAllocatorGrowable _frameDescriptors;
};

struct ComputePushConstants {
  glm::vec4 data1;
  glm::vec4 data2;
  glm::vec4 data3;
  glm::vec4 data4;
};

struct ComputeEffect {
  const char* name;

  VkPipeline pipeline;
  VkPipelineLayout layout;

  ComputePushConstants data;
};

struct GPUSceneData {
  glm::mat4 view;
  glm::mat4 proj;
  glm::mat4 viewproj;
  glm::vec4 ambientColor;
  glm::vec4 sunlightDirection;
  glm::vec4 sunlightColor;
};

struct RenderObject {
  uint32_t indexCount;
  uint32_t firstIndex;
  VkBuffer indexBuffer;

  MaterialInstance* material;

  glm::mat4 transform;
  VkDeviceAddress vertexBufferAddress;
};

struct DrawContext {
  std::vector<RenderObject> OpaqueSurfaces;
};

struct GLTFMetallic_Roughness {
  MaterialPipeline opaquePipeline;
  MaterialPipeline transparentPipeline;

  VkDescriptorSetLayout materialLayout;

  struct MaterialConstants {
    glm::vec4 colorFactors;
    glm::vec4 metal_rough_factors;
    // For alignment, bind a uniform buffer that is 256 bytes, so add some padding to meet that size
    glm::vec4 extra[14];
  };

  struct MaterialResources {
    AllocatedImage colorImage;
    VkSampler colorSampler;
    AllocatedImage metalRoughImage;
    VkSampler metalRoughSampler;
    VkBuffer dataBuffer;
    uint32_t dataBufferOffset;
  };

  DescriptorWriter writer;

  void build_pipelines(VulkanEngine* engine);
  void clear_resources(VkDevice device);

  MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources,
                                  DescriptorAllocatorGrowable& descriptorAllocator);
};

struct MeshNode : public Node {
  std::shared_ptr<MeshAsset> mesh;

  virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanEngine {
public:
  bool _isInitialized{false};
  int _frameNumber{0};
  bool stop_rendering{false};
  bool resize_requested;
  VkExtent2D _windowExtent{1700,900};

  DeletionQueue _mainDeletionQueue;
  VmaAllocator _allocator;
  
  VkInstance _instance;
  VkDebugUtilsMessengerEXT _debug_messenger;
  VkPhysicalDevice _chosenGPU;
  VkDevice _device;
  VkSurfaceKHR _surface;

  VkSwapchainKHR _swapchain;
  VkFormat _swapchainImageFormat;

  std::vector<VkImage> _swapchainImages;
  std::vector<VkImageView> _swapchainImageViews;
  VkExtent2D _swapchainExtent;

  // Immediate submit structures
  VkFence _immFence;
  VkCommandBuffer _immCommandBuffer;
  VkCommandPool _immCommandPool;

  struct SDL_Window* _window{nullptr};

  FrameData _frames[FRAME_OVERLAP];
  FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; };

  VkQueue _graphicsQueue;
  uint32_t _graphicsQueueFamily;

  AllocatedImage _drawImage;
  AllocatedImage _depthImage;
  VkExtent2D _drawExtent;
  float renderScale = 1.f;

  AllocatedImage _whiteImage;
  AllocatedImage _blackImage;
  AllocatedImage _grayImage;
  AllocatedImage _errorCheckerboardImage;

  MaterialInstance defaultData;
  GLTFMetallic_Roughness metalRoughMaterial;

  VkSampler _defaultSamplerLinear;
  VkSampler _defaultSamplerNearest;

  DescriptorAllocatorGrowable globalDescriptorAllocator;

  VkDescriptorSet _drawImageDescriptors;
  VkDescriptorSetLayout _drawImageDescriptorLayout;

  GPUSceneData sceneData;
  VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;

  VkDescriptorSetLayout _singleImageDescriptorLayout;

  VkPipeline _gradientPipeline;
  VkPipelineLayout _gradientPipelineLayout;

  VkPipelineLayout _trianglePipelineLayout;
  VkPipeline _trianglePipeline;

  VkPipelineLayout _meshPipelineLayout;
  VkPipeline _meshPipeline;

  std::vector<std::shared_ptr<MeshAsset>> testMeshes;

  std::vector<ComputeEffect> backgroundEffects;
  int currentBackgroundEffect{0};

  DrawContext mainDrawContext;
  std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes;

  Camera mainCamera;

  static VulkanEngine& Get();

  void init();

  void cleanup();

  void draw();
  void draw_background(VkCommandBuffer cmd);
  void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);
  void draw_geometry(VkCommandBuffer cmd);

  void update_scene();

  void run();

  void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

  GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

private:
  void init_vulkan();
  void init_swapchain(); 
  void init_commands(); 
  void init_sync_structures(); 
  void init_descriptors();
  void init_pipelines();
  void init_background_pipelines();
  void init_mesh_pipeline();
  void init_triangle_pipeline();
  void init_imgui();
  void init_default_data();

  void create_swapchain(uint32_t width, uint32_t height);
  void resize_swapchain();
  void destroy_swapchain();

  AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
  AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
  void destroy_image(const AllocatedImage& img);

  AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
  void destroy_buffer(const AllocatedBuffer& buffer);

};
