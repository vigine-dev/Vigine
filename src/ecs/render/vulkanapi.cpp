#include "vigine/ecs/render/vulkanapi.h"
#include "vulkandevice.h"
#include "vulkanframerenderer.h"
#include "vulkanpipelinestore.h"
#include "vulkanswapchain.h"
#include "vulkantexturestore.h"
#include "vulkantypes.h"

#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
#include <vulkan/vulkan.hpp>

using namespace vigine::graphics;

// PIMPL container for state that requires the Vulkan SDK (vk::Buffer
// and vk::DeviceMemory tables, handle counter). Keeping these fields
// out of the public header is what lets vulkanapi.h compile without
// `<vulkan/vulkan.hpp>` on the include path.
struct VulkanAPI::Impl
{
    uint64_t nextHandleId{1};
    std::unordered_map<uint64_t, vk::Buffer> bufferHandles;
    std::unordered_map<uint64_t, vk::DeviceMemory> bufferMemoryHandles;
};

VulkanAPI::VulkanAPI()
    : _impl(std::make_unique<Impl>())
{
    _vulkanDevice        = std::make_unique<VulkanDevice>();
    _vulkanSwapchain     = std::make_unique<VulkanSwapchain>(*_vulkanDevice);
    _vulkanTextureStore  = std::make_unique<VulkanTextureStore>(*_vulkanDevice);
    _vulkanPipelineStore = std::make_unique<VulkanPipelineStore>(*_vulkanDevice);
    _vulkanFrameRenderer = std::make_unique<VulkanFrameRenderer>(
        *_vulkanDevice, *_vulkanSwapchain, *_vulkanTextureStore, *_vulkanPipelineStore);
}

VulkanAPI::~VulkanAPI()
{
    cleanupSwapchainResources();

    // Destroy persistent SDF descriptor resources (not swapchain-dependent).
    // These are now owned by VulkanFrameRenderer; reset via its destructor.
    _vulkanFrameRenderer.reset();

    // Entity texture resources (pending uploads, all GPU textures, descriptor resources)
    // are cleaned up by VulkanTextureStore destructor.
    _vulkanTextureStore.reset();

    // Surface and device destruction are handled by VulkanDevice destructor.
}

void VulkanAPI::cleanupSwapchainResources()
{
    // waitIdle is inside VulkanSwapchain::cleanup() — must run before buffer destruction.
    if (_vulkanSwapchain)
        _vulkanSwapchain->cleanup();

    if (_vulkanFrameRenderer)
        _vulkanFrameRenderer->cleanupSwapchainResources();
}

bool VulkanAPI::recreateSwapchainFromSurfaceExtent()
{
    if (!_vulkanDevice || !_vulkanDevice->surface() || !_vulkanDevice->physicalDevice())
        return false;

    auto capabilities =
        _vulkanDevice->physicalDevice().getSurfaceCapabilitiesKHR(_vulkanDevice->surface());
    if (capabilities.currentExtent.width == 0 || capabilities.currentExtent.height == 0)
        return false;

    if (capabilities.currentExtent.width == (std::numeric_limits<uint32_t>::max)() ||
        capabilities.currentExtent.height == (std::numeric_limits<uint32_t>::max)())
    {
        const auto ext = _vulkanSwapchain ? _vulkanSwapchain->extent() : vk::Extent2D{};
        if (ext.width == 0 || ext.height == 0)
            return false;

        return recreateSwapchain(ext.width, ext.height);
    }

    return recreateSwapchain(capabilities.currentExtent.width, capabilities.currentExtent.height);
}

bool VulkanAPI::initializeInstance() { return _vulkanDevice->initializeInstance(); }

bool VulkanAPI::selectPhysicalDevice() { return _vulkanDevice->selectPhysicalDevice(); }

bool VulkanAPI::createLogicalDevice() { return _vulkanDevice->createLogicalDevice(); }

bool VulkanAPI::createSurface(void *nativeWindowHandle)
{
    cleanupSwapchainResources();
    return _vulkanDevice->createSurface(nativeWindowHandle);
}

