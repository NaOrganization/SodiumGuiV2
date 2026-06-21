# SdGUI Style 新架构开发文档

本文档描述如何把当前 Style / Layout / Animation / Widget 相关实现重构为新的 CSS 语义模型架构。目标是让 SdGUI 的样式系统能够承载 CSS-like cascade、盒模型、part style node、transition / animation 和强类型 selector，并使最终行为完全符合 `SdGUI_Architecture.zh-CN.md` 中 `## 15. Style` 的设计。

## 1. 最终目标

新的 Style 系统必须实现以下模型：

```text
Widget tree
-> StyleNode tree
-> Box tree
-> Layout fragments
-> Paint commands
```

核心类型命名必须统一：

```text
SdBoxStyle          CSS-like box/text/paint 基础属性集合。
SdWidgetRootStyle   Widget root style node 使用的默认样式类型。
SdWidgetPartStyle   Widget part style node 使用的默认样式类型。
SdStylePart         Widget 内部可被 selector 选择的强类型 part 标识。
SdStyleNode         root 或 part 在 runtime 中对应的样式节点。
presentation style  transition / animation 后实际用于 paint 的样式。
```

Style state 必须采用以下阶段：

```text
specified     cascade 后得到的声明值。
resolved      inherit、theme token、custom variable、em/rem 等解析后的值。
used          layout 后的实际 px 值和 box geometry。
presentation  transition / animation 后实际用于 paint 的值。
```

禁止继续把动画后的最终绘制样式称为 `computed style`。`computed` 如后续保留，只能用于 CSS computed value 语义；本项目实际 paint 使用 `presentation`。

## 2. 当前代码需要重构的部分

### 2.1 `SdUiCore.h`

当前问题：

1. `SdStyleToken` 是固定 enum，theme token 被硬编码为数组索引。
2. `SdStyleTargetTags` / `SdStylePropertyTags` 是旧 token/rule 风格目标和属性集合。
3. `SdComputedStyle` 是动态 property bag，并且字段集中在 `color/background/border/padding/width/height/radius/opacity`。
4. `SdThemeView` 仍然持有旧 `SdComputedStyle defaults`。
5. `SdStyleInteractionState` 只有 Normal/Hovered/Pressed/Focused，尚不足以表示 CSS pseudo state 集合。
6. 当前没有 `SdBoxStyle`、`SdWidgetRootStyle`、`SdWidgetPartStyle`、`SdStylePart`、`SdStyleNode`、`specified/resolved/used/presentation` 等目标架构类型。

需要重构：

1. 新增 CSS-like 基础类型：`SdLength`、`SdDisplay`、`SdPosition`、`SdOverflow`、`SdBoxSizing`、`SdBorder`、`SdTransform`、`SdTransitionList`。
2. 新增 `SdBoxStyle`，并派生 `SdWidgetRootStyle`、`SdWidgetPartStyle`。
3. 新增 `SdStylePart`，替代旧设计中的通用 part 字符串概念。
4. 新增 `SdPseudoState` 或扩展现有 interaction state，使其可表达 hovered、active/pressed、focused、disabled、checked、placeholder 等状态。
5. 将 `SdComputedStyle` 标记为旧兼容层，并在最终完成前移除。
6. 将 theme token 从固定 enum 迁移到强类型 token id / variable id 表。

### 2.2 `SdStyle.h`

当前问题：

1. `SdTheme` 使用 `std::array` 按 `SdStyleToken` 索引，无法支持任意 theme/custom variable。
2. `SdStyleRule` 仍是旧式 `targetTag + propertyTag + SdStyleValue`。
3. `SdStyleSystem::AddDefaultRules()` 在框架内部写死 Button、Slider、TextInput、Popup 等默认背景规则。
4. 已有 `SdStyleContract<TStyle>`、`SdStyleRuleBuilder<TWidget>`、`ResolveTargetStyle<TWidget>()` 等 typed style 雏形，但仍绑定旧 `Style::TokenTag`、旧 theme token 和旧 targetTag。
5. `std::function` 被用于 field descriptor 的 equals/copy/read/write/apply，后续可能造成热路径额外开销。
6. typed rules 未按 target/part/pseudo/scope 预分桶，匹配仍有线性扫描风险。

