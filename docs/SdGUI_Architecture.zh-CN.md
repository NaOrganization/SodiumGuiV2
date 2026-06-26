# SdGUI 架构设计

版本：v0.5
实现快照：2026-06-26
目标标准：C++20
项目名称：SodiumGUI
简称：SdGUI

## 1. 概览

SdGUI 是一个 C++20 GUI 框架。它的公开使用方式是逐帧声明 UI，内部实现则保留稳定的 widget record、typed user state、style node、layout cache、animation channel、layer/hit-test 数据、字体资源和 renderer 资源。

```text
外部模型：SdUi 类型化声明 API
内部模型：跨帧持久化运行时记录
渲染模型：每帧重建 CPU draw packet，后端复用 GPU 资源
```

当前代码已经从早期设计文档的概念稿推进到可运行实现。本文按仓库实际代码重写，重点描述现在已经存在的模块、数据流和边界，而不是未来规划。

## 2. 当前实现范围

已实现的主要系统：

```text
Core          SdInstance、SdContext、frame flow、state/model storage、diagnostics
Widget        SdUi、SdWidgetTag、声明 API、基础 widget、phase context
Input         frame event buffer、input snapshot、mouse/keyboard/text/gamepad 状态
Style         typed stylesheet、compiled rule bucket、cascade、design token、style animation
Layout        SdBoxTree，支持 block / inline-block / flex / absolute 基础布局
Layer         root layer、stacking context、portal、draw channel、hit-test、interaction routing
Render        SdRenderList、SdRenderData、SdRenderPacket、batch、clip、render layer、effect command
Rhi           跨后端 GPU device / command encoder / pipeline / resource set 抽象
Effects       blur、backdrop blur、drop shadow、inner shadow、mask、custom effect 注册和应用框架
Backends      Win32 platform、DirectX 11 renderer/RHI、DirectX 12 renderer/RHI、FreeType、STB font backend
Examples      Win32 + DX11、Win32 + DX12、DLL DX11 overlay 示例
Tests         CoreTests 覆盖核心声明、样式、布局、绘制、layer、model、effect 基础行为
```

## 3. 高层架构

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
  +--> SdStateStorage        widget record / object store / user state / keyed model / style node
  +--> SdInputSystem         input event buffer and snapshot
  +--> SdInteractionSystem   hover / press / click / focus / capture / drag-drop routing
  +--> SdStyleSystem         typed stylesheet, tokens, cascade, style transition
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

核心原则仍然是：

```text
用户本帧是否声明 widget
不等同于
该 widget 的内部状态是否立即销毁
```

## 4. 实际目录结构

```text
.
├── SodiumGUI.h                         # 聚合头文件
├── Core/                               # 基础类型、实例、运行时、storage、backend contract、UTF-8、text
├── Widget/                             # SdUi、widget context、基础 widget
├── Input/                              # input event 和 snapshot 系统
├── Style/                              # style core、stylesheet、resolver、animation、默认 user-agent 样式
├── Layout/                             # SdBoxTree 布局实现
├── Layer/                              # layer、stacking、portal、interaction routing
├── Animation/                          # numeric animation channel
├── Render/                             # RenderList、RenderData、RenderStats、layer resolver
├── Rhi/                                # GPU device / command encoder / render graph / transient texture pool
├── Effects/                            # built-in 和 custom effect 框架
├── backends/
│   ├── Win32/                          # Win32 platform backend
│   ├── Dx11/                           # DX11 renderer 和 RHI device
│   ├── Dx12/                           # DX12 renderer 和 RHI device
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

## 5. 公开 API 与所有权

应用显式拥有 `SdInstance` 和 backend 对象。`SdInstance` 不创建窗口、交换链或宿主 render loop，它只在宿主帧流程中收集输入、构建 UI、生成 draw packet 并提交给 renderer backend。

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

`SdInstance::BeginFrame()` 有两条路径：

```text
BeginFrame()
- 调用 platform->StartFrame(input)
- 用平台事件更新 input snapshot

