# Plan: VulkanAPI Decomposition into Helper Classes

## 📋 TL;DR

Розбити 2010-рядковий `VulkanAPI` моноліт на тонкий оркестратор (~250 рядків) + 5 фокусованих хелпер-класів: **VulkanDevice** (~250), **VulkanSwapchain** (~400), **VulkanTextureStore** (~350), **VulkanPipelineStore** (~200), **VulkanFrameRenderer** (~400). Усі хелпери — приватні деталі реалізації в `src/ecs/render/`. RenderSystem не змінюється. Кожен крок компілюється незалежно.

**Передумова:** спочатку розбити 4 залишкові великі методи (>100 рядків) у Phase 1.

---

## 🟡 Phase 1: Розбити 4 залишкові великі методи

*Передумова для екстракції класів. Все залишається в vulkanapi.cpp/h.*

### 1.1. Split `createSdfPipeline` (199 рядків → 3 × ~65)

**Файли для зміни:**
- `src/ecs/render/vulkanapi.cpp` (рядки 672–870)
- `include/vigine/ecs/render/vulkanapi.h` (private секція)

**Кроки:**
1. Витягти `createSdfDescriptorResources()` — рядки 684–724: створення descriptor set layout, pool, set allocation, опціональний atlas descriptor update. Пише: `_sdfDescriptorSetLayout`, `_sdfDescriptorPool`, `_sdfDescriptorSet`. Читає: `_device`, `_sdfAtlasHandle`.
2. Витягти `createSdfGraphicsPipelineObject()` — рядки 750–870: створення shader module, vertex input, rasterizer, blending, depth, pipeline. Пише: `_sdfGlyphPipeline`. Читає: `_device`, `_sdfPipelineLayout`, `_swapchainExtent`, `_renderPass`.
3. `createSdfPipeline()` стає оркестратором: завантажити SPIR-V → `createSdfDescriptorResources()` → створити pipeline layout (рядки 726–748) → `createSdfGraphicsPipelineObject()`.
4. Додати дві нові декларації private методів у `vulkanapi.h`.

**Мембери, що використовуються:**
- `_device`, `_sdfAtlasHandle`, `_sdfDescriptorSetLayout`, `_sdfDescriptorPool`, `_sdfDescriptorSet`, `_sdfPipelineLayout`, `_sdfGlyphPipeline`, `_swapchainExtent`, `_renderPass`

### 1.2. Split `createPipeline` (168 рядків → ~130 + хелпер)

**Файли для зміни:**
- `src/ecs/render/vulkanapi.cpp` (рядки 1452–1619)
- `include/vigine/ecs/render/vulkanapi.h` (private секція)

**Кроки:**
1. Витягти вільну функцію (file-local) `toVkFormat(VertexFormat)` — лямбда на рядках 1491–1506. Перемістити у anonymous namespace на початку файлу.
2. Витягти private метод `buildVertexInputState(const PipelineDesc&, bindings_out, attribs_out)` — рядки 1508–1532: ітерує `desc.vertexLayout`, конвертує у Vulkan binding/attribute descriptions.
3. `createPipeline()` викликає `toVkFormat` (через `buildVertexInputState`) і збирає решту (~130 рядків). Це близько до ліміту 100 рядків — решта коду — послідовна збірка Vulkan pipeline state, яку складно розбити без over-engineering.

### 1.3. Split `uploadTexture` (138 рядків → ~70 + ~70)

**Файли для зміни:**
- `src/ecs/render/vulkanapi.cpp` (рядки 1773–1910)
- `include/vigine/ecs/render/vulkanapi.h` (private секція)

**Кроки:**
1. Витягти `computeTextureDataSize(TextureFormat, uint32_t w, uint32_t h)` — рядки 1783–1804: switch по формату, повертає розмір в байтах. Вільна функція у anonymous namespace.
2. Витягти `recordImageLayoutTransitions(cmd, image, width, height)` — рядки 1848–1896: два pipeline barriers (undefined→transfer, transfer→shaderRead) + buffer-to-image copy. Private метод.
3. `uploadTexture()` стає: validate → compute size → cleanup old uploads → create staging buffer+memory → memcpy → allocate command buffer → `recordImageLayoutTransitions()` → submit with fence → store staging.

### 1.4. Split `createTexture` (112 рядків → ~75)

**Файли для зміни:**
- `src/ecs/render/vulkanapi.cpp` (рядки 1661–1772)