需要重构：

1. 把 `SdStyleSystem` 拆成以下职责：
   - `SdTheme` / variables：token id 到 typed value。
   - `SdStyleSheet`：规则集合。
   - `SdCompiledStyleSheet`：预编译 selector 和 declaration bucket。
   - `SdStyleResolver`：specified/resolved/presentation 解析。
   - `SdPropertyRegistry`：属性 metadata、impact、interpolation、offset、type traits。
2. 移除 `AddDefaultRules()` 中硬编码组件样式，改为安装 `SdDefaultUserAgentStyleSheet()`。
3. `Rule<TWidget>()` 继续保留强类型写法，但输出必须是 compiled declaration，而不是旧 `targetTag` rule。
4. 新增 `Part<TWidget>(TWidget::Parts::X)` API。
5. 支持 CSS cascade 层级、specificity、source order 和 `!important`。
6. 把 field descriptor 从 `std::function` 热路径改为可缓存 function pointer、constexpr metadata 或一次性注册的 dispatch table。

### 2.3 `SdRuntimeStorage.h`

当前问题：

1. `SdStyleCache` 持有旧 `SdComputedStyle computed`。
2. `SdTypedStyleRecord` 持有 `targetStyle`、`computedStyle`、`inlineStyle`，命名不符合新 pipeline。
3. 每个 widget 只有 typed style record，没有 root/part 多 style node 模型。
4. `typedStyles` 以 `std::type_index` 映射，适合过渡期，但不适合最终 style node tree。
5. style animation channel 仍按 typed style field 存在，没有归属到具体 `SdStyleNode` 和 property id。

需要重构：

1. 新增 `SdStyleNodeRecord`，每个 Widget root 和 part 都有独立 record。
2. 每个 style node 持有：

```text
styleNodeId
widgetId
partId
parentStyleNodeId
styleType
specifiedStyle
resolvedStyle
presentationStyle
usedBox
class mask
pseudo state
scope id
revision/cache key
property animation channels
```

3. `computedStyle` 重命名为 `presentationStyle`。
4. `targetStyle` 拆分为 `specifiedStyle` / `resolvedStyle`。
5. inline style 仅允许作为 root 或特定 part 的 target override。
6. animation channel 必须挂在 style node + property id 上，而不是挂在 widget 旧 typed style record 上。

### 2.4 `SdWidgetContext.h`

当前问题：

1. `SdWidgetState` 仍持有 `styleTokenTag`。
2. `SdWidgetContextBase` 暴露 `SdComputedStyle& style`。
3. Context 只提供 `TargetStyle<TWidget>()` / `ComputedStyle<TWidget>()`，没有 root/part style node API。
4. Layout 仍然通过 `SetDesiredSize()` 由组件直接声明尺寸。

需要重构：

1. 删除 `styleTokenTag`，改为 style identity：

```text
target type id
classes
pseudo state
scope
inline style revision
```

2. Context API 改为：

```cpp
const SdStyleNode& RootStyleNode() const;
const SdStyleNode& Part(SdStylePart part) const;

template<class TWidget>
const typename TWidget::Style& RootResolvedStyle() const;

template<class TWidget>
const typename TWidget::Style& RootPresentationStyle() const;
```

3. 若保留 `TargetStyle()`，只作为过渡兼容 API，最终必须迁移到 `resolved` / `presentation` 命名。
4. `SdLayoutContext` 需要访问 root/part used geometry 和 intrinsic measurement 输入，而不是只收 `SetDesiredSize()`。

### 2.5 `SdLayout.h`

当前问题：

1. Layout system 是简单垂直排列和手动 layout 混合。
2. `SdLayoutNode` 依赖 `desiredSize`、`childPadding`、`childSpacing`、`arrangeChildren`。
3. 没有 CSS box model：margin、border、padding、content box、box-sizing、overflow。
4. 没有 block / inline / inline-block / flex / absolute layout。
5. 没有 used value 解析：auto、percentage、min/max、em/rem。

