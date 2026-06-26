# SdGUI Architecture Design

Version: v0.5
Implementation snapshot: 2026-06-26
Target standard: C++20
Project name: SodiumGUI
Short name: SdGUI

## 1. Overview

SdGUI is a C++20 GUI framework. The public model is a per-frame typed declaration API; the internal model keeps stable widget records, typed user state, style nodes, layout cache, animation channels, layer and hit-test data, font resources, and renderer resources across frames.

```text
External model: SdUi typed declaration API
Internal model: persistent runtime records
Render model: rebuild CPU draw packets every frame, reuse backend GPU resources
```

This document describes the current codebase. It is an implementation-aligned architecture document, not a future-only design note.

## 2. Current Implementation Scope

Implemented systems:

```text
Core          SdInstance, SdContext, frame flow, state/model storage, diagnostics
Widget        SdUi, SdWidgetTag, declaration API, basic widgets, phase contexts
Input         frame event buffer, input snapshot, mouse/keyboard/text/gamepad state
Style         typed stylesheet, compiled rule buckets, cascade, design tokens, style animation
Layout        SdBoxTree with block / inline-block / flex / absolute layout support
Layer         root layers, stacking contexts, portals, draw channels, hit-test, interaction routing
Render        SdRenderList, SdRenderData, SdRenderPacket, batches, clips, render layers, effects
Rhi           GPU device / command encoder / pipeline / resource set abstraction
Effects       blur, backdrop blur, drop shadow, inner shadow, mask, custom effect framework
Backends      Win32 platform, DirectX 11 renderer/RHI, DirectX 12 renderer/RHI, FreeType, STB font backend
Examples      Win32 + DX11, Win32 + DX12, DLL DX11 overlay
Tests         CoreTests for declaration, style, layout, paint, layer, model, and effect basics
```

## 3. High-Level Architecture

```text
Application / Host Renderer
  |
  v
User-owned SdInstance
  |
  +--> ISdPlatformBackend::StartFrame(SdInputSystem&)
  |
  +--> SdUi::Declare / DeclareKeyed / DeclareStyled / DeclareStyledKeyed
  |
  v
SdContext
  |
  +--> SdStateStorage        widget records, object store, user state, keyed models, style nodes
  +--> SdInputSystem         input event buffer and snapshot
  +--> SdInteractionSystem   hover, press, click, focus, capture, drag/drop routing
  +--> SdStyleSystem         typed stylesheet, tokens, cascade, style transitions
  +--> SdAnimationSystem     lifecycle and geometry animation channels
  +--> SdBoxTree             layout tree and used box output
  +--> SdLayerSystem         stacking, portals, draw channels, hit-test records
  +--> SdRenderList          frame-local draw commands and resource updates
  +--> SdEffectRegistry      built-in and custom effect handles
  |
  v
SdRenderPacket
  |
  v
ISdRendererBackend::Render(frameInfo, packet)
  |
  v
Dx11 / Dx12 renderer, optional RHI effect passes, backend-owned textures and buffers
```

The central rule is still:

```text
Whether user code declares a widget this frame
is not the same as
whether that widget's internal state is destroyed this frame.
```

## 4. Actual Directory Layout

```text
.
├── SodiumGUI.h                         # Aggregate public header
├── Core/                               # Base types, instance, runtime, storage, backend contracts, UTF-8, text
├── Widget/                             # SdUi, widget contexts, built-in widgets
├── Input/                              # Input events and snapshots
├── Style/                              # Style core, stylesheet, resolver, animation, default user-agent style
├── Layout/                             # SdBoxTree layout implementation
├── Layer/                              # Layers, stacking, portals, interaction routing
├── Animation/                          # Numeric animation channels
├── Render/                             # RenderList, RenderData, RenderStats, layer resolver
├── Rhi/                                # GPU device, command encoder, render graph, transient texture pool
├── Effects/                            # Built-in and custom effect framework
├── backends/
│   ├── Win32/                          # Win32 platform backend
│   ├── Dx11/                           # DX11 renderer and RHI device
│   ├── Dx12/                           # DX12 renderer and RHI device
│   ├── FreeType/                       # FreeType font backend
│   └── Stb/                            # stb_truetype font backend
├── examples/
│   ├── desktop/win32_directx11
│   ├── desktop/win32_directx12
│   └── dynamic/dll_dx11_win32
├── tests/CoreTests.cpp
├── scripts/Verify.ps1
└── docs/
```

