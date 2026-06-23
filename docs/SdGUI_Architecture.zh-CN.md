# SdGUI 架构设计

版本：v0.4  
目标标准：C++20  
项目名称：SodiumGUI  
简称：SdGUI

## 1. 概览

SdGUI 是一个轻量级图形用户界面框架，围绕一种混合模型设计：

```text
外部模型：类型化声明 API
内部模型：持久化 widget 状态
```

应用代码每帧通过 `SdUi::Declare<T>()` 声明用户自定义 widget 类型来描述 UI。SdGUI 内部会保留稳定的 widget 状态、布局缓存、动画值、焦点状态、输入捕获、层级信息、字体图集数据和渲染资源。

核心设计目标是让公开 API 能灵活支持复杂的用户自定义 widget，同时实现平滑的结构动画、高效布局、持久交互状态和高性能渲染。

## 2. 设计目标

```text
1. 使用 C++20 作为基线语言标准。
2. 使用 `SdUi::Declare<T>()` 作为公开 widget 声明模型。
3. 使用持久化内部 widget 状态。
4. 使用 std::string 和 std::string_view 表示 UTF-8 字符串。
5. 在内部实现 UTF-8 到 codepoint 的解析。
6. 保留 layer 设计。
7. 不继承旧的 Element / Theme / Property 架构。
8. 使用更明确的 Widget / Style / Animation / Layout 系统。
9. 每帧重建 RenderList 数据，但保持 GPU buffer 持久存在。
10. 避免在帧关键路径中产生不必要的堆分配。
11. 让用户显式拥有一个或多个 SdGUI 实例。
12. 只使用零状态 widget tag 类型做编译期声明约束。
```

## 3. 高层架构

```text
Application
  |
  v
用户拥有的 SdGUI Instance
  |
  v
SdUi::Declare<T>() 类型化声明 API
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

与纯对象树 GUI 的主要架构差异是：一次声明并不直接意味着内部状态会立即销毁。某个 widget 即使在当前帧的应用代码中没有出现，也仍然可以在内部继续存在，用于执行离场动画。

## 4. 推荐目录结构

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

## 5. 基础类型与命名

SdGUI 使用 `Sodium` 命名空间。`Sd` 作为公开类型、函数和数据结构的前缀保留，用于标识 SodiumGUI API。

```cpp
namespace Sodium
{
	using namespace std::chrono_literals;

	using SdUInt8 = std::uint8_t;
	using SdUInt16 = std::uint16_t;
	using SdUInt32 = std::uint32_t;
	using SdUInt64 = std::uint64_t;
	using SdInt32 = std::int32_t;
	using SdSize = std::size_t;

	using SdDuration = std::chrono::nanoseconds;
	using SdTimePoint = std::chrono::time_point<std::chrono::system_clock, SdDuration>;

	using SdUtf8String = std::string;
	using SdUtf8StringView = std::string_view;
	using SdPath = std::filesystem::path;
	template<typename T>
	using SdSpan = std::span<T>;

	template<typename T>
	struct SdRange final
	{
		T begin = {};
		T end = {};

		constexpr SdRange() = default;
		constexpr SdRange(T beginValue, T endValue) : begin(beginValue), end(endValue) {}

		constexpr bool IsEmpty() const { return begin == end; }
		constexpr T Count() const { return end - begin; }
	};

	template<SdSize N>
	class SdFixedBitset final
	{
	private:
		static constexpr SdSize BitsPerWord = 64;
		static constexpr SdSize WordCount = (N + BitsPerWord - 1) / BitsPerWord;
		std::array<SdUInt64, WordCount> words = {};

	public:
		constexpr void Clear()
		{
			for (SdUInt64& word : words)
				word = 0;
		}

		constexpr bool Test(SdSize index) const
		{
			assert(index < N);
			return (words[index / BitsPerWord] & (SdUInt64{ 1 } << (index % BitsPerWord))) != 0;
		}

		constexpr void Set(SdSize index, bool enabled = true)
		{
			assert(index < N);
			SdUInt64& word = words[index / BitsPerWord];
			const SdUInt64 mask = SdUInt64{ 1 } << (index % BitsPerWord);
			if (enabled)
				word |= mask;
			else
				word &= ~mask;
		}

		constexpr void Reset(SdSize index)
		{
			Set(index, false);
		}

		constexpr bool Any() const
		{
			for (SdUInt64 word : words)
			{
				if (word != 0)
					return true;
			}
			return false;
		}
	};

	template<typename T>
	constexpr T SdInvalidIndex = static_cast<T>(-1);
}