需要重构：

1. 新增 `SdBoxNode` / `SdLayoutBox`：

```text
styleNodeId
parentBoxIndex
firstChildIndex
nextSiblingIndex
display
position
marginBox
borderBox
paddingBox
contentBox
intrinsicSize
used style values
```

2. 实现至少以下 CSS 子集：
   - `display: none/block/inline-block/flex`
   - `box-sizing`
   - width/height/min/max
   - margin/border/padding/content box
   - overflow clipping
   - position static/relative/absolute
   - flex-direction、justify-content、align-items、gap、flex-grow/shrink/basis
3. Widget 只提供 intrinsic size 和 semantic children，不再直接控制最终 rect。
4. 现有 `childPadding` / `childSpacing` 迁移到 `padding` / `gap`。
5. `manualLayout` 迁移到 `position:absolute` 或明确 overlay/floating layout 策略。

### 2.6 `SdAnimation.h` / `SdRuntime.inl`

当前问题：

1. style 动画已开始支持 typed fields，但仍使用 target/computed 命名。
2. 当前样式背景色动画仍有旧 `SdComputedStyle.background` 兼容路径。
3. transition 语义尚未覆盖 CSS 的 property、duration、delay、timing-function、discrete、layout-expensive 等规则。

需要重构：

1. 将 style animation 调度改为 style node property animation。
2. 使用 property registry 判断 interpolation 和 dirty impact。
3. 支持：

```text
transition-property
transition-duration
transition-delay
transition-timing-function
transition-behavior / discrete
```

4. animation 更新输出到 `presentationStyle`。
5. layout 属性 transition 默认禁用，只有明确声明 `ExpensiveTransition()` 才允许每帧 layout dirty。
6. diagnostics 增加 style node animation channel 数量、layout transition 数量、style resolve cache hit/miss。

### 2.7 `SdBasicWidgets.h`

当前问题：

1. 组件已经有内部 `Style`，但每个组件定义了不同字段，不统一继承 `SdWidgetRootStyle`。
2. 每个 `Style` 仍有 `TokenTag`，并通过 `context.widgetState.styleTokenTag` 接入旧 style 系统。
3. Paint 同时读取 `context.style.background/border/color` 和 typed style，形成双系统。
4. Button/Input/Window 等内部元素没有 part style node，例如 label、icon、caret、selection、titlebar。
5. Layout 仍由组件直接 `SetDesiredSize()`。

需要重构：

1. 基础组件 root 默认 `using Style = SdWidgetRootStyle`，特殊字段必须证明必要性。
2. Button 声明：

```text
root
::part(content)
::part(icon)
::part(label)
```

3. Input/TextInput 声明：

```text
root
::part(field)
::part(value)
::part(placeholder)
::part(selection)
::part(caret)
```

4. Window 声明：

```text
root
::part(titlebar)
::part(title)
::part(close-button)
::part(content)
::part(resize-handle)
```

5. Text/Label 使用 inline style node 或 text fragment style。
6. 所有 Paint 改为读取 `RootStyleNode().presentation` 和 `Part(...).presentation`。
7. 所有 Layout 改为提供 intrinsic size，最终 box 由 layout system 决定。

### 2.8 Tests 和 Examples

当前问题：

1. tests 大量断言 `SdComputedStyle`、`styleTokenTag`、旧 token rule。
2. examples 使用 `SdStyleToken` 直接设置 theme。
3. examples 使用旧 `Rule<T>().SetColorToken()` / `Transition()`，需要迁移到新 theme variable 和 strong typed property。

需要重构：

1. tests 改为验证 style node pipeline、box model、part selector、presentation transition。
2. examples 改用：

```cpp
ThemeColor("button.bg")
ThemeMetric("font.button")
SdClassList{ AppClass::Danger }
sheet.Part<SdButton>(SdButton::Parts::Label)
```