bool VulkanAPI::createSwapchain(uint32_t width, uint32_t height)
{
    if (!_vulkanDevice || !_vulkanDevice->device() || !_vulkanDevice->surface())
        return false;

    try
    {
        cleanupSwapchainResources();

        if (!_vulkanSwapchain->setup(width, height, sizeof(PushConstants)))
            return false;

        _vulkanTextureStore->initEntityTextureDescriptorResources(sizeof(PushConstants));

        if (!_vulkanFrameRenderer->createSdfPipeline())
            std::cerr << "glyph_sdf shaders not found, SDF text rendering disabled" << std::endl;

        std::cout << "Vulkan swapchain created successfully: " << _vulkanSwapchain->width() << "x"
                  << _vulkanSwapchain->height() << std::endl;
        return true;
    } catch (const std::exception &e)
    {
        std::cerr << "Failed to create swapchain: " << e.what() << std::endl;
        return false;
    }
}

void VulkanAPI::setSdfGlyphData(std::vector<GlyphQuadVertex> vertices, TextureHandle atlasHandle)
{
    if (_vulkanFrameRenderer)
        _vulkanFrameRenderer->setSdfGlyphData(std::move(vertices), atlasHandle);
}

void VulkanAPI::setEntityDrawGroups(std::vector<EntityDrawGroup> groups)
{
    if (_vulkanFrameRenderer)
        _vulkanFrameRenderer->setEntityDrawGroups(std::move(groups));
}

void VulkanAPI::setSdfClipY(float yMin, float yMax)
{
    if (_vulkanFrameRenderer)
        _vulkanFrameRenderer->setSdfClipY(yMin, yMax);
}

uint64_t VulkanAPI::lastRenderedVertexCount() const
{
    return _vulkanFrameRenderer ? _vulkanFrameRenderer->lastRenderedVertexCount() : 0u;
}

void VulkanAPI::createTextureDescriptorSet(TextureHandle handle)
{
    if (_vulkanTextureStore)
        _vulkanTextureStore->createTextureDescriptorSet(handle);
}

bool VulkanAPI::recreateSwapchain(uint32_t width, uint32_t height)
{
    if (!_vulkanDevice || !_vulkanDevice->surface())
        return false;

    return createSwapchain(width, height);
}

bool VulkanAPI::drawFrame(const glm::mat4 &viewProjection)
{
    if (!_vulkanDevice || !_vulkanDevice->device() || !_vulkanSwapchain ||
        !_vulkanSwapchain->isValid())
        return false;

    cleanupCompletedTextureUploads();

    try
    {
        if (_vulkanSwapchain->recreateRequested())
        {
            if (!recreateSwapchainFromSurfaceExtent())
                return false;
            _vulkanSwapchain->setRecreateRequested(false);
        }

        bool requestRecreate = false;
        uint32_t imageIndex  = 0;
        if (!_vulkanSwapchain->acquireImage(imageIndex, requestRecreate))
            return false;

        auto commandBuffer = _vulkanSwapchain->commandBuffers()[imageIndex];
        commandBuffer.reset();

        _vulkanFrameRenderer->recordCommandBuffer(commandBuffer, imageIndex, viewProjection);

        if (!_vulkanSwapchain->present(commandBuffer, imageIndex, requestRecreate))
            return false;

        _vulkanSwapchain->markImageInitialized(imageIndex);
        _vulkanSwapchain->advanceFrame();

        if (requestRecreate)
            _vulkanSwapchain->setRecreateRequested(true);

        return !requestRecreate;

    } catch (const std::exception &e)
    {
        const std::string message = e.what();
        if (message.find("ErrorOutOfDateKHR") != std::string::npos ||
            message.find("SuboptimalKHR") != std::string::npos)
        {
            _vulkanSwapchain->setRecreateRequested(true);
            return false;
        }
        std::cerr << "drawFrame failed: " << e.what() << std::endl;
        return false;
    }
}

bool VulkanAPI::isInitialized() const { return _vulkanDevice && _vulkanDevice->isInitialized(); }

bool VulkanAPI::hasSwapchain() const { return _vulkanSwapchain && _vulkanSwapchain->isValid(); }
uint32_t VulkanAPI::swapchainGeneration() const
{
    return _vulkanSwapchain ? _vulkanSwapchain->generation() : 0u;
}
uint32_t VulkanAPI::swapchainWidth() const
{
    return _vulkanSwapchain ? _vulkanSwapchain->width() : 0u;
}
uint32_t VulkanAPI::swapchainHeight() const
{
    return _vulkanSwapchain ? _vulkanSwapchain->height() : 0u;
}