BeginFrame(SdVec2 displaySize)
- 测试或无平台场景直接设置 display size
- 不依赖 platform backend
```

关闭顺序由应用控制。推荐先 `gui.Shutdown()`，再关闭 font、renderer、platform。

## 6. Runtime Context

`SdContext` 是当前运行时中心对象，位于 `Core/SdInstance.h`。主要成员：

```text
SdFrameState              frame index、delta time、display size、diagnostics
SdRuntimeScratch          liveIds、paintIds、box index cache、display hidden cache
SdStateStorage            widget records、object store、style nodes、models
frameOrder                本帧提交顺序
SdInputSystem             input events and snapshot
SdAnimationSystem         widget lifecycle / rect / scroll numeric animation
SdStyleAnimationChannels  style node presentation animation
SdStyleSystem             stylesheet、tokens、property registry
SdBoxTree                 frame layout tree
SdLayerSystem             draw / hit-test / portal / stacking records
SdInteractionSystem       pointer、focus、capture、drag/drop state
SdRenderSystem            renderer submission wrapper
SdRenderSharedData        font backend、white texture、effect handles、AA flags
SdRenderStats             render statistics
SdEffectRegistry          effect handle registry
backend pointers          platform、renderer、font backend
```

`SdFrameDiagnostics` 记录 submitted/live/entering/leaving/dead widget 数、box node 数、draw command 数、batch 数、resource upload 数、model/style/object 数、style resolve cache 统计和 style animation 统计。

## 7. 声明 API、匿名 ID 与 Keyed ID

公开声明入口在 `Widget/SdUi.h`：

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

`T` 必须满足：

```cpp
struct SdWidgetTag {};

template<class T>
concept SdDeclarableWidget =
    std::derived_from<T, SdWidgetTag>
    && std::is_default_constructible_v<T>;