## 5. Public API and Ownership

Application code owns `SdInstance` and backend objects. `SdInstance` does not own the native window, swap chain, or host render loop. It collects input, builds UI, produces a draw packet, and submits the packet to the renderer backend inside the host frame.

```cpp
Sodium::SdInstance gui = {};
Sodium::Backends::SdWin32Platform platform = {};
Sodium::Backends::SdDx11Renderer renderer = {};
Sodium::Backends::SdFreeTypeFontBackend fontBackend = {};

platform.Initialize(Sodium::Backends::SdWin32PlatformConfig(hwnd));
renderer.Initialize(Sodium::Backends::SdDx11RendererConfig(device, context));
fontBackend.Initialize();
gui.Initialize(platform, renderer, fontBackend);

gui.BeginFrame();
gui.ui.Declare<Sodium::SdButton>("Apply");
gui.EndFrame();
gui.Render();
```

`SdInstance::BeginFrame()` has two paths:

```text
BeginFrame()
- calls platform->StartFrame(input)
- updates the input snapshot from platform events

BeginFrame(SdVec2 displaySize)
- test or platform-less path
- directly sets display size
```

Shutdown order is controlled by the application. The intended order is `gui.Shutdown()`, then font, renderer, and platform shutdown.

## 6. Runtime Context

`SdContext` is the central runtime object in `Core/SdInstance.h`. Its main members are:

```text
SdFrameState              frame index, delta time, display size, diagnostics
SdRuntimeScratch          liveIds, paintIds, box index cache, display hidden cache
SdStateStorage            widget records, object store, style nodes, models
frameOrder                declaration order for the current frame
SdInputSystem             input events and snapshot
SdAnimationSystem         widget lifecycle / rect / scroll numeric animation
SdStyleAnimationChannels  style node presentation animation
SdStyleSystem             stylesheet, tokens, property registry
SdBoxTree                 frame layout tree
SdLayerSystem             draw / hit-test / portal / stacking records
SdInteractionSystem       pointer, focus, capture, drag/drop state
SdRenderSystem            renderer submission wrapper
SdRenderSharedData        font backend, white texture, effect handles, AA flags
SdRenderStats             render statistics
SdEffectRegistry          effect handle registry
backend pointers          platform, renderer, font backend
```

`SdFrameDiagnostics` exposes submitted/live/entering/leaving/dead widget counts, box node counts, draw command counts, batch counts, resource upload counts, model/style/object counts, style resolve cache stats, and style animation stats.

## 7. Declaration API, Anonymous IDs, and Keyed IDs

The public declaration surface is in `Widget/SdUi.h`:

```text
Declare<T>(args...)
DeclareKeyed<T>(key, args...)
DeclareStyled<T>(styleIdentity / inlineStyle, args...)
DeclareStyledKeyed<T>(key, styleIdentity / inlineStyle, args...)
Model<TWidget, TModel>(key, lifetime)
WidgetModel / ScopeModel / GlobalModel
ConfigureModel(...)
BeginPortal / EndPortal
```

`T` must satisfy:

```cpp
struct SdWidgetTag {};

template<class T>
concept SdDeclarableWidget =
    std::derived_from<T, SdWidgetTag>
    && std::is_default_constructible_v<T>;
```

Anonymous declarations use the current parent scope, widget type, and sibling ordinal to produce an `SdWidgetId`.

```cpp
ui.Declare<SdText>("Status");
ui.Declare<SdButton>("Apply");
```

Keyed declarations use parent scope, widget type, and an explicit key to produce a stable `SdWidgetId` and an `SdResolvedKey` for lookup and keyed models.

```cpp
ui.DeclareKeyed<SdTextInput>("username_input", value, "Username");
ui.DeclareStyledKeyed<SdPanel>("tools_panel", &panelStyle);
```

The key is framework identity, not widget label/title/text. In `DeclareKeyed<SdButton>("save_button", "Save")`, `"save_button"` is identity and `"Save"` is the widget argument.

## 8. Widget Records and Lifecycle

The runtime core is `SdWidgetRecord` plus `SdWidgetState`. `SdWidgetState` stores:

```text
id, lifePhase, rootLayer, targetTypeId
submittedThisFrame, inputEnabled
manualLayout, arrangeChildren, clipChildren
layoutDirty, styleDirty, animationActive
lastRect, targetRect, animatedRect, manualRect, childContentRect, computedClipRect
measuredSize, layoutWeight, opacity
stacking order/context fields
portalRoot, portalOwnerWidgetId, portalAnchorWidgetId, escapesParentClip
lastSubmittedFrame
```

