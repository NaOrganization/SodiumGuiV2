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
		using Style = SdWidgetRootStyle;

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
		using Style = SdWidgetRootStyle;

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
		}

		void OnPaint(SdPaintContext& context)
		{
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const SdColor background = presentation.backgroundColor;
			const float radius = SdResolveLength(presentation.radius, context.animatedRect.Width());
			context.renderList.AddRectFilled(context.animatedRect, ApplyOpacity(background, context.opacity), context.clipRect, radius);
		}
	};

	struct TestOverflowContainer final : SdWidgetTag
	{
		static constexpr SdStyleId TargetTypeId = "Tests.OverflowContainer"_SdId;
		using Style = SdWidgetRootStyle;

		template<class TContent>
			requires std::is_invocable_v<TContent&, SdUi&>
		void OnUpdate(SdUpdateContext& context, TContent&& content)
		{
			context.widgetState.targetTypeId = TargetTypeId;
			std::forward<TContent>(content)(context.ui);
		}

		void OnLayout(SdLayoutContext& context)
		{
			context.SetDesiredSize({ 120.0f, 80.0f });
		}
	};

	struct TestManualLayoutWidget final : SdWidgetTag
	{
		static constexpr SdStyleId TargetTypeId = "Tests.ManualLayoutWidget"_SdId;
		using Style = SdWidgetRootStyle;

		void OnUpdate(SdUpdateContext& context)
		{
			context.widgetState.targetTypeId = TargetTypeId;
		}

		void OnLayout(SdLayoutContext& context)
		{
			context.SetDesiredSize({ 40.0f, 20.0f });
			context.widgetState.manualLayout = true;
			context.widgetState.manualRect = { 33.0f, 44.0f, 73.0f, 64.0f };
		}
	};

	struct CustomComponentStyle final
	{
		static constexpr SdStyleId TargetTypeId = "Tests.CustomComponent"_SdId;
		static constexpr SdStyleId Highlight = "Tests.CustomComponent.Highlight"_SdId;
		static constexpr SdStyleId ContentPadding = "Tests.CustomComponent.ContentPadding"_SdId;
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
	bool gPaintContextObservedLayoutBox = false;

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
		static constexpr SdStyleId TargetTypeId = "Tests.TypedStyleWidget"_SdId;

		struct Style final
		{
			float width = 24.0f;
			SdColor color = SdColorWhite;

			static Style Default(const SdStyleContext& context)
			{
				Style style = {};
				style.width = 24.0f;
				style.color = context.theme.GetColorVariable(SdThemeVariableIds::Text);
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
			static constexpr SdStylePart Label = SdStylePart::Make("Tests.StyleNodeApiWidget.Part.Label"_SdId);
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
		bool initialized = true;

		bool IsInitialized() const noexcept override
		{
			return initialized;
		}

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

	struct RecordingPlatformBackend final : ISdPlatformBackend
	{
		bool initialized = true;
		bool startedFrame = false;

		bool IsInitialized() const noexcept override
		{
			return initialized;
		}

		void StartFrame(SdInputSystem& input) override
		{
			(void)input;
			startedFrame = true;
		}
	};

	struct RecordingFontBackend final : ISdFontBackend
	{
		SdUtf8String lastMeasuredText = {};
		SdUtf8String lastPaintText = {};
		SdColor lastPaintColor = {};
		SdVec2 measuredSizeOverride = {};
		SdRect glyphBoundsOverride = {};
		bool hasMeasuredSizeOverride = false;
		bool hasGlyphBoundsOverride = false;
		bool initialized = true;

		bool IsInitialized() const noexcept override
		{
			return initialized;
		}

		SdFontHandle GetFallbackFont() const noexcept override
		{
			return {};
		}

		SdVec2 MeasureText(SdUtf8StringView text, const SdTextStyle& style) override
		{
			lastMeasuredText.assign(text.data(), text.size());
			if (hasMeasuredSizeOverride)
				return measuredSizeOverride;
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
			lastPaintColor = color;
			lastPaintText.assign(text.data(), text.size());
			SdParagraphLayout layout(resource);
			layout.size = hasMeasuredSizeOverride
				? measuredSizeOverride
				: SdVec2{ static_cast<float>(text.size()), 1.0f };
			if (hasGlyphBoundsOverride)
			{
				SdTextGlyph glyph = {};
				glyph.rect = glyphBoundsOverride;
				layout.glyphs.push_back(glyph);
			}
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

	struct PaintLayoutBoxWidget final : SdWidgetTag
	{
		static constexpr SdStyleId TargetTypeId = "Tests.PaintLayoutBoxWidget"_SdId;
		using Style = SdWidgetRootStyle;

		void OnUpdate(SdUpdateContext& context)
		{
			context.widgetState.targetTypeId = TargetTypeId;
		}

		void OnLayout(SdLayoutContext& context)
		{
			context.SetDesiredSize({ 48.0f, 24.0f });
		}

		void OnPaint(SdPaintContext& context)
		{
			gPaintContextObservedLayoutBox = context.rootLayoutBox.borderBox.Width() > 0.0f
				&& context.rootLayoutBox.contentBox.Width() <= context.rootLayoutBox.borderBox.Width();
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
		bool checked = false;
		instance.ui.Declare<SdCheckBox>("Check", checked);
		float sliderValue = 0.5f;
		instance.ui.Declare<SdSliderFloat>("Slide", sliderValue, 0.0f, 1.0f);
		instance.ui.Declare<SdTextInput>(text, "Placeholder");
		instance.ui.Declare<SdScrollView>([](SdUi& ui)
		{
			ui.Declare<SdText>("Scroll");
		});
		instance.ui.Declare<SdWindow>("Window", windowOpen);
		PumpFrame(instance);

		bool buttonHasLabel = false;
		bool checkBoxHasLabel = false;
		bool sliderHasLabel = false;
		bool inputHasCaret = false;
		bool windowHasTitlebar = false;
		bool windowHasCloseButton = false;
		bool scrollViewHasThumb = false;
		for (const auto& [id, record] : instance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdButton)))
				buttonHasLabel = instance.GetStylePart(record.state.id, SdButton::Parts::Label).part == SdButton::Parts::Label;
			if (record.widgetType == std::type_index(typeid(SdCheckBox)))
				checkBoxHasLabel = instance.GetStylePart(record.state.id, SdCheckBox::Parts::Label).part == SdCheckBox::Parts::Label;
			if (record.widgetType == std::type_index(typeid(SdSliderFloat)))
				sliderHasLabel = instance.GetStylePart(record.state.id, SdSliderFloat::Parts::Label).part == SdSliderFloat::Parts::Label;
			if (record.widgetType == std::type_index(typeid(SdTextInput)))
				inputHasCaret = instance.GetStylePart(record.state.id, SdTextInput::Parts::Caret).part == SdTextInput::Parts::Caret;
			if (record.widgetType == std::type_index(typeid(SdWindow)))
			{
				windowHasTitlebar = instance.GetStylePart(record.state.id, SdWindow::Parts::Titlebar).part == SdWindow::Parts::Titlebar;
				windowHasCloseButton = instance.GetStylePart(record.state.id, SdWindow::Parts::CloseButton).part == SdWindow::Parts::CloseButton;
			}
			if (record.widgetType == std::type_index(typeid(SdScrollView)))
				scrollViewHasThumb = instance.GetStylePart(record.state.id, SdScrollView::Parts::Thumb).part == SdScrollView::Parts::Thumb;
		}

		Check(instance.GetDiagnostics().styleNodeCount >= 28, "runtime owns root and part style nodes");
		Check(buttonHasLabel, "button label part style node exists");
		Check(checkBoxHasLabel, "checkbox label part style node exists");
		Check(sliderHasLabel, "slider label part style node exists");
		Check(inputHasCaret, "text input caret part style node exists");
		Check(windowHasTitlebar, "window titlebar part style node exists");
		Check(windowHasCloseButton, "window close button part style node exists");
		Check(scrollViewHasThumb, "scroll view thumb part style node exists");

		gContextStyleNodeApiObservedRoot = false;
		gContextStyleNodeApiObservedPart = false;
		SdInstance contextInstance;
		contextInstance.BeginFrame({ 320.0f, 200.0f });
		contextInstance.ui.Declare<StyleNodeApiWidget>();
		PumpFrame(contextInstance);
		Check(gContextStyleNodeApiObservedRoot, "context exposes root style node and root presentation style");
		Check(gContextStyleNodeApiObservedPart, "context exposes part style node");

		SdInstance partStyleInstance;
		RecordingFontBackend partFontBackend = {};
		partStyleInstance.SetFontBackend(&partFontBackend);
		const SdColor labelPartColor = SdColor(18, 52, 86, 255);
		const SdColor buttonContentPartColor = SdColor(74, 33, 105, 255);
		const SdColor buttonContentBorderPartColor = SdColor(105, 74, 33, 255);
		partStyleInstance.GetStyleSystem().Part<SdButton>(SdButton::Parts::Content)
			.Set(&SdBoxStyle::backgroundColor, buttonContentPartColor)
			.Transition(&SdBoxStyle::backgroundColor, std::chrono::milliseconds(280), SdAnimationEasing::Linear)
			.Set(&SdBoxStyle::border, SdStyleValue::FromColor(buttonContentBorderPartColor));
		partStyleInstance.GetStyleSystem().Part<SdButton>(SdButton::Parts::Label)
			.Set(&SdBoxStyle::opacity, 0.42f)
			.Set(&SdBoxStyle::color, labelPartColor);
		partStyleInstance.BeginFrame({ 320.0f, 200.0f });
		partStyleInstance.ui.Declare<SdButton>("Part style");
		PumpFrame(partStyleInstance);
		bool partRuleApplied = false;
		bool contentPartRuleApplied = false;
		for (const auto& [id, record] : partStyleInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdButton)))
			{
				const SdBoxStyle& contentStyle = partStyleInstance.GetStylePart(record.state.id, SdButton::Parts::Content).presentationStyle;
				const SdBoxStyle& labelStyle = partStyleInstance.GetStylePart(record.state.id, SdButton::Parts::Label).presentationStyle;
				contentPartRuleApplied = contentStyle.backgroundColor == buttonContentPartColor
					&& contentStyle.border.left.color == buttonContentBorderPartColor;
				partRuleApplied = labelStyle.opacity == 0.42f && labelStyle.color == labelPartColor;
			}
		}
		Check(contentPartRuleApplied, "button content part background and border resolve into part style node");
		Check(partRuleApplied, "runtime resolves compiled part selector into part style node");
		const SdUInt32 buttonContentPartPackedRgb = buttonContentPartColor.Pack() & 0x00ffffffu;
		const SdUInt32 buttonContentBorderPartPackedRgb = buttonContentBorderPartColor.Pack() & 0x00ffffffu;
		const SdDrawPacket buttonPartDrawPacket = partStyleInstance.GetDrawPacket();
		const bool buttonContentPartPainted = std::any_of(
			buttonPartDrawPacket.vertices.begin(),
			buttonPartDrawPacket.vertices.end(),
			[buttonContentPartPackedRgb](const SdVertex& vertex)
			{
				return (vertex.color & 0x00ffffffu) == buttonContentPartPackedRgb
					&& (vertex.color >> 24) > 0u;
			});
		const bool buttonContentBorderPartPainted = std::any_of(
			buttonPartDrawPacket.vertices.begin(),
			buttonPartDrawPacket.vertices.end(),
			[buttonContentBorderPartPackedRgb](const SdVertex& vertex)
			{
				return (vertex.color & 0x00ffffffu) == buttonContentBorderPartPackedRgb
					&& (vertex.color >> 24) > 0u;
			});
		Check(buttonContentPartPainted, "button content part background drives button paint");
		Check(buttonContentBorderPartPainted, "button content part border drives button paint");
		Check(partFontBackend.lastPaintText == "Part style", "button label part drives text paint");
		Check(
			partFontBackend.lastPaintColor.r == labelPartColor.r
			&& partFontBackend.lastPaintColor.g == labelPartColor.g
			&& partFontBackend.lastPaintColor.b == labelPartColor.b
			&& partFontBackend.lastPaintColor.a <= BasicWidgetDetail::ApplyOpacity(labelPartColor, 0.42f).a,
			"button label part color drives text paint");

		const SdColor buttonContentPartUpdatedColor = SdColor(120, 44, 24, 255);
		partStyleInstance.GetStyleSystem().Part<SdButton>(SdButton::Parts::Content)
			.Set(&SdBoxStyle::backgroundColor, buttonContentPartUpdatedColor)
			.Transition(&SdBoxStyle::backgroundColor, std::chrono::milliseconds(280), SdAnimationEasing::Linear);
		partStyleInstance.GetStyleSystem().Part<SdButton>(SdButton::Parts::Label)
			.Set(&SdBoxStyle::opacity, 0.18f)
			.Transition(&SdBoxStyle::opacity, std::chrono::milliseconds(180), SdAnimationEasing::Linear);
		partStyleInstance.BeginFrame({ 320.0f, 200.0f });
		partStyleInstance.ui.Declare<SdButton>("Part style");
		PumpFrame(partStyleInstance);
		bool partBackgroundTransitionUsesStylesheet = false;
		bool partBackgroundPresentationUsesChannel = false;
		bool partOpacityTransitionUsesStyleNodeChannel = false;
		bool partOpacityPresentationUsesChannel = false;
		const SdPropertyId partBackgroundPropertyId = Detail::SdStylePropertyId(&SdBoxStyle::backgroundColor);
		const SdPropertyId partOpacityPropertyId = Detail::SdStylePropertyId(&SdBoxStyle::opacity);
		for (const auto& [id, record] : partStyleInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType != std::type_index(typeid(SdButton)))
				continue;
			const SdStyleNode& contentNode = partStyleInstance.GetStylePart(record.state.id, SdButton::Parts::Content);
			const SdStyleNode& labelNode = partStyleInstance.GetStylePart(record.state.id, SdButton::Parts::Label);
			for (const SdPropertyAnimationChannel& channel : partStyleInstance.GetContext().presentationChannels.GetChannels())
			{
				if (channel.styleNodeId == contentNode.styleNodeId
					&& channel.propertyId == partBackgroundPropertyId
					&& channel.transition.duration == std::chrono::milliseconds(280))
				{
					partBackgroundTransitionUsesStylesheet = true;
					if (channel.currentValue.kind == SdStyleValueKind::Color)
						partBackgroundPresentationUsesChannel = contentNode.presentationStyle.backgroundColor == channel.currentValue.color;
				}
				if (channel.styleNodeId == labelNode.styleNodeId
					&& channel.propertyId == partOpacityPropertyId
					&& channel.impact == SdStyleFieldImpact::Composite
					&& channel.interpolation == SdStyleInterpolation::Float
					&& channel.transition.duration == std::chrono::milliseconds(180))
				{
					partOpacityTransitionUsesStyleNodeChannel = true;
					if (channel.currentValue.kind == SdStyleValueKind::Float)
						partOpacityPresentationUsesChannel = labelNode.presentationStyle.opacity == channel.currentValue.number;
				}
			}
		}
		Check(partBackgroundTransitionUsesStylesheet, "part background transition uses stylesheet duration");
		Check(partBackgroundPresentationUsesChannel, "part background presentation reads style node property channel");
		Check(partOpacityTransitionUsesStyleNodeChannel, "part opacity transition uses style node property channel");
		Check(partOpacityPresentationUsesChannel, "part opacity presentation reads style node property channel");

		SdInstance checkBoxPartStyleInstance;
		RecordingFontBackend checkBoxPartFontBackend = {};
		checkBoxPartStyleInstance.SetFontBackend(&checkBoxPartFontBackend);
		const SdColor checkBoxLabelPartColor = SdColor(57, 21, 99, 255);
		const SdColor checkBoxBoxPartColor = SdColor(19, 33, 47, 255);
		const SdColor checkBoxIndicatorPartColor = SdColor(220, 96, 12, 255);
		checkBoxPartStyleInstance.GetStyleSystem().Part<SdCheckBox>(SdCheckBox::Parts::Box)
			.Set(&SdBoxStyle::backgroundColor, checkBoxBoxPartColor);
		checkBoxPartStyleInstance.GetStyleSystem().Part<SdCheckBox>(SdCheckBox::Parts::Indicator)
			.Set(&SdBoxStyle::backgroundColor, checkBoxIndicatorPartColor);
		checkBoxPartStyleInstance.GetStyleSystem().Part<SdCheckBox>(SdCheckBox::Parts::Label)
			.Set(&SdBoxStyle::color, checkBoxLabelPartColor)
			.Set(&SdBoxStyle::opacity, 1.0f);
		bool checkBoxChecked = false;
		checkBoxPartStyleInstance.BeginFrame({ 320.0f, 200.0f });
		checkBoxPartStyleInstance.ui.Declare<SdCheckBox>("Part check", checkBoxChecked);
		PumpFrame(checkBoxPartStyleInstance);
		Check(checkBoxPartFontBackend.lastPaintText == "Part check", "checkbox label part drives text paint");
		Check(
			checkBoxPartFontBackend.lastPaintColor.r == checkBoxLabelPartColor.r
			&& checkBoxPartFontBackend.lastPaintColor.g == checkBoxLabelPartColor.g
			&& checkBoxPartFontBackend.lastPaintColor.b == checkBoxLabelPartColor.b,
			"checkbox label part color drives text paint");
		bool checkBoxBoxIndicatorPartsApplied = false;
		for (const auto& [id, record] : checkBoxPartStyleInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdCheckBox)))
			{
				const SdBoxStyle& boxStyle = checkBoxPartStyleInstance.GetStylePart(record.state.id, SdCheckBox::Parts::Box).presentationStyle;
				const SdBoxStyle& indicatorStyle = checkBoxPartStyleInstance.GetStylePart(record.state.id, SdCheckBox::Parts::Indicator).presentationStyle;
				checkBoxBoxIndicatorPartsApplied = boxStyle.backgroundColor == checkBoxBoxPartColor
					&& indicatorStyle.backgroundColor == checkBoxIndicatorPartColor;
			}
		}
		Check(checkBoxBoxIndicatorPartsApplied, "checkbox box and indicator part backgrounds resolve into part style nodes");

		SdInstance sliderPartStyleInstance;
		RecordingFontBackend sliderPartFontBackend = {};
		sliderPartStyleInstance.SetFontBackend(&sliderPartFontBackend);
		const SdColor sliderLabelPartColor = SdColor(8, 94, 63, 255);
		const SdColor sliderTrackPartColor = SdColor(32, 36, 42, 255);
		const SdColor sliderFillPartColor = SdColor(116, 70, 20, 255);
		const SdColor sliderThumbPartColor = SdColor(190, 120, 30, 255);
		sliderPartStyleInstance.GetStyleSystem().Part<SdSliderFloat>(SdSliderFloat::Parts::Label)
			.Set(&SdBoxStyle::color, sliderLabelPartColor)
			.Set(&SdBoxStyle::opacity, 1.0f);
		sliderPartStyleInstance.GetStyleSystem().Part<SdSliderFloat>(SdSliderFloat::Parts::Track)
			.Set(&SdBoxStyle::backgroundColor, sliderTrackPartColor);
		sliderPartStyleInstance.GetStyleSystem().Part<SdSliderFloat>(SdSliderFloat::Parts::Fill)
			.Set(&SdBoxStyle::backgroundColor, sliderFillPartColor);
		sliderPartStyleInstance.GetStyleSystem().Part<SdSliderFloat>(SdSliderFloat::Parts::Thumb)
			.Set(&SdBoxStyle::backgroundColor, sliderThumbPartColor);
		float sliderPartValue = 0.5f;
		sliderPartStyleInstance.BeginFrame({ 320.0f, 200.0f });
		sliderPartStyleInstance.ui.Declare<SdSliderFloat>("Part slide", sliderPartValue, 0.0f, 1.0f);
		PumpFrame(sliderPartStyleInstance);
		Check(sliderPartFontBackend.lastPaintText == "Part slide", "slider label part drives text paint");
		Check(
			sliderPartFontBackend.lastPaintColor.r == sliderLabelPartColor.r
			&& sliderPartFontBackend.lastPaintColor.g == sliderLabelPartColor.g
			&& sliderPartFontBackend.lastPaintColor.b == sliderLabelPartColor.b,
			"slider label part color drives text paint");
		bool sliderTrackFillThumbPartsApplied = false;
		for (const auto& [id, record] : sliderPartStyleInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdSliderFloat)))
			{
				const SdBoxStyle& trackStyle = sliderPartStyleInstance.GetStylePart(record.state.id, SdSliderFloat::Parts::Track).presentationStyle;
				const SdBoxStyle& fillStyle = sliderPartStyleInstance.GetStylePart(record.state.id, SdSliderFloat::Parts::Fill).presentationStyle;
				const SdBoxStyle& thumbStyle = sliderPartStyleInstance.GetStylePart(record.state.id, SdSliderFloat::Parts::Thumb).presentationStyle;
				sliderTrackFillThumbPartsApplied = trackStyle.backgroundColor == sliderTrackPartColor
					&& fillStyle.backgroundColor == sliderFillPartColor
					&& thumbStyle.backgroundColor == sliderThumbPartColor;
			}
		}
		Check(sliderTrackFillThumbPartsApplied, "slider track, fill, and thumb part backgrounds resolve into part style nodes");

		SdInstance scrollViewPartStyleInstance;
		const SdColor scrollViewScrollbarPartColor = SdColor(36, 68, 102, 255);
		const SdColor scrollViewScrollbarBorderPartColor = SdColor(102, 68, 36, 255);
		const SdColor scrollViewThumbPartColor = SdColor(70, 140, 200, 255);
		scrollViewPartStyleInstance.GetStyleSystem().Part<SdScrollView>(SdScrollView::Parts::Scrollbar)
			.Set(&SdBoxStyle::backgroundColor, scrollViewScrollbarPartColor)
			.Set(&SdBoxStyle::border, SdStyleValue::FromColor(scrollViewScrollbarBorderPartColor));
		scrollViewPartStyleInstance.GetStyleSystem().Part<SdScrollView>(SdScrollView::Parts::Thumb)
			.Set(&SdBoxStyle::backgroundColor, scrollViewThumbPartColor);
		scrollViewPartStyleInstance.BeginFrame({ 320.0f, 200.0f });
		scrollViewPartStyleInstance.ui.Declare<SdScrollView>([](SdUi& ui)
		{
			ui.Declare<SdText>("Part scroll");
		});
		PumpFrame(scrollViewPartStyleInstance);
		bool scrollViewScrollbarPartApplied = false;
		bool scrollViewThumbPartApplied = false;
		for (const auto& [id, record] : scrollViewPartStyleInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdScrollView)))
			{
				const SdBoxStyle& scrollbarStyle = scrollViewPartStyleInstance.GetStylePart(record.state.id, SdScrollView::Parts::Scrollbar).presentationStyle;
				const SdBoxStyle& thumbStyle = scrollViewPartStyleInstance.GetStylePart(record.state.id, SdScrollView::Parts::Thumb).presentationStyle;
				scrollViewScrollbarPartApplied = scrollbarStyle.backgroundColor == scrollViewScrollbarPartColor
					&& scrollbarStyle.border.left.color == scrollViewScrollbarBorderPartColor;
				scrollViewThumbPartApplied = thumbStyle.backgroundColor == scrollViewThumbPartColor;
			}
		}
		Check(scrollViewScrollbarPartApplied, "scroll view scrollbar part background and border resolve into part style node");
		Check(scrollViewThumbPartApplied, "scroll view thumb part background resolves into part style node");
		const SdUInt32 scrollViewScrollbarPartPackedRgb = scrollViewScrollbarPartColor.Pack() & 0x00ffffffu;
		const SdUInt32 scrollViewScrollbarBorderPartPackedRgb = scrollViewScrollbarBorderPartColor.Pack() & 0x00ffffffu;
		const SdDrawPacket scrollViewPartDrawPacket = scrollViewPartStyleInstance.GetDrawPacket();
		const bool scrollViewScrollbarPartPainted = std::any_of(
			scrollViewPartDrawPacket.vertices.begin(),
			scrollViewPartDrawPacket.vertices.end(),
			[scrollViewScrollbarPartPackedRgb](const SdVertex& vertex)
			{
				return (vertex.color & 0x00ffffffu) == scrollViewScrollbarPartPackedRgb
					&& (vertex.color >> 24) > 0u;
			});
		const bool scrollViewScrollbarBorderPartPainted = std::any_of(
			scrollViewPartDrawPacket.vertices.begin(),
			scrollViewPartDrawPacket.vertices.end(),
			[scrollViewScrollbarBorderPartPackedRgb](const SdVertex& vertex)
			{
				return (vertex.color & 0x00ffffffu) == scrollViewScrollbarBorderPartPackedRgb
					&& (vertex.color >> 24) > 0u;
			});
		Check(scrollViewScrollbarPartPainted, "scroll view scrollbar part background drives scroll view paint");
		Check(scrollViewScrollbarBorderPartPainted, "scroll view scrollbar part border drives scroll view paint");

		SdInstance inputPartStyleInstance;
		RecordingFontBackend inputPartFontBackend = {};
		inputPartStyleInstance.SetFontBackend(&inputPartFontBackend);
		const SdColor placeholderPartColor = SdColor(91, 35, 17, 255);
		const SdColor inputFieldPartColor = SdColor(42, 77, 118, 255);
		const SdColor inputFieldBorderPartColor = SdColor(118, 77, 42, 255);
		inputPartStyleInstance.GetStyleSystem().PartRule(SdTextInput::TargetTypeId, SdTextInput::Parts::Field)
			.Set(&SdBoxStyle::backgroundColor, inputFieldPartColor)
			.Set(&SdBoxStyle::border, SdStyleValue::FromColor(inputFieldBorderPartColor));
		inputPartStyleInstance.GetStyleSystem().PartRule(SdTextInput::TargetTypeId, SdTextInput::Parts::Placeholder)
			.Set(&SdBoxStyle::color, placeholderPartColor)
			.Set(&SdBoxStyle::opacity, 1.0f);
		SdUtf8String inputValue = {};
		inputPartStyleInstance.BeginFrame({ 320.0f, 200.0f });
		inputPartStyleInstance.ui.Declare<SdTextInput>(inputValue, "Part placeholder");
		PumpFrame(inputPartStyleInstance);
		Check(inputPartFontBackend.lastPaintText == "Part placeholder", "text input placeholder part drives text paint");
		Check(
			inputPartFontBackend.lastPaintColor.r == placeholderPartColor.r
			&& inputPartFontBackend.lastPaintColor.g == placeholderPartColor.g
			&& inputPartFontBackend.lastPaintColor.b == placeholderPartColor.b,
			"text input placeholder part color drives text paint");
		bool inputFieldPartRuleApplied = false;
		for (const auto& [id, record] : inputPartStyleInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdTextInput)))
			{
				const SdBoxStyle& fieldStyle = inputPartStyleInstance.GetStylePart(record.state.id, SdTextInput::Parts::Field).presentationStyle;
				inputFieldPartRuleApplied = fieldStyle.backgroundColor == inputFieldPartColor
					&& fieldStyle.border.left.color == inputFieldBorderPartColor;
			}
		}
		Check(inputFieldPartRuleApplied, "text input field part background and border resolve into part style node");
		const SdUInt32 inputFieldPartPackedRgb = inputFieldPartColor.Pack() & 0x00ffffffu;
		const SdUInt32 inputFieldBorderPartPackedRgb = inputFieldBorderPartColor.Pack() & 0x00ffffffu;
		const SdDrawPacket inputPartDrawPacket = inputPartStyleInstance.GetDrawPacket();
		const bool inputFieldPartPainted = std::any_of(
			inputPartDrawPacket.vertices.begin(),
			inputPartDrawPacket.vertices.end(),
			[inputFieldPartPackedRgb](const SdVertex& vertex)
			{
				return (vertex.color & 0x00ffffffu) == inputFieldPartPackedRgb
					&& (vertex.color >> 24) > 0u;
			});
		const bool inputFieldBorderPartPainted = std::any_of(
			inputPartDrawPacket.vertices.begin(),
			inputPartDrawPacket.vertices.end(),
			[inputFieldBorderPartPackedRgb](const SdVertex& vertex)
			{
				return (vertex.color & 0x00ffffffu) == inputFieldBorderPartPackedRgb
					&& (vertex.color >> 24) > 0u;
			});
		Check(inputFieldPartPainted, "text input field part background drives field paint");
		Check(inputFieldBorderPartPainted, "text input field part border drives field paint");

		SdInstance windowPartStyleInstance;
		RecordingFontBackend windowPartFontBackend = {};
		windowPartStyleInstance.SetFontBackend(&windowPartFontBackend);
		const SdColor titlePartColor = SdColor(22, 88, 143, 255);
		const SdColor titlebarPartColor = SdColor(13, 47, 69, 255);
		const SdColor closeButtonPartColor = SdColor(201, 43, 98, 255);
		const SdColor windowContentPartColor = SdColor(48, 29, 84, 255);
		const SdColor windowContentBorderPartColor = SdColor(84, 48, 29, 255);
		windowPartStyleInstance.GetStyleSystem().Part<SdWindow>(SdWindow::Parts::Content)
			.Set(&SdBoxStyle::backgroundColor, windowContentPartColor)
			.Set(&SdBoxStyle::border, SdStyleValue::FromColor(windowContentBorderPartColor));
		windowPartStyleInstance.GetStyleSystem().Part<SdWindow>(SdWindow::Parts::Titlebar)
			.Set(&SdBoxStyle::backgroundColor, titlebarPartColor);
		windowPartStyleInstance.GetStyleSystem().Part<SdWindow>(SdWindow::Parts::Title)
			.Set(&SdBoxStyle::color, titlePartColor)
			.Set(&SdBoxStyle::opacity, 1.0f);
		windowPartStyleInstance.GetStyleSystem().Part<SdWindow>(SdWindow::Parts::CloseButton)
			.Set(&SdBoxStyle::color, closeButtonPartColor)
			.Set(&SdBoxStyle::opacity, 1.0f);
		bool titleWindowOpen = true;
		windowPartStyleInstance.BeginFrame({ 320.0f, 200.0f });
		windowPartStyleInstance.ui.Declare<SdWindow>("Part title", titleWindowOpen);
		PumpFrame(windowPartStyleInstance);
		Check(windowPartFontBackend.lastPaintText == "Part title", "window title part drives text paint");
		Check(
			windowPartFontBackend.lastPaintColor.r == titlePartColor.r
			&& windowPartFontBackend.lastPaintColor.g == titlePartColor.g
			&& windowPartFontBackend.lastPaintColor.b == titlePartColor.b,
			"window title part color drives text paint");
		bool windowContentPartRuleApplied = false;
		bool titlebarPartRuleApplied = false;
		bool closeButtonPartRuleApplied = false;
		for (const auto& [id, record] : windowPartStyleInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdWindow)))
			{
				const SdBoxStyle& contentStyle = windowPartStyleInstance.GetStylePart(record.state.id, SdWindow::Parts::Content).presentationStyle;
				const SdBoxStyle& titlebarStyle = windowPartStyleInstance.GetStylePart(record.state.id, SdWindow::Parts::Titlebar).presentationStyle;
				const SdBoxStyle& closeButtonStyle = windowPartStyleInstance.GetStylePart(record.state.id, SdWindow::Parts::CloseButton).presentationStyle;
				windowContentPartRuleApplied = contentStyle.backgroundColor == windowContentPartColor
					&& contentStyle.border.left.color == windowContentBorderPartColor;
				titlebarPartRuleApplied = titlebarStyle.backgroundColor == titlebarPartColor;
				closeButtonPartRuleApplied = closeButtonStyle.color == closeButtonPartColor && closeButtonStyle.opacity == 1.0f;
			}
		}
		Check(windowContentPartRuleApplied, "window content part background and border resolve into part style node");
		Check(titlebarPartRuleApplied, "window titlebar part background resolves into part style node");
		Check(closeButtonPartRuleApplied, "window close button part color resolves into part style node");
		const SdUInt32 windowContentPartPackedRgb = windowContentPartColor.Pack() & 0x00ffffffu;
		const SdUInt32 windowContentBorderPartPackedRgb = windowContentBorderPartColor.Pack() & 0x00ffffffu;
		const SdUInt32 closeButtonPartPackedRgb = closeButtonPartColor.Pack() & 0x00ffffffu;
		const SdDrawPacket windowPartDrawPacket = windowPartStyleInstance.GetDrawPacket();
		const bool windowContentPartPainted = std::any_of(
			windowPartDrawPacket.vertices.begin(),
			windowPartDrawPacket.vertices.end(),
			[windowContentPartPackedRgb](const SdVertex& vertex)
			{
				return (vertex.color & 0x00ffffffu) == windowContentPartPackedRgb
					&& (vertex.color >> 24) > 0u;
			});
		const bool windowContentBorderPartPainted = std::any_of(
			windowPartDrawPacket.vertices.begin(),
			windowPartDrawPacket.vertices.end(),
			[windowContentBorderPartPackedRgb](const SdVertex& vertex)
			{
				return (vertex.color & 0x00ffffffu) == windowContentBorderPartPackedRgb
					&& (vertex.color >> 24) > 0u;
			});
		const bool closeButtonPartPainted = std::any_of(
			windowPartDrawPacket.vertices.begin(),
			windowPartDrawPacket.vertices.end(),
			[closeButtonPartPackedRgb](const SdVertex& vertex)
			{
				return (vertex.color & 0x00ffffffu) == closeButtonPartPackedRgb
					&& (vertex.color >> 24) > 0u;
			});
		Check(windowContentPartPainted, "window content part background drives window paint");
		Check(windowContentBorderPartPainted, "window content part border drives window paint");
		Check(closeButtonPartPainted, "window close button part color drives icon paint");
	}

	void TestStyleSheetCascadeAndRegistry()
	{
		constexpr SdThemeVariableId kTestsWidthVariable = "tests.width"_SdId;

		SdPropertyRegistry registry = {};
		registry.Register<&RegistryDispatchStyle::color>(SdStyleFieldImpact::Paint, SdStyleInterpolation::Color);
		registry.Register<&RegistryDispatchStyle::opacity>(SdStyleFieldImpact::Composite, SdStyleInterpolation::Float);

		SdStyleSheet sheet = {};
		constexpr SdStyleClassId dangerClass = "Tests.Cascade.Danger"_SdId;
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
		request.targetTag = Detail::SdTypeHash<RegistryDispatchWidget>();
		request.classes = SdSpan<const SdStyleClassId>(classes, 1);
		Check(!compiled.GetBuckets().empty(), "compiled stylesheet builds selector buckets");
		Check(
			compiled.CountCandidateRules(
				std::type_index(typeid(RegistryDispatchStyle)),
				request.targetTag,
				SdStylePart::Root()) == 3,
			"compiled stylesheet bucket returns target root candidates");

		const RegistryDispatchStyle resolved = SdStyleResolver::ResolveStyle<RegistryDispatchStyle>(
			compiled,
			request,
			registry,
			RegistryDispatchStyle{});
		Check(resolved.opacity == 0.75f, "compiled stylesheet cascade applies class specificity and important");

		SdStyleSystem styleSystem;
		styleSystem.SetMetricVariable(kTestsWidthVariable, 72.0f);
		styleSystem.Rule<StyleNodeApiWidget>()
			.Class(dangerClass)
			.Set(&StyleNodeApiWidget::Style::width, ThemeMetric(kTestsWidthVariable));
		const StyleNodeApiWidget::Style systemResolved = styleSystem.ResolveTypedStyle<StyleNodeApiWidget>(
			SdStyleInteractionState::Normal,
			SdRootLayer::Content,
			SdSpan<const SdStyleClassId>(classes, 1),
			0);
		Check(systemResolved.width == 72.0f, "style system resolves typed theme metric variable");

		styleSystem.Part<StyleNodeApiWidget>(StyleNodeApiWidget::Parts::Label)
			.Set(&SdWidgetPartStyle::opacity, 0.42f);
		styleSystem.Part<SdButton>(SdButton::Parts::Label)
			.Set(&SdWidgetPartStyle::opacity, 0.42f)
			.Transition(&SdWidgetPartStyle::opacity, std::chrono::milliseconds(260), SdAnimationEasing::Linear);
		styleSystem.RootRule(SdButton::TargetTypeId)
			.Transition(&SdWidgetRootStyle::opacity, std::chrono::milliseconds(180), std::chrono::milliseconds(45), SdAnimationEasing::Linear);
		styleSystem.RootRule(SdText::TargetTypeId)
			.Transition(&SdWidgetRootStyle::display, std::chrono::milliseconds(90), std::chrono::milliseconds(15), SdAnimationEasing::Linear, SdTransitionBehavior::AllowDiscrete);
		Check(!styleSystem.GetCompiledStyleSheet().GetRules().empty(), "style system exposes compiled stylesheet with part rules");
		Check(
			styleSystem.GetCompiledStyleSheet().CountCandidateRules(
				std::type_index(typeid(SdWidgetPartStyle)),
				SdButton::TargetTypeId,
				SdButton::Parts::Label)
				< styleSystem.GetCompiledStyleSheet().GetRules().size(),
			"compiled stylesheet narrows part selector candidates");
		SdTransition partTransition = {};
		SdTransition delayedRootTransition = {};
		SdTransition discreteRootTransition = {};
		const bool partTransitionResolved = styleSystem.TryResolvePartTransition(
			SdButton::TargetTypeId,
			SdButton::Parts::Label,
			Detail::SdStylePropertyId(&SdWidgetPartStyle::opacity),
			SdStyleInteractionState::Normal,
			SdRootLayer::Content,
			{},
			0,
			partTransition);
		const bool delayedRootTransitionResolved = styleSystem.TryResolveRootTransition(
			SdButton::TargetTypeId,
			Detail::SdStylePropertyId(&SdWidgetRootStyle::opacity),
			SdStyleInteractionState::Normal,
			SdRootLayer::Content,
			{},
			0,
			delayedRootTransition);
		const bool discreteRootTransitionResolved = styleSystem.TryResolveRootTransition(
			SdText::TargetTypeId,
			Detail::SdStylePropertyId(&SdWidgetRootStyle::display),
			SdStyleInteractionState::Normal,
			SdRootLayer::Content,
			{},
			0,
			discreteRootTransition);
		Check(partTransitionResolved && partTransition.duration == std::chrono::milliseconds(260), "part stylesheet transition resolves through compiled stylesheet");
		Check(
			delayedRootTransitionResolved
			&& delayedRootTransition.duration == std::chrono::milliseconds(180)
			&& delayedRootTransition.delay == std::chrono::milliseconds(45),
			"root stylesheet transition preserves delay");
		Check(
			discreteRootTransitionResolved
			&& discreteRootTransition.behavior == SdTransitionBehavior::AllowDiscrete
			&& discreteRootTransition.delay == std::chrono::milliseconds(15),
			"root stylesheet transition preserves discrete behavior");

		const SdWidgetRootStyle panelDefault = styleSystem.ResolveRootStyle(SdPanel::TargetTypeId, SdStyleInteractionState::Normal);
		Check(panelDefault.width.unit == SdLengthUnit::Pixels && panelDefault.width.value == 240.0f, "panel default width resolves through root style");
		Check(SdResolveLength(panelDefault.padding.left, 0.0f) > 0.0f, "panel default padding resolves through root style");
		Check(SdResolveLength(panelDefault.gap, 0.0f) == styleSystem.GetTheme().GetMetricVariable(SdThemeVariableIds::SpacingSmall), "panel default gap resolves through root gap");

		constexpr SdStyleClassId textClass = "Tests.Text.RootStyle"_SdId;
		constexpr SdStyleScopeId textScope = "Tests.Text.Scope"_SdId;
		const SdStyleClassId textClasses[] = { textClass };
		styleSystem.RootRule(SdText::TargetTypeId)
			.Scope(textScope)
			.Class(textClass)
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ 1.0f, 2.0f, 3.0f, 4.0f }));
		const SdWidgetRootStyle textScoped = styleSystem.ResolveRootStyle(
			SdText::TargetTypeId,
			SdStyleInteractionState::Normal,
			SdRootLayer::Content,
			SdSpan<const SdStyleClassId>(textClasses, 1),
			textScope);
		Check(textScoped.padding.left.value == 1.0f && textScoped.padding.bottom.value == 4.0f, "text scoped padding resolves through root style");

		const SdWidgetRootStyle windowDefault = styleSystem.ResolveRootStyle(SdWindow::TargetTypeId, SdStyleInteractionState::Normal, SdRootLayer::Floating);
		Check(windowDefault.width.unit == SdLengthUnit::Pixels && windowDefault.width.value == 420.0f, "window default width resolves through root style");
		Check(windowDefault.height.unit == SdLengthUnit::Pixels && windowDefault.height.value == 260.0f, "window default height resolves through root style");
		Check(windowDefault.padding.top.value == 40.0f, "window default title padding resolves through root style");
		Check(SdResolveLength(windowDefault.gap, 0.0f) == styleSystem.GetTheme().GetMetricVariable(SdThemeVariableIds::SpacingSmall), "window default gap resolves through root gap");
		const SdWidgetPartStyle windowContentDefault = styleSystem.ResolvePartStyle(SdWindow::TargetTypeId, SdWindow::Parts::Content, windowDefault, SdStyleInteractionState::Normal, SdRootLayer::Floating);
		const SdWidgetPartStyle windowTitlebarDefault = styleSystem.ResolvePartStyle(SdWindow::TargetTypeId, SdWindow::Parts::Titlebar, windowDefault, SdStyleInteractionState::Normal, SdRootLayer::Floating);
		Check(windowContentDefault.backgroundColor == styleSystem.GetTheme().GetColorVariable(SdThemeVariableIds::WindowBg), "window content default background resolves through part style");
		Check(windowTitlebarDefault.backgroundColor == styleSystem.GetTheme().GetColorVariable(SdThemeVariableIds::ButtonBg), "window titlebar default background resolves through part style");

		const SdWidgetRootStyle buttonDefault = styleSystem.ResolveRootStyle(SdButton::TargetTypeId, SdStyleInteractionState::Normal);
		Check(buttonDefault.minWidth.unit == SdLengthUnit::Pixels && buttonDefault.minWidth.value == 82.0f, "button default min width resolves through root style");
		Check(buttonDefault.padding.left.value > buttonDefault.padding.top.value, "button default padding resolves through root style");
		Check(buttonDefault.fontSize == styleSystem.GetTheme().GetMetricVariable(SdThemeVariableIds::FontButton), "button font size resolves through root style");
		const SdWidgetPartStyle buttonContentDefault = styleSystem.ResolvePartStyle(SdButton::TargetTypeId, SdButton::Parts::Content, buttonDefault, SdStyleInteractionState::Normal);
		const SdWidgetPartStyle buttonContentHoveredDefault = styleSystem.ResolvePartStyle(SdButton::TargetTypeId, SdButton::Parts::Content, buttonDefault, SdStyleInteractionState::Hovered);
		const SdWidgetPartStyle buttonContentPressedDefault = styleSystem.ResolvePartStyle(SdButton::TargetTypeId, SdButton::Parts::Content, buttonDefault, SdStyleInteractionState::Pressed);
		Check(buttonContentDefault.backgroundColor == styleSystem.GetTheme().GetColorVariable(SdThemeVariableIds::ButtonBg), "button content default background resolves through part style");
		Check(buttonContentHoveredDefault.backgroundColor == styleSystem.GetTheme().GetColorVariable(SdThemeVariableIds::ButtonBgHover), "button content hovered background resolves through part style");
		Check(buttonContentPressedDefault.backgroundColor == styleSystem.GetTheme().GetColorVariable(SdThemeVariableIds::ButtonBgPressed), "button content pressed background resolves through part style");

		const SdWidgetRootStyle checkBoxDefault = styleSystem.ResolveRootStyle(SdCheckBox::TargetTypeId, SdStyleInteractionState::Normal);
		Check(checkBoxDefault.minHeight.unit == SdLengthUnit::Pixels && checkBoxDefault.minHeight.value == 28.0f, "checkbox default min height resolves through root style");
		Check(checkBoxDefault.padding.left.value == styleSystem.GetTheme().GetMetricVariable(SdThemeVariableIds::SpacingSmall), "checkbox default padding resolves through root style");
		Check(SdResolveLength(checkBoxDefault.gap, 0.0f) == styleSystem.GetTheme().GetMetricVariable(SdThemeVariableIds::SpacingSmall), "checkbox default label gap resolves through root style");
		Check(SdResolveLength(checkBoxDefault.radius, 18.0f) >= 2.0f, "checkbox radius resolves through root style");
		const SdWidgetPartStyle checkBoxBoxDefault = styleSystem.ResolvePartStyle(SdCheckBox::TargetTypeId, SdCheckBox::Parts::Box, checkBoxDefault, SdStyleInteractionState::Normal);
		const SdWidgetPartStyle checkBoxIndicatorDefault = styleSystem.ResolvePartStyle(SdCheckBox::TargetTypeId, SdCheckBox::Parts::Indicator, checkBoxDefault, SdStyleInteractionState::Normal);
		Check(checkBoxBoxDefault.backgroundColor == styleSystem.GetTheme().GetColorVariable(SdThemeVariableIds::PanelBg), "checkbox box default background resolves through part style");
		Check(checkBoxIndicatorDefault.backgroundColor == styleSystem.GetTheme().GetColorVariable(SdThemeVariableIds::Accent), "checkbox indicator default background resolves through part style");

		const SdWidgetRootStyle sliderDefault = styleSystem.ResolveRootStyle(SdSliderFloat::TargetTypeId, SdStyleInteractionState::Normal);
		Check(sliderDefault.width.unit == SdLengthUnit::Pixels && sliderDefault.width.value == 180.0f, "slider default width resolves through root style");
		Check(sliderDefault.height.unit == SdLengthUnit::Pixels && sliderDefault.height.value == 30.0f, "slider default height resolves through root style");
		Check(sliderDefault.padding.left.value == styleSystem.GetTheme().GetMetricVariable(SdThemeVariableIds::SpacingSmall), "slider default padding resolves through root style");
		Check(SdResolveLength(sliderDefault.gap, 0.0f) == styleSystem.GetTheme().GetMetricVariable(SdThemeVariableIds::SpacingSmall), "slider default label gap resolves through root style");
		const SdWidgetPartStyle sliderTrackDefault = styleSystem.ResolvePartStyle(SdSliderFloat::TargetTypeId, SdSliderFloat::Parts::Track, sliderDefault, SdStyleInteractionState::Normal);
		const SdWidgetPartStyle sliderFillDefault = styleSystem.ResolvePartStyle(SdSliderFloat::TargetTypeId, SdSliderFloat::Parts::Fill, sliderDefault, SdStyleInteractionState::Normal);
		const SdWidgetPartStyle sliderThumbDefault = styleSystem.ResolvePartStyle(SdSliderFloat::TargetTypeId, SdSliderFloat::Parts::Thumb, sliderDefault, SdStyleInteractionState::Normal);
		Check(sliderTrackDefault.backgroundColor == styleSystem.GetTheme().GetColorVariable(SdThemeVariableIds::PanelBg), "slider track default background resolves through part style");
		Check(sliderFillDefault.backgroundColor == styleSystem.GetTheme().GetColorVariable(SdThemeVariableIds::Accent), "slider fill default background resolves through part style");
		Check(sliderThumbDefault.backgroundColor == styleSystem.GetTheme().GetColorVariable(SdThemeVariableIds::Accent), "slider thumb default background resolves through part style");

		const SdWidgetRootStyle textInputDefault = styleSystem.ResolveRootStyle(SdTextInput::TargetTypeId, SdStyleInteractionState::Normal);
		Check(textInputDefault.width.unit == SdLengthUnit::Pixels && textInputDefault.width.value == 220.0f, "text input default width resolves through root style");
		Check(textInputDefault.minHeight.unit == SdLengthUnit::Pixels && textInputDefault.minHeight.value == 32.0f, "text input default min height resolves through root style");
		Check(textInputDefault.padding.left.value > textInputDefault.padding.top.value, "text input default padding resolves through root style");
		const SdWidgetPartStyle textInputFieldDefault = styleSystem.ResolvePartStyle(SdTextInput::TargetTypeId, SdTextInput::Parts::Field, textInputDefault, SdStyleInteractionState::Normal);
		const SdWidgetPartStyle textInputFieldFocusedDefault = styleSystem.ResolvePartStyle(SdTextInput::TargetTypeId, SdTextInput::Parts::Field, textInputDefault, SdStyleInteractionState::Focused);
		Check(textInputFieldDefault.backgroundColor == styleSystem.GetTheme().GetColorVariable(SdThemeVariableIds::PanelBg), "text input field default background resolves through part style");
		Check(textInputFieldFocusedDefault.backgroundColor == styleSystem.GetTheme().GetColorVariable(SdThemeVariableIds::ButtonBg), "text input field focused background resolves through part style");

		const SdWidgetRootStyle imageViewerDefault = styleSystem.ResolveRootStyle(SdImageViewer::TargetTypeId, SdStyleInteractionState::Normal);
		Check(imageViewerDefault.width.unit == SdLengthUnit::Pixels && imageViewerDefault.width.value == 160.0f, "image viewer default width resolves through root style");
		Check(imageViewerDefault.height.unit == SdLengthUnit::Pixels && imageViewerDefault.height.value == 120.0f, "image viewer default height resolves through root style");

		const SdWidgetRootStyle scrollViewDefault = styleSystem.ResolveRootStyle(SdScrollView::TargetTypeId, SdStyleInteractionState::Normal);
		Check(scrollViewDefault.width.unit == SdLengthUnit::Pixels && scrollViewDefault.width.value == 240.0f, "scroll view default width resolves through root style");
		Check(scrollViewDefault.height.unit == SdLengthUnit::Pixels && scrollViewDefault.height.value == 160.0f, "scroll view default height resolves through root style");
		Check(scrollViewDefault.padding.left.value == styleSystem.GetTheme().GetMetricVariable(SdThemeVariableIds::SpacingSmall), "scroll view default padding resolves through root style");
		Check(SdResolveLength(scrollViewDefault.gap, 0.0f) == styleSystem.GetTheme().GetMetricVariable(SdThemeVariableIds::SpacingSmall), "scroll view default gap resolves through root gap");
		const SdWidgetPartStyle scrollViewScrollbarDefault = styleSystem.ResolvePartStyle(SdScrollView::TargetTypeId, SdScrollView::Parts::Scrollbar, scrollViewDefault, SdStyleInteractionState::Normal);
		const SdWidgetPartStyle scrollViewThumbDefault = styleSystem.ResolvePartStyle(SdScrollView::TargetTypeId, SdScrollView::Parts::Thumb, scrollViewDefault, SdStyleInteractionState::Normal);
		Check(scrollViewScrollbarDefault.backgroundColor == styleSystem.GetTheme().GetColorVariable(SdThemeVariableIds::PanelBg), "scroll view scrollbar default background resolves through part style");
		Check(scrollViewThumbDefault.backgroundColor == styleSystem.GetTheme().GetColorVariable(SdThemeVariableIds::Accent), "scroll view thumb default background resolves through part style");

		const SdWidgetRootStyle popupDefault = styleSystem.ResolveRootStyle(SdPopup::TargetTypeId, SdStyleInteractionState::Normal, SdRootLayer::Popup);
		const SdWidgetRootStyle contextMenuDefault = styleSystem.ResolveRootStyle(SdContextMenu::TargetTypeId, SdStyleInteractionState::Normal, SdRootLayer::Popup);
		Check(popupDefault.width.unit == SdLengthUnit::Pixels && popupDefault.width.value == 220.0f, "popup default width resolves through root style");
		Check(contextMenuDefault.height.unit == SdLengthUnit::Pixels && contextMenuDefault.height.value == 140.0f, "context menu default height resolves through root style");
		Check(SdResolveLength(popupDefault.gap, 0.0f) == styleSystem.GetTheme().GetMetricVariable(SdThemeVariableIds::SpacingSmall), "popup default gap resolves through root gap");
		Check(SdResolveLength(contextMenuDefault.gap, 0.0f) == styleSystem.GetTheme().GetMetricVariable(SdThemeVariableIds::SpacingSmall), "context menu default gap resolves through root gap");

		const SdWidgetRootStyle tooltipDefault = styleSystem.ResolveRootStyle(SdTooltip::TargetTypeId, SdStyleInteractionState::Normal, SdRootLayer::Tooltip);
		Check(tooltipDefault.padding.left.value == styleSystem.GetTheme().GetMetricVariable(SdThemeVariableIds::SpacingSmall), "tooltip default padding resolves through root style");
		Check(tooltipDefault.fontSize == styleSystem.GetTheme().GetMetricVariable(SdThemeVariableIds::FontButton), "tooltip font size resolves through root style");

		constexpr SdStyleClassId panelClass = "Tests.Panel.RootStyle"_SdId;
		constexpr SdStyleScopeId panelScope = "Tests.Panel.Scope"_SdId;
		const SdStyleClassId panelClasses[] = { panelClass };
		styleSystem.RootRule(SdPanel::TargetTypeId)
			.Scope(panelScope)
			.Class(panelClass)
			.Set(&SdBoxStyle::width, SdLength::Percent(50.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ 3.0f, 4.0f, 5.0f, 6.0f }))
			.Set(&SdBoxStyle::gap, SdLength::Pixels(7.0f));
		const SdWidgetRootStyle panelScoped = styleSystem.ResolveRootStyle(
			SdPanel::TargetTypeId,
			SdStyleInteractionState::Normal,
			SdRootLayer::Content,
			SdSpan<const SdStyleClassId>(panelClasses, 1),
			panelScope);
		Check(panelScoped.width.unit == SdLengthUnit::Percent && panelScoped.width.value == 50.0f, "panel scoped root width preserves percentage length");
		Check(panelScoped.padding.left.value == 3.0f && panelScoped.padding.bottom.value == 6.0f, "panel scoped root padding writes box edges");
		Check(SdResolveLength(panelScoped.gap, 0.0f) == 7.0f, "panel scoped gap writes root gap");

		RegistryDispatchStyle localStyle = {};
		const SdPropertyDescriptor* colorProperty = registry.Find(Detail::SdStylePropertyId(&RegistryDispatchStyle::color), std::type_index(typeid(RegistryDispatchStyle)));
		Check(colorProperty && colorProperty->writeValue != nullptr, "property registry stores write dispatch table");
		if (colorProperty && colorProperty->writeValue)
			colorProperty->writeValue(&localStyle, SdStyleValue::FromColor({ 1, 2, 3, 4 }));
		Check(localStyle.color == SdColor(1, 2, 3, 4), "property registry dispatch writes typed field");
	}

	void TestStylePipelineClosingRequirements()
	{
		constexpr SdStyleClassId inlineClass = "Tests.InlineRoot.Class"_SdId;
		constexpr SdStyleScopeId inlineScope = "Tests.InlineRoot.Scope"_SdId;
		const SdStyleClassId inlineClasses[] = { inlineClass };
		const SdColor ruleColor = { 91, 12, 33, 255 };
		const SdColor inlineColor = { 20, 90, 180, 255 };
		SdWidgetRootStyle inlineRootStyle = {};
		inlineRootStyle.width = SdLength::Pixels(96.0f);
		inlineRootStyle.height = SdLength::Pixels(32.0f);
		inlineRootStyle.minWidth = SdLength::Pixels(96.0f);
		inlineRootStyle.minHeight = SdLength::Pixels(32.0f);
		inlineRootStyle.backgroundColor = inlineColor;
		inlineRootStyle.color = { 240, 241, 242, 255 };
		inlineRootStyle.border = SdBorder::All(SdLength::Pixels(1.0f), SdColorTransparent);
		inlineRootStyle.radius = SdLength::Pixels(0.0f);
		inlineRootStyle.opacity = 0.73f;

		SdInstance inlineInstance;
		inlineInstance.GetStyleSystem().RootRule(SdButton::TargetTypeId)
			.Scope(inlineScope)
			.Class(inlineClass)
			.Set(&SdBoxStyle::backgroundColor, ruleColor);
		inlineInstance.BeginFrame({ 320.0f, 200.0f });
		inlineInstance.ui.DeclareStyled<SdButton>(
			SdStyleIdentity{ SdSpan<const SdStyleClassId>(inlineClasses, 1), inlineScope },
			&inlineRootStyle,
			"Inline");
		PumpFrame(inlineInstance);
		bool rootInlineApplied = false;
		bool rootInlineCached = false;
		for (const auto& [id, record] : inlineInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType != std::type_index(typeid(SdButton)))
				continue;
			const SdStyleNode& root = inlineInstance.GetRootStyleNode(record.state.id);
			rootInlineApplied = root.resolvedStyle.backgroundColor == inlineColor
				&& root.presentationStyle.backgroundColor == inlineColor
				&& root.resolvedStyle.backgroundColor != ruleColor;
			rootInlineCached = record.styleCache.inlineStyleRevision != 0;
		}
		Check(rootInlineApplied, "inline root style overrides class and UA rules in root style node");
		Check(rootInlineCached, "root style cache records inline style revision");

		SdInstance panelPaintInstance;
		panelPaintInstance.GetRenderSharedData().flags = 0;
		const SdColor panelInlineColor = { 12, 144, 88, 255 };
		SdWidgetRootStyle panelInlineStyle = {};
		panelInlineStyle.width = SdLength::Pixels(64.0f);
		panelInlineStyle.height = SdLength::Pixels(28.0f);
		panelInlineStyle.backgroundColor = panelInlineColor;
		panelInlineStyle.border = SdBorder::All(SdLength::Pixels(1.0f), SdColorTransparent);
		panelInlineStyle.radius = SdLength::Pixels(0.0f);
		panelPaintInstance.BeginFrame({ 320.0f, 200.0f });
		panelPaintInstance.ui.DeclareStyled<SdPanel>(&panelInlineStyle);
		PumpFrame(panelPaintInstance);
		const SdUInt32 panelInlinePackedRgb = panelInlineColor.Pack() & 0x00ffffffu;
		const bool panelPaintedInlineRoot = std::any_of(
			panelPaintInstance.GetRenderData().vertices.begin(),
			panelPaintInstance.GetRenderData().vertices.end(),
			[panelInlinePackedRgb](const SdVertex& vertex)
			{
				return (vertex.color & 0x00ffffffu) == panelInlinePackedRgb
					&& (vertex.color >> 24) > 0u;
			});
		Check(panelPaintedInlineRoot, "root inline paint reads RootStyleNode presentation style");

		SdInstance cacheInstance;
		SdWidgetRootStyle cacheStyle = panelInlineStyle;
		cacheInstance.BeginFrame({ 320.0f, 200.0f });
		cacheInstance.ui.DeclareStyledKeyed<SdPanel>("cache-panel", &cacheStyle);
		PumpFrame(cacheInstance);
		cacheInstance.BeginFrame({ 320.0f, 200.0f });
		cacheInstance.ui.DeclareStyledKeyed<SdPanel>("cache-panel", &cacheStyle);
		PumpFrame(cacheInstance);
		Check(cacheInstance.GetDiagnostics().styleResolveCacheHitCount > 0, "steady frame records style resolve cache hits");
		Check(cacheInstance.GetDiagnostics().styleResolveCacheMissCount == 0, "steady frame does not miss style cache without revision changes");
		Check(cacheInstance.GetDiagnostics().styleAnimationEnsureCount == 0, "steady frame does not ensure style animation channels on cache hit");
		Check(cacheInstance.GetDiagnostics().styleAnimationFindCount > 0, "steady frame reads existing style animation channels through find");
		cacheStyle.backgroundColor = { 190, 45, 75, 255 };
		cacheInstance.BeginFrame({ 320.0f, 200.0f });
		cacheInstance.ui.DeclareStyledKeyed<SdPanel>("cache-panel", &cacheStyle);
		PumpFrame(cacheInstance);
		Check(cacheInstance.GetDiagnostics().styleResolveCacheMissCount > 0, "inline root style change invalidates style cache");

		SdInstance geometryInstance;
		float geometrySliderValue = 0.5f;
		SdUtf8String geometryInputValue = "A";
		bool geometryWindowOpen = true;
		SdWindowOptions geometryWindowOptions = {};
		geometryWindowOptions.position = { 40.0f, 50.0f };
		geometryWindowOptions.size = { 220.0f, 120.0f };
		geometryInstance.BeginFrame({ 420.0f, 260.0f });
		geometryInstance.ui.Declare<SdSliderFloat>("Geometry", geometrySliderValue, 0.0f, 1.0f);
		geometryInstance.ui.DeclareKeyed<SdTextInput>("geometry-input", geometryInputValue, "");
		geometryInstance.ui.Declare<SdWindow>("Geometry", geometryWindowOpen, geometryWindowOptions);
		PumpFrame(geometryInstance);
		bool sliderPartsHaveIndependentGeometry = false;
		bool windowTitlebarHasUsedGeometry = false;
		float firstCaretX = 0.0f;
		for (const auto& [id, record] : geometryInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdSliderFloat)))
			{
				const SdStyleNode& root = geometryInstance.GetRootStyleNode(record.state.id);
				const SdStyleNode& track = geometryInstance.GetStylePart(record.state.id, SdSliderFloat::Parts::Track);
				const SdStyleNode& thumb = geometryInstance.GetStylePart(record.state.id, SdSliderFloat::Parts::Thumb);
				sliderPartsHaveIndependentGeometry = track.layoutBox.borderBox.Width() > 0.0f
					&& thumb.layoutBox.borderBox.Width() > 0.0f
					&& track.layoutBox.borderBox.Width() < root.usedBox.borderBox.Width()
					&& thumb.layoutBox.borderBox.Width() != track.layoutBox.borderBox.Width();
			}
			if (record.widgetType == std::type_index(typeid(SdWindow)))
			{
				const SdStyleNode& titlebar = geometryInstance.GetStylePart(record.state.id, SdWindow::Parts::Titlebar);
				windowTitlebarHasUsedGeometry = std::abs(titlebar.layoutBox.borderBox.Height() - 30.0f) < 0.001f;
			}
			if (record.widgetType == std::type_index(typeid(SdTextInput)))
				firstCaretX = geometryInstance.GetStylePart(record.state.id, SdTextInput::Parts::Caret).layoutBox.borderBox.min.x;
		}
		geometryInputValue = "AAAAA";
		geometryInstance.BeginFrame({ 420.0f, 260.0f });
		geometryInstance.ui.Declare<SdSliderFloat>("Geometry", geometrySliderValue, 0.0f, 1.0f);
		geometryInstance.ui.DeclareKeyed<SdTextInput>("geometry-input", geometryInputValue, "");
		geometryInstance.ui.Declare<SdWindow>("Geometry", geometryWindowOpen, geometryWindowOptions);
		PumpFrame(geometryInstance);
		bool textInputCaretGeometryMoved = false;
		for (const auto& [id, record] : geometryInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdTextInput)))
			{
				const float secondCaretX = geometryInstance.GetStylePart(record.state.id, SdTextInput::Parts::Caret).layoutBox.borderBox.min.x;
				textInputCaretGeometryMoved = secondCaretX > firstCaretX;
			}
		}
		Check(sliderPartsHaveIndependentGeometry, "slider track and thumb parts own independent geometry");
		Check(windowTitlebarHasUsedGeometry, "window titlebar part layout height equals title height");
		Check(textInputCaretGeometryMoved, "text input caret part geometry follows text width");

		SdStyleSystem phaseSystem;
		constexpr SdThemeVariableId phaseBackgroundVariable = "tests.phase.bg"_SdId;
		constexpr SdThemeVariableId phaseRadiusVariable = "tests.phase.radius"_SdId;
		const SdColor phaseColor = { 7, 99, 155, 255 };
		phaseSystem.SetColorVariable(phaseBackgroundVariable, phaseColor);
		phaseSystem.SetMetricVariable(phaseRadiusVariable, 13.0f);
		phaseSystem.RootRule(SdPanel::TargetTypeId)
			.Set(&SdBoxStyle::backgroundColor, ThemeColor(phaseBackgroundVariable))
			.Set(&SdBoxStyle::radius, SdLength::Variable(phaseRadiusVariable));
		const SdStyleResolveResult phaseResult = phaseSystem.ResolveRootNode(SdPanel::TargetTypeId, SdStyleInteractionState::Normal);
		Check(
			phaseResult.specifiedStyle.backgroundColorVariable == phaseBackgroundVariable
			&& phaseResult.specifiedStyle.radius.unit == SdLengthUnit::Variable,
			"specified root style preserves theme variable inputs");
		Check(
			phaseResult.resolvedStyle.backgroundColor == phaseColor
			&& phaseResult.resolvedStyle.radius.unit == SdLengthUnit::Pixels
			&& phaseResult.resolvedStyle.radius.value == 13.0f,
			"resolved root style resolves theme variables");
		Check(phaseResult.presentationStyle.backgroundColor == phaseResult.resolvedStyle.backgroundColor, "presentation base starts from resolved style");
		phaseSystem.SetColorVariable(phaseBackgroundVariable, { 88, 77, 66, 255 });
		const SdStyleResolveResult updatedPhaseResult = phaseSystem.ResolveRootNode(SdPanel::TargetTypeId, SdStyleInteractionState::Normal);
		Check(updatedPhaseResult.resolvedStyle.backgroundColor == SdColor(88, 77, 66, 255), "theme variable revision changes resolved style");

		SdWidgetRootStyle inheritedRoot = {};
		inheritedRoot.color = { 3, 4, 5, 255 };
		inheritedRoot.opacity = 0.44f;
		inheritedRoot.fontSize = 21.0f;
		const SdStyleResolveResult inheritedPart = phaseSystem.ResolvePartNode(
			SdButton::TargetTypeId,
			SdButton::Parts::Label,
			inheritedRoot,
			SdStyleInteractionState::Normal);
		Check(inheritedPart.specifiedStyle.color != inheritedRoot.color, "part specified style does not pre-copy root text");
		Check(
			inheritedPart.resolvedStyle.color == inheritedRoot.color
			&& inheritedPart.resolvedStyle.opacity == inheritedRoot.opacity
			&& inheritedPart.resolvedStyle.fontSize == inheritedRoot.fontSize,
			"part text inheritance happens in resolved stage");

		SdStyleSystem uaSystem;
		Check(!uaSystem.GetCompiledStyleSheet().GetRules().empty(), "default UA stylesheet compiles non-empty");
		uaSystem.RootRule(SdPanel::TargetTypeId)
			.Set(&SdBoxStyle::width, SdLength::Pixels(333.0f));
		Check(uaSystem.ResolveRootStyle(SdPanel::TargetTypeId, SdStyleInteractionState::Normal).width.value == 333.0f, "user rule overrides UA rule");
		uaSystem.ClearRules();
		const SdWidgetRootStyle clearedPanelDefault = uaSystem.ResolveRootStyle(SdPanel::TargetTypeId, SdStyleInteractionState::Normal);
		Check(!uaSystem.GetCompiledStyleSheet().GetRules().empty(), "ClearRules reinstalls UA stylesheet");
		Check(clearedPanelDefault.width.unit == SdLengthUnit::Pixels && clearedPanelDefault.width.value == 240.0f, "ClearRules keeps UA defaults after clearing user rules");
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
		style.width = SdLength::Percent(50.0f);
		const SdResolvedBoxStyle percentageUsed = SdResolveBoxStyle(style, { 300.0f, 120.0f }, { 42.0f, 18.0f });
		Check(percentageUsed.width == 150.0f, "box model resolves percentage lengths to used values");

		SdBoxStyle borderBoxStyle = {};
		borderBoxStyle.boxSizing = SdBoxSizing::BorderBox;
		borderBoxStyle.width = SdLength::Pixels(50.0f);
		borderBoxStyle.height = SdLength::Pixels(24.0f);
		borderBoxStyle.padding = SdBoxEdges::All(SdLength::Pixels(4.0f));
		borderBoxStyle.border = SdBorder::All(SdLength::Pixels(2.0f), SdColorWhite);
		SdBoxTree borderBoxTree = {};
		borderBoxTree.AddBox(30, SdInvalidIndex<SdUInt32>, borderBoxStyle);
		borderBoxTree.Layout({ 0.0f, 0.0f, 200.0f, 100.0f });
		const SdBoxNode& borderSizedBox = borderBoxTree.GetBoxes()[0];
		Check(borderSizedBox.borderBox.Width() == 50.0f && borderSizedBox.borderBox.Height() == 24.0f, "box-sizing border-box keeps explicit size on border box");
		Check(borderSizedBox.contentBox.Width() == 38.0f && borderSizedBox.contentBox.Height() == 12.0f, "box-sizing border-box subtracts padding and border for content box");

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

		parentStyle.justifyContent = SdJustifyContent::Center;
		SdBoxTree centeredTree = {};
		const SdUInt32 centeredParent = centeredTree.AddBox(7, SdInvalidIndex<SdUInt32>, parentStyle, { 120.0f, 40.0f });
		centeredTree.AddBox(8, centeredParent, childStyle, { 20.0f, 10.0f });
		centeredTree.AddBox(9, centeredParent, childStyle, { 20.0f, 10.0f });
		centeredTree.Layout({ 0.0f, 0.0f, 200.0f, 100.0f });
		const std::vector<SdBoxNode>& centeredBoxes = centeredTree.GetBoxes();
		Check(centeredBoxes[1].borderBox.min.x == centeredBoxes[0].contentBox.min.x + 37.5f, "flex justify-content center offsets row children");

		parentStyle.justifyContent = SdJustifyContent::SpaceBetween;
		SdBoxTree spacedTree = {};
		const SdUInt32 spacedParent = spacedTree.AddBox(10, SdInvalidIndex<SdUInt32>, parentStyle, { 120.0f, 40.0f });
		spacedTree.AddBox(11, spacedParent, childStyle, { 20.0f, 10.0f });
		spacedTree.AddBox(12, spacedParent, childStyle, { 20.0f, 10.0f });
		spacedTree.Layout({ 0.0f, 0.0f, 200.0f, 100.0f });
		const std::vector<SdBoxNode>& spacedBoxes = spacedTree.GetBoxes();
		Check(spacedBoxes[2].borderBox.max.x == spacedBoxes[0].contentBox.max.x, "flex justify-content space-between distributes row children");

		parentStyle.justifyContent = SdJustifyContent::FlexStart;
		parentStyle.alignItems = SdAlignItems::Center;
		SdBoxTree alignedTree = {};
		const SdUInt32 alignedParent = alignedTree.AddBox(13, SdInvalidIndex<SdUInt32>, parentStyle, { 120.0f, 40.0f });
		alignedTree.AddBox(14, alignedParent, childStyle, { 20.0f, 10.0f });
		alignedTree.Layout({ 0.0f, 0.0f, 200.0f, 100.0f });
		const std::vector<SdBoxNode>& alignedBoxes = alignedTree.GetBoxes();
		Check(alignedBoxes[1].borderBox.min.y == alignedBoxes[0].contentBox.min.y + 15.0f, "flex align-items center offsets row children");

		parentStyle.alignItems = SdAlignItems::Stretch;
		SdBoxTree stretchedTree = {};
		const SdUInt32 stretchedParent = stretchedTree.AddBox(15, SdInvalidIndex<SdUInt32>, parentStyle, { 120.0f, 40.0f });
		stretchedTree.AddBox(16, stretchedParent, childStyle, { 20.0f, 10.0f });
		stretchedTree.Layout({ 0.0f, 0.0f, 200.0f, 100.0f });
		const std::vector<SdBoxNode>& stretchedBoxes = stretchedTree.GetBoxes();
		Check(stretchedBoxes[1].borderBox.Height() == stretchedBoxes[0].contentBox.Height(), "flex align-items stretch fills row cross axis");

		SdBoxStyle basisChildStyle = childStyle;
		basisChildStyle.flexBasis = SdLength::Pixels(34.0f);
		SdBoxTree basisTree = {};
		const SdUInt32 basisParent = basisTree.AddBox(17, SdInvalidIndex<SdUInt32>, parentStyle, { 120.0f, 40.0f });
		basisTree.AddBox(18, basisParent, basisChildStyle, { 20.0f, 10.0f });
		basisTree.Layout({ 0.0f, 0.0f, 200.0f, 100.0f });
		const std::vector<SdBoxNode>& basisBoxes = basisTree.GetBoxes();
		Check(basisBoxes[1].borderBox.Width() == 34.0f, "flex-basis overrides row child main size");

		SdBoxStyle growChildStyle = childStyle;
		growChildStyle.flexGrow = 1.0f;
		SdBoxTree growTree = {};
		const SdUInt32 growParent = growTree.AddBox(19, SdInvalidIndex<SdUInt32>, parentStyle, { 120.0f, 40.0f });
		growTree.AddBox(20, growParent, growChildStyle, { 20.0f, 10.0f });
		growTree.Layout({ 0.0f, 0.0f, 200.0f, 100.0f });
		const std::vector<SdBoxNode>& growBoxes = growTree.GetBoxes();
		Check(growBoxes[1].borderBox.Width() == growBoxes[0].contentBox.Width(), "flex-grow expands child into remaining row space");

		SdBoxStyle shrinkChildStyle = childStyle;
		shrinkChildStyle.width = SdLength::Pixels(80.0f);
		shrinkChildStyle.flexShrink = 1.0f;
		SdBoxTree shrinkTree = {};
		const SdUInt32 shrinkParent = shrinkTree.AddBox(21, SdInvalidIndex<SdUInt32>, parentStyle, { 120.0f, 40.0f });
		shrinkTree.AddBox(22, shrinkParent, shrinkChildStyle, { 80.0f, 10.0f });
		shrinkTree.AddBox(23, shrinkParent, shrinkChildStyle, { 80.0f, 10.0f });
		shrinkTree.Layout({ 0.0f, 0.0f, 200.0f, 100.0f });
		const std::vector<SdBoxNode>& shrinkBoxes = shrinkTree.GetBoxes();
		Check(shrinkBoxes[1].borderBox.Width() == 57.5f && shrinkBoxes[2].borderBox.max.x == shrinkBoxes[0].contentBox.max.x, "flex-shrink reduces overflowing row children");

		parentStyle.justifyContent = SdJustifyContent::FlexStart;
		parentStyle.alignItems = SdAlignItems::Stretch;
		parentStyle.flexDirection = SdFlexDirection::Column;
		SdBoxTree columnTree = {};
		const SdUInt32 columnParent = columnTree.AddBox(4, SdInvalidIndex<SdUInt32>, parentStyle, { 120.0f, 40.0f });
		columnTree.AddBox(5, columnParent, childStyle, { 20.0f, 10.0f });
		columnTree.AddBox(6, columnParent, childStyle, { 20.0f, 10.0f });
		columnTree.Layout({ 0.0f, 0.0f, 200.0f, 100.0f });
		const std::vector<SdBoxNode>& columnBoxes = columnTree.GetBoxes();
		Check(columnBoxes[1].borderBox.min.y == columnBoxes[0].contentBox.min.y, "flex column first child starts at content min");
		Check(columnBoxes[2].borderBox.min.y == columnBoxes[1].borderBox.max.y + 5.0f, "flex column gap advances next child");

		SdBoxStyle absoluteParentStyle = {};
		absoluteParentStyle.width = SdLength::Pixels(120.0f);
		absoluteParentStyle.height = SdLength::Pixels(80.0f);
		SdBoxStyle absoluteChildStyle = childStyle;
		absoluteChildStyle.position = SdPosition::Absolute;
		absoluteChildStyle.margin = {
			SdLength::Pixels(30.0f),
			SdLength::Pixels(12.0f),
			SdLength::Pixels(0.0f),
			SdLength::Pixels(0.0f)
		};
		SdBoxTree absoluteTree = {};
		const SdUInt32 absoluteParent = absoluteTree.AddBox(24, SdInvalidIndex<SdUInt32>, absoluteParentStyle, { 120.0f, 80.0f });
		absoluteTree.AddBox(25, absoluteParent, absoluteChildStyle, { 20.0f, 10.0f });
		absoluteTree.AddBox(26, absoluteParent, childStyle, { 20.0f, 10.0f });
		absoluteTree.Layout({ 0.0f, 0.0f, 200.0f, 100.0f });
		const std::vector<SdBoxNode>& absoluteBoxes = absoluteTree.GetBoxes();
		Check(absoluteBoxes[1].borderBox.min.x == absoluteBoxes[0].contentBox.min.x + 30.0f, "absolute box uses margin left as positioned x offset");
		Check(absoluteBoxes[1].borderBox.min.y == absoluteBoxes[0].contentBox.min.y + 12.0f, "absolute box uses margin top as positioned y offset");
		Check(absoluteBoxes[2].borderBox.min.y == absoluteBoxes[0].contentBox.min.y, "absolute box does not consume normal block flow");

		SdBoxTree explicitTree = {};
		explicitTree.AddBox(27, SdInvalidIndex<SdUInt32>, absoluteChildStyle, { 20.0f, 10.0f }, { 31.0f, 37.0f, 51.0f, 47.0f });
		explicitTree.Layout({ 0.0f, 0.0f, 200.0f, 100.0f });
		const std::vector<SdBoxNode>& explicitBoxes = explicitTree.GetBoxes();
		Check(explicitBoxes[0].borderBox.min.x == 31.0f && explicitBoxes[0].borderBox.max.y == 47.0f, "box tree honors explicit absolute border rect");

		SdInstance instance;
		instance.BeginFrame({ 320.0f, 200.0f });
		instance.ui.Declare<SdButton>("Used");
		PumpFrame(instance);
		bool hasUsedBox = false;
		bool hasPartUsedBox = false;
		bool hasShadowBox = false;
		bool hasLayoutBoxFromShadowBox = false;
		for (const auto& [id, record] : instance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdButton)))
			{
				const SdStyleNode& root = instance.GetRootStyleNode(record.state.id);
				const SdStyleNode& content = instance.GetStylePart(record.state.id, SdButton::Parts::Content);
				const SdStyleNode& label = instance.GetStylePart(record.state.id, SdButton::Parts::Label);
				const SdBoxNode* box = instance.GetBoxTree().FindBoxByStyleNodeId(record.rootStyleNodeId);
				hasUsedBox = root.usedBox.borderBox.Width() > 0.0f
					&& root.usedBox.contentBox.Width() <= root.usedBox.borderBox.Width();
				hasPartUsedBox = content.usedBox.borderBox.Width() == root.usedBox.borderBox.Width()
					&& label.usedBox.borderBox.Width() > 0.0f
					&& label.usedBox.borderBox.Width() < root.usedBox.borderBox.Width();
				hasShadowBox = box && box->borderBox.Width() > 0.0f && box->contentBox.Width() <= box->borderBox.Width();
				hasLayoutBoxFromShadowBox = box
					&& root.layoutBox.borderBox.min.x == box->borderBox.min.x
					&& root.layoutBox.contentBox.max.y == box->contentBox.max.y
					&& content.layoutBox.borderBox.Width() == root.layoutBox.borderBox.Width()
					&& label.layoutBox.borderBox.Width() < root.layoutBox.borderBox.Width();
			}
		}
		Check(hasUsedBox, "runtime writes used geometry to root style node");
		Check(hasPartUsedBox, "runtime writes independent used geometry to part style nodes");
		Check(hasShadowBox, "runtime shadow box tree indexes root style nodes");
		Check(hasLayoutBoxFromShadowBox, "runtime writes shadow box geometry to style nodes");

		SdInstance centeredButtonInstance;
		RecordingFontBackend centeredButtonFontBackend = {};
		centeredButtonFontBackend.measuredSizeOverride = { 80.0f, 20.0f };
		centeredButtonFontBackend.glyphBoundsOverride = { 4.0f, 5.0f, 76.0f, 15.0f };
		centeredButtonFontBackend.hasMeasuredSizeOverride = true;
		centeredButtonFontBackend.hasGlyphBoundsOverride = true;
		centeredButtonInstance.SetFontBackend(&centeredButtonFontBackend);
		centeredButtonInstance.GetStyleSystem().RootRule(SdButton::TargetTypeId)
			.Set(&SdBoxStyle::width, SdLength::Pixels(160.0f))
			.Set(&SdBoxStyle::height, SdLength::Pixels(48.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ 20.0f, 8.0f, 12.0f, 6.0f }));
		centeredButtonInstance.BeginFrame({ 320.0f, 200.0f });
		centeredButtonInstance.ui.Declare<SdButton>("Centered");
		PumpFrame(centeredButtonInstance);
		bool buttonLabelVisualBoundsCentered = false;
		for (const auto& [id, record] : centeredButtonInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType != std::type_index(typeid(SdButton)))
				continue;

			const SdStyleNode& root = centeredButtonInstance.GetRootStyleNode(record.state.id);
			const SdStyleNode& label = centeredButtonInstance.GetStylePart(record.state.id, SdButton::Parts::Label);
			const SdResolvedBoxStyle rootBoxStyle = SdResolveBoxStyle(root.resolvedStyle, root.layoutBox.borderBox.Size(), {});
			const SdRect contentRect = SdInsetRect(root.layoutBox.borderBox, rootBoxStyle.padding);
			const SdRect glyphRect = BasicWidgetDetail::OffsetRect(centeredButtonFontBackend.glyphBoundsOverride, label.layoutBox.borderBox.min);
			const float contentCenterX = (contentRect.min.x + contentRect.max.x) * 0.5f;
			const float contentCenterY = (contentRect.min.y + contentRect.max.y) * 0.5f;
			const float glyphCenterX = (glyphRect.min.x + glyphRect.max.x) * 0.5f;
			const float glyphCenterY = (glyphRect.min.y + glyphRect.max.y) * 0.5f;
			buttonLabelVisualBoundsCentered = std::abs(glyphCenterX - contentCenterX) < 0.001f
				&& std::abs(glyphCenterY - contentCenterY) < 0.001f;
		}
		Check(buttonLabelVisualBoundsCentered, "button label glyph bounds center within content box");

		SdInstance compactButtonInstance;
		RecordingFontBackend compactButtonFontBackend = {};
		compactButtonFontBackend.measuredSizeOverride = { 80.0f, 48.0f };
		compactButtonFontBackend.glyphBoundsOverride = { 4.0f, 19.0f, 76.0f, 29.0f };
		compactButtonFontBackend.hasMeasuredSizeOverride = true;
		compactButtonFontBackend.hasGlyphBoundsOverride = true;
		compactButtonInstance.SetFontBackend(&compactButtonFontBackend);
		compactButtonInstance.BeginFrame({ 320.0f, 200.0f });
		compactButtonInstance.ui.Declare<SdButton>("Compact");
		PumpFrame(compactButtonInstance);
		bool buttonIgnoresExcessLineBoxHeight = false;
		for (const auto& [id, record] : compactButtonInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType != std::type_index(typeid(SdButton)))
				continue;
			const SdStyleNode& root = compactButtonInstance.GetRootStyleNode(record.state.id);
			buttonIgnoresExcessLineBoxHeight = root.layoutBox.borderBox.Height() <= 40.0f;
		}
		Check(buttonIgnoresExcessLineBoxHeight, "button intrinsic height uses glyph bounds instead of excess line box");

		SdInstance alignedTextPartsInstance;
		RecordingFontBackend alignedTextFontBackend = {};
		alignedTextFontBackend.measuredSizeOverride = { 80.0f, 20.0f };
		alignedTextFontBackend.glyphBoundsOverride = { 4.0f, 5.0f, 76.0f, 15.0f };
		alignedTextFontBackend.hasMeasuredSizeOverride = true;
		alignedTextFontBackend.hasGlyphBoundsOverride = true;
		alignedTextPartsInstance.SetFontBackend(&alignedTextFontBackend);
		alignedTextPartsInstance.GetStyleSystem().RootRule(SdCheckBox::TargetTypeId)
			.Set(&SdBoxStyle::minHeight, SdLength::Pixels(48.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ 20.0f, 8.0f, 12.0f, 6.0f }))
			.Set(&SdBoxStyle::gap, SdLength::Pixels(10.0f));
		alignedTextPartsInstance.GetStyleSystem().RootRule(SdSliderFloat::TargetTypeId)
			.Set(&SdBoxStyle::height, SdLength::Pixels(48.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ 20.0f, 8.0f, 12.0f, 6.0f }))
			.Set(&SdBoxStyle::gap, SdLength::Pixels(10.0f));
		alignedTextPartsInstance.GetStyleSystem().RootRule(SdTextInput::TargetTypeId)
			.Set(&SdBoxStyle::minHeight, SdLength::Pixels(48.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ 20.0f, 8.0f, 12.0f, 6.0f }));
		bool alignedChecked = false;
		float alignedSliderValue = 0.5f;
		SdUtf8String alignedInputValue = "Input";
		bool alignedWindowOpen = true;
		SdWindowOptions alignedWindowOptions = {};
		alignedWindowOptions.position = { 32.0f, 132.0f };
		alignedWindowOptions.size = { 220.0f, 96.0f };
		alignedTextPartsInstance.BeginFrame({ 420.0f, 260.0f });
		alignedTextPartsInstance.ui.Declare<SdCheckBox>("Check", alignedChecked);
		alignedTextPartsInstance.ui.Declare<SdSliderFloat>("Slide", alignedSliderValue, 0.0f, 1.0f);
		alignedTextPartsInstance.ui.Declare<SdTextInput>(alignedInputValue, "Placeholder");
		alignedTextPartsInstance.ui.Declare<SdWindow>("Window", alignedWindowOpen, alignedWindowOptions, [](SdUi&) {});
		PumpFrame(alignedTextPartsInstance);
		bool checkBoxLabelCentered = false;
		bool checkBoxHitRectUsesLayoutBox = false;
		bool sliderLabelCentered = false;
		bool textInputValueCentered = false;
		bool windowTitleCentered = false;
		for (const auto& [id, record] : alignedTextPartsInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdCheckBox)))
			{
				const SdStyleNode& root = alignedTextPartsInstance.GetRootStyleNode(record.state.id);
				const SdStyleNode& label = alignedTextPartsInstance.GetStylePart(record.state.id, SdCheckBox::Parts::Label);
				const SdStyleNode& box = alignedTextPartsInstance.GetStylePart(record.state.id, SdCheckBox::Parts::Box);
				const SdResolvedBoxStyle rootBoxStyle = SdResolveBoxStyle(root.resolvedStyle, root.layoutBox.borderBox.Size(), {});
				const SdRect contentRect = SdInsetRect(root.layoutBox.borderBox, rootBoxStyle.padding);
				const SdRect glyphRect = BasicWidgetDetail::OffsetRect(alignedTextFontBackend.glyphBoundsOverride, label.layoutBox.borderBox.min);
				const float contentCenterY = (contentRect.min.y + contentRect.max.y) * 0.5f;
				const float glyphCenterY = (glyphRect.min.y + glyphRect.max.y) * 0.5f;
				const float boxCenterY = (box.layoutBox.borderBox.min.y + box.layoutBox.borderBox.max.y) * 0.5f;
				checkBoxLabelCentered = std::abs(glyphCenterY - contentCenterY) < 0.001f
					&& std::abs(boxCenterY - contentCenterY) < 0.001f;
				for (const SdHitTestRecord& hitRecord : alignedTextPartsInstance.GetLayerSystem().GetHitTestRecords())
				{
					if (hitRecord.widgetId != record.state.id)
						continue;
					checkBoxHitRectUsesLayoutBox = hitRecord.rect.min.x == root.layoutBox.borderBox.min.x
						&& hitRecord.rect.min.y == root.layoutBox.borderBox.min.y
						&& hitRecord.rect.max.x == root.layoutBox.borderBox.max.x
						&& hitRecord.rect.max.y == root.layoutBox.borderBox.max.y;
				}
			}
			if (record.widgetType == std::type_index(typeid(SdSliderFloat)))
			{
				const SdStyleNode& root = alignedTextPartsInstance.GetRootStyleNode(record.state.id);
				const SdStyleNode& label = alignedTextPartsInstance.GetStylePart(record.state.id, SdSliderFloat::Parts::Label);
				const SdResolvedBoxStyle rootBoxStyle = SdResolveBoxStyle(root.resolvedStyle, root.layoutBox.borderBox.Size(), {});
				const SdRect contentRect = SdInsetRect(root.layoutBox.borderBox, rootBoxStyle.padding);
				const SdRect glyphRect = BasicWidgetDetail::OffsetRect(alignedTextFontBackend.glyphBoundsOverride, label.layoutBox.borderBox.min);
				const float contentCenterY = (contentRect.min.y + contentRect.max.y) * 0.5f;
				const float glyphCenterY = (glyphRect.min.y + glyphRect.max.y) * 0.5f;
				sliderLabelCentered = std::abs(glyphCenterY - contentCenterY) < 0.001f;
			}
			if (record.widgetType == std::type_index(typeid(SdTextInput)))
			{
				const SdStyleNode& root = alignedTextPartsInstance.GetRootStyleNode(record.state.id);
				const SdStyleNode& value = alignedTextPartsInstance.GetStylePart(record.state.id, SdTextInput::Parts::Value);
				const SdResolvedBoxStyle rootBoxStyle = SdResolveBoxStyle(root.resolvedStyle, root.layoutBox.borderBox.Size(), {});
				const SdRect contentRect = SdInsetRect(root.layoutBox.borderBox, rootBoxStyle.padding);
				const SdRect glyphRect = BasicWidgetDetail::OffsetRect(alignedTextFontBackend.glyphBoundsOverride, value.layoutBox.borderBox.min);
				const float contentCenterY = (contentRect.min.y + contentRect.max.y) * 0.5f;
				const float glyphCenterY = (glyphRect.min.y + glyphRect.max.y) * 0.5f;
				textInputValueCentered = std::abs(glyphCenterY - contentCenterY) < 0.001f;
			}
			if (record.widgetType == std::type_index(typeid(SdWindow)))
			{
				const SdStyleNode& titlebar = alignedTextPartsInstance.GetStylePart(record.state.id, SdWindow::Parts::Titlebar);
				const SdStyleNode& title = alignedTextPartsInstance.GetStylePart(record.state.id, SdWindow::Parts::Title);
				const SdRect glyphRect = BasicWidgetDetail::OffsetRect(alignedTextFontBackend.glyphBoundsOverride, title.layoutBox.borderBox.min);
				const float titlebarCenterY = (titlebar.layoutBox.borderBox.min.y + titlebar.layoutBox.borderBox.max.y) * 0.5f;
				const float glyphCenterY = (glyphRect.min.y + glyphRect.max.y) * 0.5f;
				windowTitleCentered = std::abs(glyphCenterY - titlebarCenterY) < 0.001f;
			}
		}
		Check(checkBoxLabelCentered, "checkbox label and box glyph bounds center within content box");
		Check(checkBoxHitRectUsesLayoutBox, "checkbox hit-test rect uses root layout box");
		Check(sliderLabelCentered, "slider label glyph bounds center within content box");
		Check(textInputValueCentered, "text input value glyph bounds center within content box");
		Check(windowTitleCentered, "window title glyph bounds center within titlebar");

		SdInstance panelInstance;
		panelInstance.BeginFrame({ 320.0f, 200.0f });
		panelInstance.ui.Declare<SdPanel>([](SdUi& ui)
		{
			ui.Declare<SdText>("child");
		});
		PumpFrame(panelInstance);
		bool hasContentRectFromUsedBox = false;
		for (const auto& [id, record] : panelInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdPanel)))
			{
				const SdStyleNode& root = panelInstance.GetRootStyleNode(record.state.id);
				hasContentRectFromUsedBox = record.state.childContentRect.min.x == root.usedBox.contentBox.min.x
					&& record.state.childContentRect.min.y == root.usedBox.contentBox.min.y
					&& record.state.childContentRect.max.x == root.usedBox.contentBox.max.x
					&& record.state.childContentRect.max.y == root.usedBox.contentBox.max.y;
			}
		}
		Check(hasContentRectFromUsedBox, "runtime child content rect matches root used content box");

		SdInstance panelPaintInstance;
		panelPaintInstance.GetRenderSharedData().flags = 0;
		const SdColor panelPaintColor{ 17, 31, 47, 255 };
		const SdColor panelPaintBorderColor{ 47, 31, 17, 255 };
		panelPaintInstance.GetStyleSystem().RootRule(SdPanel::TargetTypeId)
			.Set(&SdBoxStyle::margin, SdStyleValue::FromSpacing({ 9.0f, 0.0f, 0.0f, 0.0f }))
			.Set(&SdBoxStyle::backgroundColor, panelPaintColor)
			.Set(&SdBoxStyle::border, SdStyleValue::FromColor(panelPaintBorderColor))
			.Set(&SdBoxStyle::radius, SdLength::Pixels(0.0f));
		panelPaintInstance.BeginFrame({ 320.0f, 200.0f });
		panelPaintInstance.ui.Declare<SdPanel>();
		PumpFrame(panelPaintInstance);
		float panelLayoutMinX = -1.0f;
		for (const auto& [id, record] : panelPaintInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdPanel)))
				panelLayoutMinX = panelPaintInstance.GetRootStyleNode(record.state.id).layoutBox.borderBox.min.x;
		}
		const SdUInt32 panelPaintPackedRgb = panelPaintColor.Pack() & 0x00ffffffu;
		float panelPaintMinX = 1000000.0f;
		for (const SdVertex& vertex : panelPaintInstance.GetRenderData().vertices)
		{
			if ((vertex.color & 0x00ffffffu) == panelPaintPackedRgb && (vertex.color >> 24) > 0u)
				panelPaintMinX = std::min(panelPaintMinX, vertex.position.x);
		}
		const bool panelPaintUsesLayoutBox = panelPaintMinX == panelLayoutMinX;
		Check(panelPaintUsesLayoutBox, "panel paint uses root layout box geometry");

		SdInstance buttonPaintInstance;
		buttonPaintInstance.GetRenderSharedData().flags = 0;
		const SdColor buttonPaintColor{ 19, 43, 67, 255 };
		const SdColor buttonPaintBorderColor{ 67, 43, 19, 255 };
		buttonPaintInstance.GetStyleSystem().RootRule(SdButton::TargetTypeId)
			.Set(&SdBoxStyle::margin, SdStyleValue::FromSpacing({ 11.0f, 0.0f, 0.0f, 0.0f }))
			.Set(&SdBoxStyle::radius, SdLength::Pixels(0.0f));
		buttonPaintInstance.GetStyleSystem().Part<SdButton>(SdButton::Parts::Content)
			.Set(&SdBoxStyle::backgroundColor, buttonPaintColor)
			.Set(&SdBoxStyle::border, SdStyleValue::FromColor(buttonPaintBorderColor))
			.Set(&SdBoxStyle::radius, SdLength::Pixels(0.0f));
		buttonPaintInstance.BeginFrame({ 320.0f, 200.0f });
		buttonPaintInstance.ui.Declare<SdButton>("Layout");
		PumpFrame(buttonPaintInstance);
		float buttonLayoutMinX = -1.0f;
		for (const auto& [id, record] : buttonPaintInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdButton)))
				buttonLayoutMinX = buttonPaintInstance.GetRootStyleNode(record.state.id).layoutBox.borderBox.min.x;
		}
		const SdUInt32 buttonPaintPackedRgb = buttonPaintColor.Pack() & 0x00ffffffu;
		float buttonPaintMinX = 1000000.0f;
		for (const SdVertex& vertex : buttonPaintInstance.GetRenderData().vertices)
		{
			if ((vertex.color & 0x00ffffffu) == buttonPaintPackedRgb && (vertex.color >> 24) > 0u)
				buttonPaintMinX = std::min(buttonPaintMinX, vertex.position.x);
		}
		const bool buttonPaintUsesLayoutBox = buttonPaintMinX == buttonLayoutMinX;
		Check(buttonPaintUsesLayoutBox, "button paint uses root layout box geometry");

		SdInstance checkBoxPaintInstance;
		checkBoxPaintInstance.GetRenderSharedData().flags = 0;
		const SdColor checkBoxPaintColor{ 23, 59, 83, 255 };
		const SdColor checkBoxPaintBorderColor{ 83, 59, 23, 255 };
		checkBoxPaintInstance.GetStyleSystem().RootRule(SdCheckBox::TargetTypeId)
			.Set(&SdBoxStyle::margin, SdStyleValue::FromSpacing({ 13.0f, 0.0f, 0.0f, 0.0f }))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ 0.0f, 0.0f, 0.0f, 0.0f }))
			.Set(&SdBoxStyle::radius, SdLength::Pixels(0.0f));
		checkBoxPaintInstance.GetStyleSystem().Part<SdCheckBox>(SdCheckBox::Parts::Box)
			.Set(&SdBoxStyle::backgroundColor, checkBoxPaintColor)
			.Set(&SdBoxStyle::border, SdStyleValue::FromColor(checkBoxPaintBorderColor))
			.Set(&SdBoxStyle::radius, SdLength::Pixels(0.0f));
		checkBoxPaintInstance.BeginFrame({ 320.0f, 200.0f });
		checkBoxPaintInstance.ui.Declare<SdCheckBox>("Layout");
		PumpFrame(checkBoxPaintInstance);
		float checkBoxLayoutMinX = -1.0f;
		for (const auto& [id, record] : checkBoxPaintInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdCheckBox)))
				checkBoxLayoutMinX = checkBoxPaintInstance.GetRootStyleNode(record.state.id).layoutBox.borderBox.min.x;
		}
		const SdUInt32 checkBoxPaintPackedRgb = checkBoxPaintColor.Pack() & 0x00ffffffu;
		float checkBoxPaintMinX = 1000000.0f;
		for (const SdVertex& vertex : checkBoxPaintInstance.GetRenderData().vertices)
		{
			if ((vertex.color & 0x00ffffffu) == checkBoxPaintPackedRgb && (vertex.color >> 24) > 0u)
				checkBoxPaintMinX = std::min(checkBoxPaintMinX, vertex.position.x);
		}
		const bool checkBoxPaintUsesLayoutBox = checkBoxPaintMinX == checkBoxLayoutMinX;
		Check(checkBoxPaintUsesLayoutBox, "checkbox paint uses root layout box geometry");

		SdInstance sliderPaintInstance;
		sliderPaintInstance.GetRenderSharedData().flags = 0;
		const SdColor sliderPaintColor{ 29, 71, 97, 255 };
		const SdColor sliderPaintBorderColor{ 97, 71, 29, 255 };
		sliderPaintInstance.GetStyleSystem().RootRule(SdSliderFloat::TargetTypeId)
			.Set(&SdBoxStyle::margin, SdStyleValue::FromSpacing({ 15.0f, 0.0f, 0.0f, 0.0f }))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ 0.0f, 0.0f, 0.0f, 0.0f }))
			.Set(&SdBoxStyle::radius, SdLength::Pixels(0.0f));
		sliderPaintInstance.GetStyleSystem().Part<SdSliderFloat>(SdSliderFloat::Parts::Track)
			.Set(&SdBoxStyle::backgroundColor, sliderPaintColor)
			.Set(&SdBoxStyle::border, SdStyleValue::FromColor(sliderPaintBorderColor))
			.Set(&SdBoxStyle::radius, SdLength::Pixels(0.0f));
		float sliderValue = 0.5f;
		sliderPaintInstance.BeginFrame({ 320.0f, 200.0f });
		sliderPaintInstance.ui.Declare<SdSliderFloat>(sliderValue, 0.0f, 1.0f);
		PumpFrame(sliderPaintInstance);
		float sliderLayoutMinX = -1.0f;
		for (const auto& [id, record] : sliderPaintInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdSliderFloat)))
				sliderLayoutMinX = sliderPaintInstance.GetRootStyleNode(record.state.id).layoutBox.borderBox.min.x;
		}
		const SdUInt32 sliderPaintPackedRgb = sliderPaintColor.Pack() & 0x00ffffffu;
		float sliderPaintMinX = 1000000.0f;
		for (const SdVertex& vertex : sliderPaintInstance.GetRenderData().vertices)
		{
			if ((vertex.color & 0x00ffffffu) == sliderPaintPackedRgb && (vertex.color >> 24) > 0u)
				sliderPaintMinX = std::min(sliderPaintMinX, vertex.position.x);
		}
		const bool sliderPaintUsesLayoutBox = sliderPaintMinX == sliderLayoutMinX;
		Check(sliderPaintUsesLayoutBox, "slider paint uses root layout box geometry");

		SdInstance textInputPaintInstance;
		textInputPaintInstance.GetRenderSharedData().flags = 0;
		const SdColor textInputPaintColor{ 31, 83, 109, 255 };
		const SdColor textInputPaintBorderColor{ 109, 83, 31, 255 };
		SdUtf8String textInputValue = {};
		textInputPaintInstance.GetStyleSystem().RootRule(SdTextInput::TargetTypeId)
			.Set(&SdBoxStyle::margin, SdStyleValue::FromSpacing({ 17.0f, 0.0f, 0.0f, 0.0f }))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ 0.0f, 0.0f, 0.0f, 0.0f }))
			.Set(&SdBoxStyle::radius, SdLength::Pixels(0.0f));
		textInputPaintInstance.GetStyleSystem().Part<SdTextInput>(SdTextInput::Parts::Field)
			.Set(&SdBoxStyle::backgroundColor, textInputPaintColor)
			.Set(&SdBoxStyle::border, SdStyleValue::FromColor(textInputPaintBorderColor))
			.Set(&SdBoxStyle::radius, SdLength::Pixels(0.0f));
		textInputPaintInstance.BeginFrame({ 320.0f, 200.0f });
		textInputPaintInstance.ui.Declare<SdTextInput>(textInputValue, "Hint");
		PumpFrame(textInputPaintInstance);
		float textInputLayoutMinX = -1.0f;
		for (const auto& [id, record] : textInputPaintInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdTextInput)))
				textInputLayoutMinX = textInputPaintInstance.GetRootStyleNode(record.state.id).layoutBox.borderBox.min.x;
		}
		const SdUInt32 textInputPaintPackedRgb = textInputPaintColor.Pack() & 0x00ffffffu;
		float textInputPaintMinX = 1000000.0f;
		for (const SdVertex& vertex : textInputPaintInstance.GetRenderData().vertices)
		{
			if ((vertex.color & 0x00ffffffu) == textInputPaintPackedRgb && (vertex.color >> 24) > 0u)
				textInputPaintMinX = std::min(textInputPaintMinX, vertex.position.x);
		}
		const bool textInputPaintUsesLayoutBox = textInputPaintMinX == textInputLayoutMinX;
		Check(textInputPaintUsesLayoutBox, "text input paint uses root layout box geometry");

		SdInstance imageViewerPaintInstance;
		imageViewerPaintInstance.GetRenderSharedData().flags = 0;
		const SdColor imageViewerPaintColor{ 37, 91, 121, 255 };
		imageViewerPaintInstance.GetStyleSystem().RootRule(SdImageViewer::TargetTypeId)
			.Set(&SdBoxStyle::margin, SdStyleValue::FromSpacing({ 19.0f, 0.0f, 0.0f, 0.0f }))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ 0.0f, 0.0f, 0.0f, 0.0f }))
			.Set(&SdBoxStyle::radius, SdLength::Pixels(0.0f));
		imageViewerPaintInstance.GetStyleSystem().RootRule(SdImageViewer::TargetTypeId)
			.Set(&SdBoxStyle::backgroundColor, imageViewerPaintColor);
		imageViewerPaintInstance.BeginFrame({ 320.0f, 200.0f });
		imageViewerPaintInstance.ui.Declare<SdImageViewer>(SdTextureHandle(3, 1), SdVec2{ 32.0f, 32.0f });
		PumpFrame(imageViewerPaintInstance);
		float imageViewerLayoutMinX = -1.0f;
		for (const auto& [id, record] : imageViewerPaintInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdImageViewer)))
				imageViewerLayoutMinX = imageViewerPaintInstance.GetRootStyleNode(record.state.id).layoutBox.borderBox.min.x;
		}
		const SdUInt32 imageViewerPaintPackedRgb = imageViewerPaintColor.Pack() & 0x00ffffffu;
		float imageViewerPaintMinX = 1000000.0f;
		for (const SdVertex& vertex : imageViewerPaintInstance.GetRenderData().vertices)
		{
			if ((vertex.color & 0x00ffffffu) == imageViewerPaintPackedRgb && (vertex.color >> 24) > 0u)
				imageViewerPaintMinX = std::min(imageViewerPaintMinX, vertex.position.x);
		}
		const bool imageViewerPaintUsesLayoutBox = imageViewerPaintMinX == imageViewerLayoutMinX;
		Check(imageViewerPaintUsesLayoutBox, "image viewer paint uses root layout box geometry");

		SdInstance popupPaintInstance;
		popupPaintInstance.GetRenderSharedData().flags = 0;
		const SdColor popupPaintColor{ 41, 93, 131, 255 };
		const SdColor popupPaintBorderColor{ 131, 93, 41, 255 };
		popupPaintInstance.GetStyleSystem().RootRule(SdPopup::TargetTypeId)
			.Set(&SdBoxStyle::radius, SdLength::Pixels(0.0f))
			.Set(&SdBoxStyle::backgroundColor, popupPaintColor)
			.Set(&SdBoxStyle::border, SdStyleValue::FromColor(popupPaintBorderColor));
		popupPaintInstance.BeginFrame({ 320.0f, 200.0f });
		popupPaintInstance.ui.Declare<SdPopup>(true, SdVec2{ 140.0f, 76.0f }, [](SdUi&)
		{
		});
		PumpFrame(popupPaintInstance);
		float popupLayoutMinX = -1.0f;
		for (const auto& [id, record] : popupPaintInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdPopup)))
				popupLayoutMinX = popupPaintInstance.GetRootStyleNode(record.state.id).layoutBox.borderBox.min.x;
		}
		const SdUInt32 popupPaintPackedRgb = popupPaintColor.Pack() & 0x00ffffffu;
		float popupPaintMinX = 1000000.0f;
		for (const SdVertex& vertex : popupPaintInstance.GetRenderData().vertices)
		{
			if ((vertex.color & 0x00ffffffu) == popupPaintPackedRgb && (vertex.color >> 24) > 0u)
				popupPaintMinX = std::min(popupPaintMinX, vertex.position.x);
		}
		const bool popupPaintUsesLayoutBox = popupPaintMinX == popupLayoutMinX;
		Check(popupPaintUsesLayoutBox, "popup paint uses root layout box geometry");

		SdInstance contextMenuPaintInstance;
		contextMenuPaintInstance.GetRenderSharedData().flags = 0;
		const SdColor contextMenuPaintColor{ 45, 97, 135, 255 };
		const SdColor contextMenuPaintBorderColor{ 135, 97, 45, 255 };
		contextMenuPaintInstance.GetStyleSystem().RootRule(SdContextMenu::TargetTypeId)
			.Set(&SdBoxStyle::radius, SdLength::Pixels(0.0f))
			.Set(&SdBoxStyle::backgroundColor, contextMenuPaintColor)
			.Set(&SdBoxStyle::border, SdStyleValue::FromColor(contextMenuPaintBorderColor));
		contextMenuPaintInstance.BeginFrame({ 320.0f, 200.0f });
		contextMenuPaintInstance.ui.Declare<SdContextMenu>(true, SdVec2{ 156.0f, 86.0f }, [](SdUi&)
		{
		});
		PumpFrame(contextMenuPaintInstance);
		float contextMenuLayoutMinX = -1.0f;
		for (const auto& [id, record] : contextMenuPaintInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdContextMenu)))
				contextMenuLayoutMinX = contextMenuPaintInstance.GetRootStyleNode(record.state.id).layoutBox.borderBox.min.x;
		}
		const SdUInt32 contextMenuPaintPackedRgb = contextMenuPaintColor.Pack() & 0x00ffffffu;
		float contextMenuPaintMinX = 1000000.0f;
		for (const SdVertex& vertex : contextMenuPaintInstance.GetRenderData().vertices)
		{
			if ((vertex.color & 0x00ffffffu) == contextMenuPaintPackedRgb && (vertex.color >> 24) > 0u)
				contextMenuPaintMinX = std::min(contextMenuPaintMinX, vertex.position.x);
		}
		const bool contextMenuPaintUsesLayoutBox = contextMenuPaintMinX == contextMenuLayoutMinX;
		Check(contextMenuPaintUsesLayoutBox, "context menu paint uses root layout box geometry");

		SdInstance tooltipPaintInstance;
		tooltipPaintInstance.GetRenderSharedData().flags = 0;
		const SdColor tooltipPaintColor{ 49, 101, 139, 255 };
		const SdColor tooltipPaintBorderColor{ 139, 101, 49, 255 };
		tooltipPaintInstance.GetStyleSystem().RootRule(SdTooltip::TargetTypeId)
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ 0.0f, 0.0f, 0.0f, 0.0f }))
			.Set(&SdBoxStyle::radius, SdLength::Pixels(0.0f))
			.Set(&SdBoxStyle::backgroundColor, tooltipPaintColor)
			.Set(&SdBoxStyle::border, SdStyleValue::FromColor(tooltipPaintBorderColor));
		tooltipPaintInstance.BeginFrame({ 320.0f, 200.0f });
		tooltipPaintInstance.ui.Declare<SdTooltip>(true, SdVec2{ 172.0f, 96.0f }, "Hint");
		PumpFrame(tooltipPaintInstance);
		float tooltipLayoutMinX = -1.0f;
		for (const auto& [id, record] : tooltipPaintInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdTooltip)))
				tooltipLayoutMinX = tooltipPaintInstance.GetRootStyleNode(record.state.id).layoutBox.borderBox.min.x;
		}
		const SdUInt32 tooltipPaintPackedRgb = tooltipPaintColor.Pack() & 0x00ffffffu;
		float tooltipPaintMinX = 1000000.0f;
		for (const SdVertex& vertex : tooltipPaintInstance.GetRenderData().vertices)
		{
			if ((vertex.color & 0x00ffffffu) == tooltipPaintPackedRgb && (vertex.color >> 24) > 0u)
				tooltipPaintMinX = std::min(tooltipPaintMinX, vertex.position.x);
		}
		const bool tooltipPaintUsesLayoutBox = tooltipPaintMinX == tooltipLayoutMinX;
		Check(tooltipPaintUsesLayoutBox, "tooltip paint uses root layout box geometry");

		SdInstance scrollViewPaintInstance;
		scrollViewPaintInstance.GetRenderSharedData().flags = 0;
		const SdColor scrollViewPaintColor{ 53, 107, 141, 255 };
		const SdColor scrollViewThumbColor = scrollViewPaintColor;
		scrollViewPaintInstance.GetStyleSystem().RootRule(SdScrollView::TargetTypeId)
			.Set(&SdBoxStyle::margin, SdStyleValue::FromSpacing({ 21.0f, 0.0f, 0.0f, 0.0f }))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ 0.0f, 0.0f, 0.0f, 0.0f }))
			.Set(&SdBoxStyle::radius, SdLength::Pixels(0.0f));
		scrollViewPaintInstance.GetStyleSystem().Part<SdScrollView>(SdScrollView::Parts::Scrollbar)
			.Set(&SdBoxStyle::backgroundColor, scrollViewPaintColor)
			.Set(&SdBoxStyle::border, SdStyleValue::FromColor(SdColor(0, 0, 0, 0)))
			.Set(&SdBoxStyle::radius, SdLength::Pixels(0.0f));
		scrollViewPaintInstance.GetStyleSystem().Part<SdScrollView>(SdScrollView::Parts::Thumb)
			.Set(&SdBoxStyle::backgroundColor, scrollViewThumbColor);
		scrollViewPaintInstance.BeginFrame({ 320.0f, 200.0f });
		scrollViewPaintInstance.ui.Declare<SdScrollView>([](SdUi&)
		{
		});
		PumpFrame(scrollViewPaintInstance);
		float scrollViewLayoutMinX = -1.0f;
		for (const auto& [id, record] : scrollViewPaintInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdScrollView)))
				scrollViewLayoutMinX = scrollViewPaintInstance.GetStylePart(record.state.id, SdScrollView::Parts::Scrollbar).layoutBox.borderBox.min.x;
		}
		const SdUInt32 scrollViewPaintPackedRgb = scrollViewPaintColor.Pack() & 0x00ffffffu;
		float scrollViewPaintMinX = 1000000.0f;
		for (const SdVertex& vertex : scrollViewPaintInstance.GetRenderData().vertices)
		{
			if ((vertex.color & 0x00ffffffu) == scrollViewPaintPackedRgb && (vertex.color >> 24) > 0u)
				scrollViewPaintMinX = std::min(scrollViewPaintMinX, vertex.position.x);
		}
		const bool scrollViewPaintUsesLayoutBox = scrollViewPaintMinX == scrollViewLayoutMinX;
		Check(scrollViewPaintUsesLayoutBox, "scroll view paint uses scrollbar part layout box geometry");

		SdInstance windowPaintInstance;
		windowPaintInstance.GetRenderSharedData().flags = 0;
		const SdColor windowPaintColor{ 57, 111, 147, 255 };
		const SdColor windowPaintBorderColor{ 147, 111, 57, 255 };
		SdWindowOptions paintedWindowOptions = {};
		bool paintedWindowOpen = true;
		paintedWindowOptions.position = { 150.0f, 92.0f };
		paintedWindowOptions.size = { 220.0f, 140.0f };
		windowPaintInstance.GetStyleSystem().RootRule(SdWindow::TargetTypeId)
			.Set(&SdBoxStyle::radius, SdLength::Pixels(0.0f))
			.Set(&SdBoxStyle::backgroundColor, windowPaintColor)
			.Set(&SdBoxStyle::border, SdStyleValue::FromColor(windowPaintBorderColor));
		windowPaintInstance.GetStyleSystem().Part<SdWindow>(SdWindow::Parts::Content)
			.Set(&SdBoxStyle::backgroundColor, windowPaintColor)
			.Set(&SdBoxStyle::border, SdStyleValue::FromColor(SdColor(0, 0, 0, 0)))
			.Set(&SdBoxStyle::radius, SdLength::Pixels(0.0f));
		windowPaintInstance.BeginFrame({ 640.0f, 360.0f });
		windowPaintInstance.ui.Declare<SdWindow>("Paint", paintedWindowOpen, paintedWindowOptions, [](SdUi&)
		{
		});
		PumpFrame(windowPaintInstance);
		float windowLayoutMinX = -1.0f;
		for (const auto& [id, record] : windowPaintInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdWindow)))
				windowLayoutMinX = windowPaintInstance.GetRootStyleNode(record.state.id).layoutBox.borderBox.min.x;
		}
		const SdUInt32 windowPaintPackedRgb = windowPaintColor.Pack() & 0x00ffffffu;
		float windowPaintMinX = 1000000.0f;
		for (const SdVertex& vertex : windowPaintInstance.GetRenderData().vertices)
		{
			if ((vertex.color & 0x00ffffffu) == windowPaintPackedRgb && (vertex.color >> 24) > 0u)
				windowPaintMinX = std::min(windowPaintMinX, vertex.position.x);
		}
		const bool windowPaintUsesLayoutBox = windowPaintMinX == windowLayoutMinX;
		Check(windowPaintUsesLayoutBox, "window paint uses root layout box geometry");

		SdInstance overflowInstance;
		overflowInstance.GetStyleSystem().RootRule(TestOverflowContainer::TargetTypeId)
			.Set(&SdBoxStyle::display, SdDisplay::Flex)
			.Set(&SdBoxStyle::position, SdPosition::Absolute)
			.Set(&SdBoxStyle::flexDirection, SdFlexDirection::Column)
			.Set(&SdBoxStyle::justifyContent, SdJustifyContent::Center)
			.Set(&SdBoxStyle::alignItems, SdAlignItems::FlexEnd)
			.Set(&SdBoxStyle::flexBasis, SdLength::Percent(40.0f))
			.Set(&SdBoxStyle::flexGrow, 2.0f)
			.Set(&SdBoxStyle::flexShrink, 3.0f)
			.Set(&SdBoxStyle::boxSizing, SdBoxSizing::BorderBox)
			.Set(&SdBoxStyle::maxWidth, SdLength::Pixels(96.0f))
			.Set(&SdBoxStyle::maxHeight, SdLength::Pixels(72.0f))
			.Set(&SdBoxStyle::margin, SdStyleValue::FromSpacing({ 2.0f, 3.0f, 4.0f, 5.0f }))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ 5.0f, 6.0f, 7.0f, 8.0f }))
			.Set(&SdBoxStyle::overflowX, SdOverflow::Hidden)
			.Set(&SdBoxStyle::overflowY, SdOverflow::Clip);
		overflowInstance.BeginFrame({ 320.0f, 200.0f });
		overflowInstance.ui.Declare<TestOverflowContainer>([](SdUi& ui)
		{
			ui.Declare<TestDrawWidget>("child");
		});
		PumpFrame(overflowInstance);
		const SdWidgetRootStyle overflowStyle = overflowInstance.GetStyleSystem().ResolveRootStyle(TestOverflowContainer::TargetTypeId, SdStyleInteractionState::Normal);
		Check(
			overflowStyle.display == SdDisplay::Flex
			&& overflowStyle.position == SdPosition::Absolute
			&& overflowStyle.flexDirection == SdFlexDirection::Column
			&& overflowStyle.justifyContent == SdJustifyContent::Center
			&& overflowStyle.alignItems == SdAlignItems::FlexEnd
			&& overflowStyle.flexBasis.unit == SdLengthUnit::Percent
			&& overflowStyle.flexBasis.value == 40.0f
			&& overflowStyle.flexGrow == 2.0f
			&& overflowStyle.flexShrink == 3.0f
			&& overflowStyle.boxSizing == SdBoxSizing::BorderBox
			&& overflowStyle.maxWidth.value == 96.0f
			&& overflowStyle.maxHeight.value == 72.0f
			&& overflowStyle.margin.left.value == 2.0f
			&& overflowStyle.overflowX == SdOverflow::Hidden
			&& overflowStyle.overflowY == SdOverflow::Clip,
			"stylesheet resolves root box model properties");
		bool childClipMatchesOverflowContent = false;
		bool childTargetMatchesFlexLayoutBox = false;
		bool childConsumesFlexAlignment = false;
		bool rootUsedBoxIncludesMargin = false;
		bool rootUsedBoxIncludesContent = false;
		SdRect overflowContentRect = {};
		for (const auto& [id, record] : overflowInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(TestOverflowContainer)))
			{
				const SdStyleNode& root = overflowInstance.GetRootStyleNode(record.state.id);
				overflowContentRect = record.state.childContentRect;
				rootUsedBoxIncludesMargin = root.usedBox.marginBox.min.x == root.usedBox.borderBox.min.x - 2.0f
					&& root.usedBox.marginBox.max.y == root.usedBox.borderBox.max.y + 5.0f;
				rootUsedBoxIncludesContent = root.usedBox.contentBox.min.x == record.state.childContentRect.min.x
					&& root.usedBox.contentBox.max.y == record.state.childContentRect.max.y;
			}
		}
		for (const auto& [id, record] : overflowInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(TestDrawWidget)))
			{
				const SdStyleNode& childRoot = overflowInstance.GetRootStyleNode(record.state.id);
				childTargetMatchesFlexLayoutBox = record.state.targetRect.min.x == childRoot.layoutBox.borderBox.min.x
					&& record.state.targetRect.min.y == childRoot.layoutBox.borderBox.min.y
					&& record.state.targetRect.max.x == childRoot.layoutBox.borderBox.max.x
					&& record.state.targetRect.max.y == childRoot.layoutBox.borderBox.max.y;
				childConsumesFlexAlignment = record.state.targetRect.min.y > overflowContentRect.min.y;
				childClipMatchesOverflowContent = record.state.computedClipRect.min.x == overflowContentRect.min.x
					&& record.state.computedClipRect.min.y == overflowContentRect.min.y
					&& record.state.computedClipRect.max.x == overflowContentRect.max.x
					&& record.state.computedClipRect.max.y == overflowContentRect.max.y;
			}
		}
		Check(rootUsedBoxIncludesMargin, "runtime used geometry resolves root margin box");
		Check(rootUsedBoxIncludesContent, "runtime used geometry resolves root content box");
		Check(childTargetMatchesFlexLayoutBox, "runtime target rect is driven by box tree layout");
		Check(childConsumesFlexAlignment, "runtime consumes flex alignment style for child layout");
		Check(childClipMatchesOverflowContent, "runtime derives child clipping from root overflow style");

		SdInstance manualInstance;
		manualInstance.BeginFrame({ 320.0f, 200.0f });
		manualInstance.ui.Declare<TestManualLayoutWidget>();
		PumpFrame(manualInstance);
		bool manualShadowBoxUsesAbsoluteRect = false;
		bool manualLayoutBoxUsesAbsoluteRect = false;
		for (const auto& [id, record] : manualInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(TestManualLayoutWidget)))
			{
				const SdBoxNode* box = manualInstance.GetBoxTree().FindBoxByStyleNodeId(record.rootStyleNodeId);
				manualShadowBoxUsesAbsoluteRect = box
					&& box->usedStyleValues.position == SdPosition::Absolute
					&& box->borderBox.min.x == record.state.manualRect.min.x
					&& box->borderBox.max.y == record.state.manualRect.max.y;
				const SdStyleNode& root = manualInstance.GetRootStyleNode(record.state.id);
				manualLayoutBoxUsesAbsoluteRect = root.layoutBox.borderBox.min.x == record.state.manualRect.min.x
					&& root.layoutBox.borderBox.max.y == record.state.manualRect.max.y;
			}
		}
		Check(manualShadowBoxUsesAbsoluteRect, "runtime maps manual layout widgets to absolute shadow boxes");
		Check(manualLayoutBoxUsesAbsoluteRect, "runtime writes manual shadow geometry to style node layout box");

		gPaintContextObservedLayoutBox = false;
		SdInstance paintLayoutBoxInstance;
		paintLayoutBoxInstance.BeginFrame({ 320.0f, 200.0f });
		paintLayoutBoxInstance.ui.Declare<PaintLayoutBoxWidget>();
		PumpFrame(paintLayoutBoxInstance);
		Check(gPaintContextObservedLayoutBox, "paint context exposes root layout box geometry");
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
		instance.GetStyleSystem().RootRule(TestContainer::TargetTypeId)
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ 6.0f, 6.0f, 6.0f, 6.0f }))
			.Set(&SdBoxStyle::gap, SdLength::Pixels(4.0f));
		instance.BeginFrame({ 640.0f, 480.0f });
		instance.ui.DeclareKeyed<TestContainer>("container", [](SdUi& ui)
		{
			ui.Declare<TestDrawWidget>("one");
			ui.Declare<TestDrawWidget>("two");
		});
		PumpFrame(instance);

		Check(instance.GetDiagnostics().submittedWidgetCount == 3, "nested declaration submits container children");
		Check(instance.GetBoxTree().GetBoxCount() == 3, "box tree node exists for each declared widget");
		Check(instance.GetDiagnostics().boxNodeCount == 3, "diagnostics expose box node count");
		Check(instance.GetDiagnostics().activeAnimationChannelCount > 0, "enter animation channels are active");
		bool hasLayoutCache = false;
		bool hasStyleCache = false;
		bool hasExtendedAnimationChannels = false;
		bool hasStyleNodeAnimationTarget = false;
		const SdWidgetRootStyle buttonStyle = instance.GetStyleSystem().ResolveRootStyle(TestDrawWidget::TargetTypeId, SdStyleInteractionState::Normal);
		const SdPropertyId backgroundPropertyId = Detail::SdStylePropertyId(&SdBoxStyle::backgroundColor);
		for (const auto& [id, record] : instance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			hasLayoutCache = hasLayoutCache || record.layoutCache.targetRect.Width() > 0.0f;
			hasStyleCache = hasStyleCache || record.styleCache.valid;
			hasExtendedAnimationChannels = hasExtendedAnimationChannels
				|| (record.animation.rectWidth != 0 && record.animation.scrollOffset != 0);
			if (record.state.targetTypeId == TestDrawWidget::TargetTypeId)
			{
				for (const SdPropertyAnimationChannel& channel : instance.GetContext().presentationChannels.GetChannels())
				{
					if (channel.styleNodeId == record.rootStyleNodeId
						&& channel.propertyId == backgroundPropertyId
						&& channel.targetValue.kind == SdStyleValueKind::Color)
					{
						hasStyleNodeAnimationTarget = channel.targetValue.color == buttonStyle.backgroundColor;
					}
				}
			}
		}
		Check(hasLayoutCache, "state storage owns layout cache");
		Check(hasStyleCache, "state storage owns presentation style cache");
		Check(hasExtendedAnimationChannels, "widget records own extended animation channel references");
		Check(hasStyleNodeAnimationTarget, "style node animation targets presentation background");

		const SdWidgetPartStyle normalContent = instance.GetStyleSystem().ResolvePartStyle(
			TestDrawWidget::TargetTypeId,
			SdButton::Parts::Content,
			buttonStyle,
			SdStyleInteractionState::Normal);
		const SdWidgetPartStyle hoveredContent = instance.GetStyleSystem().ResolvePartStyle(
			TestDrawWidget::TargetTypeId,
			SdButton::Parts::Content,
			buttonStyle,
			SdStyleInteractionState::Hovered);
		Check(normalContent.backgroundColor != hoveredContent.backgroundColor, "style interaction selector changes button content background");
		Check(normalContent.border.left.color == instance.GetStyleSystem().GetTheme().GetColorVariable(SdThemeVariableIds::Border), "button content style carries theme border color");

		const SdUInt64 previousStyleRevision = instance.GetStyleSystem().GetRevision();
		const SdColor updatedButtonColor = { 11, 22, 33, 255 };
		const SdColor updatedRootColor = { 17, 29, 41, 255 };
		const SdColor updatedTextColor = { 203, 191, 179, 255 };
		const SdColor updatedBorderColor = { 71, 83, 97, 255 };
		const float updatedOpacity = 0.38f;
		instance.GetStyleSystem().SetColorVariable(SdThemeVariableIds::ButtonBg, updatedButtonColor);
		instance.GetStyleSystem().RootRule(TestDrawWidget::TargetTypeId)
			.Set(&SdBoxStyle::backgroundColor, updatedRootColor)
			.Transition(&SdBoxStyle::backgroundColor, std::chrono::milliseconds(320), SdAnimationEasing::Linear)
			.Set(&SdBoxStyle::color, updatedTextColor)
			.Set(&SdBoxStyle::border, SdStyleValue::FromColor(updatedBorderColor))
			.Set(&SdBoxStyle::opacity, updatedOpacity)
			.Transition(&SdBoxStyle::opacity, std::chrono::milliseconds(220), SdAnimationEasing::Linear);
		instance.BeginFrame({ 640.0f, 480.0f });
		instance.ui.DeclareKeyed<TestContainer>("container", [](SdUi& ui)
		{
			ui.Declare<TestDrawWidget>("one");
			ui.Declare<TestDrawWidget>("two");
		});
		PumpFrame(instance);

		bool hasUpdatedStyleRevision = false;
		bool hasUpdatedContentPartStyle = false;
		bool hasRootBackgroundStyleNodeAnimation = false;
		bool hasRootBackgroundStylesheetTransition = false;
		bool hasRootBackgroundFromStyleNodeAnimation = false;
		bool hasRootColorStyleNodeAnimation = false;
		bool hasRootColorFromStyleNodeAnimation = false;
		bool hasRootBorderStyleNodeAnimation = false;
		bool hasRootBorderFromStyleNodeAnimation = false;
		bool hasRootOpacityStyleNodeAnimation = false;
		bool hasRootOpacityFromStyleNodeAnimation = false;
		const SdPropertyId colorPropertyId = Detail::SdStylePropertyId(&SdBoxStyle::color);
		const SdPropertyId borderPropertyId = Detail::SdStylePropertyId(&SdBoxStyle::border);
		const SdPropertyId opacityPropertyId = Detail::SdStylePropertyId(&SdBoxStyle::opacity);
		for (const auto& [id, record] : instance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.state.targetTypeId == TestDrawWidget::TargetTypeId)
			{
				hasUpdatedStyleRevision = record.styleCache.valid
					&& record.styleCache.styleRevision != previousStyleRevision;
				const SdWidgetPartStyle contentStyle = instance.GetStyleSystem().ResolvePartStyle(
					TestDrawWidget::TargetTypeId,
					SdButton::Parts::Content,
					record.styleCache.resolvedStyle,
					SdStyleInteractionState::Normal);
				hasUpdatedContentPartStyle = contentStyle.backgroundColor == updatedButtonColor;
				for (const SdPropertyAnimationChannel& channel : instance.GetContext().presentationChannels.GetChannels())
				{
					if (channel.styleNodeId == record.rootStyleNodeId
						&& channel.propertyId == backgroundPropertyId
						&& channel.impact == SdStyleFieldImpact::Paint
						&& channel.interpolation == SdStyleInterpolation::Color)
					{
						hasRootBackgroundStyleNodeAnimation = true;
						hasRootBackgroundStylesheetTransition = channel.transition.duration == std::chrono::milliseconds(320);
						if (channel.currentValue.kind == SdStyleValueKind::Color)
						{
							const SdStyleNode& rootNode = instance.GetRootStyleNode(record.state.id);
							hasRootBackgroundFromStyleNodeAnimation = rootNode.presentationStyle.backgroundColor == channel.currentValue.color;
						}
					}
					if (channel.styleNodeId == record.rootStyleNodeId
						&& channel.propertyId == colorPropertyId
						&& channel.impact == SdStyleFieldImpact::Paint
						&& channel.interpolation == SdStyleInterpolation::Color)
					{
						hasRootColorStyleNodeAnimation = true;
						if (channel.currentValue.kind == SdStyleValueKind::Color)
						{
							const SdStyleNode& rootNode = instance.GetRootStyleNode(record.state.id);
							hasRootColorFromStyleNodeAnimation = rootNode.presentationStyle.color == channel.currentValue.color;
						}
					}
					if (channel.styleNodeId == record.rootStyleNodeId
						&& channel.propertyId == borderPropertyId
						&& channel.impact == SdStyleFieldImpact::Paint
						&& channel.interpolation == SdStyleInterpolation::Color)
					{
						hasRootBorderStyleNodeAnimation = true;
						if (channel.currentValue.kind == SdStyleValueKind::Color)
						{
							const SdStyleNode& rootNode = instance.GetRootStyleNode(record.state.id);
							hasRootBorderFromStyleNodeAnimation = rootNode.presentationStyle.border.left.color == channel.currentValue.color;
						}
					}
					if (channel.styleNodeId == record.rootStyleNodeId
						&& channel.propertyId == opacityPropertyId
						&& channel.impact == SdStyleFieldImpact::Composite
						&& channel.interpolation == SdStyleInterpolation::Float)
					{
						hasRootOpacityStyleNodeAnimation = true;
						if (channel.currentValue.kind == SdStyleValueKind::Float)
						{
							const SdStyleNode& rootNode = instance.GetRootStyleNode(record.state.id);
							hasRootOpacityFromStyleNodeAnimation = rootNode.presentationStyle.opacity == channel.currentValue.number;
						}
					}
				}
			}
		}
		Check(hasUpdatedStyleRevision, "style cache refreshes when style system revision changes");
		Check(hasUpdatedContentPartStyle, "button content part style refreshes when style system revision changes");
		Check(hasRootBackgroundStyleNodeAnimation, "root background transition is tracked by style node property channel");
		Check(hasRootBackgroundStylesheetTransition, "root background transition uses stylesheet duration");
		Check(hasRootBackgroundFromStyleNodeAnimation, "root background presentation reads style node property channel");
		Check(hasRootColorStyleNodeAnimation, "root color transition is tracked by style node property channel");
		Check(hasRootColorFromStyleNodeAnimation, "root color presentation reads style node property channel");
		Check(hasRootBorderStyleNodeAnimation, "root border transition is tracked by style node property channel");
		Check(hasRootBorderFromStyleNodeAnimation, "root border presentation reads style node property channel");
		Check(hasRootOpacityStyleNodeAnimation, "root opacity transition is tracked by style node property channel");
		Check(hasRootOpacityFromStyleNodeAnimation, "root opacity presentation reads style node property channel");
		Check(instance.GetDiagnostics().activeStyleNodeAnimationChannelCount > 0, "diagnostics expose active style node animation channels");
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
			.Layer(SdRootLayer::Tooltip)
			.Set(&SdBoxStyle::backgroundColor, ThemeColor(SdThemeVariableIds::Accent));

		const SdWidgetRootStyle content = styleSystem.ResolveRootStyle(SdWidgetTargetIds::Default, SdStyleInteractionState::Normal, SdRootLayer::Content);
		const SdWidgetRootStyle overlay = styleSystem.ResolveRootStyle(SdWidgetTargetIds::Default, SdStyleInteractionState::Normal, SdRootLayer::Tooltip);
		Check(content.backgroundColor != overlay.backgroundColor, "style layer selector changes overlay background");
		Check(overlay.backgroundColor == styleSystem.GetTheme().GetColorVariable(SdThemeVariableIds::Accent), "style layer selector applies matching rule");
		styleSystem.RootRule(SdWidgetTargetIds::Default)
			.Layer(SdRootLayer::Tooltip)
			.Set(&SdBoxStyle::zIndex, SdInt32{ 17 });
		const SdWidgetRootStyle overlayWithZIndex = styleSystem.ResolveRootStyle(SdWidgetTargetIds::Default, SdStyleInteractionState::Normal, SdRootLayer::Tooltip);
		Check(overlayWithZIndex.zIndex == 17, "style layer selector resolves z-index");
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
		Check(text.backgroundColor == styleSystem.GetTheme().GetColorVariable(SdThemeVariableIds::Background), "typed component style does not leak to unrelated root targets");
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

		constexpr SdStyleClassId dangerClass = "tests.typed.danger"_SdId;
		constexpr SdStyleScopeId dialogScope = "tests.typed.dialog"_SdId;
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
			SdRootLayer::Content,
			SdSpan<const SdStyleClassId>(classList, 1),
			0);
		Check(classResolved.width == 91.0f, "typed style class selector matches class id");
		const TypedStyleWidget::Style scopeResolved = styleSystem.ResolveTypedStyle<TypedStyleWidget>(
			SdStyleInteractionState::Normal,
			SdRootLayer::Content,
			{},
			dialogScope);
		Check(scopeResolved.color == scopedColor, "typed style scope selector matches scope id");

		TypedStyleWidget::Style inlineStyle = {};
		inlineStyle.width = 123.0f;
		inlineStyle.color = { 3, 4, 5, 255 };
		const TypedStyleWidget::Style inlineResolved = styleSystem.ResolveTypedStyle<TypedStyleWidget>(
			SdStyleInteractionState::Normal,
			SdRootLayer::Content,
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
		SdTransition compiledTransition = {};
		const bool compiledTransitionResolved = transitionInstance.GetStyleSystem().TryResolveTransition<TypedStyleWidget>(
			Detail::SdStyleFieldId(&TypedStyleWidget::Style::color),
			SdStyleInteractionState::Normal,
			SdRootLayer::Content,
			{},
			0,
			compiledTransition);
		Check(compiledTransitionResolved && compiledTransition.duration == std::chrono::milliseconds(400), "typed transition resolves through compiled stylesheet");
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
		Check(transitionInstance.GetDiagnostics().activeStyleNodeAnimationChannelCount > 0, "typed style transition contributes style node animation diagnostics");
		bool typedTransitionUsesStyleNodeChannel = false;
		const SdPropertyId typedColorPropertyId = Detail::SdStyleFieldId(&TypedStyleWidget::Style::color);
		for (const auto& [id, record] : transitionInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType != std::type_index(typeid(TypedStyleWidget)))
				continue;
			for (const SdPropertyAnimationChannel& channel : transitionInstance.GetContext().presentationChannels.GetChannels())
			{
				typedTransitionUsesStyleNodeChannel = typedTransitionUsesStyleNodeChannel
					|| (channel.active
						&& channel.styleNodeId == record.rootStyleNodeId
						&& channel.propertyId == typedColorPropertyId);
			}
		}
		Check(typedTransitionUsesStyleNodeChannel, "typed style transition is driven by style node property channel");

		std::this_thread::sleep_for(std::chrono::milliseconds(450));
		transitionInstance.BeginFrame({ 640.0f, 480.0f });
		transitionInstance.ui.Declare<TypedStyleWidget>();
		PumpFrame(transitionInstance);
		Check(gTypedStylePaintColor == targetColor, "typed presentation style reaches transition target");

		SdInstance classTransitionInstance;
		classTransitionInstance.GetStyleSystem().Rule<TypedStyleWidget>()
			.Set(&TypedStyleWidget::Style::color, startColor);
		classTransitionInstance.GetStyleSystem().Rule<TypedStyleWidget>()
			.Class(dangerClass)
			.Transition(&TypedStyleWidget::Style::color, std::chrono::milliseconds(350), SdAnimationEasing::Linear);
		SdTransition classTransition = {};
		const bool plainTransitionResolved = classTransitionInstance.GetStyleSystem().TryResolveTransition<TypedStyleWidget>(
			Detail::SdStyleFieldId(&TypedStyleWidget::Style::color),
			SdStyleInteractionState::Normal,
			SdRootLayer::Content,
			{},
			0,
			classTransition);
		const bool classScopedTransitionResolved = classTransitionInstance.GetStyleSystem().TryResolveTransition<TypedStyleWidget>(
			Detail::SdStyleFieldId(&TypedStyleWidget::Style::color),
			SdStyleInteractionState::Normal,
			SdRootLayer::Content,
			SdSpan<const SdStyleClassId>(classList, 1),
			0,
			classTransition);
		Check(!plainTransitionResolved && classScopedTransitionResolved && classTransition.duration == std::chrono::milliseconds(350), "compiled typed transition preserves class selector");
	}

	void TestContextOwnershipAndRenderSubmission()
	{
		RecordingPlatformBackend platform;
		CountingRenderer initRenderer;
		RecordingFontBackend initFontBackend;
		SdInstance initInstance;
		Check(initInstance.Initialize(platform, initRenderer, initFontBackend), "instance initialize binds initialized backend interfaces");
		Check(initInstance.GetPlatformBackend() == &platform, "instance initialize stores platform backend");
		Check(initInstance.GetRendererBackend() == &initRenderer, "instance initialize stores renderer backend");
		Check(initInstance.GetFontBackend() == &initFontBackend, "instance initialize stores font backend");

		initFontBackend.initialized = false;
		SdInstance failedInitInstance;
		Check(!failedInitInstance.Initialize(platform, initRenderer, initFontBackend), "instance initialize fails when a backend reports uninitialized");

		SdInstance instance;
		const SdContext& context = instance.GetContext();
		Check(static_cast<const void*>(&context.stateStorage) == static_cast<const void*>(&instance.GetStateStorage()), "context owns state storage");
		Check(static_cast<const void*>(&context.styling) == static_cast<const void*>(&instance.GetStyleSystem()), "context owns style system");
		Check(static_cast<const void*>(&context.boxTree) == static_cast<const void*>(&instance.GetBoxTree()), "context owns box tree");
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

			SdRect sliderHitRect = {};
			for (const auto& [id, record] : instance.GetStateStorage().GetWidgetRecords())
			{
				(void)id;
				if (record.widgetType != std::type_index(typeid(SdSliderFloat)))
					continue;
				for (const SdHitTestRecord& hitRecord : instance.GetLayerSystem().GetHitTestRecords())
				{
					if (hitRecord.widgetId == record.state.id)
						sliderHitRect = hitRecord.rect;
				}
			}
			const SdVec2 sliderPressPosition = {
				sliderHitRect.max.x - 2.0f,
				(sliderHitRect.min.y + sliderHitRect.max.y) * 0.5f
			};
			instance.GetInputSystem().PushEvent(SdInputEvent{
				SdInputEventType::MouseMove,
				SdInputDevice::Mouse,
				SdMouseMoveEvent{ sliderPressPosition }
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

		{
			SdInstance instance;
			bool firstOpen = true;
			bool secondOpen = true;
			SdWindowOptions firstOptions = {};
			firstOptions.position = { 40.0f, 40.0f };
			firstOptions.size = { 180.0f, 120.0f };
			SdWindowOptions secondOptions = {};
			secondOptions.position = { 84.0f, 64.0f };
			secondOptions.size = { 180.0f, 120.0f };
			const auto declareWindows = [&]()
			{
				instance.ui.DeclareKeyed<SdWindow>("first-window", "First", firstOpen, firstOptions, [](SdUi& ui)
				{
					ui.Declare<SdText>("First body");
				});
				instance.ui.DeclareKeyed<SdWindow>("second-window", "Second", secondOpen, secondOptions, [](SdUi& ui)
				{
					ui.Declare<SdText>("Second body");
				});
			};
			const auto findWidgetByKey = [&instance](const char* key)
			{
				for (const auto& [id, record] : instance.GetStateStorage().GetWidgetRecords())
				{
					(void)id;
					if (record.debugKey == key)
						return record.state.id;
				}
				return SdWidgetId{};
			};
			const auto findDrawIndex = [&instance](SdWidgetId widgetId)
			{
				const std::vector<SdLayerDrawRecord>& records = instance.GetLayerSystem().GetDrawRecords();
				for (SdSize index = 0; index < records.size(); ++index)
				{
					if (records[index].widgetId == widgetId)
						return index;
				}
				return SdInvalidIndex<SdSize>;
			};

			instance.BeginFrame({ 360.0f, 260.0f });
			declareWindows();
			PumpFrame(instance);
			const SdWidgetId firstWindowId = findWidgetByKey("first-window");
			const SdWidgetId secondWindowId = findWidgetByKey("second-window");
			Check(findDrawIndex(secondWindowId) > findDrawIndex(firstWindowId), "window declaration order initially controls overlapping windows");

			instance.GetInputSystem().PushEvent(SdInputEvent{
				SdInputEventType::MouseMove,
				SdInputDevice::Mouse,
				SdMouseMoveEvent{ { 52.0f, 52.0f } }
			});
			instance.GetInputSystem().PushEvent(SdInputEvent{
				SdInputEventType::MouseButton,
				SdInputDevice::Mouse,
				SdMouseButtonEvent{ SdMouseButton::Left, true }
			});
			instance.BeginFrame({ 360.0f, 260.0f });
			declareWindows();
			PumpFrame(instance);
			Check(findDrawIndex(firstWindowId) > findDrawIndex(secondWindowId), "pressed window activation order raises it above later windows");
		}
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

		SdStyleAnimationChannels presentationChannels;
		const SdPropertyId styleAnimationColorPropertyId = Detail::SdStylePropertyId(&SdBoxStyle::color);
		Check(presentationChannels.Find(7, styleAnimationColorPropertyId) == nullptr, "style animation find does not create missing channels");
		SdPropertyAnimationChannel& colorChannel = presentationChannels.Ensure(7, Detail::SdStylePropertyId(&SdBoxStyle::color));
		SdPropertyAnimationChannel& sameColorChannel = presentationChannels.Ensure(7, styleAnimationColorPropertyId);
		Check(
			&sameColorChannel == &colorChannel
			&& presentationChannels.GetChannels().size() == 1,
			"style animation ensure reuses indexed channel");
		colorChannel.impact = SdStyleFieldImpact::Paint;
		colorChannel.interpolation = SdStyleInterpolation::Color;
		colorChannel.transition = SdTransition{ std::chrono::milliseconds(100), SdAnimationEasing::Linear };
		colorChannel.startValue = SdStyleValue::FromColor({ 0, 0, 0, 255 });
		colorChannel.targetValue = SdStyleValue::FromColor({ 100, 50, 25, 255 });
		colorChannel.currentValue = colorChannel.startValue;
		colorChannel.active = true;
		SdPropertyAnimationChannel& partChannel = presentationChannels.Ensure(9, Detail::SdStylePropertyId(&SdBoxStyle::backgroundColor));
		partChannel.impact = SdStyleFieldImpact::Paint;
		partChannel.interpolation = SdStyleInterpolation::Color;
		partChannel.transition = SdTransition{ std::chrono::milliseconds(100), SdAnimationEasing::Linear };
		partChannel.startValue = SdStyleValue::FromColor({ 10, 20, 30, 255 });
		partChannel.targetValue = SdStyleValue::FromColor({ 80, 90, 100, 255 });
		partChannel.currentValue = partChannel.startValue;
		partChannel.active = true;
		const SdStyleNodeId partNodeIds[] = { 9 };
		Check(
			presentationChannels.HasActiveStyleNode(7)
			&& presentationChannels.HasActiveAny(7, {})
			&& presentationChannels.HasActiveAny(3, SdSpan<const SdStyleNodeId>(partNodeIds, 1)),
			"style node animation channels expose active root and part lookup");
		presentationChannels.Update(std::chrono::milliseconds(50));
		const SdPropertyAnimationChannel* updatedColorChannel = presentationChannels.Find(7, Detail::SdStylePropertyId(&SdBoxStyle::color));
		Check(
			updatedColorChannel
			&& updatedColorChannel->currentValue.kind == SdStyleValueKind::Color
			&& updatedColorChannel->currentValue.color.r > 0
			&& updatedColorChannel->currentValue.color.r < 100
			&& presentationChannels.CountActive() == 2,
			"style node animation channel advances property color");
		presentationChannels.Update(std::chrono::milliseconds(100));
		const SdPropertyAnimationChannel* completedColorChannel = presentationChannels.Find(7, Detail::SdStylePropertyId(&SdBoxStyle::color));
		Check(
			completedColorChannel
			&& completedColorChannel->currentValue.color == completedColorChannel->targetValue.color
			&& presentationChannels.CountActive() == 0,
			"style node animation channel completes property color");
		presentationChannels.RemoveStyleNode(7);
		Check(
			presentationChannels.Find(7, styleAnimationColorPropertyId) == nullptr
			&& presentationChannels.GetChannels().size() == 1,
			"style animation index rebuilds after style node removal");

		SdStyleAnimationChannels noopStyleChannels;
		const SdColor noopColor = { 45, 55, 65, 255 };
		Detail::SetStylePropertyChannelTarget(
			noopStyleChannels,
			31,
			styleAnimationColorPropertyId,
			SdStyleFieldImpact::Paint,
			SdStyleInterpolation::Color,
			SdStyleValue::FromColor(noopColor),
			SdStyleValue::FromColor(noopColor),
			{},
			true);
		Detail::SetStylePropertyChannelTarget(
			noopStyleChannels,
			31,
			styleAnimationColorPropertyId,
			SdStyleFieldImpact::Paint,
			SdStyleInterpolation::Color,
			SdStyleValue::FromColor(noopColor),
			SdStyleValue::FromColor(noopColor),
			{},
			true);
		Check(
			noopStyleChannels.GetFrameStats().targetSetCount == 2
			&& noopStyleChannels.GetFrameStats().targetSetNoopCount == 1
			&& noopStyleChannels.GetChannels().size() == 1,
			"style animation target setter skips identical inactive targets");

		SdStyleAnimationChannels delayedStyleChannels;
		SdPropertyAnimationChannel& delayedChannel = delayedStyleChannels.Ensure(11, Detail::SdStylePropertyId(&SdBoxStyle::opacity));
		delayedChannel.impact = SdStyleFieldImpact::Composite;
		delayedChannel.interpolation = SdStyleInterpolation::Float;
		delayedChannel.transition = SdTransition{ std::chrono::milliseconds(100), SdAnimationEasing::Linear, std::chrono::milliseconds(80) };
		delayedChannel.delay = delayedChannel.transition.delay;
		delayedChannel.startValue = SdStyleValue::FromFloat(0.0f);
		delayedChannel.targetValue = SdStyleValue::FromFloat(1.0f);
		delayedChannel.currentValue = delayedChannel.startValue;
		delayedChannel.active = true;
		delayedStyleChannels.Update(std::chrono::milliseconds(40));
		Check(
			delayedChannel.currentValue.kind == SdStyleValueKind::Float
			&& delayedChannel.currentValue.number == 0.0f
			&& delayedChannel.active,
			"style node transition delay holds the start presentation value");
		delayedStyleChannels.Update(std::chrono::milliseconds(90));
		Check(
			delayedChannel.currentValue.number > 0.0f
			&& delayedChannel.currentValue.number < 1.0f
			&& delayedChannel.active,
			"style node transition starts interpolating after delay");

		SdStyleAnimationChannels discreteStyleChannels;
		SdPropertyAnimationChannel& discreteChannel = discreteStyleChannels.Ensure(12, Detail::SdStylePropertyId(&SdBoxStyle::display));
		discreteChannel.impact = SdStyleFieldImpact::Layout;
		discreteChannel.interpolation = SdStyleInterpolation::None;
		discreteChannel.transition = SdTransition{
			std::chrono::milliseconds(100),
			SdAnimationEasing::Linear,
			{},
			SdTransitionBehavior::AllowDiscrete
		};
		discreteChannel.startValue = SdStyleValue::FromFloat(0.0f);
		discreteChannel.targetValue = SdStyleValue::FromFloat(1.0f);
		discreteChannel.currentValue = discreteChannel.startValue;
		discreteChannel.active = true;
		discreteChannel.discrete = true;
		discreteStyleChannels.Update(std::chrono::milliseconds(50));
		Check(
			discreteChannel.currentValue.number == 0.0f
			&& discreteChannel.active,
			"style node discrete transition holds start value until completion");
		discreteStyleChannels.Update(std::chrono::milliseconds(60));
		Check(
			discreteChannel.currentValue.number == 1.0f
			&& !discreteChannel.active,
			"style node discrete transition jumps to target at completion");
	}

	void TestLayerSystemDirect()
	{
		SdLayerSystem layers;
		layers.BeginFrame();
		layers.AddHitTestRecord({ 1, SdRootLayer::Content, { 0.0f, 0.0f, 100.0f, 100.0f }, { 0.0f, 0.0f, 100.0f, 100.0f }, 0, true });
		layers.AddHitTestRecord({ 2, SdRootLayer::Popup, { 0.0f, 0.0f, 100.0f, 100.0f }, { 0.0f, 0.0f, 100.0f, 100.0f }, 0, true });
		layers.Finalize();
		Check(layers.HitTest({ 12.0f, 12.0f }) == 2, "layer direct higher priority wins hit-test");

		layers.BeginFrame();
		layers.AddHitTestRecord({ 1, SdRootLayer::Content, { 0.0f, 0.0f, 100.0f, 100.0f }, { 0.0f, 0.0f, 100.0f, 100.0f }, 0, true });
		layers.AddHitTestRecord({ 2, SdRootLayer::Content, { 0.0f, 0.0f, 100.0f, 100.0f }, { 0.0f, 0.0f, 100.0f, 100.0f }, 4, true });
		layers.AddHitTestRecord({ 3, SdRootLayer::Tooltip, { 0.0f, 0.0f, 100.0f, 100.0f }, { 0.0f, 0.0f, 100.0f, 100.0f }, 5, false });
		layers.Finalize();
		Check(layers.HitTest({ 12.0f, 12.0f }) == 2, "layer direct paint order wins within layer and disabled records are ignored");

		layers.BeginFrame();
		SdHitTestRecord lowerZ = {};
		lowerZ.widgetId = 1;
		lowerZ.rootLayer = SdRootLayer::Content;
		lowerZ.rect = { 0.0f, 0.0f, 100.0f, 100.0f };
		lowerZ.clipRect = { 0.0f, 0.0f, 100.0f, 100.0f };
		lowerZ.paintOrder = 100;
		lowerZ.key = SdMakeStackingKey(SdRootLayer::Content, 0, 0, 100);
		SdHitTestRecord higherZ = lowerZ;
		higherZ.widgetId = 2;
		higherZ.paintOrder = 0;
		higherZ.key = SdMakeStackingKey(SdRootLayer::Content, 20, 0, 0);
		layers.AddHitTestRecord(lowerZ);
		layers.AddHitTestRecord(higherZ);
		layers.Finalize();
		Check(layers.HitTest({ 12.0f, 12.0f }) == 2, "layer direct z-index wins before paint order");

		layers.BeginFrame();
		SdHitTestRecord normalWindow = lowerZ;
		normalWindow.widgetId = 4;
		normalWindow.paintOrder = 100;
		normalWindow.key = SdMakeStackingKey(SdRootLayer::Floating, 0, 100, 100);
		SdHitTestRecord activatedWindow = lowerZ;
		activatedWindow.widgetId = 5;
		activatedWindow.paintOrder = 0;
		activatedWindow.key = SdMakeStackingKey(SdRootLayer::Floating, 0, 0, 0);
		activatedWindow.key.activationOrder = 1;
		layers.AddHitTestRecord(normalWindow);
		layers.AddHitTestRecord(activatedWindow);
		layers.Finalize();
		Check(layers.HitTest({ 12.0f, 12.0f }) == 5, "layer direct activation order wins before tree order");

		layers.BeginFrame();
		layers.AddHitTestRecord(lowerZ);
		SdHitTestRecord inputBlocker = {};
		inputBlocker.widgetId = 3;
		inputBlocker.rootLayer = SdRootLayer::Popup;
		inputBlocker.rect = { 200.0f, 200.0f, 240.0f, 240.0f };
		inputBlocker.clipRect = { 0.0f, 0.0f, 640.0f, 480.0f };
		inputBlocker.hitTestVisible = false;
		inputBlocker.blocksLowerInput = true;
		inputBlocker.key = SdMakeStackingKey(SdRootLayer::Popup, 0, 0, 0);
		layers.AddHitTestRecord(inputBlocker);
		layers.Finalize();
		Check(layers.HitTest({ 12.0f, 12.0f }) == 0, "layer direct input blocker stops lower hit-test records");

		layers.BeginFrame();
		layers.AddDrawRecord({ 1, SdRootLayer::Content, { 0.0f, 0.0f, 640.0f, 480.0f }, 0 });
		layers.AddDrawRecord({ 2, SdRootLayer::Popup, { 0.0f, 0.0f, 640.0f, 480.0f }, 1 });
		layers.AddDrawRecord({ 3, SdRootLayer::Content, { 0.0f, 0.0f, 640.0f, 480.0f }, 2 });
		layers.AddDrawRecord({ 4, SdRootLayer::Popup, { 0.0f, 0.0f, 640.0f, 480.0f }, 3 });
		layers.Finalize();
		const std::vector<SdLayerDrawRecord>& drawRecords = layers.GetDrawRecords();
		const std::vector<SdLayerDrawChannel>& drawChannels = layers.GetDrawChannels();
		Check(
			drawRecords.size() == 4
			&& drawRecords[0].widgetId == 1
			&& drawRecords[1].widgetId == 3
			&& drawRecords[2].widgetId == 2
			&& drawRecords[3].widgetId == 4,
			"layer direct draw records sort by unified stacking key");
		Check(
			drawChannels.size() == 2
			&& drawChannels[0].rootLayer == SdRootLayer::Content
			&& drawChannels[0].recordCount == 2
			&& drawChannels[1].rootLayer == SdRootLayer::Popup
			&& drawChannels[1].recordCount == 2,
			"layer direct draw channels are built after sorting by root layer");
	}

	void TestStackingContextAndPortalRoot()
	{
		SdInstance windowInstance;
		SdWidgetRootStyle highChildStyle = {};
		highChildStyle.zIndex = 999;
		bool firstOpen = true;
		bool secondOpen = true;
		SdWindowOptions firstOptions = {};
		firstOptions.position = { 40.0f, 40.0f };
		firstOptions.size = { 180.0f, 120.0f };
		SdWindowOptions secondOptions = {};
		secondOptions.position = { 84.0f, 64.0f };
		secondOptions.size = { 180.0f, 120.0f };
		windowInstance.BeginFrame({ 360.0f, 260.0f });
		windowInstance.ui.DeclareKeyed<SdWindow>("first-window", "First", firstOpen, firstOptions, [&](SdUi& ui)
		{
			ui.DeclareStyled<TestDrawWidget>(&highChildStyle, "High child");
		});
		windowInstance.ui.DeclareKeyed<SdWindow>("second-window", "Second", secondOpen, secondOptions, [](SdUi& ui)
		{
			ui.Declare<TestDrawWidget>("Second child");
		});
		PumpFrame(windowInstance);

		SdWidgetId firstWindowId = 0;
		SdWidgetId firstChildId = 0;
		SdWidgetId secondWindowId = 0;
		for (const auto& [id, record] : windowInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.debugKey == "first-window")
				firstWindowId = record.state.id;
			if (record.debugKey == "second-window")
				secondWindowId = record.state.id;
		}
		for (const auto& [id, record] : windowInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.parentId == firstWindowId && record.widgetType == std::type_index(typeid(TestDrawWidget)))
				firstChildId = record.state.id;
		}

		const auto findDrawIndex = [&windowInstance](SdWidgetId widgetId)
		{
			const std::vector<SdLayerDrawRecord>& records = windowInstance.GetLayerSystem().GetDrawRecords();
			for (SdSize index = 0; index < records.size(); ++index)
			{
				if (records[index].widgetId == widgetId)
					return index;
			}
			return SdInvalidIndex<SdSize>;
		};
		const std::vector<SdStackingContextNode>& contexts = windowInstance.GetLayerSystem().GetStackingContexts();
		Check(contexts.size() >= 2, "windows create stacking context nodes");
		Check(firstWindowId != 0 && firstChildId != 0 && secondWindowId != 0, "stacking context test resolves window and child ids");
		Check(findDrawIndex(firstChildId) < findDrawIndex(secondWindowId), "child z-index remains inside parent stacking context");

		SdInstance portalInstance;
		portalInstance.GetStyleSystem().RootRule(TestOverflowContainer::TargetTypeId)
			.Set(&SdBoxStyle::overflowX, SdOverflow::Hidden)
			.Set(&SdBoxStyle::overflowY, SdOverflow::Hidden)
			.Set(&SdBoxStyle::width, SdLength::Pixels(80.0f))
			.Set(&SdBoxStyle::height, SdLength::Pixels(60.0f));
		portalInstance.BeginFrame({ 320.0f, 200.0f });
		portalInstance.ui.Declare<TestOverflowContainer>([](SdUi& ui)
		{
			ui.Declare<SdPopup>(true, SdVec2{ 140.0f, 76.0f }, [](SdUi& popupUi)
			{
				popupUi.Declare<TestDrawWidget>("Popup child");
			});
		});
		PumpFrame(portalInstance);

		SdWidgetId popupId = 0;
		SdWidgetId popupChildId = 0;
		for (const auto& [id, record] : portalInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.widgetType == std::type_index(typeid(SdPopup)))
				popupId = record.state.id;
		}
		for (const auto& [id, record] : portalInstance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			if (record.parentId == popupId && record.widgetType == std::type_index(typeid(TestDrawWidget)))
				popupChildId = record.state.id;
		}

		bool popupEscapesClip = false;
		bool popupChildEscapesClip = false;
		for (const SdLayerDrawRecord& drawRecord : portalInstance.GetLayerSystem().GetDrawRecords())
		{
			if (drawRecord.widgetId == popupId)
				popupEscapesClip = drawRecord.escapesParentClip && drawRecord.portalRoot == SdPortalRoot::Popup;
			if (drawRecord.widgetId == popupChildId)
				popupChildEscapesClip = drawRecord.escapesParentClip && drawRecord.portalRoot == SdPortalRoot::Popup;
		}
		Check(!portalInstance.GetLayerSystem().GetPortalRecords().empty(), "portal root records are registered");
		Check(popupId != 0 && popupChildId != 0, "portal root test resolves popup and child ids");
		Check(popupEscapesClip && popupChildEscapesClip, "popup portal root escapes parent clipping");
	}

	void TestInteractionSystemAdvancedEvents()
	{
		SdLayerSystem layers;
		layers.BeginFrame();
		layers.AddHitTestRecord({ 10, SdRootLayer::Content, { 0.0f, 0.0f, 80.0f, 80.0f }, { 0.0f, 0.0f, 200.0f, 200.0f }, 0, true });
		layers.AddHitTestRecord({ 20, SdRootLayer::Popup, { 100.0f, 0.0f, 180.0f, 80.0f }, { 0.0f, 0.0f, 200.0f, 200.0f }, 1, true });
		layers.Finalize();

		SdInteractionSystem interaction;
		SdInputSnapshot input = {};
		input.mouse.position = { 20.0f, 20.0f };
		input.mouse.buttons[static_cast<SdSize>(SdMouseButton::Left)].isDown = true;
		input.mouse.buttons[static_cast<SdSize>(SdMouseButton::Left)].wentDown = true;
		input.mouse.buttons[static_cast<SdSize>(SdMouseButton::Right)].isDown = true;
		input.mouse.buttons[static_cast<SdSize>(SdMouseButton::Right)].wentDown = true;
		interaction.Update(layers, input, 1);
		Check(interaction.IsPressed(10), "interaction press captures left pointer target");
		Check(interaction.IsPressed(10, SdMouseButton::Right), "interaction tracks independent right button press");
		Check(interaction.GetPointerRoute().size() == 3, "interaction builds tunnel target bubble route");

		input.mouse.buttons[static_cast<SdSize>(SdMouseButton::Left)].wentDown = false;
		input.mouse.buttons[static_cast<SdSize>(SdMouseButton::Right)].wentDown = false;
		input.mouse.position = { 40.0f, 20.0f };
		interaction.Update(layers, input, 2);
		Check(interaction.IsDragSource(10), "interaction starts drag after pointer moves past threshold");

		input.mouse.buttons[static_cast<SdSize>(SdMouseButton::Left)].isDown = false;
		input.mouse.buttons[static_cast<SdSize>(SdMouseButton::Left)].wentUp = true;
		input.mouse.position = { 120.0f, 20.0f };
		interaction.Update(layers, input, 3);
		Check(interaction.IsDropTarget(20), "interaction routes drop to hit widget under release");
		Check(interaction.WasOutsideClicked(10), "interaction detects outside release for active widget");

		input = {};
		input.mouse.position = { 20.0f, 20.0f };
		input.mouse.wheelDelta = { 0.0f, -1.0f };
		input.keyboard.keys[static_cast<SdSize>(SdKeyCode::Tab)].wentDown = true;
		interaction.Update(layers, input, 4);
		Check(interaction.IsWheelTarget(10), "interaction routes scroll wheel to hovered hit widget");
		Check(interaction.GetState().keyboardWidget == interaction.GetState().focusedWidget, "interaction keeps keyboard routing on focused widget");

		input = {};
		input.mouse.position = { 20.0f, 20.0f };
		input.mouse.buttons[static_cast<SdSize>(SdMouseButton::Left)].isDown = true;
		input.mouse.buttons[static_cast<SdSize>(SdMouseButton::Left)].wentDown = true;
		interaction.Update(layers, input, 5);
		input.mouse.buttons[static_cast<SdSize>(SdMouseButton::Left)].wentDown = false;
		interaction.Update(layers, input, 36);
		Check(interaction.WasLongPressed(10), "interaction detects long press");
		input.mouse.buttons[static_cast<SdSize>(SdMouseButton::Left)].isDown = false;
		input.mouse.buttons[static_cast<SdSize>(SdMouseButton::Left)].wentUp = true;
		interaction.Update(layers, input, 37);

		input = {};
		input.mouse.position = { 20.0f, 20.0f };
		input.mouse.buttons[static_cast<SdSize>(SdMouseButton::Left)].isDown = true;
		input.mouse.buttons[static_cast<SdSize>(SdMouseButton::Left)].wentDown = true;
		interaction.Update(layers, input, 38);
		input.mouse.buttons[static_cast<SdSize>(SdMouseButton::Left)].isDown = false;
		input.mouse.buttons[static_cast<SdSize>(SdMouseButton::Left)].wentDown = false;
		input.mouse.buttons[static_cast<SdSize>(SdMouseButton::Left)].wentUp = true;
		interaction.Update(layers, input, 39);
		input.mouse.buttons[static_cast<SdSize>(SdMouseButton::Left)].isDown = true;
		input.mouse.buttons[static_cast<SdSize>(SdMouseButton::Left)].wentDown = true;
		input.mouse.buttons[static_cast<SdSize>(SdMouseButton::Left)].wentUp = false;
		interaction.Update(layers, input, 40);
		input.mouse.buttons[static_cast<SdSize>(SdMouseButton::Left)].isDown = false;
		input.mouse.buttons[static_cast<SdSize>(SdMouseButton::Left)].wentDown = false;
		input.mouse.buttons[static_cast<SdSize>(SdMouseButton::Left)].wentUp = true;
		interaction.Update(layers, input, 41);
		Check(interaction.WasDoubleClicked(10), "interaction detects double click");

		interaction.SetFocusScope(10);
		interaction.SetModalScope(20);
		Check(interaction.GetState().focusScopeRootWidget == 10, "interaction stores focus scope root");
		Check(interaction.GetState().modalScopeRootWidget == 20, "interaction stores modal scope root");
		interaction.ClearModalScope(20);
		Check(interaction.GetState().modalScopeRootWidget == 0, "interaction clears modal scope root");
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
	TestStylePipelineClosingRequirements();
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
	TestAnimationSystemDirect();
	TestLayerSystemDirect();
	TestStackingContextAndPortalRoot();
	TestInteractionSystemAdvancedEvents();
	TestRenderListBatchingDirect();

	if (gFailedChecks != 0)
	{
		std::printf("%d checks failed.\n", gFailedChecks);
		return 1;
	}

	std::printf("All core tests passed.\n");
	return 0;
}