```

匿名声明通过当前 parent scope、widget type 和 sibling ordinal 生成 `SdWidgetId`。它适合静态结构或顺序稳定的局部控件。

```cpp
ui.Declare<SdText>("Status");
ui.Declare<SdButton>("Apply");
```

keyed 声明通过 parent scope、widget type 和显式 key 生成稳定 `SdWidgetId`，并登记 `SdResolvedKey`，用于稳定寻址和 keyed model。

```cpp
ui.DeclareKeyed<SdTextInput>("username_input", value, "Username");
ui.DeclareStyledKeyed<SdPanel>("tools_panel", &panelStyle);
```

key 是框架身份，不是控件 label/title/text。`DeclareKeyed<SdButton>("save_button", "Save")` 中 `"save_button"` 是 identity，`"Save"` 才是 widget 参数。

## 8. Widget Record 与生命周期

运行时记录的核心是 `SdWidgetRecord` 和 `SdWidgetState`。`SdWidgetState` 包含：

```text
id、lifePhase、rootLayer、targetTypeId
submittedThisFrame、inputEnabled
manualLayout、arrangeChildren、clipChildren
layoutDirty、styleDirty、animationActive
lastRect、targetRect、animatedRect、manualRect、childContentRect、computedClipRect
measuredSize、layoutWeight、opacity
stacking order/context 字段
portalRoot、portalOwnerWidgetId、portalAnchorWidgetId、escapesParentClip
lastSubmittedFrame
```

生命周期：

```text
第一次创建         -> Entering
提交且动画完成     -> Alive
本帧未提交         -> Leaving，inputEnabled=false
离场动画归零       -> Dead
Sweep 阶段         -> 删除 widget record、对象、user state、style node，并移除 animation channel
```

`MarkSubmitted()` 每帧重置大量 frame-transient 字段，例如 root layer、manual layout、portal、clip、stacking context，并把 record 加入 `frameOrder`。如果一个 `Leaving` widget 又被提交，会回到 `Entering`，继续从当前 animation 值过渡。

## 9. StateStorage、Typed State 与 Model

`SdStateStorage` 当前使用 `std::unordered_map<SdWidgetId, SdWidgetRecord>` 保存 widget record，同时通过内部 object store 管理：

```text
widget object               Declare<T>() 创建的 T 实例
userStates                  context.State<T>() 的 typed state
typedStyles                 resolved / presentation / inline typed style
styleNodes                  root style node 和 part style node
modelBuckets                keyed model，按 SdResolvedKey 分桶
widgetIdByResolvedKey       resolved key 到 widget id 的调试/寻址映射
```

`State<T>()` 绑定当前 widget record，跟随 widget 生命周期清理。

```cpp
struct State { bool pressed = false; };
State& state = context.State<State>();
```

`Model<T>()` 绑定 `SdResolvedKey`。只有 keyed widget 或通过 `ui.Model<TWidget>(key)` 显式访问的场景才有稳定模型语义。当前支持四种生命周期：

```text
Widget    归属具体 widget id
Scope     归属当前 parent scope
Global    归属全局 key scope
Manual    默认手动生命周期
```

这解决了“widget record 离场后，业务配置是否保留”的问题。交互瞬时状态放在 `State<T>()`，跨显示周期或外部配置放在 `Model<T>()`。

## 10. Widget Callback 与 Context

Widget 是普通 C++ 类型，不使用虚函数基类。`SdUi::DeclareResolved()` 使用 C++20 `requires` 检测可选 callback：

```text
OnCreate(SdCreateContext&, args...)
OnUpdate(SdUpdateContext&, args...)
OnLayout(SdLayoutContext&)
OnArrange(SdArrangeContext&)
OnPaint(SdPaintContext&)
```

`OnCreate` 只在 widget object 新建或类型变化时调用。`OnUpdate` 在声明阶段立即调用，允许继续嵌套声明子 widget。`OnLayout` 写入 desired size。`OnArrange` 在 layout box 已产生后给复杂 widget 安排内部 part。`OnPaint` 写入 `SdRenderList`。

Context 提供：

```text
instance、ui、id、parentId、widgetState、theme、resolvedKey
State<T>() / Model<T>()
RootStyleNode() / Part(part) / EnsurePart(part)
SetPartUsedBox / SetPartLayoutBox / SetPartBorderBox
RootResolvedStyle<TWidget>() / RootPresentationStyle<TWidget>()
IsHovered / IsPressed / WasClicked / WasDoubleClicked / WasLongPressed
IsWheelTarget / IsKeyboardTarget / IsDragSource / IsDragTarget / IsDropTarget
IsCaptured / IsFocused
```

## 11. 内置 Widget

当前内置 widget 位于 `Widget/SdBasicWidgets.h`：

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

这些 widget 都是 `SdWidgetTag` 类型，并通过 `TargetTypeId` 接入默认 user-agent stylesheet。复杂控件会暴露 style part，例如：

```text
Button      Content / Icon / Label
CheckBox    Box / Indicator / Label
Slider      Label / Track / Fill / Thumb
TextInput   Field / Value / Placeholder / Selection / Caret
Window      Titlebar / Title / CloseButton / Content / ResizeHandle
ScrollView  Scrollbar / Thumb
```

内置 widget 在 `OnLayout` 计算基础尺寸，在 `OnArrange` 设置 part 的 used/layout box，在 `OnPaint` 通过 `SdRenderList` 输出 rect、text、image、shadow、blur 等绘制命令。

## 12. Style 系统

Style 是强类型 C++ stylesheet，而不是运行时 CSS 字符串解析器。核心类型：

```text
SdBoxStyle              display、position、zIndex、overflow、box sizing、尺寸、margin、padding、border、radius、
                        color、background、opacity、font、flex、gap、transform、transitions
