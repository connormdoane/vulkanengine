#pragma once

#include <vk_types.h>
#include <vk_descriptors.h>

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
};

constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanEngine {
public:
  bool _isInitialized{false};
  int _frameNumber{0};
  bool stop_rendering{false};
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
  VkExtent2D _drawExtent;

  DescriptorAllocator globalDescriptorAllocator;

  VkDescriptorSet _drawImageDescriptors;
  VkDescriptorSetLayout _drawImageDescriptorLayout;

  VkPipeline _gradientPipeline;
  VkPipelineLayout _gradientPipelineLayout;

  static VulkanEngine& Get();

  void init();

  void cleanup();

  void draw();
  void draw_background(VkCommandBuffer cmd);
  void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);

  void run();

  void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

private:
  void init_vulkan();
  void init_swapchain(); 
  void init_commands(); 
  void init_sync_structures(); 
  void init_descriptors();
  void init_pipelines();
  void init_background_pipelines();
  void init_imgui();

  void create_swapchain(uint32_t width, uint32_t height);
  void destroy_swapchain();
};
