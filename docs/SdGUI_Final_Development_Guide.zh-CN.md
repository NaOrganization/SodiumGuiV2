# SdGUI 最终开发依据

目标版本：v0.4 完整架构收尾
当前基线：MVP 已完成，完整架构约 72%
最终目标：达到架构文档定义的 100% 可用状态

## 1. 最终目标

SdGUI 必须完成从“可运行 MVP”到“架构稳定 GUI 框架”的收尾。最终状态应满足：

1. `SdInstance` 仍是用户唯一需要显式拥有的运行时对象。
2. 用户通过 `SdUi::Declare<T>()` 和 `DeclareKeyed<T>()` 声明 UI。
3. 内部使用持久 widget record、typed state、keyed model、layout cache、style cache、animation channel。
4. Layout、Animation、Style、Layer、Input、Render 均作为明确系统参与 frame flow。
5. Renderer backend 使用单次 `Render(frameInfo, packet)` 提交。
6. Win32 + DX11 + FreeType 路径稳定可用。
7. 有可重复构建、核心 smoke test、关键模块单元测试。
8. 示例不依赖未文档化的内部行为。

## 2. 当前已完成基线

当前代码已具备：

1. `SdInstance`
2. `SdFrameState`
3. `SdContext` skeleton
4. `SdUi::Declare<T>()`
5. `SdUi::DeclareKeyed<T>()`
6. anonymous/keyed widget id
7. `SdStateStorage`
8. typed user state
9. keyed model storage
10. widget lifecycle：`Entering / Alive / Leaving / Dead`
11. phase context：create / update / layout / paint
12. frame-local layout node vector
13. active animation channel
14. token + rule style system 基础
15. layer draw / hit-test record
16. interaction state：hover / active / focus / capture
17. frame-local `SdRenderList / SdDrawPacket`
18. FreeType glyph atlas and text rendering
19. DX11 persistent buffer renderer
20. desktop Win32 + DX11 示例
21. dynamic DLL DX11 示例

## 3. 最终必须完成项

### Core Runtime

必须完成：

1. `SdContext` 不再只是 backend 指针容器，必须成为运行时系统聚合入口。
2. `SdFrameState` 持有 frame index、delta time、display size、diagnostics。
3. `SdInstance` 保留公开 API，但内部调度必须清晰拆分。
4. `SdRuntime.inl` 不应继续承担全部核心逻辑。
5. frame flow 必须明确包含：

```text
BeginInput
PlatformInput
BeginUi
Declaration
EndDeclaration
Style
Layout
Animation
LayerHitTest
Interaction
Paint
BuildDrawPacket
Sweep
Render
```

完成标准：

1. 每个 frame stage 有独立函数或系统入口。
2. 示例行为不回退。
3. diagnostics 能观察每帧核心状态。

### Id / Declaration

必须完成：

1. 新增或明确 `SdIdStack`。
2. anonymous id 使用 parent scope + type hash + ordinal。
3. keyed id 使用 parent scope + type hash + key hash。
4. resolved key 与 widget id 分离。
5. key 不得传入 widget callback。
6. `ui.Model<TWidget>(key)` 和 `ui.ConfigureModel<TWidget>(key, fn)` 只访问 keyed model。
7. 匿名 widget 不允许静默创建 persistent model。

完成标准：

1. 动态列表、窗口、外部配置控件可使用稳定 key。
2. label/title/text 与 key 语义完全分离。
3. debug diagnostics 可显示 resolved key / debug key。

### StateStorage

必须完成：

1. `SdStateStorage` 统一拥有：
   - widget records
   - typed user state
   - keyed model
   - layout cache
   - style cache
   - animation references
   - interaction state references
2. Dead widget 被 sweep 时，必须同步释放：
   - widget object
   - widget record
   - typed user state
   - animation channels
   - layout/style cache
3. keyed model 不因 widget record 删除而自动释放。

完成标准：

1. 离场 widget 动画结束后无状态泄漏。
2. keyed model 跨显示周期保留。
3. storage stats 可显示 created / reused / leaving / removed / model count。

### Layout

必须完成：

1. Layout 使用 frame-local vector node。
2. parent / child / sibling 使用 index，不使用 heap tree。
3. 每个 declared widget 都有 layout node。
4. 支持：
   - vertical stack
   - manual rect
   - padding
   - spacing
   - clip children
   - leaving widget layoutWeight
   - window children layout