SdWidgetRootStyle       widget root style，继承 SdBoxStyle
SdWidgetPartStyle       part style，继承 SdBoxStyle，并带 inheritText
SdStyleNode             root/part runtime node，保存 specified/resolved/presentation/used/layout box
SdStyleSheet            typed rule builder
SdCompiledStyleSheet    编译后的 rule bucket
SdStyleSystem           token、property registry、default sheet、resolve 和 transition 查询
```

Selector 维度：

```text
targetTag
part
pseudoState
rootLayer
class
scope
```

Cascade layer：

```text
UserAgent < Theme < User < Scoped < Inline
```

同层规则按 specificity 和 source order 选择。默认样式由 `SdDefaultUserAgentStyleSheet()` 生成，覆盖全局、Panel、Window、Button、CheckBox、Slider、TextInput、ImageViewer、ScrollView、Popup、ContextMenu 和 Tooltip。

Design token 由 `SdDesignTokenSet` 提供，当前包括 text/background/window/panel/button/accent/border/danger/selection 颜色，以及 spacing、font、radius、duration 等 metric。

Style value pipeline：

```text
specified      cascade 后的目标值
resolved       token / variable / length variable 解析后的值
presentation   style animation 后实际绘制的值
used/layout     layout 阶段写入 SdStyleNode 的 box geometry
```

`SdStyleAnimationChannels` 按 style node 和 property id 稀疏创建 channel，当前支持 color、float、spacing、vec2 等插值，并记录 layout/paint/composite impact。

## 13. Layout 与 Arrange

布局实现是 `Layout/SdBoxLayout.h` 中的 `SdBoxTree`。每个 live widget 产生一个 box node：

```text
styleNodeId
parentBoxIndex / firstChildIndex / nextSiblingIndex
display / position
marginBox / borderBox / paddingBox / contentBox
explicitBorderBox
intrinsicSize
resolved used style values
```

布局阶段流程：

```text
1. 收集非 Dead widget，并按提交 order 排序。
2. 对每个 widget resolve style。
3. 调用 OnLayout 得到 desiredSize。
4. 把 root style、desiredSize、parent box、manual rect 写入 SdBoxTree。
5. SdBoxTree::Layout(displayRect) 计算 used boxes。
6. 回写 targetRect、childContentRect、layoutCache 和 root style node used/layout box。
7. 调用 OnArrange，让 widget 设置内部 part 的 layout box。
8. 为 rectX/rectY/rectWidth/rectHeight 设置动画目标。
```

当前 `SdBoxTree` 支持 block、inline-block、flex row/column、gap、justifyContent、alignItems、flexGrow/flexShrink/flexBasis、box sizing、margin、padding、border、absolute/manual layout 等基础能力。

需要注意：当前代码里的 `layoutWeight` 是 widget 生命周期动画值，但 box layout 并没有按 `layoutWeight` 缩放占用尺寸。结构平滑主要由 rect 动画、opacity 和控件自身 open/visible 状态实现。这一点与旧架构文档的“离场 widget 按 layoutWeight 缩放布局空间”不同。

## 14. Layer、Portal 与 Interaction

Layer 系统不只是 root layer enum。当前包含：

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

Root layer 由 `SdRootLayer` 定义，包含 Content、Floating、Popup、Tooltip、Modal 等层级。Widget 也可以通过 `context.widgetState.rootLayer`、`zIndex`、opacity、portal 状态产生 stacking context。

Portal 通过 `SdUi::BeginPortal(root, owner, anchor)` / `EndPortal()` 影响声明期间的 widget record：

```text
portalRoot
portalOwnerWidgetId
portalAnchorWidgetId
escapesParentClip
```

`SdLayerSystem::Finalize()` 会排序 draw records、hit-test records 并建立 draw channels。`SdInteractionSystem` 基于上一帧 finalized layer/hit-test 信息和当前 input snapshot 计算 hover、press、release、click、double-click、long-press、outside-click、wheel target、keyboard target、focus、capture、drag/drop 状态。

## 15. Input

Input 使用事件缓冲和快照模型。`ISdPlatformBackend::StartFrame(SdInputSystem&)` 把平台事件写入 input system。Win32 backend 负责把窗口消息转换为鼠标、键盘、文本、resize、focus、quit 等事件。

`SdInputSnapshot` 保存：

```text
mouse
keyboard
textInput
display
gamepads
events
frameIndex
```

`SdInstance::BeginInputFrame()` 增加 frame index、计算 delta time 并开始 input frame。`FinishInputAndBeginUiFrame()` finalize input，然后用 layer/hit-test 数据更新 interaction system。

## 16. Render、RHI 与 Effects

`SdRenderList` 是 widget paint 阶段写入的 frame-local 绘制入口。它最终构建 `SdRenderPacket`：

```text
SdRenderCommandBuffer     DrawBatch、PushClipRect、PopClip、Begin/EndRenderLayer、ApplyEffect
vertices                  SdVertex(position, uv, color)
indices                   uint32 indices
batches                   texture + clip + vertex/index range
effectParameters          effect command 的参数字节
resourceUpdates           texture/font atlas upload requests
effectRegistry            effect handle resolver
packetVersion             通常使用 frame index
```

Renderer backend contract 是：

```cpp
virtual void Render(const SdRendererFrameInfo& frameInfo,
                    const SdRenderPacket& packet) = 0;
