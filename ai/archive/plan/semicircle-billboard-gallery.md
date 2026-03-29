# 🖼️ Plan: Semicircle Billboard Gallery

## 📋 TL;DR

Розмістити текстуровані площини по 180° напівкола з центром на позиції камери (XZ) і радіусом 19.5 (поточна відстань). Додати spherical billboard — площини автоматично повертаються обличчям до камери по всіх осях. Увімкнення/вимкнення по клавіші **B**. Зміни в 6 файлах, нульові зміни в шейдерах.

---

## 🔴 Current State

- 6 текстурованих площин розташовані **лінійно** з `spacing=3.5`, Y=2.2, Z=−15
- Статична Y-ротація −12°…+12° (arc effect) — фіксована, не слідкує за камерою
- Камера за замовчуванням на (0, 1.6, 4.5), дивиться у −Z
- Відстань від камери до центральної площини: 19.5 одиниць
- Billboard механізму не існує в кодовій базі

---

## 🟢 Phase 1: Billboard Flag на TransformComponent

**⛓️ Залежності:** немає (перша фаза)

**🎯 Мета:** додати інфраструктуру billboard в TransformComponent — прапорець і метод обчислення billboard-матриці.

### 1.1. Додати `_billboard` прапорець

**Файл:** `include/vigine/ecs/render/transformcomponent.h`

- Додати `bool _billboard{false};` до private секції (після `_scale`)
- Додати public методи:
  - `void setBillboard(bool enabled) { _billboard = enabled; }`
  - `bool isBillboard() const { return _billboard; }`
- Дефолтне значення `false` — жоден існуючий ентіті не змінить поведінку

> ✅ **Верифікація 1.1:** `cmake --build build --target vigine` — компіляція без помилок. Grep `_billboard` у transformcomponent.h — знайдено 3 рази (поле + setter + getter).

### 1.2. Додати `getBillboardModelMatrix()`

**Файли:**
- `include/vigine/ecs/render/transformcomponent.h` — декларація
- `src/ecs/render/transformcomponent.cpp` — реалізація

**Декларація (public):**
```cpp
glm::mat4 getBillboardModelMatrix(const glm::vec3 &cameraPosition) const;
```

**Реалізація (spherical billboard):**
1. Обчислити `toCamera = cameraPosition - _position`
2. Edge case: якщо `length(toCamera) < 0.001f` → повернути `getModelMatrix()`
3. `forward = normalize(toCamera)` — Z-стовпець матриці
4. Обрати reference up-вектор:
   - Якщо `abs(dot(forward, worldUp)) > 0.999f` → `refUp = glm::vec3(0.0f, 0.0f, -1.0f)`
   - Інакше → `refUp = glm::vec3(0.0f, 1.0f, 0.0f)`
5. `right = normalize(cross(refUp, forward))` — X-стовпець
6. `up = cross(forward, right)` — Y-стовпець
7. Побудувати 4×4 матрицю:
   ```
   | right.x   up.x   forward.x   _position.x |
   | right.y   up.y   forward.y   _position.y |
   | right.z   up.z   forward.z   _position.z |
   | 0         0      0           1            |
   ```
8. Apply scale: `model = model * glm::scale(glm::mat4(1.0f), _scale)`
9. Повернути `model`

**Інваріант:** для камери на (0,1.6,4.5) і площини на (0,2.2,−15): `toCamera = (0, −0.6, 19.5)`, forward ≈ (0, −0.03, 1.0) → площина дивиться приблизно у +Z → візуально подібно до поточного стану.

> ✅ **Верифікація 1.2:** `cmake --build build --target vigine` — компіляція без помилок. Новий метод існує але ніким не викликається — безпечно.

### 🏁 Phase 1 Verification

- 🔨 `cmake --build build --target vigine` — zero errors
- 🖥️ Run `example-window` — візуально ідентично поточному стану (billboard ще ніким не використовується)