5. `Measure` 和 `Arrange` 必须分离。
6. leaving widget 仍参与布局，占用尺寸按 `layoutWeight` 收缩。

完成标准：

1. Window 内控件布局稳定。
2. Widget 出现/消失有结构动画。
3. layout node count 可诊断。
4. 不依赖 unordered_map 遍历顺序决定布局结果。

### Animation

必须完成：

1. lifecycle 只决定目标状态，不直接插值。
2. `SdAnimationSystem` 只更新 active channels。
3. 支持 channel：
   - layoutWeight
   - opacity
   - rect position
   - rect size
   - style color
   - scroll offset
4. leaving widget 重新提交时，必须从当前值平滑返回。
5. transition 默认值来自 style/theme。

完成标准：

1. inactive widget 不进入 animation hot path。
2. enter / leave / layout transition 行为稳定。
3. animation diagnostics 显示 active channel count。

### Style

必须完成：

1. `SdTheme`
2. `SdStyleToken`
3. `SdStyleRule`
4. `SdComputedStyle`
5. type selector
6. interaction selector
7. layer/context selector 基础能力
8. computed style cache

控件绘制不得继续大量 hardcode 主视觉颜色。

必须迁移到 style 的控件：

1. Text
2. Panel
3. Button
4. CheckBox
5. Window
6. ImageViewer
7. Slider
8. TextInput
9. ScrollView
10. Popup / Tooltip / ContextMenu

完成标准：

1. Button hover / pressed 来自 style selector。
2. Window / Panel / CheckBox 主色来自 computed style。
3. style dirty 才刷新 cache。
4. hot path 不做字符串 selector 匹配。

### Layer / HitTest

必须完成：

1. `SdLayerSystem` 统一负责：
   - draw order
   - hit-test order
   - clip/scissor routing
   - popup/floating/modal/overlay priority
2. hit-test 顺序必须与 paint order 一致。
3. leaving widget 默认不参与 hit-test。
4. clip rect 不应由控件各自手工散落维护。

完成标准：

1. Floating window 可遮挡 content。
2. Popup 可压过 window/content。
3. 高 layer widget 优先接收输入。
4. hit-test diagnostics 可读。

### Input / Interaction

必须完成：

1. `SdInteractionSystem` 统一维护：
   - hoveredWidget
   - activeWidget
   - focusedWidget
   - capturedWidget
   - clickedWidget
2. Button、CheckBox、Window drag/resize、Slider、TextInput 必须使用统一 interaction。
3. 鼠标 capture 后，拖动离开原 rect 仍保持 active。
4. keyboard focus 与 mouse active 分离。
5. TextInput 使用 `SdTextInputState` 和 IME target。

完成标准：

1. Button / CheckBox 无重复 hover/click 判断逻辑。
2. Window 拖动和 resize 使用 capture。
3. Slider 拖动离开控件仍继续更新。
4. TextInput 可输入、选中、处理 composition text。

### Render

必须完成：

1. CPU render data 每帧重建。
2. GPU buffer 持久存在，仅容量不足时增长。
3. DrawPacket 为同步借用视图，backend 不得持久保存 span。
4. RendererBackend 必须保存并恢复宿主管线状态。
5. Draw command batching 至少按 texture + scissor 合批。
6. atlas / texture upload 只在 dirty 或 resource update 时提交。

完成标准：

1. DX11 backend 使用 `Render(frameInfo, packet)`。
2. 示例不直接调用 renderer `BeginFrame / Submit / EndFrame`。
3. pipeline state 不泄漏给宿主。
4. draw stats 可显示 commands / batches / vertices / indices / uploads。

### Font / Text

必须完成：

1. UTF-8 validation。
2. UTF-8 decode to codepoint。
3. FreeType glyph lookup。
4. missing glyph fallback。
5. atlas packing。
6. glyph atlas upload。
7. paragraph layout 基础缓存。
8. TextInput 所需 caret / selection 测量基础。

最终版本暂不强制完成：

1. complex shaping
2. RTL
3. 多字体 fallback chain
4. emoji/color glyph 完整支持

### Built-in Widgets

最终必须具备：

