#include "SodiumGUI.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

namespace
{
	using namespace Sodium;

	int gFailedChecks = 0;

	void Check(bool condition, const char* message)
	{
		if (!condition)
		{
			std::printf("FAIL: %s\n", message);
			++gFailedChecks;
		}
	}

	void PumpFrame(SdInstance& instance)
	{
		instance.EndFrame();
	}

	SdColor ApplyOpacity(SdColor color, float opacity)
	{
		color.a = static_cast<SdUInt8>(static_cast<float>(color.a) * std::clamp(opacity, 0.0f, 1.0f));
		return color;
	}

	struct TestDrawWidget final : SdWidgetTag
	{
		static constexpr SdStyleId TargetTypeId = SdWidgetTargetIds::Button;

		struct Style final
		{
			static constexpr SdStyleId Background = SdStylePropertyIds::Background;
			static constexpr SdStyleId Border = SdStylePropertyIds::Border;
			static constexpr SdStyleId Color = SdStylePropertyIds::Color;
			static constexpr SdStyleId Radius = SdStylePropertyIds::Radius;
		};

		struct State
		{
			SdUtf8String label = {};
			bool clicked = false;
		};

		void OnUpdate(SdUpdateContext& context, SdUtf8StringView label)
		{
			State& state = context.State<State>();
			state.label.assign(label.data(), label.size());
			state.clicked = context.WasClicked();
			context.widgetState.targetTypeId = TargetTypeId;
		}

		void OnUpdate(SdUpdateContext& context, SdUtf8StringView label, bool& clicked)
		{
			OnUpdate(context, label);
			clicked = context.State<State>().clicked;
		}

		void OnLayout(SdLayoutContext& context)
		{
			context.SetDesiredSize({ 120.0f, 32.0f });
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const SdColor background = presentation.backgroundColor;
			const SdColor border = presentation.border.left.color;
			const SdColor color = presentation.color;
			const float radius = SdResolveLength(presentation.radius, context.animatedRect.Width());
			context.renderList.AddRectFilled(context.animatedRect, ApplyOpacity(background, context.opacity), context.clipRect, radius);
			context.renderList.AddRect(context.animatedRect, ApplyOpacity(border, context.opacity), context.clipRect, 1.0f, radius);
			context.renderList.AddText(state.label, { context.animatedRect.min.x + 8.0f, context.animatedRect.min.y + 8.0f }, ApplyOpacity(color, context.opacity), context.clipRect);
		}
	};

	struct TestContainer final : SdWidgetTag
	{
		static constexpr SdStyleId TargetTypeId = SdWidgetTargetIds::Panel;

		struct Style final
		{
			static constexpr SdStyleId Background = SdStylePropertyIds::Background;
			static constexpr SdStyleId Radius = SdStylePropertyIds::Radius;
		};

		template<class TContent>
			requires std::is_invocable_v<TContent&, SdUi&>
		void OnUpdate(SdUpdateContext& context, TContent&& content)
		{
			context.widgetState.targetTypeId = TargetTypeId;
			std::forward<TContent>(content)(context.ui);
		}

		void OnLayout(SdLayoutContext& context)
		{
			context.SetDesiredSize({ 220.0f, 110.0f });
			context.widgetState.arrangeChildren = true;
			context.widgetState.clipChildren = true;
			context.widgetState.childPadding = { 6.0f, 6.0f, 6.0f, 6.0f };
			context.widgetState.childSpacing = 4.0f;
		}

		void OnPaint(SdPaintContext& context)
		{
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const SdColor background = presentation.backgroundColor;
			const float radius = SdResolveLength(presentation.radius, context.animatedRect.Width());
			context.renderList.AddRectFilled(context.animatedRect, ApplyOpacity(background, context.opacity), context.clipRect, radius);
		}
	};

	struct CustomComponentStyle final
	{
		static constexpr SdStyleId TargetTypeId = SdStyleIdLiteral("Tests.CustomComponent");
		static constexpr SdStyleId Highlight = SdStyleIdLiteral("Tests.CustomComponent.Highlight");
		static constexpr SdStyleId ContentPadding = SdStyleIdLiteral("Tests.CustomComponent.ContentPadding");
	};

	struct CustomComponentWidget final : SdWidgetTag
	{
		static constexpr SdStyleId TargetTypeId = CustomComponentStyle::TargetTypeId;

