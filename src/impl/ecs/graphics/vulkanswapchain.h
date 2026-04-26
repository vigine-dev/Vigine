#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace vigine
{
namespace ecs
{
namespace graphics
{

class VulkanDevice;

class VulkanSwapchain
{
  public:
    explicit VulkanSwapchain(VulkanDevice &device);
    ~VulkanSwapchain();

    // Create all swapchain resources (images, depth, render pass, framebuffers,
    // command buffers, sync). pushConstantSize = sizeof(PushConstants).
    bool setup(uint32_t width, uint32_t height, uint32_t pushConstantSize);

    // Destroy all swapchain-owned Vulkan resources (calls waitIdle).
    void cleanup();

    // Acquire the next swapchain image for rendering.
    bool acquireImage(uint32_t &imageIndex, bool &requestRecreate);

    // Submit the recorded command buffer and present the image.
    bool present(vk::CommandBuffer cmd, uint32_t imageIndex, bool &requestRecreate);

    // --- Accessors ---
    vk::RenderPass renderPass() const { return _renderPass.get(); }
    vk::PipelineLayout pipelineLayout() const { return _pipelineLayout.get(); }
    vk::Extent2D extent() const { return _swapchainExtent; }
    vk::Format format() const { return _swapchainFormat; }

    const std::vector<vk::Image> &images() const { return _swapchainImages; }
    const std::vector<vk::CommandBuffer> &commandBuffers() const { return _commandBuffers; }
    const std::vector<vk::UniqueFramebuffer> &framebuffers() const
    {
        return _swapchainFramebuffers;
    }
    const std::vector<vk::UniqueImageView> &imageViews() const { return _swapchainImageViews; }

    uint32_t imageCount() const { return static_cast<uint32_t>(_swapchainImages.size()); }
    std::size_t currentFrame() const { return _currentFrame; }
    uint32_t generation() const { return _swapchainGeneration; }
    uint32_t width() const { return _swapchainExtent.width; }
    uint32_t height() const { return _swapchainExtent.height; }
    bool isValid() const { return static_cast<bool>(_swapchain); }

    void advanceFrame() { _currentFrame = (_currentFrame + 1) % kMaxFramesInFlight; }

    bool recreateRequested() const { return _swapchainRecreateRequested; }
    void setRecreateRequested(bool v) { _swapchainRecreateRequested = v; }

    bool imageInitialized(uint32_t index) const
    {
        return index < _imageInitialized.size() && _imageInitialized[index] != 0;
    }
    void markImageInitialized(uint32_t index)
    {
        if (index < _imageInitialized.size())
            _imageInitialized[index] = 1;
    }

    static constexpr std::size_t kMaxFramesInFlight = 2;

  private:
    bool createSwapchainImages(uint32_t width, uint32_t height);
    bool createDepthResources();
    bool createRenderPass();
    bool createFramebuffers();
    bool createCommandResources();
    bool createSyncPrimitives();

    VulkanDevice &_device;
    uint32_t _pushConstantSize{0};

    vk::UniqueSwapchainKHR _swapchain;
    vk::Format _swapchainFormat{vk::Format::eUndefined};
    vk::Format _depthFormat{vk::Format::eUndefined};
    vk::Extent2D _swapchainExtent{};
    std::vector<vk::Image> _swapchainImages;
    std::vector<vk::UniqueImageView> _swapchainImageViews;
    vk::UniqueImage _depthImage;
    vk::UniqueDeviceMemory _depthImageMemory;
    vk::UniqueImageView _depthImageView;
    std::vector<vk::UniqueFramebuffer> _swapchainFramebuffers;
    vk::UniqueRenderPass _renderPass;
    vk::UniquePipelineLayout _pipelineLayout;
    vk::UniqueCommandPool _commandPool;
    std::vector<vk::CommandBuffer> _commandBuffers;
    std::vector<uint8_t> _imageInitialized;
    std::vector<vk::UniqueSemaphore> _imageAvailableSemaphores;
    std::vector<vk::UniqueSemaphore> _renderFinishedSemaphores;
    std::vector<vk::UniqueFence> _inFlightFences;
    std::vector<vk::Fence> _imagesInFlight;
    std::size_t _currentFrame{0};
    uint32_t _swapchainGeneration{0};
    bool _swapchainRecreateRequested{false};
};

} // namespace graphics
} // namespace ecs
} // namespace vigine
