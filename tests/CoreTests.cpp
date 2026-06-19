#include "SodiumGUI.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

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

	SdWidgetId FirstWidgetId(const SdInstance& instance)
	{
		const auto& records = instance.GetStateStorage().GetWidgetRecords();
		return records.empty() ? 0 : records.begin()->first;
	}

	void TestSmokeAndDrawPacket()
	{
		SdInstance instance;
		instance.BeginFrame({ 640.0f, 480.0f });
		instance.ui.Declare<SdButton>("Run");
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
		instance.ui.Declare<SdText>("anonymous");
		PumpFrame(instance);
		const SdWidgetId anonymousId = FirstWidgetId(instance);

		instance.BeginFrame({ 640.0f, 480.0f });
		instance.ui.Declare<SdText>("anonymous changed");
		PumpFrame(instance);
		Check(FirstWidgetId(instance) == anonymousId, "anonymous id stays stable for same parent/type/ordinal");

		instance.BeginFrame({ 640.0f, 480.0f });
		instance.ui.DeclareKeyed<SdButton>("stable_button", "Label A");
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
		instance.ui.DeclareKeyed<SdButton>("stable_button", "Label B");
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

		auto& model = instance.ui.Model<SdScrollView>("scroll_view");
		model.scrollY = 42.0f;
		instance.BeginFrame({ 640.0f, 480.0f });
		PumpFrame(instance);
		Check(instance.ui.Model<SdScrollView>("scroll_view").scrollY == 42.0f, "keyed model persists without widget record");

		std::this_thread::sleep_for(std::chrono::milliseconds(220));
		instance.BeginFrame({ 640.0f, 480.0f });
		PumpFrame(instance);
		Check(instance.GetDiagnostics().removedWidgetCount >= 1, "dead widget is swept");
		Check(instance.GetDiagnostics().modelCount >= 1, "keyed model remains after widget sweep");
	}

	void TestLayoutAnimationAndStyle()
	{
		SdInstance instance;
		float sliderValue = 0.5f;
		instance.BeginFrame({ 640.0f, 480.0f });
		instance.ui.DeclareKeyed<SdWindow>("window", "Window", [&sliderValue](SdUi& ui)
		{
			ui.Declare<SdPanel>();
			ui.Declare<SdSliderFloat>(sliderValue, 0.0f, 1.0f);
		});
		PumpFrame(instance);

		Check(instance.GetDiagnostics().submittedWidgetCount == 3, "nested declaration submits window children");
		Check(instance.GetLayoutSystem().GetNodes().size() == 3, "layout node exists for each declared widget");
		Check(instance.GetDiagnostics().activeAnimationChannelCount > 0, "enter animation channels are active");

		const SdComputedStyle normal = instance.GetStyleSystem().Resolve(SdStyleWidgetClass::Button, SdStyleInteractionState::Normal);
		const SdComputedStyle hovered = instance.GetStyleSystem().Resolve(SdStyleWidgetClass::Button, SdStyleInteractionState::Hovered);
		Check(normal.background != hovered.background, "style interaction selector changes button background");
	}

	void TestInputLayerAndBuiltIns()
	{
		SdInstance instance;
		bool clicked = false;
		bool popupOpen = true;
		float value = 0.0f;
		SdUtf8String textValue = "A";

		instance.BeginFrame({ 640.0f, 480.0f });
		instance.ui.Declare<SdButton>("Click", clicked);
		instance.ui.Declare<SdSliderFloat>(value, 0.0f, 1.0f);
		instance.ui.Declare<SdTextInput>(textValue);
		instance.ui.DeclareKeyed<SdScrollView>("scroll", [] (SdUi& ui)
		{
			ui.Declare<SdText>("inside");
		});
		instance.ui.Declare<SdPopup>(popupOpen, [] (SdUi& ui)
		{
			ui.Declare<SdText>("popup");
		});
		instance.ui.Declare<SdContextMenu>(popupOpen, SdPopupOptions{}, [] (SdUi& ui)
		{
			ui.Declare<SdText>("menu");
		});
		instance.ui.Declare<SdTooltip>(true, "tip");
		PumpFrame(instance);

		Check(instance.GetLayerSystem().GetHitTestRecords().size() > 0, "layer system stores hit-test records");
		Check(instance.GetDiagnostics().drawCommandCount > 0, "built-in widgets emit draw commands");

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
		instance.ui.Declare<SdButton>("Click", clicked);
		PumpFrame(instance);
		Check(clicked, "button click uses unified interaction state");
	}
}

int main()
{
	TestSmokeAndDrawPacket();
	TestIdAndKeySemantics();
	TestLifecycleStateAndModel();
	TestLayoutAnimationAndStyle();
	TestInputLayerAndBuiltIns();

	if (gFailedChecks != 0)
	{
		std::printf("%d checks failed.\n", gFailedChecks);
		return 1;
	}

	std::printf("All core tests passed.\n");
	return 0;
}
