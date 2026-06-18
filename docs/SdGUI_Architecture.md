# SdGUI Architecture Design

Version: v0.4  
Target standard: C++20  
Project name: SodiumGUI  
Short name: SdGUI

## 1. Overview

SdGUI is a lightweight graphical user interface framework designed around a hybrid model:

```text
External model: typed declaration API
Internal model: persistent widget state
```

Application code describes UI every frame by declaring user-defined widget types through `SdUi::Declare<T>()`. Internally, SdGUI keeps stable widget state, layout cache, animation values, focus state, input capture, layer information, font atlas data, and render resources.

The core design goal is to keep the public API flexible for complex user-defined widgets while allowing smooth structural animation, efficient layout, persistent interaction state, and high-performance rendering.

## 2. Design Goals

```text
1. Use C++20 as the baseline language standard.
2. Use `SdUi::Declare<T>()` as the public widget declaration model.
3. Use persistent internal widget state.
4. Use std::string and std::string_view for UTF-8 strings.
5. Implement UTF-8 to codepoint parsing internally.
6. Preserve the layer design.
7. Do not inherit the old Element / Theme / Property architecture.
8. Use more explicit Widget / Style / Animation / Layout systems.
9. Rebuild RenderList data every frame, but keep GPU buffers persistent.
10. Avoid unnecessary heap allocation in frame-critical paths.
11. Let users own one or more SdGUI instances explicitly.
12. Use a zero-state widget tag type only for compile-time declaration constraints.
```

## 3. High-Level Architecture

```text
Application
  |
  v
User-owned SdGUI Instance
  |
  v
SdUi::Declare<T>() typed declaration API
  |
  v
Context / Frame / IdStack / StateStorage
  |
  v
Input / Widget / Layout / Style / Animation / Layer
  |
  v
RenderList / FontAtlas / TextureAtlas
  |
  v
PlatformBackend / RendererBackend / GlyphBackend
```

The main architectural difference from a pure object tree GUI is that declarations do not directly imply immediate destruction of internal state. A widget may be absent from application code in the current frame but still exist internally while it performs a leaving animation.

## 4. Recommended Directory Layout

```text
SdGUI/
│SdGUI.h
├─ Core/
├─ Input/
├─ Widget/
├─ Layout/
├─ Style/
├─ Animation/
├─ Layer/
├─ Render/
├─ Backends/
│  ├─ Win32/
│  ├─ Dx9/
│  ├─ Dx11/
│  ├─ Dx12/
│  ├─ OpenGL/
│  └─ FreeType/
├─ examples/
└─ docs/

// Hander and Source in same level
```

## 5. Base Types and Naming

SdGUI uses the `Sodium` namespace. The `Sd` prefix is preserved for public types, functions, and data structures that belong to the SodiumGUI API.

```cpp
namespace Sodium
{
    using SdUInt8 = std::uint8_t;
    using SdUInt16 = std::uint16_t;
    using SdUInt32 = std::uint32_t;
    using SdUInt64 = std::uint64_t;
    using SdSize = std::size_t;

    using SdDuration = std::chrono::nanoseconds;
    using SdUtf8String = std::string;
    using SdUtf8StringView = std::string_view;
    using SdPath = std::filesystem::path;

    using SdWidgetId = std::uint64_t;
    using SdLayerId = std::uint32_t;
    using SdFrameIndex = std::uint64_t;
    using SdTextureHandle = void*;
    using SdShaderHandle = void*;
}
```

Recommended naming conventions:

```text
Types: PascalCase
Functions: PascalCase
Local variables and fields: camelCase
Constants: kPascalCase
Namespaces: PascalCase
```

## 6. String and UTF-8 Policy

SdGUI uses UTF-8 as the internal and external text encoding.

```text
Owned string: std::string
Non-owning string view: std::string_view
```

SdGUI should not depend on platform wide-character conversion as its core string model. UTF-8 decoding is handled internally.

```cpp
namespace Sodium::Utf8
{
    bool IsValid(std::string_view text);
    bool TryReadCodepoint(std::string_view text, size_t& offset, uint32_t& codepoint);
    std::vector<uint32_t> DecodeToCodepoints(std::string_view text);
}
```