virtual SdTextureHandle CreateTexture(const SdTextureDesc& desc) = 0;
virtual void DestroyTexture(SdTextureHandle texture) = 0;
virtual Rhi::ISdGpuDevice* GetRhiDeviceInterface() noexcept;
```

渲染后端只提交 UI，不负责 clear/present。它需要保存并恢复宿主 pipeline state，复用 GPU vertex/index buffer，仅在容量不足时增长，并在绘制前处理 packet 内的 resource updates。

RHI 位于 `Rhi/`，提供：

```text
ISdGpuDevice              texture/buffer/shader/sampler/layout/resource set/pipeline 创建与销毁
ISdCommandEncoder         render pass、pipeline、resource set、buffer、viewport、scissor、draw
SdRenderGraph             简单 render graph 和 transient texture pool
```

Effect 系统通过 `SdEffectRegistry` 管理 handle 和 effector。`SdInstance` 构造时注册 blur、backdrop blur、drop shadow、inner shadow、mask。若 renderer backend 暴露 RHI device，`Initialize()` 会初始化 effect registry。

## 17. Font 与 Text

字体接口是 `ISdFontBackend`，位于 `Core/SdText.h`。它负责：

```text
fallback font
MeasureText
BuildParagraphLayout
ConfigureRenderSharedData
DrainPendingUploads
```

当前有两个 font backend：

```text
backends/FreeType     FreeType 实现
backends/Stb          stb_truetype 实现
```

`SdRenderSharedData` 持有 font backend、white texture、white pixel uv、baked line uv、fast arc samples 和 built-in effect handles。字体 backend 通过 `DrainPendingUploads()` 把 atlas 更新写入 render list 的 upload requests。

## 18. Backend Contract

平台 backend：

```cpp
class ISdPlatformBackend
{
public:
    virtual bool IsInitialized() const noexcept = 0;
    virtual void StartFrame(SdInputSystem& input) = 0;
};
```

Renderer backend：

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

Font backend：

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

当前实际帧流程：

```text
BeginFrame
1. frameIndex++，计算 deltaTime。
2. input.BeginFrame(frameIndex)。
3. 平台路径调用 platform->StartFrame(input)，测试路径直接写 display size。
4. input.FinalizeFrame()。
5. interactionSystem.Update(layerSystem, inputSnapshot, frameIndex)。
6. renderList.Reset()，layerSystem.BeginFrame()。
7. 清空 frameOrder、nextOrder，stateStorage.BeginFrame()。
8. reset presentation/style/frame diagnostics。
9. ui.BeginDeclarationFrame()，重置 id stack 和 portal stack。

Declaration
1. Resolve anonymous/keyed widget id。
2. GetOrCreateWidgetRecord(id)。
3. 若类型变化或 object 不存在，创建 widget object 并安装 style/layout/arrange/paint thunk。
4. MarkSubmitted()，登记 parent、order、resolved key、debug key，并重置 frame-transient state。
5. 应用 portal frame 和 style identity / inline style。
6. created 时调用 OnCreate。
7. 调用 OnUpdate，并通过 SdIdScope 允许嵌套声明。

EndFrame
1. EndDeclarationStage：未提交 widget 进入 Leaving。
2. RunLifecycleAnimationStage：更新 layoutWeight/opacity，切换 Alive/Dead。
3. RunLayoutAndPaintStage：resolve style、layout、arrange、rect animation、style animation、layer、hit-test、paint。
4. Drain font uploads 到 render list。
5. RunSweepStage：删除 Dead widget。
6. RefreshDiagnostics。
7. 清除 submittedThisFrame 标志。

