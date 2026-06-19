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
			context.widgetState.styleClass = SdStyleWidgetClass::Button;
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
			context.renderList.AddRectFilled(context.animatedRect, ApplyOpacity(context.style.background, context.opacity), context.clipRect, context.style.radius);
			context.renderList.AddRect(context.animatedRect, ApplyOpacity(context.style.border, context.opacity), context.clipRect, 1.0f, context.style.radius);
			context.renderList.AddText(state.label, { context.animatedRect.min.x + 8.0f, context.animatedRect.min.y + 8.0f }, ApplyOpacity(context.style.color, context.opacity), context.clipRect);
		}
	};

	struct TestContainer final : SdWidgetTag
	{
		template<class TContent>
			requires std::is_invocable_v<TContent&, SdUi&>
		void OnUpdate(SdUpdateContext& context, TContent&& content)
		{
			context.widgetState.styleClass = SdStyleWidgetClass::Panel;
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
			context.renderList.AddRectFilled(context.animatedRect, ApplyOpacity(context.style.background, context.opacity), context.clipRect, context.style.radius);
		}
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

		std::this_thread::sleep_for(std::chrono::milliseconds(220));
		instance.BeginFrame({ 640.0f, 480.0f });
		PumpFrame(instance);
		Check(instance.GetDiagnostics().removedWidgetCount >= 1, "dead widget is swept");
		Check(instance.GetDiagnostics().modelCount >= 1, "keyed model remains after widget sweep");
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
		const SdComputedStyle buttonStyle = instance.GetStyleSystem().Resolve(SdStyleWidgetClass::Button, SdStyleInteractionState::Normal);
		for (const auto& [id, record] : instance.GetStateStorage().GetWidgetRecords())
		{
			(void)id;
			hasLayoutCache = hasLayoutCache || record.layoutCache.targetRect.Width() > 0.0f;
			hasStyleCache = hasStyleCache || record.styleCache.valid;
			hasExtendedAnimationChannels = hasExtendedAnimationChannels
				|| (record.animation.styleColorR != 0 && record.animation.scrollOffset != 0);
			if (record.state.styleClass == SdStyleWidgetClass::Button)
			{
				const SdAnimationChannel& red = instance.GetContext().animationSystem.GetChannel(record.animation.styleColorR);
				const SdAnimationChannel& green = instance.GetContext().animationSystem.GetChannel(record.animation.styleColorG);
				const SdAnimationChannel& blue = instance.GetContext().animationSystem.GetChannel(record.animation.styleColorB);
				hasStyleAnimationTarget = red.target == static_cast<float>(buttonStyle.background.r)
					&& green.target == static_cast<float>(buttonStyle.background.g)
					&& blue.target == static_cast<float>(buttonStyle.background.b);
			}
		}
		Check(hasLayoutCache, "state storage owns layout cache");
		Check(hasStyleCache, "state storage owns computed style cache");
		Check(hasExtendedAnimationChannels, "widget records own extended animation channel references");
		Check(hasStyleAnimationTarget, "style color animation targets computed background");

		const SdComputedStyle normal = instance.GetStyleSystem().Resolve(SdStyleWidgetClass::Button, SdStyleInteractionState::Normal);
		const SdComputedStyle hovered = instance.GetStyleSystem().Resolve(SdStyleWidgetClass::Button, SdStyleInteractionState::Hovered);
		Check(normal.background != hovered.background, "style interaction selector changes button background");
		Check(normal.border == instance.GetStyleSystem().GetTheme().GetColor(SdStyleToken::ColorBorder), "computed style carries theme border color");
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
		SdStyleRule overlayRule = {};
		overlayRule.widgetClass = SdStyleWidgetClass::Default;
		overlayRule.interactionState = SdStyleInteractionState::Normal;
		overlayRule.layerPriority = SdLayerPriority::Overlay;
		overlayRule.backgroundToken = SdStyleToken::ColorAccent;
		overlayRule.hasBackground = true;
		overlayRule.matchLayer = true;
		styleSystem.AddRule(overlayRule);

		const SdComputedStyle content = styleSystem.Resolve(SdStyleWidgetClass::Default, SdStyleInteractionState::Normal, SdLayerPriority::Content);
		const SdComputedStyle overlay = styleSystem.Resolve(SdStyleWidgetClass::Default, SdStyleInteractionState::Normal, SdLayerPriority::Overlay);
		Check(content.background != overlay.background, "style layer selector changes overlay background");
		Check(overlay.background == styleSystem.GetTheme().GetColor(SdStyleToken::ColorAccent), "style layer selector applies matching rule");
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
	TestIdAndKeySemantics();
	TestLifecycleStateAndModel();
	TestLayoutAnimationAndStyle();
	TestIdStackAndStyleLayerSelectors();
	TestContextOwnershipAndRenderSubmission();
	TestInputLayerAndCustomWidgets();
	TestInputSystemTextState();
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