### 📂 Relevant Files
- `include/vigine/ecs/render/transformcomponent.h` — додати прапорець, getter, setter, декларацію нового методу
- `src/ecs/render/transformcomponent.cpp` — реалізація `getBillboardModelMatrix()`

---

## 🟢 Phase 2: Billboard-aware appendModelMatrices

**⛓️ Залежності:** Phase 1 (getBillboardModelMatrix існує)

**🎯 Мета:** дозволити RenderSystem передавати override anchor-матрицю при billboard.

### 2.1. Додати overload `appendModelMatrices`

**Файли:**
- `include/vigine/ecs/render/rendercomponent.h` — декларація
- `src/ecs/render/rendercomponent.cpp` — реалізація

**Декларація (public, після існуючого `appendModelMatrices`):**
```cpp
void appendModelMatrices(std::vector<glm::mat4> &modelMatrices, const glm::mat4 &anchorOverride) const;
```

**Реалізація:** ідентична існуючому `appendModelMatrices()` (rendercomponent.cpp рядок 277), але замість `_transform.getModelMatrix()` використовує `anchorOverride`:

```cpp
void RenderComponent::appendModelMatrices(std::vector<glm::mat4> &modelMatrices,
                                          const glm::mat4 &anchorOverride) const
{
    const glm::mat4 anchorMatrix = anchorOverride;

    if (_text.drawBaseInstance())
        modelMatrices.push_back(anchorMatrix);

    if (!_text.enabled())
        return;

    for (const auto &offset : _text.voxelOffsets())
    {
        glm::mat4 model = anchorMatrix;
        model           = glm::translate(model, offset);
        model           = glm::scale(model, glm::vec3(_text.voxelSize()));
        modelMatrices.push_back(std::move(model));
    }
}
```

Існуючий одно-параметровий метод залишається **без змін**.

> ✅ **Верифікація 2.1:** `cmake --build build --target vigine` — компіляція без помилок. Новий overload ніким не викликається.

### 🏁 Phase 2 Verification

- 🔨 `cmake --build build --target vigine` — zero errors
- 🖥️ Run `example-window` — візуально ідентично

### 📂 Relevant Files
- `include/vigine/ecs/render/rendercomponent.h` — додати overload декларацію після рядка 48
- `src/ecs/render/rendercomponent.cpp` — додати overload реалізацію після рядка 296

---

## 🟢 Phase 3: Billboard логіка в RenderSystem

**⛓️ Залежності:** Phase 1 (isBillboard(), getBillboardModelMatrix()), Phase 2 (appendModelMatrices overload)

**🎯 Мета:** під час `update()` обчислювати billboard-матрицю для позначених ентіті.

### 3.1. Додати `_billboardEnabled` toggle

**Файл:** `include/vigine/ecs/render/rendersystem.h`

- Додати private member (після `_glyphDirty`): `bool _billboardEnabled{true};`
- Додати public методи (після `setSprintActive`):
  ```cpp
  void setBillboardEnabled(bool enabled);
  bool isBillboardEnabled() const;
  void toggleBillboard();
  ```

**Файл:** `src/ecs/render/rendersystem.cpp` — реалізація (додати після `setSprintActive`):

```cpp
void RenderSystem::setBillboardEnabled(bool enabled) { _billboardEnabled = enabled; }

bool RenderSystem::isBillboardEnabled() const { return _billboardEnabled; }

void RenderSystem::toggleBillboard()
{
    _billboardEnabled = !_billboardEnabled;
    std::cout << "[RenderSystem] Billboard " << (_billboardEnabled ? "enabled" : "disabled")
              << std::endl;
}
```

> ✅ **Верифікація 3.1:** `cmake --build build --target vigine` — компіляція без помилок. Нові методи ніким не викликаються.

### 3.2. Застосувати billboard в update()

**Файл:** `src/ecs/render/rendersystem.cpp`

**Місце:** метод `update()`, рядок з `rc.appendModelMatrices(group.modelMatrices);`

**Замінити:**
```cpp
rc.appendModelMatrices(group.modelMatrices);
```