**Кроки:**
1. Витягти вільну функцію `toVkTextureFormat(TextureFormat)` — рядки 1666–1688: format switch. Anonymous namespace.
2. Витягти вільну функцію `toVkSamplerAddressMode(TextureWrapMode)` — рядки 1744–1755: wrap mode switch. Anonymous namespace.
3. `createTexture()` стає: map format → create image → allocate memory → create view → create sampler → store handles. Тіло ~75 рядків (біля цільового ліміту).

**Верифікація Phase 1:** `cmake --build build --target vigine && cmake --build build --target example-window` + запуск `build/bin/example-window.exe`

---

## 🔵 Phase 2: Extract VulkanDevice

*Init-time ресурси без runtime залежностей. Найчистіша межа екстракції.*

### 2.1. Створити `src/ecs/render/vulkandevice.h` + `src/ecs/render/vulkandevice.cpp`

**Клас VulkanDevice:**

**Мембери (переміщені з VulkanAPI):**
- `_instance` (vk::UniqueInstance)
- `_physicalDevice` (vk::PhysicalDevice)
- `_device` (vk::UniqueDevice)
- `_surface` (vk::SurfaceKHR)
- `_graphicsQueue`, `_presentQueue` (vk::Queue)
- `_graphicsQueueFamily`, `_presentQueueFamily` (uint32_t)
- `_validationLayers` (vector), `_enableValidationLayers` (bool)
- `_initialized` (bool)
- `_surfaceFactory` (unique_ptr\<SurfaceFactory\>)

**Методи (переміщені з VulkanAPI):**
- `initializeInstance()` (vulkanapi.cpp рядки 181–224)
- `selectPhysicalDevice()` (рядки 226–262)
- `createLogicalDevice()` (рядки 264–310)
- `createSurface(void*)` (рядки 312–354)
- `findMemoryType(uint32_t, vk::MemoryPropertyFlags)` (рядки 1420–1434)

**Публічні аксесори:**
- `instance()`, `physicalDevice()`, `device()`, `graphicsQueue()`, `presentQueue()`
- `graphicsQueueFamily()`, `presentQueueFamily()`, `isInitialized()`

**Конструктор:** без аргументів, викликає `getPlatformSurfaceFactory()`
**Деструктор:** знищує surface через instance якщо валідний

**Includes:** `<vulkan/vulkan.hpp>`, `"surfacefactory.h"`, platform headers

### 2.2. Додати до CMakeLists.txt

Додати `${SRC_DIR}/ecs/render/vulkandevice.cpp` до `SOURCES`.

### 2.3. Оновити `include/vigine/ecs/render/vulkanapi.h`

- Forward-declare `class VulkanDevice;`
- Додати `#include "vulkandevice.h"` (або forward-declare + include в .cpp)
- Замінити 12 декларацій мемберів на `std::unique_ptr<VulkanDevice> _vulkanDevice;`
- Залишити публічні аксесор-методи (`getInstance()`, `getPhysicalDevice()`, тощо) — вони тепер делегують

### 2.4. Оновити `src/ecs/render/vulkanapi.cpp`

- `#include "ecs/render/vulkandevice.h"`
- Конструктор: `_vulkanDevice = std::make_unique<VulkanDevice>();`
- Деструктор: використовувати `_vulkanDevice->instance()` для cleanup surface
- Замінити: `_instance` → `_vulkanDevice->instance()`, `_device` → `_vulkanDevice->device()`, `_physicalDevice` → `_vulkanDevice->physicalDevice()`, `_graphicsQueue` → `_vulkanDevice->graphicsQueue()`, `_graphicsQueueFamily` → `_vulkanDevice->graphicsQueueFamily()`, `findMemoryType(...)` → `_vulkanDevice->findMemoryType(...)`
- Видалити тіла переміщених методів з vulkanapi.cpp
- Методи VulkanAPI `initializeInstance()`, `selectPhysicalDevice()`, `createLogicalDevice()`, `createSurface()` стають 1-рядковими делегатами

**Залежності:** немає (корінь ланцюга)
**Верифікація:** build + run

---

## 🔵 Phase 3: Extract VulkanSwapchain

*Swapchain lifecycle, render pass, framebuffers, command pool, sync primitives.*
*Може виконуватися паралельно з Phase 4.*

### 3.1. Створити `src/ecs/render/vulkanswapchain.h` + `src/ecs/render/vulkanswapchain.cpp`

**Клас VulkanSwapchain:**