The initial UTF-8 implementation must support:

```text
1. ASCII
2. 2-byte UTF-8 sequences
3. 3-byte UTF-8 sequences
4. 4-byte UTF-8 sequences
5. Invalid sequence fallback
```

## 7. Context

`Context` is the central runtime object. It owns or references all major framework systems.

```text
Context
- FrameState
- IdStack
- StateStorage
- InputManager
- LayoutContext
- StyleContext
- AnimationContext
- LayerManager
- RenderContext
- PlatformBackend
- RendererBackend
- GlyphBackend
```

Example application loop:

```cpp
Sodium::SdInstance mainUi;

mainUi.Initialize(platform, renderer, fontBackend);

if (mainUi.IsRunning())
{
    dx.BeginFrame(clearColor);

    // Sodium doing: build UI and submit render commands.
    mainUi.BeginFrame();

    mainUi.ui.Declare<DemoWindow>();

    mainUi.EndFrame();

    dx.Present();
}

mainUi.Shutdown();
```

The application owns each `Instance`. A process may create a single UI instance or multiple independent UI instances.

```cpp
Sodium::SdInstance editorUi;
Sodium::SdInstance overlayUi;

editorUi.ui.Declare<EditorWindow>();
overlayUi.ui.Declare<OverlayPanel>();
```

## 8. Typed Declaration API and Persistent State

The public widget API is centered on `SdUi::Declare<T>()`:

```cpp
if (showAdvanced)
{
    sd.ui.Declare<AdvancedSettingsPanel>();
}
```

Internally, `AdvancedSettingsPanel` has a stable `SdWidgetId`. When `showAdvanced` becomes `false`, the widget state is not deleted immediately. It enters a leaving phase and continues to participate in layout and rendering until its animation completes.

Semantically, not declaring a widget means the user wants the component to stop being displayed. Depending on the widget configuration, it may disappear immediately or leave through an animation.

This gives SdGUI the desired behavior:

```text
User code controls whether a widget is submitted this frame.
Internal state controls whether the widget is still visually alive.
```

`Declare<T>()` is the only public widget declaration primitive. Built-in controls such as text, buttons, check boxes, panels, scroll views, and windows are ordinary tagged widget types declared through the same API.

```cpp
sd.ui.Declare<SdText>("Advanced Settings");
sd.ui.Declare<SdCheckBox>(enableCache);
sd.ui.Declare<SdSliderFloat>(radius, 0.f, 100.f);
```

## 9. Widget Lifecycle

SdGUI does not introduce a separate presence system. Structural appearance and disappearance are part of `SdWidgetState`.

```cpp
enum class SdWidgetLifePhase
{
    Entering,
    Alive,
    Leaving,
    Dead
};

struct SdWidgetState
{
    SdWidgetId id = 0;
    SdWidgetLifePhase lifePhase = SdWidgetLifePhase::Entering;

    bool submittedThisFrame = false;
    bool inputEnabled = true;
    bool layoutDirty = true;
    bool styleDirty = true;
    bool animationActive = false;

    SdRect lastRect = {};
    SdRect targetRect = {};
    SdRect animatedRect = {};

    SdVec2 measuredSize = {};
    float layoutWeight = 0.f;
    float opacity = 0.f;

    SdFrameIndex lastSubmittedFrame = 0;
};
```

Lifecycle rules:

```text
First submitted      -> Entering
Enter animation done -> Alive
Not submitted        -> Leaving
Leave animation done -> Dead
Dead                 -> removed from StateStorage
```

If a leaving widget is submitted again before the leave animation completes, it transitions back toward `Entering` or `Alive` from its current animated values. It must not jump visually.

## 10. Widget Type Declaration and Grouping

For structural animation, a group of widgets needs a stable boundary. In SdGUI this boundary is a declared widget type, not a separate public scope API.

```cpp
class SettingsPanel final : public Sodium::SdWidgetTag
{
public:
    struct State
    {
        bool enabled = false;
    };

    void OnUpdate(Sodium::SdUpdateContext& context)
    {
        State& state = context.State<State>();
        context.ui.Declare<SdText>("Settings");
        context.ui.Declare<SdCheckBox>(state.enabled);
    }
};

if (showPanel)
    sd.ui.Declare<SettingsPanel>();
```