**На:**
```cpp
if (_billboardEnabled && rc.getTransform().isBillboard())
{
    const glm::mat4 bbMatrix =
        rc.getTransform().getBillboardModelMatrix(_camera.position());
    rc.appendModelMatrices(group.modelMatrices, bbMatrix);
}
else
{
    rc.appendModelMatrices(group.modelMatrices);
}
```

**Поведінка:** жоден ентіті ще не має `isBillboard()==true`, тому гілка `if` ніколи не виконується → візуально ідентично.

> ✅ **Верифікація 3.2:** `cmake --build build` — компіляція без помилок. Запуск `example-window` — візуально ідентично.

### 🏁 Phase 3 Verification

- 🔨 `cmake --build build` — zero errors
- 🖥️ Run `example-window` — візуально ідентично поточному стану (жоден ентіті не має billboard=true)

### 📂 Relevant Files
- `include/vigine/ecs/render/rendersystem.h` — додати `_billboardEnabled`, 3 public методи
- `src/ecs/render/rendersystem.cpp` — реалізація toggle; зміна в `update()`: if billboard → use billboard matrix

---

## 🟢 Phase 4: Semicircle Gallery Layout

**⛓️ Залежності:** Phase 1 (`setBillboard()`), Phase 3 (billboard обробляється в `update()`)

**🎯 Мета:** розмістити площини по 180° напівколу і ввімкнути billboard.

### 4.1. Замінити лінійний layout на semicircle

**Файл:** `example/window/task/vulkan/setuptexturedplanestask.cpp`

**Місце 1:** struct `PlaneConfig` (рядки 48–54) — видалити поле `float rotationY`:

Замінити:
```cpp
struct PlaneConfig
{
    std::string entityName;
    std::string textureEntityName;
    glm::vec3 position;
    float rotationY;
};
```

На:
```cpp
struct PlaneConfig
{
    std::string entityName;
    std::string textureEntityName;
    glm::vec3 position;
};
```

**Місце 2:** блок обчислення позицій (рядки 65–83) — замінити лінійний layout на semicircle:

Замінити:
```cpp
const float spacing = 3.5f;
const float startX  = -static_cast<float>(textureCount - 1) * spacing / 2.0f;

std::vector<PlaneConfig> planes;
planes.reserve(textureCount);
for (size_t i = 0; i < textureCount; ++i)
{
    const float rotY =
        (textureCount > 1)
            ? -12.0f + 24.0f * (static_cast<float>(i) / static_cast<float>(textureCount - 1))
            : 0.0f;
    planes.push_back({
        "TexturedPlane" + std::to_string(i),
        "TextureEntity_" + std::to_string(i),
        {startX + static_cast<float>(i) * spacing, 2.2f, -15.0f},
        rotY
    });
}
```

На:
```cpp
constexpr float galleryY      = 2.2f;
constexpr float centerX       = 0.0f;
constexpr float centerZ       = 4.5f;    // Camera default Z position
constexpr float radius        = 19.5f;   // Distance from camera to gallery center
constexpr float halfArcRad    = glm::radians(90.0f); // 180° total arc

std::vector<PlaneConfig> planes;
planes.reserve(textureCount);
for (size_t i = 0; i < textureCount; ++i)
{
    const float t = (textureCount > 1)
        ? static_cast<float>(i) / static_cast<float>(textureCount - 1)
        : 0.5f;
    const float angle = -halfArcRad + t * 2.0f * halfArcRad;
    const float x     = centerX + radius * std::sin(angle);
    const float z     = centerZ - radius * std::cos(angle);
    planes.push_back({
        "TexturedPlane" + std::to_string(i),
        "TextureEntity_" + std::to_string(i),
        {x, galleryY, z}
    });
}
```

**Місце 3:** рядок `transform.setRotation(...)` (≈ рядок 163) — видалити:

Замінити:
```cpp
transform.setRotation({0.0f, glm::radians(config.rotationY), 0.0f});
```