3. 删除以旧 token enum 为核心的测试。

## 3. 建议新增或重组的文件

建议最终拆分为以下文件：

```text
SdStyleCore.h
- SdBoxStyle
- SdWidgetRootStyle
- SdWidgetPartStyle
- SdStylePart
- SdStyleNodeId
- SdPseudoState
- SdLength / SdDisplay / SdPosition / SdOverflow / SdBoxSizing

SdStyleProperty.h
- SdPropertyId
- SdPropertyDescriptor
- SdPropertyRegistry
- interpolation metadata
- dirty impact metadata

SdStyleSheet.h
- SdStyleSheet
- SdStyleRuleBuilder
- SdCompiledSelector
- SdCompiledStyleSheet
- specificity / source order / cascade layer

SdStyleResolver.h
- specified -> resolved
- resolved -> presentation
- theme variable resolution
- inherit/custom variable resolution

SdStyleNode.h
- SdStyleNode
- SdStyleNodeTree
- root/part style node storage

SdBoxLayout.h
- SdBoxNode
- SdBoxTree
- block/flex/absolute layout algorithms

SdStyleAnimation.h
- property transition channels
- keyframe animation placeholder
```

现有 `SdStyle.h` 可以作为聚合头或迁移期兼容头，但最终不应继续承载所有实现。

## 4. 分阶段实施计划

### Phase 1：基础类型和命名迁移

目标：

1. 引入 `SdBoxStyle`、`SdWidgetRootStyle`、`SdWidgetPartStyle`、`SdStylePart`。
2. 引入 `presentation` 命名，停止新增 `computed` 命名 API。
3. 保留旧 `SdComputedStyle`，但标记为 compatibility path。

工作：

1. 新建 `SdStyleCore.h`。
2. 把基本 style value 类型从 `SdUiCore.h` 迁移或拆分。
3. 增加 `SdStylePart::Make()` 和强类型 class/scope id。
4. 增加 `SdStyleNode` 最小结构，但暂时只支持 root。

完成标准：

1. 代码中可以声明 `using Style = SdWidgetRootStyle`。
2. 新文档中的核心类型能被编译引用。
3. 不破坏现有 tests。

### Phase 2：Style Node Tree 和 Part 声明

目标：

1. 每个 widget 拥有 root style node。
2. 组件可以声明 part style node。
3. Context 可以访问 root/part style node。

工作：

1. 新增 `SdComponentContract<TWidget>`。
2. 支持：

```cpp
contract.Root().Style<SdWidgetRootStyle>().Box();
contract.Part(Parts::Label).Style<SdWidgetPartStyle>().Inline().InheritText();
```

3. `SdStateStorage` 增加 style node 连续存储。
4. `SdWidgetContextBase` 增加：

```cpp
RootStyleNode()
Part(SdStylePart)
RootPresentationStyle<TWidget>()
```

完成标准：

1. Button/Input 可以声明 Parts。
2. root 和 part 在 runtime 中都有稳定 style node id。
3. Paint 能读取 root/part presentation style。

### Phase 3：Property Registry 和 Strong Typed Stylesheet

目标：

1. 属性通过 pointer-to-member 注册，不通过字符串热路径查找。
2. selector 和 declaration 编译为稳定 ID。

工作：

1. 实现 `SdPropertyRegistry`。
2. 把现有 `SdStyleContract<TStyle>` 改造成 constexpr metadata 优先，减少 `std::function` 热路径。
3. 实现：

```cpp
sheet.Rule<SdButton>()
sheet.Rule<SdButton>().Class(AppClass::Danger)
sheet.Part<SdButton>(SdButton::Parts::Label)
```

4. 添加 specificity、source order、cascade layer。

完成标准：

1. 同一属性多条规则按 CSS cascade 得到正确结果。
2. class 和 part selector 不做运行时字符串比较。
3. stylesheet 能按 target type / part / pseudo 预分桶。

### Phase 4：specified/resolved/presentation Pipeline

目标：

1. 实现 CSS-like style value pipeline。
2. 替换旧 `target/computed` typed style record。