The declared widget type provides:

```text
1. Stable SdWidgetId
2. Parent layout node
3. Shared lifecycle
4. Shared clipping region
5. Shared opacity/layout animation
```

Without such a boundary, a sequence of loose declarations may not have enough semantic information to animate as one unit.

`SdWidgetTag` is a zero-state compile-time tag. It must not become a virtual runtime base class.

```cpp
namespace Sodium
{
    struct SdWidgetTag {};

    template<class T>
    concept SdDeclarableWidget =
        std::derived_from<T, SdWidgetTag>
        && std::is_default_constructible_v<T>;
}
```

The tag exists only to constrain `Declare<T>()`, document user intent, and produce clear compile-time diagnostics. Widget lifecycle, parent-child relations, layout, animation, input, and rendering are still owned by SdGUI internal data structures.

## 11. StateStorage

`StateStorage` holds cross-frame data.

```text
StateStorage
- SdWidgetState
- User widget state
- Layout cache
- Style cache
- Animation channels
- Scroll state
- Text input state
- Focus and capture data
```

Initial implementation:

```cpp
std::unordered_map<SdWidgetId, SdWidgetState>
```

Potential later optimizations:

```text
1. flat hash map
2. robin hood hash map
3. slot map
4. dense vector plus sparse index
5. arena-backed storage
```

StateStorage must not create and destroy large numbers of objects every frame.

User-defined widgets should not keep references to frame-local systems. Persistent per-widget data is accessed through typed state stored in `StateStorage`.

```cpp
class Card final : public Sodium::SdWidgetTag
{
public:
    struct State
    {
        SdVec2 size = {};
        bool selected = false;
    };

    void OnCreate(Sodium::SdCreateContext& context, SdVec2 initialSize)
    {
        context.State<State>().size = initialSize;
    }

    void OnUpdate(Sodium::SdUpdateContext& context)
    {
        State& state = context.State<State>();
        if (context.input.clicked)
            state.selected = !state.selected;
    }
};
```

The implementation may start with `std::unordered_map<SdWidgetId, SdWidgetState>`, but typed user state should move toward dense type-specific pools or slot-map storage to avoid per-widget heap allocation in frame-critical paths.

## 12. Layout

Layout is split into two phases:

```text
Measure:
- Compute desired size.
- Text, padding, style, children, and layout rules affect measuredSize.

Arrange:
- Compute targetRect.
- Apply layoutWeight to structural size.
- Animate animatedRect toward targetRect.
```

Leaving widgets still participate in layout, but their occupied size is scaled by `layoutWeight`.

```cpp
actualHeight = measuredHeight * state.layoutWeight;
actualWidth = measuredWidth;
```

This enables smooth structural transitions:

```text
Widget appears:
- layoutWeight 0 -> 1
- neighboring widgets move smoothly to make room

Widget disappears:
- layoutWeight 1 -> 0
- neighboring widgets move smoothly to close the gap
```

Layout data should be stored in contiguous frame-local arrays.

```text
Avoid:
- Per-node heap allocation
- shared_ptr tree as the frame-critical layout structure

Prefer:
- vector-backed layout nodes
- index-based parent/child links
- frame arena for temporary data
```

## 13. Animation

Animation is responsible only for numeric interpolation. It does not decide whether a widget exists.

Common animation channels:

```text
layoutWeight
opacity
rect.position
rect.size
style.color
style.radius
scroll.offset
```

Recommended transition data:

```cpp
struct Transition
{
    Duration duration = 160ms;
    Easing easing = Easing::OutCubic;
};

struct WidgetTransition
{
    Transition enter = {};
    Transition leave = {};
    Transition layout = {};
};
```

Default structural animation:

```text
Entering:
- layoutWeight 0 -> 1
- opacity 0 -> 1
- offsetY -6px -> 0

Leaving:
- layoutWeight 1 -> 0
- opacity 1 -> 0
- inputEnabled = false
```