**Мембери (переміщені з VulkanAPI):**
- `_swapchain` (vk::UniqueSwapchainKHR)
- `_swapchainFormat` (vk::Format), `_depthFormat` (vk::Format)
- `_swapchainExtent` (vk::Extent2D)
- `_swapchainImages` (vector\<vk::Image\>)
- `_swapchainImageViews` (vector\<vk::UniqueImageView\>)
- `_depthImage`, `_depthImageMemory`, `_depthImageView`
- `_swapchainFramebuffers` (vector\<vk::UniqueFramebuffer\>)
- `_renderPass` (vk::UniqueRenderPass)
- `_pipelineLayout` (vk::UniquePipelineLayout) — push-constant-only layout
- `_commandPool` (vk::UniqueCommandPool)
- `_commandBuffers` (vector\<vk::CommandBuffer\>)
- `_imageInitialized` (vector\<uint8_t\>)
- `_swapchainGeneration` (uint32_t), `_swapchainRecreateRequested` (bool)
- `_imageAvailableSemaphores`, `_renderFinishedSemaphores` (vector\<vk::UniqueSemaphore\>)
- `_inFlightFences` (vector\<vk::UniqueFence\>), `_imagesInFlight` (vector\<vk::Fence\>)
- `_currentFrame` (size_t), `kMaxFramesInFlight` (static constexpr)
- `VulkanPerImageBuffer` struct (inner struct + ensureCapacity/upload)

**Методи (переміщені з VulkanAPI):**
- `cleanup()` ← `cleanupSwapchainResources()` (vulkanapi.cpp рядки 102–137)
- `createSwapchainImages(w, h)` (рядки 406–494)
- `createDepthResources()` (рядки 496–557)
- `createRenderPass()` (рядки 559–631) — також створює `_pipelineLayout`
- `createFramebuffers()` (рядки 871–893)
- `createCommandResources()` (рядки 895–907)
- `createSyncPrimitives()` (рядки 909–941)
- `acquireSwapchainImage(imageIndex, requestRecreate)` (рядки 1088–1121)
- `submitAndPresent(cmd, imageIndex, requestRecreate)` (рядки 1366–1404)
- `recreateSwapchainFromSurfaceExtent()` (рядки 139–158)

**Публічні аксесори:**
- `renderPass()`, `pipelineLayout()`, `swapchainExtent()`, `swapchainFormat()`
- `swapchainImages()`, `commandBuffers()`, `framebuffers()`
- `imageViews()`, `imageCount()`, `currentFrame()`
- `swapchainGeneration()`, `width()`, `height()`, `isValid()`
- `swapchainRecreateRequested()` / `setSwapchainRecreateRequested(bool)`
- `imageInitialized(index)` / `markImageInitialized(index)`
- `advanceFrame()`

**Конструктор:** приймає `VulkanDevice&` (зберігає як референс для device/surface/queue access)

### 3.2. Додати `vulkanswapchain.cpp` до CMakeLists.txt SOURCES

### 3.3. Оновити `vulkanapi.h`: forward-declare VulkanSwapchain, `std::unique_ptr<VulkanSwapchain> _vulkanSwapchain;`, видалити переміщені мембери

### 3.4. Оновити `vulkanapi.cpp`:
- `#include "ecs/render/vulkanswapchain.h"`
- Створити в конструкторі: `_vulkanSwapchain = std::make_unique<VulkanSwapchain>(*_vulkanDevice);`
- `createSwapchain()` оркеструє через `_vulkanSwapchain->` виклики
- Замінити: `_renderPass` → `_vulkanSwapchain->renderPass()`, `_swapchainExtent` → `_vulkanSwapchain->swapchainExtent()`, тощо
- `drawFrame()` використовує `_vulkanSwapchain->acquireSwapchainImage()`, `_vulkanSwapchain->submitAndPresent()`

**Залежності:** Phase 2 (VulkanDevice)
**Верифікація:** build + run

---

## 🔵 Phase 4: Extract VulkanTextureStore

*Texture CRUD, staging upload, entity texture descriptor set management.*
*Може виконуватися паралельно з Phase 3.*

### 4.1. Створити `src/ecs/render/vulkantexturestore.h` + `src/ecs/render/vulkantexturestore.cpp`

**Клас VulkanTextureStore:**