Lifecycle:

```text
created first time      -> Entering
submitted + animation done -> Alive
not submitted this frame   -> Leaving, inputEnabled=false
leave animation reaches zero -> Dead
sweep stage              -> removes record, object, user state, style nodes, animation channels
```

`MarkSubmitted()` resets frame-transient fields such as root layer, manual layout, portal, clip, and stacking state. It also registers the resolved key and appends the record to frame order. A `Leaving` widget submitted again transitions back toward `Entering` from its current animation values.

## 9. StateStorage, Typed State, and Models

`SdStateStorage` currently stores widget records in `std::unordered_map<SdWidgetId, SdWidgetRecord>` and uses an internal object store for:

```text
widget object               T instance created by Declare<T>()
userStates                  typed state returned by context.State<T>()
typedStyles                 resolved / presentation / inline typed style
styleNodes                  root and part style nodes
modelBuckets                keyed models bucketed by SdResolvedKey
widgetIdByResolvedKey       debug/lookup mapping from resolved key to widget id
```

`State<T>()` is bound to the current widget record and is cleaned with the widget lifecycle.

```cpp
struct State { bool pressed = false; };
State& state = context.State<State>();
```

`Model<T>()` is bound to `SdResolvedKey`. It is meaningful for keyed widgets or explicit `ui.Model<TWidget>(key)` access. The implemented model lifetimes are:

```text
Widget    owned by a concrete widget id
Scope     owned by the current parent scope
Global    owned by the global key scope
Manual    default manual lifetime
```

Short-lived interaction state belongs in `State<T>()`. Data that should survive widget removal or be configured externally belongs in `Model<T>()`.

## 10. Widget Callbacks and Contexts

Widgets are normal C++ types, not virtual runtime objects. `SdUi::DeclareResolved()` detects optional callbacks with C++20 `requires`:

```text
OnCreate(SdCreateContext&, args...)
OnUpdate(SdUpdateContext&, args...)
OnLayout(SdLayoutContext&)
OnArrange(SdArrangeContext&)
OnPaint(SdPaintContext&)
```

`OnCreate` runs only when the widget object is newly created or the stored type changes. `OnUpdate` runs during declaration and may submit child widgets. `OnLayout` writes desired size. `OnArrange` runs after layout boxes exist and lets complex widgets place internal parts. `OnPaint` writes commands to `SdRenderList`.

Contexts expose:

```text
instance, ui, id, parentId, widgetState, theme, resolvedKey
State<T>() / Model<T>()
RootStyleNode() / Part(part) / EnsurePart(part)
SetPartUsedBox / SetPartLayoutBox / SetPartBorderBox
RootResolvedStyle<TWidget>() / RootPresentationStyle<TWidget>()
IsHovered / IsPressed / WasClicked / WasDoubleClicked / WasLongPressed
IsWheelTarget / IsKeyboardTarget / IsDragSource / IsDragTarget / IsDropTarget
IsCaptured / IsFocused
```

## 11. Built-In Widgets

Built-in widgets live in `Widget/SdBasicWidgets.h`:

```text
SdText
SdPanel
SdButton
SdCheckBox
SdSliderFloat
SdTextInput
SdWindow
SdImageViewer
SdScrollView
SdPopup
SdContextMenu
SdTooltip
```

Each is an `SdWidgetTag` type and uses `TargetTypeId` to participate in the default user-agent stylesheet. Complex controls expose style parts:

```text
Button      Content / Icon / Label
CheckBox    Box / Indicator / Label
Slider      Label / Track / Fill / Thumb
TextInput   Field / Value / Placeholder / Selection / Caret
Window      Titlebar / Title / CloseButton / Content / ResizeHandle
ScrollView  Scrollbar / Thumb
```

Built-in widgets compute base size in `OnLayout`, set part boxes in `OnArrange`, and output rectangles, text, images, shadows, blur, and related commands through `SdRenderList` in `OnPaint`.

## 12. Style System

The style system is a strongly typed C++ stylesheet system, not a runtime CSS string parser. Core types:

```text
SdBoxStyle              display, position, zIndex, overflow, box sizing, sizes, margin, padding, border,
                        radius, color, background, opacity, font, flex, gap, transform, transitions
SdWidgetRootStyle       widget root style, derives from SdBoxStyle
SdWidgetPartStyle       part style, derives from SdBoxStyle and adds inheritText
SdStyleNode             root/part runtime node with specified/resolved/presentation/used/layout boxes
SdStyleSheet            typed rule builder
SdCompiledStyleSheet    compiled rule buckets
SdStyleSystem           tokens, property registry, default sheet, resolve, transition lookup
```

Selector dimensions:

```text
targetTag
part
pseudoState
rootLayer
class
scope
```

Cascade layers:

```text
UserAgent < Theme < User < Scoped < Inline
```

Within a layer, rules are selected by specificity and source order. The default user-agent stylesheet covers global, Panel, Window, Button, CheckBox, Slider, TextInput, ImageViewer, ScrollView, Popup, ContextMenu, and Tooltip targets.

Design tokens are stored in `SdDesignTokenSet`. Current defaults cover text/background/window/panel/button/accent/border/danger/selection colors and spacing, font, radius, and duration metrics.

Style value phases:

```text
specified      target value after cascade
resolved       token / variable / length variable resolved value
presentation   value after style animation, used by paint
used/layout     box geometry written during layout
```

`SdStyleAnimationChannels` creates sparse channels by style node and property id. It supports color, float, spacing, and vec2 interpolation and tracks layout/paint/composite impact.

## 13. Layout and Arrange

Layout is implemented by `SdBoxTree` in `Layout/SdBoxLayout.h`. Each live widget produces one box node:

```text
styleNodeId
parentBoxIndex / firstChildIndex / nextSiblingIndex
display / position
marginBox / borderBox / paddingBox / contentBox
explicitBorderBox
intrinsicSize
resolved used style values
```

Layout stage:

```text
1. Collect non-Dead widgets and sort by declaration order.
2. Resolve style for each widget.
3. Call OnLayout to get desiredSize.
4. Add root style, desiredSize, parent box, and manual rect to SdBoxTree.
5. SdBoxTree::Layout(displayRect) computes used boxes.
6. Write targetRect, childContentRect, layout cache, and root style node used/layout boxes.
7. Call OnArrange so widgets can set internal part boxes.
8. Set rectX/rectY/rectWidth/rectHeight animation targets.
```

`SdBoxTree` currently supports block, inline-block, flex row/column, gap, justifyContent, alignItems, flexGrow/flexShrink/flexBasis, box sizing, margin, padding, border, and absolute/manual layout.

Important implementation detail: `layoutWeight` currently exists as a widget lifecycle animation value, but the box layout solver does not scale occupied layout space by `layoutWeight`. Smooth structure changes currently come from rect animation, opacity, and widget-specific open/visible state. This differs from the old design document.

## 14. Layer, Portals, and Interaction

The layer system is more than a root layer enum. Current types include:

```text
SdStackingKey
SdStackingContextNode
SdPortalRecord
SdLayerDrawRecord
SdHitTestRecord
SdLayerDrawChannel
SdLayerSystem
SdInteractionSystem
```

`SdRootLayer` contains Content, Floating, Popup, Tooltip, Modal, and related layers. Widgets may also create stacking contexts through root layer, zIndex, opacity, or portal state.

Portals are declared with `SdUi::BeginPortal(root, owner, anchor)` / `EndPortal()`. During declaration they write:

```text
portalRoot
portalOwnerWidgetId
portalAnchorWidgetId
escapesParentClip
```

`SdLayerSystem::Finalize()` sorts draw records and hit-test records and builds draw channels. `SdInteractionSystem` uses the previous finalized layer/hit-test data plus the current input snapshot to compute hover, press, release, click, double-click, long-press, outside-click, wheel target, keyboard target, focus, capture, and drag/drop state.

## 15. Input

Input uses an event buffer plus snapshot model. `ISdPlatformBackend::StartFrame(SdInputSystem&)` transfers platform events into the input system. The Win32 backend maps window messages to mouse, keyboard, text, resize, focus, and quit events.

`SdInputSnapshot` stores:

```text
mouse
keyboard
textInput
display
gamepads
events
frameIndex
```

`SdInstance::BeginInputFrame()` increments frame index, computes delta time, and starts the input frame. `FinishInputAndBeginUiFrame()` finalizes input and updates the interaction system using layer/hit-test data.

## 16. Render, RHI, and Effects

`SdRenderList` is the frame-local drawing API used by widget paint. It builds an `SdRenderPacket`:

```text
SdRenderCommandBuffer     DrawBatch, PushClipRect, PopClip, Begin/EndRenderLayer, ApplyEffect
vertices                  SdVertex(position, uv, color)
indices                   uint32 indices
batches                   texture + clip + vertex/index ranges
effectParameters          effect command parameter bytes
resourceUpdates           texture/font atlas upload requests
effectRegistry            effect handle resolver
packetVersion             usually frame index
```

Renderer backend contract:

```cpp
virtual void Render(const SdRendererFrameInfo& frameInfo,
                    const SdRenderPacket& packet) = 0;
virtual SdTextureHandle CreateTexture(const SdTextureDesc& desc) = 0;
virtual void DestroyTexture(SdTextureHandle texture) = 0;
virtual Rhi::ISdGpuDevice* GetRhiDeviceInterface() noexcept;
```

Renderer backends submit UI only. They do not clear or present the host swap chain. They are expected to save and restore host pipeline state, reuse GPU vertex/index buffers, grow capacity only when required, and process packet resource updates before drawing.

RHI lives in `Rhi/` and provides:

```text
ISdGpuDevice              texture/buffer/shader/sampler/layout/resource set/pipeline creation and destruction
ISdCommandEncoder         render pass, pipeline, resource set, buffer, viewport, scissor, draw
SdRenderGraph             simple render graph and transient texture pool
```

Effects are managed by `SdEffectRegistry`. `SdInstance` registers blur, backdrop blur, drop shadow, inner shadow, and mask in its constructor. If the renderer exposes an RHI device, `Initialize()` initializes the effect registry.

## 17. Fonts and Text

The font interface is `ISdFontBackend` in `Core/SdText.h`. It is responsible for:

```text
fallback font
MeasureText
BuildParagraphLayout
ConfigureRenderSharedData
DrainPendingUploads
```

Current font backends:

```text
backends/FreeType     FreeType implementation
backends/Stb          stb_truetype implementation
```

`SdRenderSharedData` stores the font backend, white texture, white pixel UV, baked line UVs, fast arc samples, and built-in effect handles. Font backends use `DrainPendingUploads()` to append atlas updates to the render list upload requests.

## 18. Backend Contracts

Platform backend:

```cpp
class ISdPlatformBackend
{
public:
    virtual bool IsInitialized() const noexcept = 0;
    virtual void StartFrame(SdInputSystem& input) = 0;
};
```

Renderer backend:

```cpp
class ISdRendererBackend
{
public:
    virtual bool IsInitialized() const noexcept = 0;
    virtual void Render(const SdRendererFrameInfo& frameInfo,
                        const SdRenderPacket& packet) = 0;
    virtual Rhi::ISdGpuDevice* GetRhiDeviceInterface() noexcept;
    virtual SdTextureHandle CreateTexture(const SdTextureDesc& desc) = 0;
    virtual void DestroyTexture(SdTextureHandle texture) = 0;
};
```

Font backend:

```cpp
class ISdFontBackend
{
public:
    virtual bool IsInitialized() const noexcept = 0;
    virtual SdFontHandle GetFallbackFont() const noexcept = 0;
    virtual SdVec2 MeasureText(SdUtf8StringView text, const SdTextStyle& style) = 0;
    virtual SdParagraphLayout BuildParagraphLayout(...) = 0;
    virtual void ConfigureRenderSharedData(SdRenderSharedData& sharedData) const = 0;
    virtual void DrainPendingUploads(std::vector<SdUploadRequest>& uploads) = 0;
};
```

## 19. Frame Flow

Current frame flow:

```text
BeginFrame
1. Increment frameIndex and compute deltaTime.
2. input.BeginFrame(frameIndex).
3. Platform path calls platform->StartFrame(input); test path directly writes display size.
4. input.FinalizeFrame().
5. interactionSystem.Update(layerSystem, inputSnapshot, frameIndex).
6. renderList.Reset(), layerSystem.BeginFrame().
7. Clear frameOrder and nextOrder, stateStorage.BeginFrame().
8. Reset presentation/style/frame diagnostics.
9. ui.BeginDeclarationFrame(), resetting id stack and portal stack.

Declaration
1. Resolve anonymous/keyed widget id.
2. GetOrCreateWidgetRecord(id).
3. If the type changed or no object exists, create the widget object and install style/layout/arrange/paint thunks.
4. MarkSubmitted(), registering parent, order, resolved key, debug key, and frame-transient state.
5. Apply portal frame and style identity / inline style.
6. If created, call OnCreate.
7. Call OnUpdate inside SdIdScope so nested declarations get a parent scope.

EndFrame
1. EndDeclarationStage: non-submitted widgets enter Leaving.
2. RunLifecycleAnimationStage: update layoutWeight/opacity and switch Alive/Dead.
3. RunLayoutAndPaintStage: style, layout, arrange, rect animation, style animation, layer, hit-test, paint.
4. Drain font uploads into the render list.
5. RunSweepStage: remove Dead widgets.
6. RefreshDiagnostics.
7. Clear submittedThisFrame.

Render
1. renderList.BuildPacket(frameIndex, effectRegistry).
2. renderSystem.Render(renderer, frameInfo, packet).
3. Renderer backend uploads resource updates, vertices, indices, and submits draw/effect commands.
```