```

推荐命名约定：

```text
类型：PascalCase
函数：PascalCase
局部变量和字段：camelCase
常量：kPascalCase
命名空间：PascalCase
```

## 6. 字符串与 UTF-8 策略

SdGUI 内部和外部文本编码统一使用 UTF-8。

```text
拥有所有权的字符串：std::string
非拥有视图字符串：std::string_view
```

SdGUI 不应把平台宽字符转换作为核心字符串模型。UTF-8 解码由框架内部处理。

```cpp
namespace Sodium::Utf8
{
    bool IsValid(std::string_view text);
    bool TryReadCodepoint(std::string_view text, size_t& offset, uint32_t& codepoint);
    std::vector<uint32_t> DecodeToCodepoints(std::string_view text);
}
```

初始 UTF-8 实现必须支持：

```text
1. ASCII
2. 2 字节 UTF-8 序列
3. 3 字节 UTF-8 序列
4. 4 字节 UTF-8 序列
5. 非法序列 fallback
```

## 7. Context

`Context` 是中心运行时对象。它拥有或引用框架的所有主要系统。

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

示例应用循环：

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

应用程序拥有每个 `Instance`。一个进程可以创建一个 UI 实例，也可以创建多个相互独立的 UI 实例。

```cpp
Sodium::SdInstance editorUi;
Sodium::SdInstance overlayUi;

editorUi.ui.Declare<EditorWindow>();
overlayUi.ui.Declare<OverlayPanel>();
```

## 8. 类型化声明 API 与持久状态

公开 widget API 以 `SdUi::Declare<T>()` 为中心：

```cpp
if (showAdvanced)
{
    sd.ui.Declare<AdvancedSettingsPanel>();
}
```

在内部，`AdvancedSettingsPanel` 拥有稳定的 `SdWidgetId`。当 `showAdvanced` 变为 `false` 时，widget 状态不会立即删除。它会进入离场阶段，并继续参与布局和渲染，直到动画完成。

从语义上来描述，不再声明代表的是希望组件不再显示，当然按照不同的设置组件在实际视觉来看可以是一下子消失的也可以是带动画的

这使 SdGUI 获得目标行为：

```text
用户代码控制某个 widget 本帧是否被声明。
内部状态控制该 widget 是否仍然在视觉上存活。
```

`Declare<T>()` 是唯一公开的 widget 声明原语。文本、按钮、复选框、面板、滚动视图和窗口等内置控件，都是通过同一 API 声明的普通 tagged widget 类型。

```cpp
sd.ui.Declare<SdText>("Advanced Settings");
sd.ui.Declare<SdCheckBox>(enableCache);
sd.ui.Declare<SdSliderFloat>(radius, 0.f, 100.f);
```

### 8.1 匿名声明与 keyed 声明

`SdUi::Declare<T>()` 默认不要求传入 key。不是所有组件都需要被外部访问，匿名声明应保持低成本和低样板。

```cpp
ui.Declare<SdPanel>();
ui.Declare<SdButton>("Increment");
ui.Declare<SdText>(frameText);
```

匿名 widget 的身份由当前声明上下文、widget 类型和 sibling ordinal 解析。它适合纯显示、局部交互和不需要外部寻址的控件。匿名 ID 对声明顺序敏感，动态列表、可重排内容和需要外部访问的组件不应依赖匿名 ID。

需要稳定外部寻址时使用 keyed 声明。key 是框架层参数，只用于身份解析、调试和可选 model 寻址，不传给 widget 的 `OnCreate` / `OnUpdate`。

```cpp
ui.DeclareKeyed<SdButton>("increment_button", "Increment");
ui.DeclareKeyed<SdWindow>("main_window", "SodiumGUI Demo", open, options, content);
ui.DeclareKeyed<SdMultiCombo>("weapon_filter", "Weapons");
```

在上例中，`"weapon_filter"` 是框架 key，`"Weapons"` 才是 `SdMultiCombo` 控件收到的 label。key 与 label/title/text 必须保持概念分离：

```text
key              -> 框架层稳定身份和外部寻址。
label/title/text -> 控件层显示或业务参数。
```

不应把 `Declare<T>()` 的第一位普通控件参数隐式解释为 key；否则 `ui.Declare<SdButton>("Increment")` 无法清楚表达 `"Increment"` 是 label 还是身份。keyed 组件必须通过明确 API 声明，例如 `DeclareKeyed<T>()` 或等价命名。

### 8.2 SdWidgetId 生成规则

匿名声明：

```text
widgetId = Hash(parent_scope, type_hash, ordinal)
```

keyed 声明：

```text
widgetId = Hash(parent_scope, type_hash, key_hash)
```

`SdWidgetId` 是内部运行时 ID，用于索引 widget record、生命周期、布局、动画和输入状态。用户代码不应长期保存或手工构造 `SdWidgetId`。

keyed 声明还应记录一个可调试、可查找的 resolved key：

```text
resolvedKey = Hash(key_scope, key_hash)
resolvedKey -> SdWidgetId
```

resolved key 的具体形式可以是 hash，也可以在 debug build 中保留原始 key path。它的用途是外部配置、诊断和可选 model 寻址，而不是取代 `SdWidgetId` 的内部 record 索引职责。

## 9. Widget 生命周期

SdGUI 不引入独立的 presence 系统。结构出现和消失是 `SdWidgetState` 的一部分。

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

生命周期规则：

```text
首次提交            -> Entering
进入动画完成        -> Alive
未提交              -> Leaving
离场动画完成        -> Dead
Dead                -> 从 StateStorage 中移除
```

如果一个正在离场的 widget 在离场动画完成前又被提交，它必须从当前动画值平滑转回 `Entering` 或 `Alive`，不能发生视觉跳变。

## 10. Widget 类型声明与分组

结构动画需要一个稳定的分组边界。在 SdGUI 中，这个边界是被声明的 widget 类型，而不是单独的公开 scope API。

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

被声明的 widget 类型提供：

```text
1. 稳定 SdWidgetId
2. 父级 layout node
3. 共享生命周期
4. 共享裁剪区域
5. 共享 opacity/layout 动画
```

如果没有这样的边界，一组松散声明的 widget 可能没有足够的语义信息作为整体动画。

`SdWidgetTag` 是零状态编译期 tag。它不能变成虚函数运行时基类。

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

这个 tag 只用于约束 `Declare<T>()`、表达用户意图，并产生清晰的编译期诊断。Widget 生命周期、父子关系、布局、动画、输入和渲染仍然由 SdGUI 内部数据结构拥有。

## 11. StateStorage

`StateStorage` 保存跨帧数据。

```text
StateStorage
- SdWidgetState
- 用户 widget 状态
- Optional keyed component model
- Layout cache
- Style cache
- Animation channels
- Scroll state
- Text input state
- Focus and capture data
```

初始实现：

```cpp
std::unordered_map<SdWidgetId, SdWidgetState>
```

后续可选优化：

```text
1. flat hash map
2. robin hood hash map
3. slot map
4. dense vector plus sparse index
5. arena-backed storage
```

StateStorage 不应在每帧创建和销毁大量对象。

用户自定义 widget 不应保存对帧级系统的引用。持久的 per-widget 数据通过存储在 `StateStorage` 中的 typed state 访问。

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

实现可以从 `std::unordered_map<SdWidgetId, SdWidgetState>` 开始，但 typed user state 应逐步迁移到按类型密集存储的 pool 或 slot-map 存储，以避免帧关键路径上的 per-widget 堆分配。

### 11.1 State 与 Model 的边界

SdGUI 的持久数据分为三层：

```text
WidgetRecord
- 框架固定拥有。
- 绑定 SdWidgetId。
- 保存生命周期、widget object、布局、动画、输入、paint/update 回调。