		struct Style final
		{
			SdColor highlight = {};
			SdSpacing contentPadding = {};

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Paint(&Style::highlight).InterpolatesAsColor();
				contract.Layout(&Style::contentPadding);
			}
		};
	};

	struct StatefulWidget final : SdWidgetTag
	{
		struct State
		{
			int updateCount = 0;
		};

		void OnUpdate(SdUpdateContext& context)
		{
			++context.State<State>().updateCount;
		}

		void OnLayout(SdLayoutContext& context)
		{
			context.SetDesiredSize({ 24.0f, 12.0f });
		}
	};

	struct ModelWidget final : SdWidgetTag
	{
		struct Model
		{
			float value = 0.0f;
		};

		void OnLayout(SdLayoutContext& context)
		{
			context.SetDesiredSize({ 32.0f, 16.0f });
		}
	};

	float gTypedStyleLayoutWidth = 0.0f;
	SdColor gTypedStylePaintColor = {};
	bool gContextStyleNodeApiObservedRoot = false;
	bool gContextStyleNodeApiObservedPart = false;

	struct RegistryDispatchStyle final
	{
		SdColor color = SdColorWhite;
		float opacity = 1.0f;
	};

	struct RegistryDispatchWidget final : SdWidgetTag
	{
		using Style = RegistryDispatchStyle;
	};

	struct TypedStyleWidget final : SdWidgetTag
	{
		static constexpr SdStyleId TargetTypeId = SdStyleIdLiteral("Tests.TypedStyleWidget");

		struct Style final
		{
			float width = 24.0f;
			SdColor color = SdColorWhite;

			static Style Default(const SdStyleContext& context)
			{
				Style style = {};
				style.width = 24.0f;
				style.color = context.theme.GetColorVariable(SdThemeVariableLiteral("text"));
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Layout(&Style::width);
				contract.Paint(&Style::color).InterpolatesAsColor();
			}
		};

		void OnUpdate(SdUpdateContext& context)
		{
			context.widgetState.targetTypeId = TargetTypeId;
		}

		void OnLayout(SdLayoutContext& context)
		{
			const Style& style = context.RootResolvedStyle<TypedStyleWidget>();
			gTypedStyleLayoutWidth = style.width;
			context.SetDesiredSize({ style.width, 16.0f });
		}

		void OnPaint(SdPaintContext& context)
		{
			const Style& style = context.RootPresentationStyle<TypedStyleWidget>();
			gTypedStylePaintColor = style.color;
		}
	};

	struct StyleNodeApiWidget final : SdWidgetTag
	{
		struct Parts final
		{
			static constexpr SdStylePart Label = SdStylePart::Make("Tests.StyleNodeApiWidget.Part.Label");
		};

		struct Style final
		{
			float width = 32.0f;
			SdColor color = SdColorWhite;

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Layout(&Style::width);
				contract.Paint(&Style::color).InterpolatesAsColor();
			}
		};

		void OnUpdate(SdUpdateContext& context)
		{
			context.EnsurePart(Parts::Label);
		}

		void OnLayout(SdLayoutContext& context)
		{
			const Style& style = context.RootResolvedStyle<StyleNodeApiWidget>();
			context.SetDesiredSize({ style.width, 12.0f });
		}

		void OnPaint(SdPaintContext& context)
		{
			const SdStyleNode& root = context.RootStyleNode();
			const SdStyleNode& label = context.Part(Parts::Label);
			const Style& style = context.RootPresentationStyle<StyleNodeApiWidget>();
			gContextStyleNodeApiObservedRoot = root.kind == SdStyleNodeKind::Root
				&& root.widgetId == context.id
				&& style.width == 32.0f;
			gContextStyleNodeApiObservedPart = label.kind == SdStyleNodeKind::Part
				&& label.parentStyleNodeId == root.styleNodeId
				&& label.part == Parts::Label;
		}
	};

	struct CountingRenderer final : ISdRendererBackend
	{
		SdUInt32 renderCount = 0;
		SdUInt32 lastCommandCount = 0;
		SdVec2 lastDisplaySize = {};

		void Render(const SdRendererFrameInfo& frameInfo, const SdDrawPacket& packet) override
		{
			++renderCount;
			lastCommandCount = static_cast<SdUInt32>(packet.commands.size());
			lastDisplaySize = frameInfo.displaySize;
		}

		SdTextureHandle CreateTexture(const SdTextureDesc& desc) override
		{
			(void)desc;
			return {};
		}

		void DestroyTexture(SdTextureHandle texture) override
		{
			(void)texture;
		}
	};

	struct RecordingFontBackend final : ISdFontBackend
	{
		SdUtf8String lastMeasuredText = {};
		SdUtf8String lastPaintText = {};

		SdFontHandle GetFallbackFont() const noexcept override
		{
			return {};
		}

		SdVec2 MeasureText(SdUtf8StringView text, const SdTextStyle& style) override
		{
			lastMeasuredText.assign(text.data(), text.size());
			return { static_cast<float>(text.size()) * style.pixelSize * 0.5f, style.pixelSize };
		}

		SdParagraphLayout BuildParagraphLayout(
			SdUtf8StringView text,
			const SdTextStyle& style,
			float maxWidth,
			const SdColor& color,
			std::pmr::memory_resource* resource) override
		{
			(void)style;
			(void)maxWidth;
			(void)color;
			lastPaintText.assign(text.data(), text.size());
			SdParagraphLayout layout(resource);
			layout.size = { static_cast<float>(text.size()), 1.0f };
			return layout;
		}

		void ConfigureRenderSharedData(SdRenderSharedData& sharedData) const override
		{
			(void)sharedData;
		}

		void DrainPendingUploads(std::vector<SdUploadRequest>& uploads) override
		{
			(void)uploads;
		}
	};

	SdWidgetId FirstWidgetId(const SdInstance& instance)
	{
		const auto& records = instance.GetStateStorage().GetWidgetRecords();
		return records.empty() ? 0 : records.begin()->first;
	}

	void TestSmokeAndDrawPacket()
	{
		SdInstance instance;
		instance.BeginFrame({ 640.0f, 480.0f });
		instance.ui.Declare<TestDrawWidget>("Run");
		PumpFrame(instance);

		const SdFrameDiagnostics& diagnostics = instance.GetDiagnostics();
		Check(diagnostics.submittedWidgetCount == 1, "smoke submitted widget count");
		Check(diagnostics.liveWidgetCount == 1, "smoke live widget count");
		Check(instance.GetDrawPacket().commands.size() > 0, "smoke draw packet is non-empty");
	}

	void TestStyleCorePhaseOneTypes()
	{
		SdWidgetRootStyle rootStyle = {};
		rootStyle.display = SdDisplay::Flex;
		rootStyle.boxSizing = SdBoxSizing::BorderBox;
		rootStyle.width = SdLength::Pixels(120.0f);
		rootStyle.padding = SdBoxEdges::All(SdLength::Pixels(4.0f));
		rootStyle.backgroundColor = { 9, 8, 7, 255 };

		SdStyleNode rootNode = {};
		rootNode.styleNodeId = 1;
		rootNode.widgetId = 42;
		rootNode.part = SdStylePart::Root();
		rootNode.pseudoState = SdPseudoState::FromInteraction(SdStyleInteractionState::Pressed);
		rootNode.specifiedStyle = rootStyle;
		rootNode.resolvedStyle = rootStyle;
		rootNode.presentationStyle = rootStyle;

		Check(rootNode.part.IsRoot(), "style node root part is strongly typed");
		Check(rootNode.pseudoState.Has(SdPseudoStateFlag::Hovered), "pseudo state maps hovered bit");
		Check(rootNode.pseudoState.Has(SdPseudoStateFlag::Active), "pseudo state maps active bit");
		Check(rootNode.presentationStyle.width.unit == SdLengthUnit::Pixels, "presentation style carries CSS-like length");
		Check(rootNode.presentationStyle.backgroundColor == SdColor(9, 8, 7, 255), "presentation style carries paint color");
	}

	void TestStyleNodeRuntimeParts()
	{
		SdInstance instance;
		SdUtf8String text = "Value";
		bool windowOpen = true;
		instance.BeginFrame({ 640.0f, 480.0f });
		instance.ui.Declare<SdButton>("Parted");
		instance.ui.Declare<SdTextInput>(text, "Placeholder");
		instance.ui.Declare<SdWindow>("Window", windowOpen);
		PumpFrame(instance);

		bool buttonHasLabel = false;
		bool inputHasCaret = false;
		bool windowHasTitlebar = false;
		for (const auto& [id, record] : instance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdButton)))
				buttonHasLabel = instance.GetStylePart(record.state.id, SdButton::Parts::Label).part == SdButton::Parts::Label;
			if (record.widgetType == std::type_index(typeid(SdTextInput)))
				inputHasCaret = instance.GetStylePart(record.state.id, SdTextInput::Parts::Caret).part == SdTextInput::Parts::Caret;
			if (record.widgetType == std::type_index(typeid(SdWindow)))
				windowHasTitlebar = instance.GetStylePart(record.state.id, SdWindow::Parts::Titlebar).part == SdWindow::Parts::Titlebar;
		}

		Check(instance.GetDiagnostics().styleNodeCount >= 16, "runtime owns root and part style nodes");
		Check(buttonHasLabel, "button label part style node exists");
		Check(inputHasCaret, "text input caret part style node exists");
		Check(windowHasTitlebar, "window titlebar part style node exists");

		gContextStyleNodeApiObservedRoot = false;
		gContextStyleNodeApiObservedPart = false;
		SdInstance contextInstance;
		contextInstance.BeginFrame({ 320.0f, 200.0f });
		contextInstance.ui.Declare<StyleNodeApiWidget>();
		PumpFrame(contextInstance);
		Check(gContextStyleNodeApiObservedRoot, "context exposes root style node and root presentation style");
		Check(gContextStyleNodeApiObservedPart, "context exposes part style node");

		SdInstance partStyleInstance;
		partStyleInstance.GetStyleSystem().Part<SdButton>(SdButton::Parts::Label)
			.Set(&SdBoxStyle::opacity, 0.42f);
		partStyleInstance.BeginFrame({ 320.0f, 200.0f });
		partStyleInstance.ui.Declare<SdButton>("Part style");
		PumpFrame(partStyleInstance);
		bool partRuleApplied = false;
		for (const auto& [id, record] : partStyleInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdButton)))
				partRuleApplied = partStyleInstance.GetStylePart(record.state.id, SdButton::Parts::Label).presentationStyle.opacity == 0.42f;
		}
		Check(partRuleApplied, "runtime resolves compiled part selector into part style node");
	}

	void TestStyleSheetCascadeAndRegistry()
	{
		SdPropertyRegistry registry = {};
		registry.Register<&RegistryDispatchStyle::color>(SdStyleFieldImpact::Paint, SdStyleInterpolation::Color);
		registry.Register<&RegistryDispatchStyle::opacity>(SdStyleFieldImpact::Composite, SdStyleInterpolation::Float);

		SdStyleSheet sheet = {};
		constexpr SdStyleClassId dangerClass = SdStyleClassLiteral("Tests.Cascade.Danger");
		sheet.Rule<RegistryDispatchWidget>()
			.Set(&RegistryDispatchStyle::opacity, 0.25f);
		sheet.Rule<RegistryDispatchWidget>()
			.Class(dangerClass)
			.Set(&RegistryDispatchStyle::opacity, 0.50f);
		sheet.Rule<RegistryDispatchWidget>()
			.Class(dangerClass)
			.SetImportant(&RegistryDispatchStyle::opacity, 0.75f);
		const SdCompiledStyleSheet compiled = sheet.Compile();
		const SdStyleClassId classes[] = { dangerClass };
		SdStyleResolveRequest request = {};
		request.targetTag = SdStyleIdLiteral(typeid(RegistryDispatchWidget).name());
		request.classes = SdSpan<const SdStyleClassId>(classes, 1);

		const RegistryDispatchStyle resolved = SdStyleResolver::ResolveStyle<RegistryDispatchStyle>(
			compiled,
			request,
			registry,
			RegistryDispatchStyle{});
		Check(resolved.opacity == 0.75f, "compiled stylesheet cascade applies class specificity and important");

		SdStyleSystem styleSystem;
		styleSystem.SetMetricVariable("tests.width", 72.0f);
		styleSystem.Rule<StyleNodeApiWidget>()
			.Class(dangerClass)
			.Set(&StyleNodeApiWidget::Style::width, ThemeMetric("tests.width"));
		const StyleNodeApiWidget::Style systemResolved = styleSystem.ResolveTypedStyle<StyleNodeApiWidget>(
			SdStyleInteractionState::Normal,
			SdLayerPriority::Content,
			SdSpan<const SdStyleClassId>(classes, 1),
			0);
		Check(systemResolved.width == 72.0f, "style system resolves typed theme metric variable");

		styleSystem.Part<StyleNodeApiWidget>(StyleNodeApiWidget::Parts::Label)
			.Set(&SdWidgetPartStyle::opacity, 0.42f);
		Check(!styleSystem.GetCompiledStyleSheet().GetRules().empty(), "style system exposes compiled stylesheet with part rules");

		const SdWidgetRootStyle panelDefault = styleSystem.ResolveRootStyle(SdPanel::TargetTypeId, SdStyleInteractionState::Normal);
		Check(panelDefault.width.unit == SdLengthUnit::Pixels && panelDefault.width.value == 240.0f, "panel default width resolves through root style");
		Check(SdResolveLength(panelDefault.padding.left, 0.0f) > 0.0f, "panel default padding resolves through root style");
		Check(SdResolveLength(panelDefault.gap, 0.0f) == styleSystem.GetTheme().GetMetricVariable(SdThemeVariableLiteral("spacing.small")), "panel default child spacing resolves through root gap");

		constexpr SdStyleClassId textClass = SdStyleClassLiteral("Tests.Text.RootStyle");
		constexpr SdStyleScopeId textScope = SdStyleScopeLiteral("Tests.Text.Scope");
		const SdStyleClassId textClasses[] = { textClass };
		styleSystem.RootRule(SdText::TargetTypeId)
			.Scope(textScope)
			.Class(textClass)
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ 1.0f, 2.0f, 3.0f, 4.0f }));
		const SdWidgetRootStyle textScoped = styleSystem.ResolveRootStyle(
			SdText::TargetTypeId,
			SdStyleInteractionState::Normal,
			SdLayerPriority::Content,
			SdSpan<const SdStyleClassId>(textClasses, 1),
			textScope);
		Check(textScoped.padding.left.value == 1.0f && textScoped.padding.bottom.value == 4.0f, "text scoped padding resolves through root style");

		const SdWidgetRootStyle windowDefault = styleSystem.ResolveRootStyle(SdWindow::TargetTypeId, SdStyleInteractionState::Normal, SdLayerPriority::Floating);
		Check(windowDefault.width.unit == SdLengthUnit::Pixels && windowDefault.width.value == 420.0f, "window default width resolves through root style");
		Check(windowDefault.height.unit == SdLengthUnit::Pixels && windowDefault.height.value == 260.0f, "window default height resolves through root style");
		Check(windowDefault.padding.top.value == 40.0f, "window default title padding resolves through root style");

		const SdWidgetRootStyle buttonDefault = styleSystem.ResolveRootStyle(SdButton::TargetTypeId, SdStyleInteractionState::Normal);
		Check(buttonDefault.minWidth.unit == SdLengthUnit::Pixels && buttonDefault.minWidth.value == 82.0f, "button default min width resolves through root style");
		Check(buttonDefault.padding.left.value > buttonDefault.padding.top.value, "button default padding resolves through root style");
		Check(buttonDefault.fontSize == styleSystem.GetTheme().GetMetricVariable(SdThemeVariableLiteral("font.button")), "button font size resolves through root style");

		const SdWidgetRootStyle checkBoxDefault = styleSystem.ResolveRootStyle(SdCheckBox::TargetTypeId, SdStyleInteractionState::Normal);
		Check(checkBoxDefault.minHeight.unit == SdLengthUnit::Pixels && checkBoxDefault.minHeight.value == 28.0f, "checkbox default min height resolves through root style");
		Check(checkBoxDefault.padding.left.value == styleSystem.GetTheme().GetMetricVariable(SdThemeVariableLiteral("spacing.small")), "checkbox default padding resolves through root style");
		Check(SdResolveLength(checkBoxDefault.radius, 18.0f) >= 2.0f, "checkbox radius resolves through root style");

		const SdWidgetRootStyle sliderDefault = styleSystem.ResolveRootStyle(SdSliderFloat::TargetTypeId, SdStyleInteractionState::Normal);
		Check(sliderDefault.width.unit == SdLengthUnit::Pixels && sliderDefault.width.value == 180.0f, "slider default width resolves through root style");
		Check(sliderDefault.height.unit == SdLengthUnit::Pixels && sliderDefault.height.value == 30.0f, "slider default height resolves through root style");
		Check(sliderDefault.padding.left.value == styleSystem.GetTheme().GetMetricVariable(SdThemeVariableLiteral("spacing.small")), "slider default padding resolves through root style");

		const SdWidgetRootStyle textInputDefault = styleSystem.ResolveRootStyle(SdTextInput::TargetTypeId, SdStyleInteractionState::Normal);
		Check(textInputDefault.width.unit == SdLengthUnit::Pixels && textInputDefault.width.value == 220.0f, "text input default width resolves through root style");
		Check(textInputDefault.minHeight.unit == SdLengthUnit::Pixels && textInputDefault.minHeight.value == 32.0f, "text input default min height resolves through root style");
		Check(textInputDefault.padding.left.value > textInputDefault.padding.top.value, "text input default padding resolves through root style");

		const SdWidgetRootStyle imageViewerDefault = styleSystem.ResolveRootStyle(SdImageViewer::TargetTypeId, SdStyleInteractionState::Normal);
		Check(imageViewerDefault.width.unit == SdLengthUnit::Pixels && imageViewerDefault.width.value == 160.0f, "image viewer default width resolves through root style");
		Check(imageViewerDefault.height.unit == SdLengthUnit::Pixels && imageViewerDefault.height.value == 120.0f, "image viewer default height resolves through root style");

		const SdWidgetRootStyle scrollViewDefault = styleSystem.ResolveRootStyle(SdScrollView::TargetTypeId, SdStyleInteractionState::Normal);
		Check(scrollViewDefault.width.unit == SdLengthUnit::Pixels && scrollViewDefault.width.value == 240.0f, "scroll view default width resolves through root style");
		Check(scrollViewDefault.height.unit == SdLengthUnit::Pixels && scrollViewDefault.height.value == 160.0f, "scroll view default height resolves through root style");
		Check(scrollViewDefault.padding.left.value == styleSystem.GetTheme().GetMetricVariable(SdThemeVariableLiteral("spacing.small")), "scroll view default padding resolves through root style");

		const SdWidgetRootStyle popupDefault = styleSystem.ResolveRootStyle(SdPopup::TargetTypeId, SdStyleInteractionState::Normal, SdLayerPriority::Popup);
		const SdWidgetRootStyle contextMenuDefault = styleSystem.ResolveRootStyle(SdContextMenu::TargetTypeId, SdStyleInteractionState::Normal, SdLayerPriority::Popup);
		Check(popupDefault.width.unit == SdLengthUnit::Pixels && popupDefault.width.value == 220.0f, "popup default width resolves through root style");
		Check(contextMenuDefault.height.unit == SdLengthUnit::Pixels && contextMenuDefault.height.value == 140.0f, "context menu default height resolves through root style");

		const SdWidgetRootStyle tooltipDefault = styleSystem.ResolveRootStyle(SdTooltip::TargetTypeId, SdStyleInteractionState::Normal, SdLayerPriority::Overlay);
		Check(tooltipDefault.padding.left.value == styleSystem.GetTheme().GetMetricVariable(SdThemeVariableLiteral("spacing.small")), "tooltip default padding resolves through root style");
		Check(tooltipDefault.fontSize == styleSystem.GetTheme().GetMetricVariable(SdThemeVariableLiteral("font.button")), "tooltip font size resolves through root style");

		constexpr SdStyleClassId panelClass = SdStyleClassLiteral("Tests.Panel.RootStyle");
		constexpr SdStyleScopeId panelScope = SdStyleScopeLiteral("Tests.Panel.Scope");
		const SdStyleClassId panelClasses[] = { panelClass };
		styleSystem.RootRule(SdPanel::TargetTypeId)
			.Scope(panelScope)
			.Class(panelClass)
			.Set(&SdBoxStyle::width, SdLength::Pixels(333.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ 3.0f, 4.0f, 5.0f, 6.0f }))
			.Set(&SdBoxStyle::gap, SdLength::Pixels(7.0f));
		const SdWidgetRootStyle panelScoped = styleSystem.ResolveRootStyle(
			SdPanel::TargetTypeId,
			SdStyleInteractionState::Normal,
			SdLayerPriority::Content,
			SdSpan<const SdStyleClassId>(panelClasses, 1),
			panelScope);
		Check(panelScoped.width.unit == SdLengthUnit::Pixels && panelScoped.width.value == 333.0f, "panel scoped root width overrides default");
		Check(panelScoped.padding.left.value == 3.0f && panelScoped.padding.bottom.value == 6.0f, "panel scoped root padding writes box edges");
		Check(SdResolveLength(panelScoped.gap, 0.0f) == 7.0f, "panel scoped child spacing writes root gap");

		RegistryDispatchStyle localStyle = {};
		const SdPropertyDescriptor* colorProperty = registry.Find(Detail::SdStylePropertyId(&RegistryDispatchStyle::color), std::type_index(typeid(RegistryDispatchStyle)));
		Check(colorProperty && colorProperty->writeValue != nullptr, "property registry stores write dispatch table");
		if (colorProperty && colorProperty->writeValue)
			colorProperty->writeValue(&localStyle, SdStyleValue::FromColor({ 1, 2, 3, 4 }));
		Check(localStyle.color == SdColor(1, 2, 3, 4), "property registry dispatch writes typed field");
	}

	void TestBoxModelAndUsedStyleNodes()
	{
		SdBoxStyle style = {};
		style.padding = SdBoxEdges::All(SdLength::Pixels(4.0f));
		style.border = SdBorder::All(SdLength::Pixels(2.0f), SdColorWhite);
		style.margin = SdBoxEdges::All(SdLength::Pixels(3.0f));
		const SdUsedBox used = SdBuildUsedBox({ 10.0f, 20.0f, 110.0f, 70.0f }, style);
		Check(used.marginBox.min.x == 7.0f && used.marginBox.max.y == 73.0f, "box model computes margin box");
		Check(used.paddingBox.min.x == 12.0f && used.paddingBox.max.x == 108.0f, "box model computes padding box");
		Check(used.contentBox.min.x == 16.0f && used.contentBox.max.x == 104.0f, "box model computes content box");

		SdBoxStyle parentStyle = {};
		parentStyle.display = SdDisplay::Flex;
		parentStyle.flexDirection = SdFlexDirection::Row;
		parentStyle.gap = SdLength::Pixels(5.0f);
		parentStyle.width = SdLength::Pixels(120.0f);
		parentStyle.height = SdLength::Pixels(40.0f);
		SdBoxStyle childStyle = {};
		childStyle.width = SdLength::Pixels(20.0f);
		childStyle.height = SdLength::Pixels(10.0f);
		SdBoxTree tree = {};
		const SdUInt32 parent = tree.AddBox(1, SdInvalidIndex<SdUInt32>, parentStyle, { 120.0f, 40.0f });
		tree.AddBox(2, parent, childStyle, { 20.0f, 10.0f });
		tree.AddBox(3, parent, childStyle, { 20.0f, 10.0f });
		tree.Layout({ 0.0f, 0.0f, 200.0f, 100.0f });
		const std::vector<SdBoxNode>& boxes = tree.GetBoxes();
		Check(boxes.size() == 3, "box tree stores root and children");
		Check(boxes[1].borderBox.min.x == boxes[0].contentBox.min.x, "flex first child starts at content min");
		Check(boxes[2].borderBox.min.x == boxes[1].borderBox.max.x + 5.0f, "flex gap advances next child");

		SdInstance instance;
		instance.BeginFrame({ 320.0f, 200.0f });
		instance.ui.Declare<SdButton>("Used");
		PumpFrame(instance);
		bool hasUsedBox = false;
		for (const auto& [id, record] : instance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdButton)))
			{
				const SdStyleNode& root = instance.GetRootStyleNode(record.state.id);
				hasUsedBox = root.usedBox.borderBox.Width() > 0.0f
					&& root.usedBox.contentBox.Width() <= root.usedBox.borderBox.Width();
			}
		}
		Check(hasUsedBox, "runtime writes used geometry to root style node");
	}

	void TestIdAndKeySemantics()
	{
		SdInstance instance;
		instance.BeginFrame({ 640.0f, 480.0f });
		instance.ui.Declare<TestDrawWidget>("anonymous");
		PumpFrame(instance);
		const SdWidgetId anonymousId = FirstWidgetId(instance);

		instance.BeginFrame({ 640.0f, 480.0f });
		instance.ui.Declare<TestDrawWidget>("anonymous changed");
		PumpFrame(instance);
		Check(FirstWidgetId(instance) == anonymousId, "anonymous id stays stable for same parent/type/ordinal");

		instance.BeginFrame({ 640.0f, 480.0f });
		instance.ui.DeclareKeyed<TestDrawWidget>("stable_button", "Label A");
		PumpFrame(instance);
		SdWidgetId keyedId = 0;
		SdResolvedKey resolvedKey = 0;
		for (const auto& [id, record] : instance.GetStateStorage().GetWidgetRecords())
		{
			if (record.debugKey == "stable_button")
			{
				keyedId = id;
				resolvedKey = record.resolvedKey;
			}
		}

		instance.BeginFrame({ 640.0f, 480.0f });
		instance.ui.DeclareKeyed<TestDrawWidget>("stable_button", "Label B");
		PumpFrame(instance);
		const SdWidgetId remappedId = instance.GetStateStorage().FindWidgetIdByResolvedKey(resolvedKey);
		Check(keyedId != 0 && remappedId == keyedId, "keyed id is independent from label text");
		Check(resolvedKey != 0 && resolvedKey != keyedId, "resolved key is separate from widget id");
	}

	void TestLifecycleStateAndModel()
	{
		SdInstance instance;
		instance.BeginFrame({ 640.0f, 480.0f });
		instance.ui.Declare<StatefulWidget>();
		PumpFrame(instance);
		const SdWidgetId widgetId = FirstWidgetId(instance);
		Check(widgetId != 0, "stateful widget created");

		instance.BeginFrame({ 640.0f, 480.0f });
		instance.ui.Declare<StatefulWidget>();
		PumpFrame(instance);
		const SdWidgetRecord* record = instance.GetStateStorage().FindWidgetRecord(widgetId);
		Check(record != nullptr, "stateful widget reused");
		Check(record && record->userStates.size() == 1, "typed state is stored on widget record");

		auto& model = instance.ui.Model<ModelWidget>("stable_model");
		model.value = 42.0f;
		instance.BeginFrame({ 640.0f, 480.0f });
		PumpFrame(instance);
		Check(instance.ui.Model<ModelWidget>("stable_model").value == 42.0f, "keyed model persists without widget record");
		const SdUInt32 liveObjectCountBeforeSweep = instance.GetStateStorage().GetStats().liveObjectCount;

		std::this_thread::sleep_for(std::chrono::milliseconds(220));
		instance.BeginFrame({ 640.0f, 480.0f });
		PumpFrame(instance);
		Check(instance.GetDiagnostics().removedWidgetCount >= 1, "dead widget is swept");
		Check(instance.GetStateStorage().FindWidgetRecord(widgetId) == nullptr, "dead widget record is removed");
		Check(instance.GetDiagnostics().liveObjectCount < liveObjectCountBeforeSweep, "dead widget object storage is released");
		Check(instance.GetDiagnostics().modelCount >= 1, "keyed model remains after widget sweep");
	}

	void TestBasicTextModelI18n()
	{
		SdInstance instance;
		RecordingFontBackend fontBackend = {};
		instance.SetFontBackend(&fontBackend);

		instance.ui.Model<SdText>("title").SetText("Hello");
		instance.BeginFrame({ 640.0f, 480.0f });
		instance.ui.DeclareKeyed<SdText>("title", "Fallback");
		PumpFrame(instance);
		Check(fontBackend.lastMeasuredText == "Hello", "SdText model text drives layout");
		Check(fontBackend.lastPaintText == "Hello", "SdText model text drives paint");

		instance.ui.ConfigureModel<SdText>("title", [](SdText::Model& model)
		{
			model.SetText("Bonjour");
		});
		instance.BeginFrame({ 640.0f, 480.0f });
		instance.ui.DeclareKeyed<SdText>("title", "Fallback");
		PumpFrame(instance);
		Check(fontBackend.lastMeasuredText == "Bonjour", "SdText model can be changed externally");
		Check(fontBackend.lastPaintText == "Bonjour", "SdText paints externally changed model text");

		instance.ui.Model<SdText>("title").ClearText();
		instance.BeginFrame({ 640.0f, 480.0f });
		instance.ui.DeclareKeyed<SdText>("title", "Fallback");
		PumpFrame(instance);
		Check(fontBackend.lastMeasuredText == "Fallback", "SdText falls back to declared text when model text is clear");
	}

	void TestLayoutAnimationAndStyle()
	{
		SdInstance instance;
		instance.BeginFrame({ 640.0f, 480.0f });
		instance.ui.DeclareKeyed<TestContainer>("container", [](SdUi& ui)
		{
			ui.Declare<TestDrawWidget>("one");
			ui.Declare<TestDrawWidget>("two");
		});
		PumpFrame(instance);

		Check(instance.GetDiagnostics().submittedWidgetCount == 3, "nested declaration submits container children");
		Check(instance.GetLayoutSystem().GetNodes().size() == 3, "layout node exists for each declared widget");
		Check(instance.GetDiagnostics().layoutNodeCount == 3, "diagnostics expose layout node count");
		Check(instance.GetDiagnostics().activeAnimationChannelCount > 0, "enter animation channels are active");
		bool hasLayoutCache = false;
		bool hasStyleCache = false;
		bool hasExtendedAnimationChannels = false;
		bool hasStyleAnimationTarget = false;
		const SdWidgetRootStyle buttonStyle = instance.GetStyleSystem().ResolveRootStyle(TestDrawWidget::TargetTypeId, SdStyleInteractionState::Normal);
		for (const auto& [id, record] : instance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			hasLayoutCache = hasLayoutCache || record.layoutCache.targetRect.Width() > 0.0f;
			hasStyleCache = hasStyleCache || record.styleCache.valid;
			hasExtendedAnimationChannels = hasExtendedAnimationChannels
				|| (record.animation.styleColorR != 0 && record.animation.scrollOffset != 0);
			if (record.state.targetTypeId == TestDrawWidget::TargetTypeId)
			{
				const SdAnimationChannel& red = instance.GetContext().animationSystem.GetChannel(record.animation.styleBackgroundR);
				const SdAnimationChannel& green = instance.GetContext().animationSystem.GetChannel(record.animation.styleBackgroundG);
				const SdAnimationChannel& blue = instance.GetContext().animationSystem.GetChannel(record.animation.styleBackgroundB);
				hasStyleAnimationTarget = red.target == static_cast<float>(buttonStyle.backgroundColor.r)
					&& green.target == static_cast<float>(buttonStyle.backgroundColor.g)
					&& blue.target == static_cast<float>(buttonStyle.backgroundColor.b);
			}
		}
		Check(hasLayoutCache, "state storage owns layout cache");
		Check(hasStyleCache, "state storage owns presentation style cache");
		Check(hasExtendedAnimationChannels, "widget records own extended animation channel references");
		Check(hasStyleAnimationTarget, "style color animation targets presentation background");

		const SdWidgetRootStyle normal = instance.GetStyleSystem().ResolveRootStyle(TestDrawWidget::TargetTypeId, SdStyleInteractionState::Normal);
		const SdWidgetRootStyle hovered = instance.GetStyleSystem().ResolveRootStyle(TestDrawWidget::TargetTypeId, SdStyleInteractionState::Hovered);
		Check(normal.backgroundColor != hovered.backgroundColor, "style interaction selector changes button background");
		Check(normal.border.left.color == instance.GetStyleSystem().GetTheme().GetColorVariable(SdThemeVariableLiteral("border")), "presentation style carries theme border color");

		const SdUInt64 previousStyleRevision = instance.GetStyleSystem().GetRevision();
		const SdColor updatedButtonColor = { 11, 22, 33, 255 };
		instance.GetStyleSystem().SetColorVariable("button.bg", updatedButtonColor);
		instance.BeginFrame({ 640.0f, 480.0f });
		instance.ui.DeclareKeyed<TestContainer>("container", [](SdUi& ui)
		{
			ui.Declare<TestDrawWidget>("one");
			ui.Declare<TestDrawWidget>("two");
		});
		PumpFrame(instance);

		bool hasUpdatedStyleRevision = false;
		bool hasUpdatedStyleAnimationTargets = false;
		for (const auto& [id, record] : instance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.state.targetTypeId == TestDrawWidget::TargetTypeId)
			{
				hasUpdatedStyleRevision = record.styleCache.valid
					&& record.styleCache.styleRevision != previousStyleRevision
					&& record.styleCache.resolvedStyle.backgroundColor == updatedButtonColor;
				const SdAnimationChannel& red = instance.GetContext().animationSystem.GetChannel(record.animation.styleBackgroundR);
				const SdAnimationChannel& green = instance.GetContext().animationSystem.GetChannel(record.animation.styleBackgroundG);
				const SdAnimationChannel& blue = instance.GetContext().animationSystem.GetChannel(record.animation.styleBackgroundB);
				hasUpdatedStyleAnimationTargets = red.target == static_cast<float>(updatedButtonColor.r)
					&& green.target == static_cast<float>(updatedButtonColor.g)
					&& blue.target == static_cast<float>(updatedButtonColor.b)
					&& red.value != red.target;
			}
		}
		Check(hasUpdatedStyleRevision, "style cache refreshes when style system revision changes");
		Check(hasUpdatedStyleAnimationTargets, "style background animation retargets when style system revision changes");
	}

	void TestIdStackAndStyleLayerSelectors()
	{
		SdIdStack idStack;
		idStack.BeginFrame();
		const SdWidgetId firstAnonymous = idStack.ResolveAnonymousWidgetId(1001);
		const SdWidgetId secondAnonymous = idStack.ResolveAnonymousWidgetId(1001);
		Check(firstAnonymous != secondAnonymous, "id stack anonymous ordinal changes sibling id");

		idStack.BeginFrame();
		Check(idStack.ResolveAnonymousWidgetId(1001) == firstAnonymous, "id stack anonymous id is stable across frames");

		SdResolvedKey resolvedKey = 0;
		const SdWidgetId keyedId = idStack.ResolveKeyedWidgetId(2002, "stable", resolvedKey);
		Check(keyedId != 0 && resolvedKey != 0 && keyedId != resolvedKey, "id stack separates keyed widget id and resolved key");
		Check(idStack.ResolveModelKey(2002, "stable") == resolvedKey, "model key matches keyed declaration resolved key");
		Check(idStack.ResolveModelKey(3003, "stable") != resolvedKey, "model key includes widget type");

		idStack.PushParent(keyedId);
		SdResolvedKey childResolvedKey = 0;
		const SdWidgetId childKeyedId = idStack.ResolveKeyedWidgetId(2002, "stable", childResolvedKey);
		idStack.PopParent();
		Check(childKeyedId != keyedId && childResolvedKey != resolvedKey, "id stack keys are scoped by parent");

		SdStyleSystem styleSystem;
		styleSystem.RootRule(SdWidgetTargetIds::Default)
			.Layer(SdLayerPriority::Overlay)
			.Set(&SdBoxStyle::backgroundColor, ThemeColor("accent"));

		const SdWidgetRootStyle content = styleSystem.ResolveRootStyle(SdWidgetTargetIds::Default, SdStyleInteractionState::Normal, SdLayerPriority::Content);
		const SdWidgetRootStyle overlay = styleSystem.ResolveRootStyle(SdWidgetTargetIds::Default, SdStyleInteractionState::Normal, SdLayerPriority::Overlay);
		Check(content.backgroundColor != overlay.backgroundColor, "style layer selector changes overlay background");
		Check(overlay.backgroundColor == styleSystem.GetTheme().GetColorVariable(SdThemeVariableLiteral("accent")), "style layer selector applies matching rule");
	}

	void TestComponentStyleTargetTypeIds()
	{
		SdStyleSystem styleSystem;
		styleSystem.Rule<CustomComponentWidget>()
			.Set(&CustomComponentWidget::Style::highlight, SdColor{ 9, 8, 7, 6 })
			.Set(&CustomComponentWidget::Style::contentPadding, SdSpacing{ 3.0f, 4.0f, 5.0f, 6.0f });

		const CustomComponentWidget::Style custom = styleSystem.ResolveTypedStyle<CustomComponentWidget>(SdStyleInteractionState::Normal);
		const SdWidgetRootStyle text = styleSystem.ResolveRootStyle(SdWidgetTargetIds::Text, SdStyleInteractionState::Normal);
		Check(custom.highlight == SdColor(9, 8, 7, 6), "component target id resolves typed component style");
		Check(custom.contentPadding.left == 3.0f, "component target id resolves typed layout property");
		Check(text.backgroundColor == styleSystem.GetTheme().GetColorVariable(SdThemeVariableLiteral("background")), "typed component style does not leak to unrelated root targets");
	}

	void TestTypedStyleContractAndRuntimeCache()
	{
		SdStyleSystem styleSystem;
		const auto& contract = styleSystem.GetContract<TypedStyleWidget>();
		Check(contract.GetFields().size() == 2, "typed style contract records component fields");
		Check(contract.Find(Sodium::Detail::SdStyleFieldId(&TypedStyleWidget::Style::color)) != nullptr, "typed style contract can find color field");

		const SdColor danger = { 180, 50, 40, 255 };
		styleSystem.Rule<TypedStyleWidget>()
			.Set(&TypedStyleWidget::Style::width, 77.0f)
			.Set(&TypedStyleWidget::Style::color, danger);
		const TypedStyleWidget::Style resolved = styleSystem.ResolveTypedStyle<TypedStyleWidget>(SdStyleInteractionState::Normal);
		Check(resolved.width == 77.0f, "typed style rule sets layout field");
		Check(resolved.color == danger, "typed style rule sets paint field");

		constexpr SdStyleClassId dangerClass = SdStyleClassLiteral("tests.typed.danger");
		constexpr SdStyleScopeId dialogScope = SdStyleScopeLiteral("tests.typed.dialog");
		const SdStyleClassId classList[] = { dangerClass };
		const SdColor scopedColor = { 40, 180, 90, 255 };
		styleSystem.Rule<TypedStyleWidget>()
			.Class(dangerClass)
			.Set(&TypedStyleWidget::Style::width, 91.0f);
		styleSystem.Rule<TypedStyleWidget>()
			.Scope(dialogScope)
			.Set(&TypedStyleWidget::Style::color, scopedColor);
		const TypedStyleWidget::Style classResolved = styleSystem.ResolveTypedStyle<TypedStyleWidget>(
			SdStyleInteractionState::Normal,
			SdLayerPriority::Content,
			SdSpan<const SdStyleClassId>(classList, 1),
			0);
		Check(classResolved.width == 91.0f, "typed style class selector matches class id");
		const TypedStyleWidget::Style scopeResolved = styleSystem.ResolveTypedStyle<TypedStyleWidget>(
			SdStyleInteractionState::Normal,
			SdLayerPriority::Content,
			{},
			dialogScope);
		Check(scopeResolved.color == scopedColor, "typed style scope selector matches scope id");

		TypedStyleWidget::Style inlineStyle = {};
		inlineStyle.width = 123.0f;
		inlineStyle.color = { 3, 4, 5, 255 };
		const TypedStyleWidget::Style inlineResolved = styleSystem.ResolveTypedStyle<TypedStyleWidget>(
			SdStyleInteractionState::Normal,
			SdLayerPriority::Content,
			SdSpan<const SdStyleClassId>(classList, 1),
			dialogScope,
			&inlineStyle);
		Check(inlineResolved.width == inlineStyle.width && inlineResolved.color == inlineStyle.color, "inline target style overrides typed rules");

		SdInstance instance;
		instance.GetStyleSystem().Rule<TypedStyleWidget>()
			.Set(&TypedStyleWidget::Style::width, 88.0f)
			.Set(&TypedStyleWidget::Style::color, danger);
		gTypedStyleLayoutWidth = 0.0f;
		gTypedStylePaintColor = {};
		instance.BeginFrame({ 640.0f, 480.0f });
		instance.ui.Declare<TypedStyleWidget>();
		PumpFrame(instance);
		Check(gTypedStyleLayoutWidth == 88.0f, "layout reads typed target style from context");
		Check(gTypedStylePaintColor == danger, "paint reads typed presentation style from context");

		SdInstance identityInstance;
		identityInstance.GetStyleSystem().Rule<TypedStyleWidget>()
			.Class(dangerClass)
			.Set(&TypedStyleWidget::Style::width, 144.0f);
		identityInstance.GetStyleSystem().Rule<TypedStyleWidget>()
			.Scope(dialogScope)
			.Set(&TypedStyleWidget::Style::color, scopedColor);
		gTypedStyleLayoutWidth = 0.0f;
		gTypedStylePaintColor = {};
		identityInstance.BeginFrame({ 640.0f, 480.0f });
		identityInstance.ui.DeclareStyled<TypedStyleWidget>(
			SdStyleIdentity{ SdSpan<const SdStyleClassId>(classList, 1), dialogScope },
			nullptr);
		PumpFrame(identityInstance);
		Check(gTypedStyleLayoutWidth == 144.0f, "runtime typed style class identity affects target style");
		Check(gTypedStylePaintColor == scopedColor, "runtime typed style scope identity affects presentation style");

		SdInstance inlineInstance;
		inlineInstance.GetStyleSystem().Rule<TypedStyleWidget>()
			.Set(&TypedStyleWidget::Style::width, 88.0f)
			.Set(&TypedStyleWidget::Style::color, danger);
		gTypedStyleLayoutWidth = 0.0f;
		gTypedStylePaintColor = {};
		inlineInstance.BeginFrame({ 640.0f, 480.0f });
		inlineInstance.ui.DeclareStyled<TypedStyleWidget>(&inlineStyle);
		PumpFrame(inlineInstance);
		Check(gTypedStyleLayoutWidth == inlineStyle.width, "runtime inline target style overrides rule width");
		Check(gTypedStylePaintColor == inlineStyle.color, "runtime inline target style overrides presentation color");

		SdInstance transitionInstance;
		const SdColor startColor = { 10, 20, 30, 255 };
		const SdColor targetColor = { 210, 220, 230, 255 };
		transitionInstance.GetStyleSystem().Rule<TypedStyleWidget>()
			.Set(&TypedStyleWidget::Style::color, startColor);
		transitionInstance.GetStyleSystem().Rule<TypedStyleWidget>()
			.Transition(&TypedStyleWidget::Style::color, std::chrono::milliseconds(400), SdAnimationEasing::Linear);
		gTypedStylePaintColor = {};
		transitionInstance.BeginFrame({ 640.0f, 480.0f });
		transitionInstance.ui.Declare<TypedStyleWidget>();
		PumpFrame(transitionInstance);
		Check(gTypedStylePaintColor == startColor, "typed presentation style starts at initial target");

		std::this_thread::sleep_for(std::chrono::milliseconds(80));
		transitionInstance.GetStyleSystem().Rule<TypedStyleWidget>()
			.Set(&TypedStyleWidget::Style::color, targetColor);
		transitionInstance.BeginFrame({ 640.0f, 480.0f });
		transitionInstance.ui.Declare<TypedStyleWidget>();
		PumpFrame(transitionInstance);
		Check(gTypedStylePaintColor != startColor && gTypedStylePaintColor != targetColor, "typed presentation style interpolates between targets");
		Check(transitionInstance.GetDiagnostics().activeAnimationChannelCount > 0, "typed style transition contributes active animation diagnostics");

		std::this_thread::sleep_for(std::chrono::milliseconds(450));
		transitionInstance.BeginFrame({ 640.0f, 480.0f });
		transitionInstance.ui.Declare<TypedStyleWidget>();
		PumpFrame(transitionInstance);
		Check(gTypedStylePaintColor == targetColor, "typed presentation style reaches transition target");
	}

	void TestContextOwnershipAndRenderSubmission()
	{
		SdInstance instance;
		const SdContext& context = instance.GetContext();
		Check(static_cast<const void*>(&context.stateStorage) == static_cast<const void*>(&instance.GetStateStorage()), "context owns state storage");
		Check(static_cast<const void*>(&context.styleSystem) == static_cast<const void*>(&instance.GetStyleSystem()), "context owns style system");
		Check(static_cast<const void*>(&context.layoutSystem) == static_cast<const void*>(&instance.GetLayoutSystem()), "context owns layout system");
		Check(static_cast<const void*>(&context.renderSystem) == static_cast<const void*>(&instance.GetRenderSystem()), "context owns render system");
		Check(static_cast<const void*>(&context.renderStats) == static_cast<const void*>(&instance.GetRenderStats()), "context owns render stats");

		CountingRenderer renderer;
		instance.SetRendererBackend(&renderer);
		instance.BeginFrame({ 320.0f, 240.0f });
		instance.ui.Declare<TestDrawWidget>("Submit");
		PumpFrame(instance);
		instance.Render();

		Check(renderer.renderCount == 1, "SdInstance::Render submits exactly once");
		Check(renderer.lastCommandCount > 0, "SdInstance::Render submits draw packet commands");
		Check(renderer.lastDisplaySize.x == 320.0f && renderer.lastDisplaySize.y == 240.0f, "SdInstance::Render submits frame display size");
	}

	void TestInputLayerAndCustomWidgets()
	{
		SdInstance instance;
		bool clicked = false;

		instance.BeginFrame({ 640.0f, 480.0f });
		instance.ui.Declare<TestDrawWidget>("Click", clicked);
		PumpFrame(instance);

		Check(instance.GetLayerSystem().GetHitTestRecords().size() > 0, "layer system stores hit-test records");
		Check(instance.GetDiagnostics().drawCommandCount > 0, "custom widgets emit draw commands");

		instance.GetInputSystem().PushEvent(SdInputEvent{
			SdInputEventType::MouseMove,
			SdInputDevice::Mouse,
			SdMouseMoveEvent{ { 20.0f, 20.0f } }
		});
		instance.GetInputSystem().PushEvent(SdInputEvent{
			SdInputEventType::MouseButton,
			SdInputDevice::Mouse,
			SdMouseButtonEvent{ SdMouseButton::Left, true }
		});
		instance.GetInputSystem().PushEvent(SdInputEvent{
			SdInputEventType::MouseButton,
			SdInputDevice::Mouse,
			SdMouseButtonEvent{ SdMouseButton::Left, false }
		});
		instance.BeginFrame({ 640.0f, 480.0f });
		instance.ui.Declare<TestDrawWidget>("Click", clicked);
		PumpFrame(instance);
		Check(clicked, "custom widget click uses unified interaction state");
	}

	void TestInputSystemTextState()
	{
		SdInputSystem input;
		input.BeginFrame(1);
		input.PushEvent(SdInputEvent{
			SdInputEventType::TextCommit,
			SdInputDevice::Text,
			SdTextCommitEvent{ "Z" }
		});
		input.PushEvent(SdInputEvent{
			SdInputEventType::TextCompositionUpdate,
			SdInputDevice::Text,
			SdTextCompositionEvent{ "ime", 0, true }
		});
		input.FinalizeFrame();
		Check(input.GetCommittedText() == "Z", "input system stores committed text");
		Check(input.GetCompositionText() == "ime", "input system stores composition text");

		input.SetTextInputTarget(7, { 10.0f, 12.0f, 11.0f, 28.0f }, 16.0f);
		input.ActivateTextInput(7);
		const SdTextInputTarget* target = input.GetActiveTextInputTarget();
		Check(target && target->id == 7 && target->lineHeight == 16.0f, "input system tracks active text target");
		input.DeactivateTextInput(7);
		Check(input.GetActiveTextInputTarget() == nullptr, "input system clears active text target");
	}

	void TestBuiltInWidgetDeclarations()
	{
		SdInstance instance;
		RecordingFontBackend fontBackend = {};
		instance.SetFontBackend(&fontBackend);
		bool clicked = false;
		bool checked = false;
		float sliderValue = 0.25f;
		SdUtf8String inputValue = "edit";
		bool windowOpen = true;
		SdWindowOptions windowOptions = {};
		windowOptions.position = { 84.0f, 72.0f };
		windowOptions.size = { 360.0f, 220.0f };

		instance.BeginFrame({ 800.0f, 600.0f });
		instance.ui.Declare<SdPanel>([](SdUi& ui)
		{
			ui.Declare<SdText>("Panel text");
			ui.Declare<SdButton>("Nested");
		});
		instance.ui.Declare<SdButton>("Run", clicked);
		instance.ui.Declare<SdCheckBox>("Enabled", checked);
		instance.ui.Declare<SdCheckBox>(checked);
		instance.ui.Declare<SdSliderFloat>("Amount", sliderValue, 0.0f, 1.0f);
		instance.ui.Declare<SdTextInput>(inputValue, "Placeholder");
		instance.ui.Declare<SdWindow>("Window", windowOpen, windowOptions, [](SdUi& ui)
		{
			ui.Declare<SdText>("Window body");
		});
		instance.ui.Declare<SdImageViewer>(SdTextureHandle(3, 1), SdVec2{ 32.0f, 32.0f });
		instance.ui.Declare<SdScrollView>([](SdUi& ui)
		{
			ui.Declare<SdText>("Scroll child");
		});
		instance.ui.Declare<SdPopup>(true, SdVec2{ 360.0f, 90.0f }, [](SdUi& ui)
		{
			ui.Declare<SdText>("Popup body");
		});
		instance.ui.Declare<SdContextMenu>(true, SdVec2{ 420.0f, 120.0f }, [](SdUi& ui)
		{
			ui.Declare<SdButton>("Menu item");
		});
		instance.ui.Declare<SdTooltip>(true, SdVec2{ 500.0f, 150.0f }, "Tooltip");
		PumpFrame(instance);

		Check(instance.GetDiagnostics().submittedWidgetCount >= 17, "built-in widgets submit roots and children");
		Check(instance.GetDiagnostics().drawCommandCount > 0, "built-in widgets emit draw commands");
		Check(instance.GetLayerSystem().GetHitTestRecords().size() > 0, "built-in input widgets register hit tests");
		Check(fontBackend.lastMeasuredText.size() > 0, "built-in text-bearing widgets measure text");
	}

	void TestBuiltInWidgetInteraction()
	{
		{
			SdInstance instance;
			bool clicked = false;
			instance.BeginFrame({ 320.0f, 240.0f });
			instance.ui.Declare<SdButton>("Click", clicked);
			PumpFrame(instance);

			instance.GetInputSystem().PushEvent(SdInputEvent{
				SdInputEventType::MouseMove,
				SdInputDevice::Mouse,
				SdMouseMoveEvent{ { 20.0f, 20.0f } }
			});
			instance.GetInputSystem().PushEvent(SdInputEvent{
				SdInputEventType::MouseButton,
				SdInputDevice::Mouse,
				SdMouseButtonEvent{ SdMouseButton::Left, true }
			});
			instance.GetInputSystem().PushEvent(SdInputEvent{
				SdInputEventType::MouseButton,
				SdInputDevice::Mouse,
				SdMouseButtonEvent{ SdMouseButton::Left, false }
			});
			instance.BeginFrame({ 320.0f, 240.0f });
			instance.ui.Declare<SdButton>("Click", clicked);
			PumpFrame(instance);
			Check(clicked, "SdButton reports clicked through unified interaction");
		}

		{
			SdInstance instance;
			bool checked = false;
			instance.BeginFrame({ 320.0f, 240.0f });
			instance.ui.Declare<SdCheckBox>("Enable", checked);
			PumpFrame(instance);

			instance.GetInputSystem().PushEvent(SdInputEvent{
				SdInputEventType::MouseMove,
				SdInputDevice::Mouse,
				SdMouseMoveEvent{ { 20.0f, 20.0f } }
			});
			instance.GetInputSystem().PushEvent(SdInputEvent{
				SdInputEventType::MouseButton,
				SdInputDevice::Mouse,
				SdMouseButtonEvent{ SdMouseButton::Left, true }
			});
			instance.GetInputSystem().PushEvent(SdInputEvent{
				SdInputEventType::MouseButton,
				SdInputDevice::Mouse,
				SdMouseButtonEvent{ SdMouseButton::Left, false }
			});
			instance.BeginFrame({ 320.0f, 240.0f });
			instance.ui.Declare<SdCheckBox>("Enable", checked);
			PumpFrame(instance);
			Check(checked, "SdCheckBox toggles external bool on click");
		}

		{
			SdInstance instance;
			float value = 0.0f;
			instance.BeginFrame({ 320.0f, 240.0f });
			instance.ui.Declare<SdSliderFloat>(value, 0.0f, 1.0f);
			PumpFrame(instance);

			instance.GetInputSystem().PushEvent(SdInputEvent{
				SdInputEventType::MouseMove,
				SdInputDevice::Mouse,
				SdMouseMoveEvent{ { 198.0f, 24.0f } }
			});
			instance.GetInputSystem().PushEvent(SdInputEvent{
				SdInputEventType::MouseButton,
				SdInputDevice::Mouse,
				SdMouseButtonEvent{ SdMouseButton::Left, true }
			});
			instance.BeginFrame({ 320.0f, 240.0f });
			instance.ui.Declare<SdSliderFloat>(value, 0.0f, 1.0f);
			PumpFrame(instance);
			Check(value > 0.9f, "SdSliderFloat updates value from captured pointer");
		}

		{
			SdInstance instance;
			SdUtf8String value = "A";
			instance.BeginFrame({ 320.0f, 240.0f });
			instance.ui.Declare<SdTextInput>(value, "Text");
			PumpFrame(instance);

			instance.GetInputSystem().PushEvent(SdInputEvent{
				SdInputEventType::MouseMove,
				SdInputDevice::Mouse,
				SdMouseMoveEvent{ { 20.0f, 20.0f } }
			});
			instance.GetInputSystem().PushEvent(SdInputEvent{
				SdInputEventType::MouseButton,
				SdInputDevice::Mouse,
				SdMouseButtonEvent{ SdMouseButton::Left, true }
			});
			instance.GetInputSystem().PushEvent(SdInputEvent{
				SdInputEventType::MouseButton,
				SdInputDevice::Mouse,
				SdMouseButtonEvent{ SdMouseButton::Left, false }
			});
			instance.GetInputSystem().PushEvent(SdInputEvent{
				SdInputEventType::TextCommit,
				SdInputDevice::Text,
				SdTextCommitEvent{ "Z" }
			});
			instance.BeginFrame({ 320.0f, 240.0f });
			instance.ui.Declare<SdTextInput>(value, "Text");
			PumpFrame(instance);
			Check(value == "AZ", "SdTextInput appends committed text while focused");
			Check(instance.GetInputSystem().GetActiveTextInputTarget() != nullptr, "SdTextInput activates IME target while focused");

			instance.GetInputSystem().PushEvent(SdInputEvent{
				SdInputEventType::Key,
				SdInputDevice::Keyboard,
				SdKeyEvent{ SdKeyCode::Backspace, true, false }
			});
			instance.BeginFrame({ 320.0f, 240.0f });
			instance.ui.Declare<SdTextInput>(value, "Text");
			PumpFrame(instance);
			Check(value == "A", "SdTextInput handles backspace at the end of UTF-8 text");
		}
	}

	void TestLayoutSystemDirect()
	{
		SdLayoutSystem layout;
		layout.BeginFrame(3);
		layout.AddNode({
			1,
			0,
			{},
			SdLayoutResult{ { 120.0f, 120.0f } },
			{ 10.0f, 10.0f, 130.0f, 130.0f },
			{ 4.0f, 6.0f, 4.0f, 6.0f },
			1.0f,
			5.0f,
			true,
			true,
			true
		});
		layout.AddNode({
			2,
			1,
			{},
			SdLayoutResult{ { 50.0f, 20.0f } },
			{},
			{},
			1.0f,
			0.0f,
			false,
			false,
			false
		});
		layout.AddNode({
			3,
			1,
			{},
			SdLayoutResult{ { 50.0f, 40.0f } },
			{},
			{},
			0.5f,
			0.0f,
			false,
			false,
			false
		});

		layout.Measure({ 320.0f, 240.0f });
		layout.Arrange({ 0.0f, 0.0f, 320.0f, 240.0f });
		const std::vector<SdLayoutNode>& nodes = layout.GetNodes();
		Check(nodes.size() == 3, "layout direct stores frame-local nodes");
		Check(nodes[0].firstChildIndex == 1 && nodes[1].nextSiblingIndex == 2, "layout direct links parent children by index");
		Check(nodes[1].targetRect.min.x == 14.0f && nodes[1].targetRect.min.y == 16.0f, "layout direct applies child padding");
		Check(nodes[2].targetRect.min.y == 41.0f, "layout direct applies spacing and previous layoutWeight");
		Check(nodes[2].layoutWeight == 0.5f, "layout direct preserves leaving layout weight");
	}

	void TestAnimationSystemDirect()
	{
		SdAnimationSystem animation;
		const SdAnimationChannelId channel = animation.EnsureChannel(1, SdAnimationChannelKind::Opacity, 0.0f);
		animation.SetTarget(channel, 1.0f, SdTransition{ std::chrono::milliseconds(100), SdAnimationEasing::Linear });
		Check(animation.GetActiveChannelCount() == 1, "animation direct tracks active channel");

		animation.Update(std::chrono::milliseconds(50));
		const float halfValue = animation.GetValue(channel);
		Check(halfValue > 0.0f && halfValue < 1.0f, "animation direct advances active channel");

		animation.SetTarget(channel, 0.0f, SdTransition{ std::chrono::milliseconds(100), SdAnimationEasing::Linear });
		animation.Update(std::chrono::milliseconds(50));
		const float returningValue = animation.GetValue(channel);
		Check(returningValue > 0.0f && returningValue < halfValue, "animation direct retargets from current value");

		animation.Update(std::chrono::milliseconds(100));
		Check(animation.GetValue(channel) == 0.0f && animation.GetActiveChannelCount() == 0, "animation direct completes channel");

		const SdAnimationChannelId styleColor = animation.EnsureChannel(2, SdAnimationChannelKind::StyleColor, 0.0f);
		const SdAnimationChannelId scrollOffset = animation.EnsureChannel(2, SdAnimationChannelKind::ScrollOffset, 0.0f);
		animation.SetTarget(styleColor, 255.0f, SdTransition{ std::chrono::milliseconds(100), SdAnimationEasing::Linear });
		animation.SetTarget(scrollOffset, 32.0f, SdTransition{ std::chrono::milliseconds(100), SdAnimationEasing::Linear });
		animation.Update(std::chrono::milliseconds(50), false, false, false, false, true, false);
		Check(animation.GetValue(styleColor) > 0.0f && animation.GetValue(scrollOffset) == 0.0f, "animation direct can update style color independently");
		animation.Update(std::chrono::milliseconds(50), false, false, false, false, false, true);
		Check(animation.GetValue(scrollOffset) > 0.0f, "animation direct can update scroll offset independently");
	}

	void TestLayerSystemDirect()
	{
		SdLayerSystem layers;
		layers.BeginFrame();
		layers.AddHitTestRecord({ 1, SdLayerPriority::Content, { 0.0f, 0.0f, 100.0f, 100.0f }, { 0.0f, 0.0f, 100.0f, 100.0f }, 0, true });
		layers.AddHitTestRecord({ 2, SdLayerPriority::Popup, { 0.0f, 0.0f, 100.0f, 100.0f }, { 0.0f, 0.0f, 100.0f, 100.0f }, 0, true });
		Check(layers.HitTest({ 12.0f, 12.0f }) == 2, "layer direct higher priority wins hit-test");

		layers.BeginFrame();
		layers.AddHitTestRecord({ 1, SdLayerPriority::Content, { 0.0f, 0.0f, 100.0f, 100.0f }, { 0.0f, 0.0f, 100.0f, 100.0f }, 0, true });
		layers.AddHitTestRecord({ 2, SdLayerPriority::Content, { 0.0f, 0.0f, 100.0f, 100.0f }, { 0.0f, 0.0f, 100.0f, 100.0f }, 4, true });
		layers.AddHitTestRecord({ 3, SdLayerPriority::Overlay, { 0.0f, 0.0f, 100.0f, 100.0f }, { 0.0f, 0.0f, 100.0f, 100.0f }, 5, false });
		Check(layers.HitTest({ 12.0f, 12.0f }) == 2, "layer direct paint order wins within layer and disabled records are ignored");
	}

	void TestRenderListBatchingDirect()
	{
		SdRenderStats stats = {};
		SdRenderSharedData shared = {};
		shared.whiteTexture = SdTextureHandle(1, 1);
		SdRenderList list(&stats, &shared);
		list.Reset();

		const SdRect clip = { 0.0f, 0.0f, 200.0f, 200.0f };
		list.AddRectFilled({ 0.0f, 0.0f, 10.0f, 10.0f }, SdColorWhite, clip);
		list.AddRectFilled({ 20.0f, 0.0f, 30.0f, 10.0f }, SdColorWhite, clip);
		Check(list.GetDrawData().commands.size() == 1, "render list batches matching texture and clip");
		Check(list.GetDrawData().vertices.size() > 0 && list.GetDrawData().indices.size() > 0, "render list emits geometry");

		list.AddRectFilled({ 40.0f, 0.0f, 50.0f, 10.0f }, SdColorWhite, { 0.0f, 0.0f, 50.0f, 50.0f });
		Check(list.GetDrawData().commands.size() == 2, "render list splits batch by clip");
	}
}

int main()
{
	TestSmokeAndDrawPacket();
	TestStyleCorePhaseOneTypes();
	TestStyleNodeRuntimeParts();
	TestStyleSheetCascadeAndRegistry();
	TestBoxModelAndUsedStyleNodes();
	TestIdAndKeySemantics();
	TestLifecycleStateAndModel();
	TestBasicTextModelI18n();
	TestLayoutAnimationAndStyle();
	TestIdStackAndStyleLayerSelectors();
	TestComponentStyleTargetTypeIds();
	TestTypedStyleContractAndRuntimeCache();
	TestContextOwnershipAndRenderSubmission();
	TestInputLayerAndCustomWidgets();
	TestInputSystemTextState();
	TestBuiltInWidgetDeclarations();
	TestBuiltInWidgetInteraction();
	TestLayoutSystemDirect();
	TestAnimationSystemDirect();
	TestLayerSystemDirect();
	TestRenderListBatchingDirect();

	if (gFailedChecks != 0)
	{
		std::printf("%d checks failed.\n", gFailedChecks);
		return 1;
	}

	std::printf("All core tests passed.\n");
	return 0;
}