## 20. Performance and Diagnostics

Current performance strategy:

```text
1. Widget records, typed widget objects, user state, and models persist across frames.
2. RenderList / RenderData clear every frame but keep vector capacity.
3. SdBoxTree uses vectors and index links, not a shared_ptr tree.
4. Stylesheets compile into rule buckets; resolve visits candidate buckets.
5. Style properties use property ids and pointer-to-member metadata.
6. Widget lifecycle, rect, scroll, and style transitions use sparse animation channels.
7. Renderer backends reuse GPU buffers, textures, descriptors/resource sets, and generation handles.
8. Font atlas updates are batched as upload requests in render packets.
9. FrameDiagnostics and RenderStats expose runtime counters for tests and regressions.
```

## 21. Differences From the Previous Architecture Document

```text
1. The directory layout is now actual: Rhi, Effects, and Stb backend exist; Dx9/OpenGL backends do not.
2. Backend interfaces have landed as ISdPlatformBackend / ISdRendererBackend / ISdFontBackend.
   The renderer consumes SdRenderPacket, not the earlier simplified RenderData interface.
3. The public UI API includes keyed, styled, styled keyed, model, and portal APIs, not only Declare<T>().
4. Widget phases include OnArrange; built-in composite controls use Arrange for part boxes.
5. StateStorage is widget record + typed object store + style nodes + typed styles + keyed model buckets.
6. Models implement SdModelLifetime: Widget / Scope / Global / Manual.
7. Style implements typed stylesheets, compiled buckets, design tokens, property registry, and style node presentation animation.
   There is no runtime CSS parser yet.
8. Layout implements SdBoxTree block/flex/absolute basics. The old design rule where layoutWeight scales leaving layout space is not wired into box layout yet.
9. Layer has stacking contexts, portals, draw channels, hit-test records, and interaction routing, not only root layer ordering.
10. Render data now includes command buffers, render layers, effect commands, effect parameters, and resource updates.
11. RHI and Effects are part of the actual architecture. DX11/DX12 renderers provide both renderer backend and RHI device capabilities.
12. Font backend design is centered on paragraph layout, shared render data, and pending uploads, with FreeType and STB implementations.
13. Built-in widgets now include TextInput, ImageViewer, ScrollView, Popup, ContextMenu, and Tooltip.
14. Frame diagnostics are part of Core API for observing widget/style/layout/render state.
```

## 22. Current Limits and Follow-Up Areas

Observed follow-up areas:

```text
1. layoutWeight is not currently used as structural occupied-size input in SdBoxTree.
2. Style is a typed rule builder and compiled sheet system; there is no CSS text parser/offline compiler yet.
3. Text shaping depends on current font backend capability; complex shaping is not a core feature yet.
4. RenderGraph/RHI exists, but the main UI render path is still renderer-backend packet consumption.
5. Backend coverage is Windows + DirectX focused. OpenGL/Vulkan/Metal backends are not implemented.
6. More examples and user-facing documentation are still needed as the API stabilizes.
```

## 23. Final Model Summary

The actual model is:

```text
The application owns SdInstance and backends.
Each frame declares widgets through SdUi.
Declarations resolve to stable SdWidgetId / SdResolvedKey values.
SdStateStorage preserves widget records, typed objects, state, style nodes, and models.
EndFrame runs lifecycle, style, layout, arrange, animation, layer sorting, hit-test data generation, and visible widget paint.
Paint writes to SdRenderList.
RenderList builds SdRenderPacket.
Renderer backend submits the packet inside the host render loop and does not own clear/present.
```

This keeps the low boilerplate of immediate-style declaration while supporting persistent state, style, animation, layers, fonts, and GPU resource reuse.