Render
1. renderList.BuildPacket(frameIndex, effectRegistry)。
2. renderSystem.Render(renderer, frameInfo, packet)。
3. renderer backend 上传资源更新、顶点、索引，提交 draw/effect 命令。
```

## 20. 性能原则与诊断

当前实现的性能策略：

```text
1. Widget record、typed widget object、user state、model 跨帧保留。
2. RenderList / RenderData 每帧 clear，但 vector capacity 保留。
3. SdBoxTree 使用 vector 和 index 链接，不构建 shared_ptr tree。
4. Style sheet 预编译为 rule bucket，resolve 时只访问候选 bucket。
5. Style property 通过 property id / pointer-to-member metadata 读写。
6. Widget lifecycle、rect、scroll 和 style transition 使用稀疏 animation channel。
7. Renderer backend 复用 GPU buffer、texture、descriptor/resource set，并处理 generation handle。
8. Font atlas 更新通过 upload request 批量进入 render packet。
9. FrameDiagnostics 和 RenderStats 暴露关键统计，便于测试与性能回归定位。
```

## 21. 与旧架构文档的主要差异

```text
1. 目录结构从“推荐布局”变成实际布局：新增 Rhi、Effects、Stb backend；没有 Dx9/OpenGL backend。
2. Backend interface 已落地为 ISdPlatformBackend / ISdRendererBackend / ISdFontBackend；
   renderer 消费 SdRenderPacket，不再是旧文档里的简化 RenderData 接口。
3. Public UI API 不止 Declare<T>()，还包括 keyed、styled、styled keyed、model 和 portal API。
4. Widget phase 新增 OnArrange；复杂内置控件使用 Arrange 设置 part box。
5. StateStorage 不只是 widget state map，而是 widget record + typed object store + style node + typed style + keyed model bucket。
6. Model 已实现 SdModelLifetime：Widget / Scope / Global / Manual。
7. Style 系统已实现 typed stylesheet、compiled buckets、design tokens、property registry、style node presentation animation；
   当前没有运行时 CSS parser。
8. Layout 已实现 SdBoxTree 的 block/flex/absolute 基础布局；旧文档中“layoutWeight 缩放离场布局空间”的规则尚未按该方式接入 box layout。
9. Layer 已扩展为 stacking context、portal、draw channel、hit-test record 和 interaction routing，不只是 root layer 顺序。
10. Render 数据已扩展为 command buffer、render layer、effect command、effect parameters 和 resource updates。
11. RHI 和 Effects 已成为实际架构的一部分；DX11/DX12 renderer 同时提供 renderer backend 和 RHI device 能力。
12. Font 后端接口已围绕 paragraph layout、shared render data 和 pending upload 设计，并存在 FreeType 与 STB 两套实现。
13. 基础 widget 范围已扩展到 TextInput、ImageViewer、ScrollView、Popup、ContextMenu、Tooltip。
14. Frame diagnostics 已成为 Core API，可用于观测 widget/style/layout/render 运行状态。
```

## 22. 当前限制与后续重点

按实际代码观察，后续仍可继续完善的方向：

```text
1. layoutWeight 尚未作为 box layout 的结构占用缩放输入。
2. Style 系统是强类型 rule builder 和 compiled sheet，尚未提供 CSS 文本 parser/offline compiler。
3. Text shaping 仍由现有 font backend 能力决定，复杂文字 shaping 不是当前核心。
4. RenderGraph/RHI 已存在，但 UI 主渲染路径仍由 DX11/DX12 renderer 直接消费 SdRenderPacket。
5. 后端覆盖集中在 Windows + DirectX，尚无 OpenGL/Vulkan/Metal 平台实现。
6. 仍需随着 API 稳定补充更多示例和文档化的 user-facing style/model 用法。
```

## 23. 最终模型总结

SdGUI 当前的实际模型可以概括为：

```text
应用拥有 SdInstance 和 backends。
每帧通过 SdUi 声明 widget。
声明解析为稳定 SdWidgetId / SdResolvedKey。
SdStateStorage 保存 widget record、typed object、state、style node 和 model。
EndFrame 统一执行 lifecycle、style、layout、arrange、animation、layer 排序、hit-test 数据生成和可见 widget paint。
Paint 写入 SdRenderList。
RenderList 构建 SdRenderPacket。
Renderer backend 在宿主 render loop 内提交 packet，但不接管 clear/present。
```

这个模型保留 immediate-style 声明的低样板，同时通过持久化 record 支持状态、样式、动画、layer、字体和 GPU 资源复用。