1. `SdText`
2. `SdPanel`
3. `SdButton`
4. `SdCheckBox`
5. `SdSliderFloat`
6. `SdTextInput`
7. `SdScrollView`
8. `SdWindow`
9. `SdImageViewer`
10. `SdPopup`
11. `SdContextMenu`
12. `SdTooltip`

可延后但接口需预留：

1. Table
2. List
3. Combo
4. MultiCombo
5. Tree

完成标准：

1. 所有内置控件均是 `SdWidgetTag`。
2. 所有控件通过 `Declare<T>()` 声明。
3. 需要外部配置的控件使用 keyed model。
4. 控件不保存 frame-local render/input/backend 指针。

### Backend

必须完成：

1. `ISdPlatformBackend`
2. `ISdRendererBackend`
3. `ISdFontBackend`
4. Win32 backend
5. DX11 renderer backend
6. FreeType font backend

Renderer contract 必须明确：

1. 不负责 Present。
2. 不负责 Clear。
3. 只消费 SdGUI draw packet。
4. 保存并恢复宿主管线状态。
5. texture handle 属于创建它的 renderer backend。
6. stale texture handle 必须被安全忽略。

完成标准：

1. 新 backend 不需要阅读示例 Main.cpp 即可实现。
2. backend interface 注释完整。
3. 示例只调用 `gui.Render()` 或等价单次提交。

### Tests / Diagnostics

必须新增测试入口。

最低测试要求：

1. core smoke test
   - 创建 `SdInstance`
   - BeginFrame
   - Declare widget
   - EndFrame
   - 检查 lifecycle / draw packet
2. id tests
   - anonymous id 稳定性
   - keyed id 稳定性
   - key 与 label 分离
3. lifecycle tests
   - Entering -> Alive
   - Alive -> Leaving
   - Leaving -> Dead
   - Leaving 重新提交
4. state tests
   - typed state 创建
   - Dead sweep 后释放
   - keyed model 保留
5. layout tests
   - vertical stack
   - manual rect
   - padding / spacing
   - leaving layoutWeight
6. animation tests
   - active channel update
   - completion
   - re-enter smoothing
7. style tests
   - token fallback
   - interaction selector
   - computed style cache
8. input tests
   - hover
   - capture
   - click
   - root layer
9. render tests
   - draw packet 非空
   - batch count
   - upload count
10. build verification
   - desktop example
   - dynamic dll example
   - core tests

完成标准：

1. 一条命令可完成构建和测试。
2. 示例构建作为集成门禁。
3. Debug diagnostics 可输出：
   - submitted widgets
   - live widgets
   - entering/leaving/dead
   - layout nodes
   - hit-test records
   - active animation channels
   - draw commands/batches/vertices/indices
   - uploads
   - model count

## 4. 不再进入最终范围的内容

以下内容不作为 v0.4 完成条件：

1. Dx9 backend
2. Dx12 backend
3. OpenGL backend
4. Vulkan backend
5. complex text shaping
6. RTL
7. full theme editor
8. visual style editor
9. docking system
10. retained-mode object tree API
11. old Element / Theme / Property 架构兼容

## 5. 最终验收标准

项目达到 100% 时必须满足：

1. `examples/examples.sln` Debug x64 构建通过。
2. desktop Win32 DX11 示例可运行。
3. dynamic DLL DX11 示例可构建。
4. core smoke test 可在无窗口环境运行。
5. 所有基础模块测试通过。
6. `SdInstance` API 稳定。
7. `Declare<T>() / DeclareKeyed<T>() / Model<T>()` 语义稳定。
8. widget lifecycle、layout、animation、style、layer、input、render 形成完整闭环。
9. 内置基础控件满足实际 UI 编写需求。
10. 文档与源码行为一致。

## 6. 当前剩余重点

当前最需要补齐的不是继续堆控件，而是：

1. 收敛 `SdRuntime.inl`。
2. 完成 `SdContext` 和系统所有权边界。
3. 完成 `SdIdStack`。
4. 补齐 Layout/Animation/Style/Layer/Input 的最终行为。
5. 实现 Slider、TextInput、ScrollView、Popup/Tooltip/ContextMenu。
6. 增加自动化测试和一键验证入口。

完成这些后，SdGUI v0.4 可视为架构完成。