工作：

1. `SdStyleNode` 存储 specified、resolved、presentation。
2. 支持 inherit、theme token、custom variable 基础能力。
3. inline style 作为最高优先级 target 输入。
4. 删除或隔离 `ResolveTargetStyle<TWidget>()` 旧路径。

完成标准：

1. Paint 只使用 `presentation`。
2. Layout 使用 `resolved/used`。
3. 旧 `context.style` 不再参与基础组件绘制。

### Phase 5：CSS Box Model Layout

目标：

1. 引入 CSS 盒模型。
2. LayoutSystem 从 `desiredSize + arrangeChildren` 迁移到 box tree。

工作：

1. 实现 `SdBoxNode` 连续数组。
2. 实现 margin/border/padding/content box。
3. 实现 width/height/min/max、auto、percentage、box-sizing。
4. 实现 block layout。
5. 实现 flex layout。
6. 实现 overflow clipping。
7. 将 `childPadding` / `childSpacing` 映射到 padding/gap 后删除。

完成标准：

1. Div/Button/Input/Text 能通过 box model 完成布局。
2. flex row/column、gap、align-items、justify-content 结果可测。
3. 组件不再依赖 `SetDesiredSize()` 决定最终 box。

### Phase 6：Transition / Animation 完整化

目标：

1. transition 以 style node property 为单位。
2. 输出到 presentation style。

工作：

1. 实现 property animation channel：

```text
styleNodeId
propertyId
start value
target value
current presentation value
transition timing
```

2. 支持 duration、delay、timing-function、discrete。
3. layout impact 属性变化时正确标记 layout dirty。
4. layout transition 默认禁用，显式 expensive 才允许。

完成标准：

1. backgroundColor/opacity transition 与 CSS 语义一致。
2. width/padding transition 能工作，但 diagnostics 能标记 layout transition。
3. 不可插值属性直接跳到目标。

### Phase 7：基础组件迁移

目标：

1. 基础组件全部基于 root/part style node。
2. 基础组件不再走旧 token target 和旧 computed style。

必须迁移：

```text
SdDiv
SdText / SdLabel
SdButton
SdCheckBox
SdSliderFloat
SdTextInput / SdInput
SdPanel
SdWindow
SdImageViewer
SdScrollView
SdPopup
SdContextMenu
SdTooltip
```

完成标准：

1. 每个组件 root 使用 `SdWidgetRootStyle`。
2. 需要内部样式的组件声明 `SdStylePart`。
3. Paint 只读 root/part style node 的 used geometry 和 presentation style。
4. Layout 只提供 intrinsic data，由 box model 决定最终 rect。

### Phase 8：兼容层删除

目标：

完全移除旧架构路径。

必须删除或替换：

```text
SdStyleToken enum 作为核心 theme token 的用途
SdStyleTargetTags / SdStylePropertyTags 作为核心 selector/property 的用途
SdComputedStyle 作为 runtime style 输出
SdStyleRule 旧 property bag rule
SdWidgetState::styleTokenTag
SdStyleCache::computed
context.style
Context::ComputedStyle<TWidget>()
组件中的 Style::TokenTag
组件中的 context.widgetState.styleTokenTag = ...
```

完成标准：

1. `rg "SdComputedStyle|styleTokenTag|SdStyleTargetTags|SdStylePropertyTags|Style::TokenTag|context.style|ComputedStyle<"` 在核心代码中无结果；测试兼容文件也不应依赖。
2. examples 不再使用 `SdStyleToken::ColorButton` 等旧 token。
3. tests 不再断言旧 computed style。

## 5. 测试计划

### 5.1 Style Cascade Tests

必须覆盖：

1. user-agent / theme / user / scoped / inline / important 优先级。
2. specificity 和 source order。
3. typed class 匹配。
4. part selector 匹配。
5. pseudo state 匹配。
6. inherited property 传播。
7. theme/custom variable 变更触发最小重算。

### 5.2 Box Model Tests

必须覆盖：