**Мембери (переміщені з VulkanAPI):**
- `_textureHandles`, `_textureMemoryHandles`, `_textureViewHandles`, `_textureSamplerHandles`, `_textureDescs` (всі unordered_map\<uint64_t, ...\>)
- `TextureUploadStaging` struct + `_pendingTextureUploads` vector
- `_entityTextureDescriptorSetLayout` (vk::UniqueDescriptorSetLayout)
- `_entityTextureDescriptorPool` (vk::UniqueDescriptorPool)
- `_entityTexturePipelineLayout` (vk::UniquePipelineLayout) — textured pipeline layout
- `_textureDescriptorSets` (unordered_map\<uint64_t, vk::DescriptorSet\>)
- Власний `_nextTextureHandleId` counter (uint64_t)

**Методи (переміщені з VulkanAPI):**
- `createTexture(TextureDesc)` (рядки 1661–1772)
- `uploadTexture(handle, pixels, w, h)` (рядки 1773–1910) + sub-helpers з Phase 1.3
- `destroyTexture(handle)` (рядки 1911–1948)
- `getTextureImageView(handle)` (рядки 1991–1995)
- `getTextureSampler(handle)` (рядки 1997–2001)
- `createTextureDescriptorSet(handle)` (рядки 1031–1066)
- `cleanupCompletedTextureUploads()` (рядки 2003–2019)
- `initEntityTextureDescriptorResources()` (рядки 633–670)

**Публічні аксесори:**
- `entityTexturePipelineLayout()` — потрібен VulkanPipelineStore::createPipeline()
- `entityTextureDescriptorSet(TextureHandle)` — потрібен frame renderer для draw calls
- `imageView(handle)`, `sampler(handle)`

**Конструктор:** приймає `VulkanDevice&`

### 4.2. Додати `vulkantexturestore.cpp` до CMakeLists.txt SOURCES

### 4.3. Оновити `vulkanapi.h`: forward-declare, unique_ptr member, видалити 11 переміщених мемберів

### 4.4. Оновити `vulkanapi.cpp`:
- Делегувати `createTexture()`, `uploadTexture()`, `destroyTexture()`, тощо до `_vulkanTextureStore`
- `createSwapchain()`: викликати `_vulkanTextureStore->initEntityTextureDescriptorResources()`
- Деструктор: `_vulkanTextureStore.reset()` (перед `_vulkanDevice`)

**Залежності:** Phase 2 (VulkanDevice). Незалежно від Phase 3.
**Верифікація:** build + run

---

## 🔵 Phase 5: Extract VulkanPipelineStore

*Pipeline + shader module CRUD.*

### 5.1. Створити `src/ecs/render/vulkanpipelinestore.h` + `src/ecs/render/vulkanpipelinestore.cpp`

**Клас VulkanPipelineStore:**

**Мембери (переміщені з VulkanAPI):**
- `_pipelineHandles` (unordered_map\<uint64_t, vk::Pipeline\>)
- `_shaderModuleHandles` (unordered_map\<uint64_t, vk::ShaderModule\>)
- Власний `_nextPipelineHandleId` counter

**Методи (переміщені з VulkanAPI):**
- `createPipeline(PipelineDesc, renderPass, extent, defaultLayout, textureLayout)` (рядки 1452–1619) + `buildVertexInputState` з Phase 1.2
- `destroyPipeline(handle)` (рядки 1620–1628)
- `createShaderModule(spirv)` (рядки 1950–1965)
- `destroyShaderModule(handle)` (рядки 1967–1975)

**Сигнатура createPipeline:**
```cpp
PipelineHandle createPipeline(const PipelineDesc& desc,
                              vk::RenderPass renderPass,
                              vk::Extent2D swapchainExtent,
                              vk::PipelineLayout defaultLayout,
                              vk::PipelineLayout textureLayout);
```

**Публічні аксесори:**
- `pipeline(PipelineHandle)` → vk::Pipeline — потрібен frame renderer
- `shaderModule(ShaderModuleHandle)` → vk::ShaderModule

**Конструктор:** приймає `VulkanDevice&`

### 5.2. Додати `vulkanpipelinestore.cpp` до CMakeLists.txt SOURCES

### 5.3. Оновити `vulkanapi.h` + `vulkanapi.cpp`:
- `VulkanAPI::createPipeline()` делегує до `_vulkanPipelineStore->createPipeline(desc, _vulkanSwapchain->renderPass(), ...)`
- `VulkanAPI::destroyPipeline()` делегує
- Shader module методи делегують

**Залежності:** Phase 2 (VulkanDevice), Phase 3 (для renderPass/extent/pipelineLayout), Phase 4 (для entityTexturePipelineLayout)
**Верифікація:** build + run

---

## ✅ Phase 6: Extract VulkanFrameRenderer

*Frame recording, entity/SDF draw calls, SDF pipeline, per-frame buffers.*