State<T>
- 组件实例的 typed user state。
- 绑定 WidgetRecord / SdWidgetId。
- 跟随 widget 生命周期清理。
- 用于 hover、pressed、opened、layout cache 等当前 UI 实例状态。

Model<T>
- 可选的 keyed component model。
- 绑定 resolved key。
- 只在组件需要跨显示周期、跨位置外部配置或业务数据持久化时使用。
- 不随 widget record 离场自动删除。
```

`context.State<T>()` 和 `context.Model<T>()` 的语义必须不同：

```cpp
State& state = context.State<State>();
Model& model = context.Model<Model>();
```

`State<T>()` 面向当前 widget 实例。widget 进入 `Dead` 并被 sweep 后，相关 typed state 可以被释放。

`Model<T>()` 面向 stable key。它只对 keyed widget 有意义，匿名 widget 调用 model API 应在 debug build 中 assert 或进入明确的错误路径。model 的创建与使用由组件内部自行决定，框架只提供 keyed model storage，不强制所有 keyed widget 都创建 model。

例如 `SdButton` 可以只使用 state：

```cpp
class SdButton final : public SdWidgetTag
{
public:
    struct State
    {
        bool hovered = false;
        bool pressed = false;
        bool clicked = false;
    };

    void OnUpdate(SdUpdateContext& context, SdUtf8StringView label)
    {
        State& state = context.State<State>();
        // update interaction state
    }
};
```

`SdMultiCombo` 这类需要外部追加选项的控件可以选择使用 model：

```cpp
class SdMultiCombo final : public SdWidgetTag
{
public:
    struct Model
    {
        std::vector<Option> options = {};
        std::vector<SdUtf8String> selectedIds = {};
    };