// GraphicsBackend interface implementation

bool VulkanAPI::initializeDevice(void *nativeWindow)
{
    if (!initializeInstance())
        return false;
    if (!selectPhysicalDevice())
        return false;
    if (!createLogicalDevice())
        return false;
    if (!createSurface(nativeWindow))
        return false;
    return true;
}

bool VulkanAPI::resize(uint32_t width, uint32_t height) { return recreateSwapchain(width, height); }

bool VulkanAPI::beginFrame()
{
    // Frame initialization logic will be extracted from drawFrame() in future phases
    return true;
}

bool VulkanAPI::endFrame()
{
    // Frame presentation logic will be extracted from drawFrame() in future phases
    return true;
}

void VulkanAPI::submitDrawCall(const DrawCallDesc &desc)
{
    // Draw call submission logic will be extracted from drawFrame() in future phases
}

PipelineHandle VulkanAPI::createPipeline(const PipelineDesc &desc)
{
    if (!_vulkanDevice || !_vulkanDevice->device() || !_vulkanSwapchain ||
        !_vulkanSwapchain->isValid() || !_vulkanPipelineStore)
        return {};

    const vk::PipelineLayout texLayout = _vulkanTextureStore
                                             ? _vulkanTextureStore->entityTexturePipelineLayout()
                                             : vk::PipelineLayout{};
    return _vulkanPipelineStore->createPipeline(desc, _vulkanSwapchain->renderPass(),
                                                _vulkanSwapchain->extent(),
                                                _vulkanSwapchain->pipelineLayout(), texLayout);
}

void VulkanAPI::destroyPipeline(PipelineHandle handle)
{
    if (_vulkanPipelineStore)
        _vulkanPipelineStore->destroyPipeline(handle);
}

BufferHandle VulkanAPI::createBuffer(const BufferDesc &desc)
{
    // Buffer creation logic will be implemented when migrating geometry to components
    BufferHandle handle;
    handle.value = _impl->nextHandleId++;
    return handle;
}

void VulkanAPI::uploadBuffer(BufferHandle handle, const void *data, size_t size)
{
    // Buffer upload logic will be implemented when migrating geometry to components
}

void VulkanAPI::destroyBuffer(BufferHandle handle)
{
    auto bufIt = _impl->bufferHandles.find(handle.value);
    auto memIt = _impl->bufferMemoryHandles.find(handle.value);

    if (bufIt != _impl->bufferHandles.end())
    {
        _vulkanDevice->device().destroyBuffer(bufIt->second);
        _impl->bufferHandles.erase(bufIt);
    }

    if (memIt != _impl->bufferMemoryHandles.end())
    {
        _vulkanDevice->device().freeMemory(memIt->second);
        _impl->bufferMemoryHandles.erase(memIt);
    }
}

TextureHandle VulkanAPI::createTexture(const TextureDesc &desc)
{
    return _vulkanTextureStore ? _vulkanTextureStore->createTexture(desc) : TextureHandle{};
}

void VulkanAPI::uploadTexture(TextureHandle handle, const void *pixels, uint32_t width,
                              uint32_t height)
{
    if (_vulkanTextureStore)
        _vulkanTextureStore->uploadTexture(handle, pixels, width, height);
}

void VulkanAPI::destroyTexture(TextureHandle handle)
{
    if (_vulkanTextureStore)
        _vulkanTextureStore->destroyTexture(handle);
}

ShaderModuleHandle VulkanAPI::createShaderModule(const std::vector<char> &spirv)
{
    return _vulkanPipelineStore ? _vulkanPipelineStore->createShaderModule(spirv)
                                : ShaderModuleHandle{};
}

void VulkanAPI::destroyShaderModule(ShaderModuleHandle handle)
{
    if (_vulkanPipelineStore)
        _vulkanPipelineStore->destroyShaderModule(handle);
}

void VulkanAPI::setViewProjection(const glm::mat4 &viewProjection)
{
    _currentViewProjection = viewProjection;
}

void VulkanAPI::setPushConstants(const PushConstantData &data) { _currentPushConstants = data; }

void VulkanAPI::cleanupCompletedTextureUploads()
{
    if (_vulkanTextureStore)
        _vulkanTextureStore->cleanupCompletedTextureUploads();
}