### 6.1. Створити `src/ecs/render/vulkanframerenderer.h` + `src/ecs/render/vulkanframerenderer.cpp`

**Клас VulkanFrameRenderer:**

**Мембери (переміщені з VulkanAPI):**
- `_entityDrawGroups` (vector\<EntityDrawGroup\>)
- `_instancedBufferStates` (unordered_map\<uint64_t, vector\<VulkanPerImageBuffer\>\>)
- `_sdfGlyphVertices` (vector\<GlyphQuadVertex\>)
- `_sdfGlyphUploadNeeded` (vector\<bool\>), `_sdfGlyphBuffers` (vector\<VulkanPerImageBuffer\>)
- `_sdfAtlasHandle` (TextureHandle)
- `_sdfDescriptorSetLayout`, `_sdfDescriptorPool`, `_sdfDescriptorSet`, `_sdfPipelineLayout`, `_sdfGlyphPipeline` — всі SDF pipeline ресурси
- `_sdfClipYMin`, `_sdfClipYMax` (float)
- `_demoRotationAngle` (float)
- `_lastRenderedVertexCount` (uint64_t)
- `_lastFrameTime` (chrono::steady_clock::time_point)

**Методи (переміщені з VulkanAPI):**
- `createSdfPipeline()` (рядки 672–870) + sub-helpers з Phase 1.1
- `recordCommandBuffer(cmd, imageIndex, viewProjection, ...)` (рядки 1122–1217)
- `recordEntityDrawCalls(cmd, imageIndex, pushConstants)` (рядки 1218–1303)
- `recordSdfDrawCalls(cmd, imageIndex, pushConstants)` (рядки 1304–1364)
- `setEntityDrawGroups(groups)` (рядок 1012)
- `setSdfGlyphData(vertices, atlasHandle)` (рядки 976–1009)
- `setSdfClipY(yMin, yMax)` (inline в header)
- `cleanupSwapchainResources()` — resets SDF pipeline + per-image buffers

**Контекст, потрібний для recording:**
- `VulkanDevice&` — для device, findMemoryType
- `VulkanSwapchain&` — для renderPass, swapchainExtent, framebuffers, swapchainImages, imageInitialized, pipelineLayout
- `VulkanTextureStore&` — для entityTextureDescriptorSet(), entityTexturePipelineLayout()
- `VulkanPipelineStore&` — для pipeline(handle) lookup

**Конструктор:** приймає `VulkanDevice&`

### 6.2. Додати `vulkanframerenderer.cpp` до CMakeLists.txt SOURCES

### 6.3. Оновити `vulkanapi.h` + `vulkanapi.cpp`:
- `drawFrame()` оркеструє: time update → acquire (`_vulkanSwapchain`) → record (`_vulkanFrameRenderer`) → submit (`_vulkanSwapchain`)
- `createSwapchain()` викликає `_vulkanFrameRenderer->createSdfPipeline()`
- `cleanupSwapchainResources()` викликає `_vulkanFrameRenderer->cleanupSwapchainResources()`
- Делегувати setEntityDrawGroups, setSdfGlyphData, setSdfClipY

### 6.4. Перемістити `PushConstants` struct
- Перемістити з VulkanAPI private у спільне місце: `src/ecs/render/vulkantypes.h` — використовується і VulkanSwapchain (для sizeof при pipeline layout), і VulkanFrameRenderer (для push constant запису).

**Залежності:** Phase 2, 3, 4, 5 (всі попередні)
**Верифікація:** build + run + візуальна перевірка (SDF текст, ентіті, ротація, текстури)

---

## ✅ Phase 7: Фінальна очистка + документація — DONE

### 7.1. Фінальний стан VulkanAPI

Після всіх екстракцій VulkanAPI міститиме:
- 5 unique_ptr мемберів: `_vulkanDevice`, `_vulkanSwapchain`, `_vulkanTextureStore`, `_vulkanPipelineStore`, `_vulkanFrameRenderer`
- `_currentViewProjection`, `_currentPushConstants` — GraphicsBackend state
- `_bufferHandles`, `_bufferMemoryHandles` — buffer CRUD (заглушки, залишаються)
- `_nextHandleId` — тільки для buffer handles
- Всі публічні методи — 1–3 рядкові делегати
- `createSwapchain()` / `drawFrame()` — ~30-рядкові оркестратори
- **Ціль: ~250 рядків загалом**