    struct State
    {
        bool opened = false;
        bool hovered = false;
    };

    void OnUpdate(SdUpdateContext& context, SdUtf8StringView label)
    {
        Model& model = context.Model<Model>();
        State& state = context.State<State>();
        // render and update view state using persistent model data
    }
};
```

外部访问不应直接操作当前 widget object。需要配置持久数据时应访问 keyed model：

```cpp
ui.Model<SdMultiCombo>("weapon_filter").AddOption("ak47", "AK-47");
ui.Model<SdMultiCombo>("weapon_filter").AddOption("m4a1", "M4A1");
```

或使用 callback 风格：

```cpp
ui.ConfigureModel<SdMultiCombo>("weapon_filter", [](auto& model)
{
    model.AddOption("ak47", "AK-47");
});
```

这样 keyed widget 暂时不被声明、离场并最终移除 widget record 后，外部配置的 model 仍然可以保留。是否清理 model 应由组件或用户通过明确 API 决定，例如 `ClearModel` / `RemoveModel`，而不是由 widget 离场隐式触发。

## 12. Layout

Layout 分为两个阶段：

```text
Measure:
- 计算期望尺寸。
- 文本、padding、style、children 和布局规则都会影响 measuredSize。

Arrange:
- 计算 targetRect。
- 将 layoutWeight 应用于结构尺寸。
- 将 animatedRect 动画到 targetRect。
```

正在离场的 widget 仍然参与布局，但其占用尺寸会按 `layoutWeight` 缩放。

```cpp
actualHeight = measuredHeight * state.layoutWeight;
actualWidth = measuredWidth;
```

这可以实现平滑的结构过渡：

```text
Widget 出现：
- layoutWeight 0 -> 1
- 相邻 widget 平滑移动，为它腾出空间

Widget 消失：
- layoutWeight 1 -> 0
- 相邻 widget 平滑移动，闭合空隙
```

Layout 数据应存储在连续的帧级数组中。

```text
避免：
- 每个 node 单独堆分配
- 在帧关键布局结构中使用 shared_ptr tree

优先：
- vector-backed layout nodes
- 基于 index 的 parent/child 链接
- 用 frame arena 存储临时数据
```

## 13. Animation

Animation 只负责数值插值。它不决定 widget 是否存在。

常见动画通道：

```text
layoutWeight
opacity
rect.position
rect.size
style.color
style.radius
scroll.offset
```

推荐 transition 数据：

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

默认结构动画：

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

性能规则：

```text
只更新活动动画通道。
动画非活动的 widget 不进入动画更新路径。
```

## 14. Widget System

Widget 在概念上拆分为：

```text
1. 公开声明类型
2. 持久内部记录
3. 行为更新
4. 布局贡献
5. 样式 cascade 和 transition 解析
6. Paint/render intent
```

用户自定义 widget 是继承自 `SdWidgetTag` 的普通 C++ 类型。它们可以实现可选的阶段回调。SdGUI 使用 C++20 `requires` 检测回调，不要求虚函数。

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

所有回调都是可选的。传给 `Declare<T>(args...)` 的构造参数只用于首次创建或 `OnCreate`，不得意味着每帧重构。

```cpp
sd.ui.Declare<Card>(SdVec2{ 100.f, 100.f });
```

推荐回调分发方式：

```cpp
template<class T>
void CallOnUpdate(T& widget, SdUpdateContext& context)
{
    if constexpr (requires { widget.OnUpdate(context); })
        widget.OnUpdate(context);
}
```

回调接收阶段专用 context 对象。这些 context 提供该阶段所需的数据访问能力，但不会让 widget 拥有帧级系统。

```cpp
struct SdWidgetContextBase
{
    SdInstance& instance;
    SdContext& context;
    SdWidgetId id = 0;
    SdWidgetId parentId = 0;