На: (видалити цей рядок повністю)

**Верифікація позицій (для 6 площин):**
| i | t     | angle (°) | x      | z      |
|---|-------|-----------|--------|--------|
| 0 | 0.0   | −90       | −19.5  | 4.5    |
| 1 | 0.2   | −54       | −15.78 | −6.96  |
| 2 | 0.4   | −18       | −6.03  | −14.04 |
| 3 | 0.6   | +18       | +6.03  | −14.04 |
| 4 | 0.8   | +54       | +15.78 | −6.96  |
| 5 | 1.0   | +90       | +19.5  | 4.5    |

> ✅ **Верифікація 4.1:** `cmake --build build` — компіляція без помилок. Площини розставлені по дузі, але без billboard дивляться у +Z (поки).

### 4.2. Увімкнути billboard на площинах

**Файл:** `example/window/task/vulkan/setuptexturedplanestask.cpp`

**Місце:** блок де створюється transform (≈ рядок 158), після `transform.setPosition(config.position)` і `transform.setScale(...)`:

**Додати:**
```cpp
transform.setBillboard(true);
```

**Результат:** площини тепер розставлені по напівколу і кожна дивиться на камеру. При русі камери — площини обертаються за нею.

> ✅ **Верифікація 4.2:** Run `example-window` — 6 площин по дузі 180°, кожна повернута обличчям до камери, при русі камери площини слідкують.

### 🏁 Phase 4 Verification

- 🔨 `cmake --build build` — zero errors
- 🖥️ Run `example-window`:
  - 6 площин розташовані по дузі 180°
  - Центральні площини (i=2, i=3) близько до Z=−14 (як було Z=−15)
  - Всі площини повернуті обличчям до камери
  - При русі камери (WASD + миша) — площини слідкують за камерою
- 🧪 Додати 7-му картинку в `resource/img/` → restart → 7 площин по дузі

### 📂 Relevant Files
- `example/window/task/vulkan/setuptexturedplanestask.cpp` — semicircle layout + `setBillboard(true)`

---

## 🟢 Phase 5: Keyboard Toggle (клавіша B)

**⛓️ Залежності:** Phase 3 (`toggleBillboard()` існує на RenderSystem)

**🎯 Мета:** натискання B перемикає billboard on/off.

### 5.1. Додати key constant та toggle handler

**Файл:** `example/window/task/window/runwindowtask.cpp`

**Місце 1:** блок key constants (рядок ~47, після `constexpr unsigned int kKeyRayToggle = 'R';`):

Додати:
```cpp
constexpr unsigned int kKeyBillboardToggle = 'B';
```

**Місце 2:** метод `onKeyDown()`, у блоці toggle-клавіш (біля обробки `kKeyRayToggle`, ≈ рядок 365–385). Додати **після** блоку ray toggle (перед наступним `if` або `}`):

```cpp
if (event.keyCode == kKeyBillboardToggle && !event.isRepeat)
{
    if (_renderSystem)
        _renderSystem->toggleBillboard();
}
```

**Патерн ідентичний** існуючому toggle для `kKeyRayToggle`:
- `!event.isRepeat` — тільки перше натискання (ігнорує auto-repeat)
- Прямий виклик на `_renderSystem` (вже доступний як member)

> ✅ **Верифікація 5.1:** `cmake --build build` — компіляція без помилок. Run `example-window`:
> - Натиснути B → консоль: `[RenderSystem] Billboard disabled`; площини зупиняють billboard (залишаються у фіксованій ротації getBillboardModelMatrix з останньої позиції камери → переходять на getModelMatrix)
> - Натиснути B знову → консоль: `[RenderSystem] Billboard enabled`; площини знову слідкують за камерою

### 🏁 Phase 5 Verification

- 🔨 `cmake --build build` — zero errors
- 🖥️ Run `example-window`:
  - B вмикає/вимикає billboard, підтвердження в консолі
  - При вимкненому billboard площини стоїть нерухомо (фіксовано обличчям у +Z)
  - При увімкненому billboard площини відновлюють слідкування за камерою