Performance rule:

```text
Only active animation channels are updated.
Inactive widgets are not touched by the animation update path.
```

## 14. Widget System

Widgets are split conceptually into:

```text
1. Public declaration type
2. Persistent internal record
3. Behavior update
4. Layout contribution
5. Style resolution
6. Paint/render intent
```

User-defined widgets are plain C++ types derived from `SdWidgetTag`. They may implement optional phase callbacks. SdGUI detects callbacks with C++20 `requires`; it does not require virtual functions.

```cpp
class SimpleWindow final : public Sodium::SdWidgetTag
{
public:
    void OnCreate(Sodium::SdCreateContext& context);
    void OnUpdate(Sodium::SdUpdateContext& context);
    void OnLayout(Sodium::SdLayoutContext& context);
    void OnPaint(Sodium::SdPaintContext& context);
};
```

All callbacks are optional. Construction arguments passed to `Declare<T>(args...)` are used only for first creation or `OnCreate`; they must not imply per-frame reconstruction.

```cpp
sd.ui.Declare<Card>(SdVec2{ 100.f, 100.f });
```

Recommended callback dispatch:

```cpp
template<class T>
void CallOnUpdate(T& widget, SdUpdateContext& context)
{
    if constexpr (requires { widget.OnUpdate(context); })
        widget.OnUpdate(context);
}
```

Callbacks receive phase-specific context objects. These contexts provide access to the data needed by that phase without making the widget own frame-local systems.

```cpp
struct SdWidgetContextBase
{
    SdInstance& instance;
    SdContext& context;
    SdWidgetId id = 0;
    SdWidgetId parentId = 0;

    SdWidgetState& widgetState;
    SdComputedStyle& style;
    SdThemeView theme;

    template<class T>
    T& State();
};

struct SdCreateContext : SdWidgetContextBase
{
};

struct SdUpdateContext : SdWidgetContextBase
{
    SdUi& ui;
    SdInputView input;
    SdDuration deltaSeconds = {};
    SdFrameIndex frameIndex = 0;
};

struct SdLayoutContext : SdWidgetContextBase
{
    SdLayoutConstraints constraints = {};
    SdLayoutResult& result;

    void SetDesiredSize(SdVec2 size);
};

struct SdPaintContext : SdWidgetContextBase
{
    SdRenderList& renderList;
    SdRect rect = {};
    SdRect animatedRect = {};
    SdRect clipRect = {};
    float opacity = 1.f;
    SdLayerId layer = 0;
};
```

`RenderList` is frame-local and must only be accessed through `SdPaintContext`. User widgets must not store persistent `RenderList*` or renderer backend pointers.

Widget behavior produces interaction results from input state. Rendering should not directly own behavior state; it reads final geometry, style, animation values, and user state through context accessors.

## 15. Style

SdGUI does not use the previous Theme / Property model. Style is based on tokens, rules, and computed styles.

```text
StyleToken:
- ColorText
- ColorBackground
- ColorAccent
- SpacingSmall
- SpacingMedium
- RadiusSmall
- FontBody
- DurationFast

StyleRule:
- widget type
- state selector
- layer/context selector
- token override

ComputedStyle:
- final color
- background
- padding
- border
- radius
- font
- transition
```

Style provides target values. Visual interpolation is handled by Animation.

Performance rule:

```text
Style rules should be resolved through token IDs and preprocessed selectors.
Avoid per-frame string matching on the hot path.
```

## 16. Layer

Layer design is preserved and is independent from the old Element architecture.

```cpp
enum class LayerPriority
{
    Background,
    Content,
    Floating,
    Popup,
    Overlay,
    Debug
};
```

Layer responsibilities:

```text
1. Draw order
2. Hit-test order
3. Clip/scissor routing
4. Popup, tooltip, modal, overlay support
5. DrawList or draw channel ownership
```

Example:

```cpp
class ContextMenu final : public Sodium::SdWidgetTag
{
public:
    void OnCreate(Sodium::SdCreateContext& context)
    {
        context.widgetState.layerPriority = Sodium::SdLayerPriority::Popup;
    }

    void OnUpdate(Sodium::SdUpdateContext& context)
    {
        context.ui.Declare<MenuItem>("Copy");
        context.ui.Declare<MenuItem>("Paste");
    }
};

if (showContextMenu)
    sd.ui.Declare<ContextMenu>();
```

Input hit testing must process layers from highest priority to lowest priority.

## 17. Input

Input uses a frame event model.

```text
InputManager
- MouseState
- KeyboardState
- TextInputState
- GamepadState
- FocusState
- CaptureState
```

Widget behavior queries input state to produce:

```text
hovered
pressed
clicked
active
focused
changed
```

Leaving widgets should normally have `inputEnabled = false`.

## 18. Render

Render data is frame-local. Vertex data, index data, and draw commands are rebuilt every frame.

This is intentional and expected for immediate and hybrid immediate UI frameworks.

Important distinction:

```text
Rebuild CPU-side vertex/index contents every frame: yes.
Recreate GPU vertex/index buffer resources every frame: no.
```

Recommended frame render data:

```cpp
struct RenderData
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<DrawCommand> commands;

    void Reset()
    {
        vertices.clear();
        indices.clear();
        commands.clear();
    }
};
```

Rules:

```text
1. RenderData is reset every frame.
2. Vectors keep capacity.
3. RendererBackend owns persistent GPU buffers.
4. GPU buffers grow only when capacity is insufficient.
5. TextureAtlas and FontAtlas upload only when dirty.
6. Use uint32_t indices by default.
7. RendererBackend must save the host pipeline configuration before binding the SdGUI pipeline and fully restore it before Render returns.
```

Backend upload model:

```cpp
class RendererBackend
{
private:
    GpuBuffer vertexBuffer;
    GpuBuffer indexBuffer;

public:
    void Render(const RenderData& data)
    {
        vertexBuffer.EnsureCapacity(data.vertices.size() * sizeof(Vertex));
        indexBuffer.EnsureCapacity(data.indices.size() * sizeof(uint32_t));

        vertexBuffer.Upload(data.vertices);
        indexBuffer.Upload(data.indices);

        SubmitDrawCommands(data.commands);
    }
};
```

Draw commands should be batched by:

```text
1. texture
2. scissor
3. shader
4. pipeline state
```

## 19. Font and Glyph

Text rendering flow:

```text
std::string_view UTF-8
-> codepoint stream
-> glyph lookup
-> missing glyph fallback
-> atlas packing
-> text quads
```

Font system:

```text
FontAtlas
- FontFace
- GlyphCache
- fallback glyph
- glyph texture packing

GlyphBackend
- FreeType backend
- replaceable glyph parser backend
```

The initial version does not need complex text shaping, but UTF-8 decoding, glyph lookup, fallback glyph, and atlas upload must be stable.

## 20. Backend Interfaces

Platform backend:

```cpp
class IPlatformBackend
{
public:
    virtual ~IPlatformBackend() = default;
    virtual bool Initialize(const PlatformConfig& config) = 0;
    virtual void PollEvents(InputManager& input) = 0;
    virtual bool IsRunning() const = 0;
};
```

Renderer backend:

```cpp
class IRendererBackend
{
public:
    virtual ~IRendererBackend() = default;
    virtual bool Initialize(const RendererConfig& config) = 0;
    virtual SdTextureHandle CreateTexture(SdSizeI size, std::span<const uint32_t> pixels) = 0;
    virtual void Render(const RenderData& data) = 0;
};
```

The renderer backend only consumes render commands or draw packets generated by SdGUI. Swap-chain present, clearing, frame begin, and frame end belong to the host renderer and are outside SdGUI lifecycle management.

The renderer backend is a temporary submitter inside the host render flow and must not leak SdGUI pipeline state back to the caller. Before setting its own shaders, input layout, blend/rasterizer/depth-stencil state, viewport, scissor, buffers, textures, samplers, or other mutable pipeline state, the backend should save the original configuration it will modify. Before `Render` returns, it must restore the saved state so the host pipeline is identical before and after the SdGUI submission.

Glyph backend:

```cpp
class IGlyphBackend
{
public:
    virtual ~IGlyphBackend() = default;
    virtual bool Initialize(const GlyphConfig& config) = 0;
    virtual std::optional<GlyphInfo> LoadGlyph(FontId font, uint32_t codepoint, uint32_t size) = 0;
};
```

## 21. Frame Flow

```text
BeginFrame
1. Update time.
2. Poll platform input.
3. Begin InputManager frame.
4. Mark all SdWidgetState entries as submittedThisFrame = false.
5. Clear frame-local layout and render data.

Declaration stage
1. `SdUi::Declare<T>()` resolves a stable SdWidgetId from parent stack, explicit key, type identity, and callsite information.
2. Find or create the internal widget record and typed widget object.
3. Mark SdWidgetState as submittedThisFrame = true.
4. Call OnCreate for newly created widgets.
5. Call OnUpdate for submitted widgets and allow nested `context.ui.Declare<U>()`.
6. Record the parent-child declaration relationship.

EndFrame
1. Widgets not submitted this frame enter Leaving if still alive.
2. Style resolution produces ComputedStyle.
3. Layout calls OnLayout where available, computes measuredSize and targetRect.
4. Animation updates layoutWeight, opacity, and rect values.
5. Paint calls OnPaint where available and writes frame-local RenderList data.
6. RenderContext builds final RenderData.
7. Dead widgets and typed user state are removed from StateStorage.

Renderer submission
1. Upload dirty texture and font atlases.
2. Upload current frame vertices and indices.
3. Submit draw commands to RendererBackend.
4. Do not call backend Present; presentation is owned by the host render loop.
```

## 22. Performance Principles

```text
1. SdWidgetState is persistent across frames.
2. Frame-local data uses contiguous arrays.
3. Clear vectors every frame, but preserve capacity.
4. Do not allocate LayoutNode with per-node heap allocation.
5. Update only active animation channels.
6. Resolve style through token IDs and cached selectors.
7. Rebuild RenderList every frame, but reuse GPU buffers.
8. Upload TextureAtlas and FontAtlas only when dirty.
9. Remove Leaving widgets as soon as their animation completes.
10. Avoid shared_ptr-heavy or virtual-tree-heavy frame-critical paths.
11. Prefer RAII for backend resources.
12. RendererBackend must save modified host pipeline state before submission and restore it after submission.
13. Avoid new/delete and malloc/free in normal frame execution.
```

## 23. Initial Development Milestones

```text
1. Core Context and frame loop
2. IdStack and SdWidgetId generation
3. StateStorage and SdWidgetState lifecycle
4. InputManager
5. SdWidgetTag and `SdUi::Declare<T>()`
6. Typed widget object storage
7. SdCreateContext / SdUpdateContext / SdLayoutContext / SdPaintContext
8. SdLayoutContext with declared widget boundary support
9. AnimationContext with active channel update
10. Basic StyleToken and ComputedStyle
11. RenderData and DrawList
12. FontAtlas and UTF-8 decoding
13. LayerManager
14. Basic tagged widgets: Text, Button, Checkbox, Slider, Panel, Window
15. Win32 platform backend
16. First renderer backend
17. Example application
```

## 24. Final Model Summary

The central rule of SdGUI is:

```text
Whether the user declares a widget this frame
is not the same as
whether the widget visually disappears this frame.
```

Each widget has an internal lifecycle:

```text
Entering / Alive / Leaving / Dead
```

Users declare widgets through `SdUi::Declare<T>()`. `T` is a normal C++ type derived from a zero-state `SdWidgetTag` and may implement optional `OnCreate`, `OnUpdate`, `OnLayout`, and `OnPaint` callbacks. Widgets access persistent data through `StateStorage` and phase-specific contexts. `RenderList` is passed only through `SdPaintContext` and remains frame-local.

Layout uses `layoutWeight` for continuous structural changes. Animation updates numeric transitions. Layer controls drawing and input order. RenderList is rebuilt linearly every frame, while GPU buffers and atlas textures remain persistent.

This model preserves simple frame-by-frame declaration while allowing smooth layout transitions, persistent interaction state, and a high-performance rendering path.