    SdWidgetState& widgetState;
    SdStyleNode& rootStyleNode;
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

`RenderList` 是帧级对象，只能通过 `SdPaintContext` 访问。用户 widget 不得持久保存 `RenderList*` 或 renderer backend 指针。

Widget 行为从输入状态产生交互结果。渲染不应直接拥有行为状态；它读取最终几何、样式、动画值，并通过 context accessor 访问用户状态。

## 15. Style

SdGUI 的 Style 系统采用 CSS 语义模型的 C++ 强类型实现。目标不是提供一个类似 CSS 的外观 API，而是让 CSS 经过编译或翻译后，能在 SdGUI 中得到尽量一致的布局、绘制和 transition 结果。框架负责 selector 匹配、cascade、theme/variable 解析、box layout、dirty 判断和 transition 调度；Widget 负责声明 root style、可样式化 part，以及自身的语义绘制。

核心命名：

```text
SdBoxStyle          CSS-like box/text/paint 基础属性集合。
SdWidgetRootStyle   Widget root style node 使用的默认样式类型。
SdWidgetPartStyle   Widget part style node 使用的默认样式类型。
SdStylePart         Widget 内部可被 selector 选择的强类型 part 标识。
SdStyleNode         root 或 part 在 runtime 中对应的样式节点。
presentation style  transition / animation 后实际用于 paint 的样式。
```

`SdWidgetRootStyle` 和 `SdWidgetPartStyle` 默认继承同一套 `SdBoxStyle` 属性，但命名上区分 root 与 part 的语义。

```cpp
struct SdBoxStyle
{
    SdDisplay display = SdDisplay::Block;
    SdPosition position = SdPosition::Static;

    SdLength width = SdAuto;
    SdLength height = SdAuto;
    SdLength minWidth = {};
    SdLength minHeight = {};
    SdLength maxWidth = SdNone;
    SdLength maxHeight = SdNone;

    SdSpacing margin = {};
    SdSpacing padding = {};
    SdBorder border = {};
    SdBoxSizing boxSizing = SdBoxSizing::ContentBox;
    SdOverflow overflow = SdOverflow::Visible;

    SdColor color = SdColorWhite;
    SdFontFamily fontFamily = {};
    float fontSize = 16.0f;
    float lineHeight = 0.0f;
    SdTextAlign textAlign = SdTextAlign::Left;

    SdColor backgroundColor = SdColorTransparent;
    float opacity = 1.0f;
    SdTransform transform = {};

    SdTransitionList transitions = {};
};

struct SdWidgetRootStyle : SdBoxStyle
{
};

struct SdWidgetPartStyle : SdBoxStyle
{
};
```

基础 Widget 默认使用 `SdWidgetRootStyle` 作为 root style。复杂 Widget 通过 `SdStylePart` 暴露内部可选择的 style node，而不是在 root style 里嵌套子 style。

### 15.1 Style Node 与 Part

每个 Widget 至少有一个 root style node。能被 CSS 单独选择、布局、绘制或过渡的内部结构必须声明为 part style node。

```cpp
struct SdButton final : SdWidgetTag
{
    using Style = SdWidgetRootStyle;

    struct Parts final
    {
        static constexpr SdStylePart Content = SdStylePart::Make("content");
        static constexpr SdStylePart Icon = SdStylePart::Make("icon");
        static constexpr SdStylePart Label = SdStylePart::Make("label");
    };

    static void Describe(SdComponentContract<SdButton>& contract)
    {
        contract.Root()
            .Style<SdWidgetRootStyle>()
            .Box();

        contract.Part(Parts::Content)
            .Style<SdWidgetPartStyle>()
            .Box();

        contract.Part(Parts::Icon)
            .Style<SdWidgetPartStyle>()
            .Box()
            .Optional();

        contract.Part(Parts::Label)
            .Style<SdWidgetPartStyle>()
            .Inline()
            .InheritText();
    }
};
```

Input 这类复合控件也按同一规则声明内部 part：

```cpp
struct SdInput final : SdWidgetTag
{
    using Style = SdWidgetRootStyle;

    struct Parts final
    {
        static constexpr SdStylePart Field = SdStylePart::Make("field");
        static constexpr SdStylePart Value = SdStylePart::Make("value");
        static constexpr SdStylePart Placeholder = SdStylePart::Make("placeholder");
        static constexpr SdStylePart Selection = SdStylePart::Make("selection");
        static constexpr SdStylePart Caret = SdStylePart::Make("caret");
    };

    static void Describe(SdComponentContract<SdInput>& contract)
    {
        contract.Root().Style<SdWidgetRootStyle>().Box();
        contract.Part(Parts::Field).Style<SdWidgetPartStyle>().Box();
        contract.Part(Parts::Value).Style<SdWidgetPartStyle>().Inline().InheritText();
        contract.Part(Parts::Placeholder).Style<SdWidgetPartStyle>().Inline().InheritText();
        contract.Part(Parts::Selection).Style<SdWidgetPartStyle>().Box();
        contract.Part(Parts::Caret).Style<SdWidgetPartStyle>().Box();
    }
};
```

### 15.2 Style Value Pipeline

为贴近 CSS，style state 采用 CSS 的阶段语义：

```text
specified     cascade 后得到的声明值。
resolved      inherit、theme token、custom variable、em/rem 等解析后的值。
used          layout 后的实际 px 值和 box geometry。
presentation  transition / animation 后实际用于 paint 的值。
```

`presentation` 替代旧设计里容易混淆的 computed/current 命名。layout 使用 `used` geometry，paint 使用 `presentation` style。

```cpp
void SdButton::OnPaint(SdPaintContext& context)
{
    const SdStyleNode& root = context.RootStyleNode();
    const SdStyleNode& label = context.Part(SdButton::Parts::Label);

    DrawBackground(root.used.borderBox, root.presentation);
    DrawBorder(root.used.borderBox, root.presentation);
    DrawText(label.textRun, label.used.contentBox.min, label.presentation);
}
```

paint 阶段不重新决定样式，不临时查父级样式。继承、变量和 transition 都必须在 style pipeline 中完成。

### 15.3 CSS-like Selector 与 Cascade

每个 style node 保留 style identity：

```text
target type    类似 HTML tag，例如 SdButton / SdLabel / UserWidget。
part           root 或 SdStylePart。
classes        强类型 class，不在热路径使用字符串。
pseudo state   hovered / pressed / focused / disabled。
scope          window、panel、popup 或用户定义 style scope。
inline style   用户传入的 root style target。
```

cascade 优先级遵循 CSS 语义：

```text
1. user-agent stylesheet，也就是 SdGUI 默认样式。
2. theme stylesheet。
3. user stylesheet。
4. scoped stylesheet。
5. inline style target。
6. !important。

同层内部按 specificity -> source order 排序。
```

用户侧不传字符串 class。class、part、property 和 selector 在注册或离线编译阶段转换为稳定 ID。

```cpp
namespace AppClass
{
    inline constexpr SdClass Primary = SdClass::Make("primary");
    inline constexpr SdClass Danger = SdClass::Make("danger");
}

SdStyleSheet sheet;

sheet.Rule<SdButton>()
    .Set(&SdWidgetRootStyle::display, SdDisplay::InlineFlex)
    .Set(&SdWidgetRootStyle::padding, SdSpacing{ 6.0f, 10.0f, 6.0f, 10.0f })
    .Set(&SdWidgetRootStyle::backgroundColor, ThemeColor("button.bg"));

sheet.Rule<SdButton>().Class(AppClass::Danger).Pseudo(SdPseudoState::Hovered)
    .Set(&SdWidgetRootStyle::backgroundColor, ThemeColor("danger.hover"));

sheet.Part<SdButton>(SdButton::Parts::Label)
    .Set(&SdWidgetPartStyle::color, ThemeColor("button.text"))
    .Set(&SdWidgetPartStyle::fontSize, ThemeMetric("font.button"));
```

### 15.4 Transition 与 Animation

属性能力由 style contract 或全局 property registry 声明。声明内容包括影响范围、插值方式和是否为昂贵 layout transition。

```cpp
contract.Property(&SdBoxStyle::backgroundColor)
    .AffectsPaint()
    .InterpolatesAsColor();

contract.Property(&SdBoxStyle::opacity)
    .AffectsComposite()
    .InterpolatesAsFloat();

contract.Property(&SdBoxStyle::width)
    .AffectsLayout()
    .InterpolatesAsLength()
    .ExpensiveTransition();

contract.Property(&SdBoxStyle::display)
    .AffectsLayout()
    .Discrete();
```

stylesheet 决定具体规则是否启用 transition：

```cpp
sheet.Rule<SdButton>()
    .Transition(&SdWidgetRootStyle::backgroundColor, 160ms, SdAnimationEasing::OutCubic)
    .Transition(&SdWidgetRootStyle::opacity, 120ms, SdAnimationEasing::Linear);
```

runtime diff 流程：

```text
1. 解析新的 specified/resolved style。
2. 对比旧 target/resolved style。
3. 字段未变化：不处理。
4. 字段变化且不可插值或未声明 transition：presentation 直接跳到目标值。
5. 字段变化且可插值并启用 transition：启动或更新属性 animation channel。
6. 根据字段影响范围标记 layoutDirty、paintDirty 或 compositeDirty。
```

### 15.5 用户侧使用

用户使用强类型 class 和可选 inline root style。inline style 只作为 target，不直接等于 presentation。

```cpp
void DrawUi(SdUi& ui)
{
    ui.Declare<SdDiv>([](SdUi& ui)
    {
        ui.Declare<SdButton>("Save", SdClassList{ AppClass::Primary });
        ui.Declare<SdButton>("Delete", SdClassList{ AppClass::Danger });
        ui.Declare<SdInput>("username");
    });
}
```

```cpp
SdWidgetRootStyle customButton = {};
customButton.padding = { 8.0f, 14.0f, 8.0f, 14.0f };
customButton.backgroundColor = { 40, 120, 220, 255 };

ui.Declare<SdButton>("Custom", SdInlineStyle{ &customButton });
```

### 15.6 性能原则

Style 和 layout 的实现必须优先避免 CPU 热路径开销：

```text
1. selector 在加载期或离线阶段编译为 SdCompiledSelector。
2. frame 阶段只比较整数 ID、bitset 和预处理 selector key。
3. stylesheet 按 target type、part、pseudo、scope 预分桶，避免全表扫描。
4. SdClassList 使用 fixed bitset 或 small-vector，常见路径不堆分配。
5. property 使用 pointer-to-member 生成 property id、offset 和 type traits，避免字符串查找。
6. style diff 输出 dirty mask：layout / paint / composite。
7. inherited property 使用 revision 传播，父级继承值不变时子节点不重算。
8. style node、box node 和 layout fragment 使用连续数组，通过 index 引用。
9. layout algorithm 避免虚函数树，优先使用 C++20 concepts、constexpr metadata 和分派表。
10. transition channel 按 property id 稀疏创建，只更新 active channels。
11. CSS 文件可离线编译为 SdCompiledStyleSheet，runtime 不解析 selector 字符串。
```
## 16. Layer

Layer 设计被保留，并且独立于旧的 Element 架构。

```cpp
enum class SdRootLayer
{
    Background,
    Content,
    Floating,
    Popup,
    Tooltip,
    ModalBackdrop,
    Modal,
    DragPreview,
    DebugOverlay
};
```

Layer 职责：

```text
1. 绘制顺序
2. Hit-test 顺序
3. Clip/scissor 路由
4. Popup、tooltip、modal、overlay 支持
5. DrawList 或 draw channel 所有权
```

示例：

```cpp
class ContextMenu final : public Sodium::SdWidgetTag
{
public:
    void OnCreate(Sodium::SdCreateContext& context)
    {
        context.widgetState.rootLayer = Sodium::SdRootLayer::Popup;
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

输入 hit testing 必须从最高优先级 layer 到最低优先级 layer 处理。

## 17. Input

Input 使用帧事件模型。

```text
InputManager
- MouseState
- KeyboardState
- TextInputState
- GamepadState
- FocusState
- CaptureState
```

Widget 行为查询输入状态并产生：

```text
hovered
pressed
clicked
active
focused
changed
```

正在离场的 widget 通常应设置 `inputEnabled = false`。

## 18. Render

Render 数据是帧级数据。顶点数据、索引数据和 draw command 每帧重建。

这对于 immediate 和 hybrid immediate UI 框架是有意为之且符合预期的。

重要区别：

```text
每帧重建 CPU 侧 vertex/index 内容：是。
每帧重建 GPU vertex/index buffer 资源：否。
```

推荐帧级 render 数据：

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

规则：

```text
1. RenderData 每帧 reset。
2. vector 保留 capacity。
3. RendererBackend 拥有持久 GPU buffer。
4. GPU buffer 只在 capacity 不足时增长。
5. TextureAtlas 和 FontAtlas 只在 dirty 时上传。
6. 默认使用 uint32_t indices。
7. RendererBackend 在绑定 SdGUI 管线前必须保存宿主原有管线配置，并在 Render 返回前完整还原。
```

Backend upload 模型：

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

Draw command 应按以下因素合批：

```text
1. texture
2. scissor
3. shader
4. pipeline state
```

## 19. Font and Glyph

文本渲染流程：

```text
std::string_view UTF-8
-> codepoint stream
-> glyph lookup
-> missing glyph fallback
-> atlas packing
-> text quads
```

字体系统：

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

初始版本不需要复杂文本 shaping，但 UTF-8 解码、glyph lookup、fallback glyph 和 atlas upload 必须稳定。

## 20. Backend Interfaces

Platform backend：

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

Renderer backend：

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

Renderer backend 只负责消费 SdGUI 生成的渲染指令或 draw packet。交换链 present、清屏、帧开始和帧结束属于宿主渲染系统的职责，不属于 SdGUI 框架生命周期管理范畴。

Renderer backend 是宿主渲染流程中的临时提交者，不能把 SdGUI 的 pipeline state 泄漏给调用方。后端在设置自己的 shader、input layout、blend/rasterizer/depth-stencil state、viewport、scissor、buffer、texture 和 sampler 等状态前，应保存会被修改的原有配置；`Render` 结束前必须按保存结果还原，使宿主在调用 SdGUI 前后的渲染管线状态保持一致。

Glyph backend：

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
1. 更新时间。
2. 轮询平台输入。
3. 开始 InputManager frame。
4. 将所有 SdWidgetState entries 标记为 submittedThisFrame = false。
5. 清空帧级 layout 和 render 数据。

Declaration stage
1. `SdUi::Declare<T>()` 根据 parent stack、type identity 和 ordinal 解析匿名 SdWidgetId。
2. `SdUi::DeclareKeyed<T>()` 根据 parent/key scope、type identity 和 explicit key 解析稳定 SdWidgetId，并登记 resolved key。
3. 查找或创建内部 widget record 和 typed widget object。
4. 将 SdWidgetState 标记为 submittedThisFrame = true。
5. 对新创建的 widget 调用 OnCreate。
6. 对已提交的 widget 调用 OnUpdate，并允许嵌套 `context.ui.Declare<U>()`。
7. keyed widget 可按组件需要访问 resolved key 绑定的 optional model。
8. 记录 parent-child declaration relationship。

EndFrame
1. 本帧未提交的 widget 若仍存活，则进入 Leaving。
2. Style cascade 解析 specified/resolved style，并更新 runtime presentation style。
3. Layout 在可用时调用 OnLayout，并基于 target/used style 计算 measuredSize 与 targetRect。
4. Animation 更新 layoutWeight、opacity 和 rect 值。
5. Paint 在可用时调用 OnPaint，并基于 used geometry 和 presentation style 写入帧级 RenderList 数据。
6. RenderContext 构建最终 RenderData。
7. Dead widget 和 typed user state 从 StateStorage 中移除。
8. keyed model 不因 widget record 离场自动删除，只通过明确 API 或组件策略清理。

Renderer submission
1. 上传 dirty texture 和 font atlas。
2. 上传当前帧 vertices 和 indices。
3. 向 RendererBackend 提交 draw commands。
4. 不调用 backend Present；present 由宿主渲染循环负责。
```

## 22. 性能原则

```text
1. SdWidgetState 跨帧持久存在。
2. 帧级数据使用连续数组。
3. 每帧 clear vector，但保留 capacity。
4. 不为 LayoutNode 做逐节点堆分配。
5. 只更新活动动画通道。
6. 通过预处理 target/class/property ID 和 cached selector 解析 style。
7. 每帧重建 RenderList，但复用 GPU buffer。
8. TextureAtlas 和 FontAtlas 只在 dirty 时上传。
9. Leaving widget 的动画完成后应尽快移除。
10. 避免在帧关键路径中使用大量 shared_ptr 或虚函数树。
11. 后端资源优先使用 RAII。
12. RendererBackend 必须在提交前保存被修改的宿主管线状态，并在提交结束后还原。
13. 正常帧执行中避免 new/delete 和 malloc/free。
```

## 23. 初始开发里程碑

```text
1. Core Context 和 frame loop
2. IdStack 和 SdWidgetId generation
3. Anonymous/keyed declaration 和 resolved key registry
4. StateStorage、optional keyed model 和 SdWidgetState lifecycle
5. InputManager
6. SdWidgetTag 和 `SdUi::Declare<T>()` / `DeclareKeyed<T>()`
7. Typed widget object storage
8. SdCreateContext / SdUpdateContext / SdLayoutContext / SdPaintContext
9. 支持 declared widget boundary 的 SdLayoutContext
10. 带 active channel update 的 AnimationContext
11. SdBoxStyle、SdWidgetRootStyle、SdWidgetPartStyle、SdStylePart、stylesheet cascade 和 presentation style
12. RenderData 和 DrawList
13. FontAtlas 和 UTF-8 decoding
14. LayerManager
15. 基础 tagged widgets：Text、Button、Checkbox、Slider、Panel、Window
16. Win32 platform backend
17. 第一个 renderer backend
18. 示例应用
```

## 24. 最终模型总结

SdGUI 的中心规则是：

```text
用户本帧是否声明某个 widget
不等同于
该 widget 本帧是否在视觉上消失。
```

每个 widget 都有内部生命周期：

```text
Entering / Alive / Leaving / Dead
```

用户通过 `SdUi::Declare<T>()` 声明匿名 widget，通过 `SdUi::DeclareKeyed<T>()` 声明需要稳定外部寻址的 widget。`T` 是继承自零状态 `SdWidgetTag` 的普通 C++ 类型，并且可以实现可选的 `OnCreate`、`OnUpdate`、`OnLayout` 和 `OnPaint` 回调。Widget 通过 `StateStorage` 和阶段专用 context 访问持久数据。`State<T>()` 绑定当前 widget record，`Model<T>()` 绑定 stable key 且由组件选择是否使用。`RenderList` 只通过 `SdPaintContext` 传入，并保持帧级生命周期。

Layout 使用 `layoutWeight` 实现连续结构变化。Animation 更新数值 transition。Layer 控制绘制和输入顺序。RenderList 每帧线性重建，而 GPU buffer 和 atlas texture 保持持久。

该模型保留简单的逐帧声明方式，同时支持平滑布局过渡、持久交互状态和高性能渲染路径。