1. content-box / border-box。
2. margin / border / padding / content box 计算。
3. width/height/min/max。
4. auto 和 percentage。
5. overflow clipping。
6. block layout。
7. flex row/column、gap、align-items、justify-content、flex-grow/shrink/basis。

### 5.3 Component Part Tests

必须覆盖：

1. Button root/content/icon/label part 生成。
2. Input field/value/placeholder/selection/caret part 生成。
3. part style 能继承 root text 属性。
4. part style transition 与 root style transition 独立。

### 5.4 Transition Tests

必须覆盖：

1. color interpolation。
2. opacity interpolation。
3. discrete property jump。
4. width/padding expensive transition。
5. pseudo state 切换触发 transition。
6. theme variable 改变后 transition retarget。

### 5.5 Performance Tests

必须覆盖：

1. 大量 widget/class/part 情况下 style resolve 不全表扫描。
2. steady frame 中无 style/layout change 时 cache hit。
3. inherited property 未变化时子树跳过重算。
4. normal frame 不发生 new/delete。
5. active transition channel 数量可诊断。

## 6. 最终验收要求

最终验收必须以完全符合架构设计为准，不能只保留兼容接口。

### 6.1 架构一致性

必须满足：

1. 核心 Style 类型为 `SdBoxStyle`、`SdWidgetRootStyle`、`SdWidgetPartStyle`。
2. 所有可选择内部节点通过 `SdStylePart` 声明。
3. Runtime style node 使用 `SdStyleNode`。
4. style pipeline 使用 specified/resolved/used/presentation。
5. Paint 只读取 used geometry 和 presentation style。
6. Layout 基于 CSS box model。
7. Selector / class / part / property 全部是强类型 ID。
8. CSS selector 不在 frame hot path 做字符串匹配。

### 6.2 代码清理

核心代码中必须移除旧路径：

```text
SdComputedStyle
SdStyleToken 作为核心 theme enum
SdStyleRule 旧 property bag rule
SdWidgetState::styleTokenTag
SdStyleCache::computed
context.style
ComputedStyle<TWidget>()
Style::TokenTag
```

如果因迁移期暂时保留，必须在同一 PR 或同一里程碑内删除；最终验收不允许存在。

### 6.3 行为一致性

必须满足：

1. 基础 CSS 盒模型测试通过。
2. CSS-like cascade 测试通过。
3. Button/Input/Div/Text 等基础组件样式表现符合新文档。
4. 同等 CSS 规则翻译为 SdGUI typed stylesheet 后，布局和绘制结果在定义的 CSS 子集内一致。
5. transition 的最终 presentation 值与 CSS timing 语义一致。

### 6.4 性能要求

必须满足：

1. style selector 预编译。
2. stylesheet 预分桶。
3. style/layout node 连续存储。
4. steady frame 不重复 resolve 未变化 style。
5. inherited property 有 revision propagation。
6. normal frame 无动态字符串匹配。
7. normal frame 无非必要堆分配。

### 6.5 示例和文档

必须满足：

1. examples 使用 typed class、typed stylesheet、theme variable。
2. examples 不再直接设置旧 `SdStyleToken`。
3. 文档中的 Button/Input/Div 使用示例可以直接映射到代码。
4. `SdGUI_Architecture.zh-CN.md` 与本文档描述一致。

## 7. 推荐提交顺序

推荐按以下顺序提交，避免一次性大爆炸式重构：

```text
1. 新增基础类型和空实现，不接入 runtime。
2. 引入 StyleNode tree，root-only 先跑通。
3. 接入 part declaration，Button/Input 做最小 part。
4. 新 stylesheet resolver 与旧 resolver 并行。
5. 新 box layout 与旧 layout 并行。
6. 迁移基础组件到 root/part style node。
7. 切默认路径到新 resolver + box layout。
8. 删除旧 computed/token/styleTokenTag 路径。
9. 完成 CSS 子集行为测试和性能测试。
```

每一步必须保持 tests 可运行。任何阶段只要引入兼容层，就必须在后续阶段明确删除。