### 7.2. Оновити `include/vigine/ecs/render/vulkanapi.h`
- Видалити всі переміщені private мембери
- Тільки `#include` forward declarations + unique_ptr для хелперів
- Залишити всі public method signatures (API стабільне для RenderSystem)

### 7.3. Оновити документацію
- `doc/README.md` — додати VulkanDevice, VulkanSwapchain, VulkanTextureStore, VulkanPipelineStore, VulkanFrameRenderer до опису архітектури
- `doc/core-class-diagram.md` — додати composition: VulkanAPI → 5 helpers
- `ai/plan/vulkanapi-refactoring.md` — додати посилання на цей план, позначити Phase 7 як виконану

---

## 📂 Relevant Files

| Файл | Роль |
|------|------|
| `include/vigine/ecs/render/vulkanapi.h` | Модифікація: прибрати мембери, додати unique_ptr helpers |
| `src/ecs/render/vulkanapi.cpp` | Модифікація: делегувати до helpers, залишити оркестрацію |
| `src/ecs/render/vulkandevice.h` + `.cpp` | **Новий**: instance, device, surface, queues |
| `src/ecs/render/vulkanswapchain.h` + `.cpp` | **Новий**: swapchain, render pass, framebuffers, sync |
| `src/ecs/render/vulkantexturestore.h` + `.cpp` | **Новий**: texture CRUD, descriptor sets |
| `src/ecs/render/vulkanpipelinestore.h` + `.cpp` | **Новий**: pipeline + shader CRUD |
| `src/ecs/render/vulkanframerenderer.h` + `.cpp` | **Новий**: frame recording, SDF pipeline, draw calls |
| `src/ecs/render/vulkantypes.h` | **Новий**: shared PushConstants struct |
| `CMakeLists.txt` | Модифікація: додати 5 .cpp до SOURCES |
| `include/vigine/ecs/render/rendersystem.h` | **Без змін** |
| `src/ecs/render/rendersystem.cpp` | **Без змін** |
| `include/vigine/ecs/render/graphicsbackend.h` | **Без змін** |
| `include/vigine/ecs/render/graphicshandles.h` | **Без змін** |

---

## ✅ Verification

1. **Після Phase 1:** `cmake --build build --target vigine && cmake --build build --target example-window` + запуск — жоден метод >~130 рядків
2. **Після Phase 2:** build + run — всі init paths працюють (instance, device, surface creation)
3. **Після Phase 3:** build + run — swapchain create/recreate/resize працює, фрейми рендеряться
4. **Після Phase 4:** build + run — текстури завантажуються, SDF atlas працює, текстуровані площини рендеряться
5. **Після Phase 5:** build + run — pipelines компілюються при першому draw, shader modules завантажуються
6. **Після Phase 6:** build + run — ротація ентіті, SDF текст, clip planes, vertex count telemetry
7. **Після Phase 7:** виміряти vulkanapi.cpp ≤ 300 рядків, кожен хелпер ≤ 450

---

## 💡 Decisions

- **Хелпери в `src/ecs/render/`** (не `include/`) — приватні деталі реалізації VulkanAPI, ніколи не включаються RenderSystem
- **RenderSystem без змін** — всі виклики йдуть через стабільний публічний API VulkanAPI, який делегує внутрішньо
- **Кожен хелпер отримує `VulkanDevice&`** як параметр конструктора для device/queue access
- **`_nextHandleId` розділяється** — кожен store отримує свій лічильник; handle ID мають бути унікальними тільки в межах типу
- **`PushConstants` struct** переміщується в спільний `vulkantypes.h` — кілька хелперів посилаються на його layout (pipeline layout `sizeof`, push constant recording)
- **Phase 3 і 4 паралелізуються** — обидві залежать тільки від Phase 2, не одна від одної
- **Buffer CRUD** (createBuffer, uploadBuffer, destroyBuffer) залишається в VulkanAPI — це заглушки, що будуть реалізовані при продовженні міграції геометрії
- **Scope excludes:** зміни публічного API VulkanAPI, зміни RenderSystem, зміни GraphicsBackend interface, нові фічі

---

## 🔗 Dependency Graph

```
Phase 1 (method splitting)
    │
    v
Phase 2 (VulkanDevice)
    │
    ├──────────────┐
    v              v
Phase 3          Phase 4
(VulkanSwapchain) (VulkanTextureStore)
    │              │
    └──────┬───────┘
           v
       Phase 5 (VulkanPipelineStore)
           │
           v
       Phase 6 (VulkanFrameRenderer)
           │
           v
       Phase 7 (cleanup + docs)
```