- 🧪 Затиснути B (auto-repeat) — toggle не спрацьвує більше одного разу

### 📂 Relevant Files
- `example/window/task/window/runwindowtask.cpp` — `kKeyBillboardToggle = 'B'`; обробка в `onKeyDown()`

---

## ✅ Verification (загальна)

1. 🔨 **Compile check** після кожної фази — `cmake --build build` must succeed
2. 🖥️ **Фази 1–3:** візуально ідентично поточному стану (billboard ще не активований на ентіті)
3. 🖥️ **Фаза 4:** 6 площин по дузі 180°, billboard працює
4. ⌨️ **Фаза 5:** клавіша B toggle billboard on/off
5. 🧪 **Edge cases:** додати/прибрати картинки → layout адаптується; камера строго зверху → graceful fallback

---

## 💡 Decisions

- **Spherical billboard** (повний поворот по всіх осях, не тільки Y) — за вибором користувача
- **Semicircle arc = 180°** (повне півколо) — за вибором користувача
- **Toggle key = B** — за вибором користувача
- Billboard прапорець на `TransformComponent`, toggle на `RenderSystem` — billboard активний коли обидва true
- `_billboardEnabled` дефолт = `true` (billboard увімкнено з старту, бо площини створюються з `setBillboard(true)`)
- Шейдери **не змінюються** — billboard обчислюється на CPU через model matrix
- Semicircle центр = (0, 4.5) в XZ — позиція камери за замовчуванням; радіус = 19.5
- Edge case: камера збігається з позицією площини → fallback на `getModelMatrix()`
- Edge case: `toCamera ≈ worldUp` → використати альтернативний reference vector `(0,0,-1)`

---

## ⚠️ Further Considerations

1. **AABB picking**: `buildEntityAabb()` (анонімна функція в `src/ecs/render/rendersystem.cpp`) використовує `transform.getModelMatrix()` — не `getBillboardModelMatrix()`. Для billboard ентіті AABB не буде точно відповідати візуальній ротації. Рекомендація: оновити в окремому завданні, бо пікінг працює прийнятно для наближених AABB.
2. **Semicircle параметри** (`radius`, `halfArcRad`, `centerZ`) захардкоджені в `SetupTexturedPlanesTask`. Якщо потрібна конфігурація — можна винести в constants або config struct в наступному завданні.
3. **Billboard для інших ентіті**: механізм generic — будь-який ентіті з `setBillboard(true)` автоматично підтримується. Не потрібні додаткові зміни для нових ентіті.

---

## 📂 All Relevant Files

| Файл | Зміни | Фаза |
|------|-------|------|
| `include/vigine/ecs/render/transformcomponent.h` | +`_billboard` bool, +`setBillboard()`, +`isBillboard()`, +`getBillboardModelMatrix()` | 1 |
| `src/ecs/render/transformcomponent.cpp` | +реалізація `getBillboardModelMatrix()` | 1 |
| `include/vigine/ecs/render/rendercomponent.h` | +overload `appendModelMatrices(matrices, anchorOverride)` | 2 |
| `src/ecs/render/rendercomponent.cpp` | +реалізація overload | 2 |
| `include/vigine/ecs/render/rendersystem.h` | +`_billboardEnabled`, +`setBillboardEnabled()`, +`isBillboardEnabled()`, +`toggleBillboard()` | 3 |
| `src/ecs/render/rendersystem.cpp` | +реалізація toggle; зміна в `update()`: if billboard → use billboard matrix | 3 |
| `example/window/task/vulkan/setuptexturedplanestask.cpp` | semicircle layout замість лінійного; видалити `rotationY`; `setBillboard(true)` | 4 |
| `example/window/task/window/runwindowtask.cpp` | +`kKeyBillboardToggle = 'B'`; +обробка в `onKeyDown()` | 5 |

## 🆕 New Files Created

Немає — всі зміни в існуючих файлах.
