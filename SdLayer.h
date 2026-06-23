#pragma once

#include "SdInput.h"
#include "SdUiCore.h"

#include <algorithm>
#include <cassert>
#include <array>
#include <vector>

namespace Sodium
{
	struct SdStackingKey final
	{
		SdUInt16 rootLayer = static_cast<SdUInt16>(SdRootLayer::Content);
		SdUInt16 stackingContextDepth = 0;
		SdUInt32 stackingContextId = 0;
		SdInt32 contextZIndex = 0;
		SdUInt32 contextActivationOrder = 0;
		SdUInt32 contextTreeOrder = 0;
		SdInt32 zIndex = 0;
		SdUInt32 activationOrder = 0;
		SdUInt32 treeOrder = 0;
		SdUInt32 paintOrder = 0;

		friend constexpr bool operator==(const SdStackingKey&, const SdStackingKey&) = default;
	};

	inline constexpr SdStackingKey SdMakeStackingKey(
		SdRootLayer rootLayer,
		SdInt32 zIndex = 0,
		SdUInt32 treeOrder = 0,
		SdUInt32 paintOrder = 0,
		SdUInt32 stackingContextId = 0,
		SdUInt16 stackingContextDepth = 0,
		SdUInt32 activationOrder = 0,
		SdInt32 contextZIndex = 0,
		SdUInt32 contextActivationOrder = 0,
		SdUInt32 contextTreeOrder = 0) noexcept
	{
		return {
			static_cast<SdUInt16>(rootLayer),
			stackingContextDepth,
			stackingContextId,
			contextZIndex,
			contextActivationOrder,
			contextTreeOrder,
			zIndex,
			activationOrder,
			treeOrder,
			paintOrder
		};
	}

	inline constexpr bool SdLessStackingKey(const SdStackingKey& left, const SdStackingKey& right) noexcept
	{
		if (left.rootLayer != right.rootLayer)
			return left.rootLayer < right.rootLayer;

		if (left.contextZIndex != right.contextZIndex)
			return left.contextZIndex < right.contextZIndex;

		if (left.contextActivationOrder != right.contextActivationOrder)
			return left.contextActivationOrder < right.contextActivationOrder;

		if (left.contextTreeOrder != right.contextTreeOrder)
			return left.contextTreeOrder < right.contextTreeOrder;

		if (left.stackingContextDepth != right.stackingContextDepth)
			return left.stackingContextDepth < right.stackingContextDepth;

		if (left.zIndex != right.zIndex)
			return left.zIndex < right.zIndex;

		if (left.activationOrder != right.activationOrder)
			return left.activationOrder < right.activationOrder;

		if (left.treeOrder != right.treeOrder)
			return left.treeOrder < right.treeOrder;

		if (left.stackingContextId != right.stackingContextId)
			return left.stackingContextId < right.stackingContextId;

		return left.paintOrder < right.paintOrder;
	}

	struct SdLayerRecord final
	{
		SdWidgetId widgetId = 0;
		SdStackingKey key = {};
		SdRect bounds = {};
		SdRect hitRect = {};
		SdRect clipRect = {};
		SdPortalRoot portalRoot = SdPortalRoot::None;
		bool visible = true;
		bool hitTestVisible = true;
		bool blocksLowerInput = false;
		bool escapesParentClip = false;
	};

	struct SdStackingContextNode final
	{
		SdUInt32 id = 0;
		SdUInt32 parentId = 0;
		SdWidgetId ownerWidgetId = 0;
		SdUInt16 depth = 0;
		SdRootLayer rootLayer = SdRootLayer::Content;
		SdInt32 zIndex = 0;
		SdUInt32 activationOrder = 0;
		SdUInt32 treeOrder = 0;
	};

	struct SdPortalRecord final
	{
		SdWidgetId portalWidgetId = 0;
		SdWidgetId ownerWidgetId = 0;
		SdWidgetId anchorWidgetId = 0;
		SdPortalRoot root = SdPortalRoot::None;
		SdRootLayer rootLayer = SdRootLayer::Popup;
		SdRect anchorRect = {};
		bool closeOnOutsideClick = true;
		bool escapeParentClip = true;
	};

	struct SdHitTestRecord final
	{
		SdWidgetId widgetId = 0;
		SdRootLayer rootLayer = SdRootLayer::Content;
		SdRect rect = {};
		SdRect clipRect = {};
		SdUInt32 paintOrder = 0;
		bool inputEnabled = true;
		SdStackingKey key = {};
		bool visible = true;
		bool hitTestVisible = true;
		bool pointerEventsNone = false;
		bool blocksLowerInput = false;
		bool modalBoundary = false;
		SdPortalRoot portalRoot = SdPortalRoot::None;
	};

	struct SdLayerDrawRecord final
	{
		SdWidgetId widgetId = 0;
		SdRootLayer rootLayer = SdRootLayer::Content;
		SdRect clipRect = {};
		SdUInt32 paintOrder = 0;
		SdStackingKey key = {};
		SdRect bounds = {};
		bool visible = true;
		bool escapesParentClip = false;
		SdPortalRoot portalRoot = SdPortalRoot::None;
	};

	struct SdLayerDrawChannel final
	{
		SdRootLayer rootLayer = SdRootLayer::Content;
		SdUInt32 firstRecord = 0;
		SdUInt32 recordCount = 0;
	};

	class SdLayerSystem final
	{
	private:
		std::vector<SdLayerDrawRecord> drawRecords = {};
		std::vector<SdLayerDrawChannel> drawChannels = {};
		std::vector<SdHitTestRecord> hitTestRecords = {};
		std::vector<SdStackingContextNode> stackingContexts = {};
		std::vector<SdPortalRecord> portalRecords = {};
		bool finalized = true;

		static bool UsesDefaultStackingKey(const SdStackingKey& key) noexcept
		{
			return key == SdStackingKey{};
		}

		static SdLayerDrawRecord NormalizeDrawRecord(const SdLayerDrawRecord& record) noexcept
		{
			SdLayerDrawRecord result = record;
			if (UsesDefaultStackingKey(result.key))
				result.key = SdMakeStackingKey(result.rootLayer, 0, result.paintOrder, result.paintOrder);
			result.paintOrder = result.key.paintOrder;
			return result;
		}

		static SdHitTestRecord NormalizeHitTestRecord(const SdHitTestRecord& record) noexcept
		{
			SdHitTestRecord result = record;
			if (UsesDefaultStackingKey(result.key))
				result.key = SdMakeStackingKey(result.rootLayer, 0, result.paintOrder, result.paintOrder);
			result.paintOrder = result.key.paintOrder;
			if (result.pointerEventsNone)
			{
				result.hitTestVisible = false;
				result.blocksLowerInput = false;
			}
			result.hitTestVisible = result.hitTestVisible && result.inputEnabled;
			return result;
		}

		void BuildDrawChannels()
		{
			drawChannels.clear();
			for (SdUInt32 index = 0; index < static_cast<SdUInt32>(drawRecords.size()); ++index)
			{
				const SdLayerDrawRecord& record = drawRecords[index];
				const SdRootLayer rootLayer = static_cast<SdRootLayer>(record.key.rootLayer);
				if (drawChannels.empty() || drawChannels.back().rootLayer != rootLayer)
				{
					drawChannels.push_back({
						rootLayer,
						index,
						0
					});
				}
				++drawChannels.back().recordCount;
			}
		}

		static SdRect IntersectRect(const SdRect& left, const SdRect& right)
		{
			SdRect result = {
				std::max(left.min.x, right.min.x),
				std::max(left.min.y, right.min.y),
				std::min(left.max.x, right.max.x),
				std::min(left.max.y, right.max.y)
			};
			result.max.x = std::max(result.min.x, result.max.x);
			result.max.y = std::max(result.min.y, result.max.y);
			return result;
		}

	public:
		void BeginFrame()
		{
			drawRecords.clear();
			drawChannels.clear();
			hitTestRecords.clear();
			stackingContexts.clear();
			portalRecords.clear();
			finalized = false;
		}

		void AddStackingContext(const SdStackingContextNode& node)
		{
			stackingContexts.push_back(node);
		}

		void AddPortalRecord(const SdPortalRecord& record)
		{
			if (record.root == SdPortalRoot::None)
				return;
			portalRecords.push_back(record);
		}

		void AddDrawRecord(const SdLayerDrawRecord& record)
		{
			SdLayerDrawRecord normalized = NormalizeDrawRecord(record);
			if (!normalized.visible)
				return;
			drawRecords.push_back(normalized);
			finalized = false;
		}

		void AddHitTestRecord(const SdHitTestRecord& record)
		{
			SdHitTestRecord normalized = NormalizeHitTestRecord(record);
			if (!normalized.visible)
				return;
			if (!normalized.hitTestVisible && !normalized.blocksLowerInput)
				return;
			hitTestRecords.push_back(normalized);
			finalized = false;
		}

		void Finalize()
		{
			std::stable_sort(drawRecords.begin(), drawRecords.end(), [](const SdLayerDrawRecord& left, const SdLayerDrawRecord& right)
			{
				return SdLessStackingKey(left.key, right.key);
			});
			std::stable_sort(hitTestRecords.begin(), hitTestRecords.end(), [](const SdHitTestRecord& left, const SdHitTestRecord& right)
			{
				return SdLessStackingKey(left.key, right.key);
			});
			BuildDrawChannels();
			finalized = true;
		}

		SdWidgetId HitTest(const SdVec2& point) const
		{
			assert(finalized && "SdLayerSystem::HitTest called before Finalize().");
			const SdHitTestRecord* bestHit = nullptr;
			const SdHitTestRecord* bestBlocker = nullptr;
			for (const SdHitTestRecord& record : hitTestRecords)
			{
				if (!record.visible)
					continue;
				const SdRect clipped = IntersectRect(record.rect, record.clipRect);
				const bool insideClip = record.clipRect.Contains(point);
				const bool insideHit = clipped.Contains(point);
				if (record.hitTestVisible && insideHit)
				{
					if (!bestHit || SdLessStackingKey(bestHit->key, record.key))
						bestHit = &record;
				}
				if (!record.blocksLowerInput || !insideClip)
					continue;
				if (!bestBlocker || SdLessStackingKey(bestBlocker->key, record.key))
					bestBlocker = &record;
			}
			if (bestBlocker && (!bestHit || SdLessStackingKey(bestHit->key, bestBlocker->key)))
				return 0;
			return bestHit ? bestHit->widgetId : 0;
		}

		const std::vector<SdHitTestRecord>& GetHitTestRecords() const noexcept
		{
			return hitTestRecords;
		}

		const std::vector<SdLayerDrawRecord>& GetDrawRecords() const noexcept
		{
			return drawRecords;
		}

		const std::vector<SdLayerDrawChannel>& GetDrawChannels() const noexcept
		{
			return drawChannels;
		}

		const std::vector<SdStackingContextNode>& GetStackingContexts() const noexcept
		{
			return stackingContexts;
		}

		const std::vector<SdPortalRecord>& GetPortalRecords() const noexcept
		{
			return portalRecords;
		}
	};

	struct SdInteractionState final
	{
		SdWidgetId hoveredWidget = 0;
		SdWidgetId activeWidget = 0;
		SdWidgetId focusedWidget = 0;
		SdWidgetId capturedWidget = 0;
		SdWidgetId pressedWidget = 0;
		SdWidgetId releasedWidget = 0;
		SdWidgetId clickedWidget = 0;
		SdWidgetId outsideClickWidget = 0;
		SdWidgetId wheelWidget = 0;
		SdWidgetId keyboardWidget = 0;
		SdWidgetId focusScopeRootWidget = 0;
		SdWidgetId modalScopeRootWidget = 0;
		SdWidgetId dragSourceWidget = 0;
		SdWidgetId dragTargetWidget = 0;
		SdWidgetId dropTargetWidget = 0;
		SdMouseButton pressedButton = SdMouseButton::Left;
		bool clicked = false;
		bool outsideClick = false;
		bool dragActive = false;
		bool dragStarted = false;
		bool dragReleased = false;
	};

	enum class SdPointerCapturePolicy : SdUInt8
	{
		None,
		Press,
		Drag,
		Manual
	};

	enum class SdInteractionRoutePhase : SdUInt8
	{
		Tunnel,
		Target,
		Bubble
	};

	struct SdInteractionRouteNode final
	{
		SdWidgetId widgetId = 0;
		SdInteractionRoutePhase phase = SdInteractionRoutePhase::Target;
	};

	struct SdMouseButtonInteraction final
	{
		SdWidgetId activeWidget = 0;
		SdWidgetId capturedWidget = 0;
		SdWidgetId pressedWidget = 0;
		SdWidgetId releasedWidget = 0;
		SdWidgetId clickedWidget = 0;
		SdWidgetId doubleClickedWidget = 0;
		SdWidgetId longPressedWidget = 0;
		SdVec2 pressPosition = {};
		SdUInt64 pressFrame = 0;
		SdUInt64 lastClickFrame = 0;
		bool longPressFired = false;
	};

	class SdInteractionSystem final
	{
	private:
		SdInteractionState state = {};
		std::array<SdMouseButtonInteraction, static_cast<SdSize>(SdMouseButton::Count)> mouseButtons = {};
		std::vector<SdInteractionRouteNode> pointerRoute = {};
		SdUInt64 frameIndex = 0;
		SdPointerCapturePolicy capturePolicy = SdPointerCapturePolicy::Press;
		SdUInt64 doubleClickFrameThreshold = 20;
		SdUInt64 longPressFrameThreshold = 30;
		float dragThreshold = 4.0f;

		static float DistanceSquared(SdVec2 left, SdVec2 right) noexcept
		{
			const SdVec2 delta = left - right;
			return delta.x * delta.x + delta.y * delta.y;
		}

		static bool IsButtonIndexValid(SdMouseButton button) noexcept
		{
			return static_cast<SdSize>(button) < static_cast<SdSize>(SdMouseButton::Count);
		}

	public:
		void Update(const SdLayerSystem& layerSystem, const SdInputSnapshot& input, SdUInt64 newFrameIndex = 0)
		{
			frameIndex = newFrameIndex;
			state.clicked = false;
			state.outsideClick = false;
			state.dragStarted = false;
			state.dragReleased = false;
			state.pressedWidget = 0;
			state.releasedWidget = 0;
			state.clickedWidget = 0;
			state.outsideClickWidget = 0;
			state.wheelWidget = 0;
			state.keyboardWidget = state.focusedWidget;
			state.dropTargetWidget = 0;
			pointerRoute.clear();
			const SdWidgetId hitWidget = layerSystem.HitTest(input.GetMousePosition());
			state.hoveredWidget = state.capturedWidget != 0 && capturePolicy != SdPointerCapturePolicy::None ? state.capturedWidget : hitWidget;
			if (input.GetMouseWheelDelta().x != 0.0f || input.GetMouseWheelDelta().y != 0.0f)
				state.wheelWidget = hitWidget;

			for (SdSize index = 0; index < mouseButtons.size(); ++index)
			{
				const SdMouseButton button = static_cast<SdMouseButton>(index);
				SdMouseButtonInteraction& buttonState = mouseButtons[index];
				buttonState.pressedWidget = 0;
				buttonState.releasedWidget = 0;
				buttonState.clickedWidget = 0;
				buttonState.doubleClickedWidget = 0;
				buttonState.longPressedWidget = 0;

				if (input.IsMouseButtonDown(button))
				{
					buttonState.activeWidget = hitWidget;
					buttonState.pressedWidget = hitWidget;
					buttonState.pressPosition = input.GetMousePosition();
					buttonState.pressFrame = frameIndex;
					buttonState.longPressFired = false;
					if (hitWidget != 0 && capturePolicy == SdPointerCapturePolicy::Press)
						buttonState.capturedWidget = hitWidget;
					if (index == static_cast<SdSize>(SdMouseButton::Left))
					{
						state.activeWidget = hitWidget;
						state.focusedWidget = hitWidget != 0 ? hitWidget : state.focusedWidget;
						state.pressedWidget = hitWidget;
						state.pressedButton = button;
					}
				}

				const bool held = input.IsMouseButtonHeld(button);
				const bool released = input.IsMouseButtonUp(button);
				const bool dragDistanceReached = DistanceSquared(input.GetMousePosition(), buttonState.pressPosition) >= dragThreshold * dragThreshold;
				if (held && buttonState.activeWidget != 0)
				{
					if (!state.dragActive && dragDistanceReached)
					{
						state.dragActive = true;
						state.dragStarted = true;
						state.dragSourceWidget = buttonState.activeWidget;
						if (capturePolicy == SdPointerCapturePolicy::Drag)
							buttonState.capturedWidget = buttonState.activeWidget;
					}
					if (!buttonState.longPressFired && frameIndex > buttonState.pressFrame && frameIndex - buttonState.pressFrame >= longPressFrameThreshold)
					{
						buttonState.longPressFired = true;
						buttonState.longPressedWidget = buttonState.activeWidget;
					}
				}

				if (state.dragActive)
					state.dragTargetWidget = hitWidget;

				if (released)
				{
					buttonState.releasedWidget = buttonState.activeWidget;
					const bool clickedSameWidget = buttonState.activeWidget != 0 && buttonState.activeWidget == hitWidget && !state.dragActive;
					buttonState.clickedWidget = clickedSameWidget ? buttonState.activeWidget : 0;
					if (buttonState.clickedWidget != 0)
					{
						if (buttonState.lastClickFrame != 0 && frameIndex - buttonState.lastClickFrame <= doubleClickFrameThreshold)
							buttonState.doubleClickedWidget = buttonState.clickedWidget;
						buttonState.lastClickFrame = frameIndex;
					}
					else if (buttonState.activeWidget != 0 && hitWidget != buttonState.activeWidget)
					{
						state.outsideClick = true;
						state.outsideClickWidget = buttonState.activeWidget;
					}
					if (state.dragActive)
					{
						state.dragReleased = true;
						state.dropTargetWidget = hitWidget;
					}
					if (index == static_cast<SdSize>(SdMouseButton::Left))
					{
						state.releasedWidget = buttonState.releasedWidget;
						state.clickedWidget = buttonState.clickedWidget;
						state.clicked = state.clickedWidget != 0;
						state.activeWidget = 0;
					}
					buttonState.activeWidget = 0;
					buttonState.capturedWidget = 0;
					if (index == static_cast<SdSize>(SdMouseButton::Left))
					{
						state.capturedWidget = 0;
						state.dragActive = false;
					}
				}
			}

			const SdMouseButtonInteraction& leftButton = mouseButtons[static_cast<SdSize>(SdMouseButton::Left)];
			state.capturedWidget = leftButton.capturedWidget;
			BuildPointerRoute(hitWidget);
		}

		void BuildPointerRoute(SdWidgetId targetWidget)
		{
			if (targetWidget == 0)
				return;
			pointerRoute.push_back({ targetWidget, SdInteractionRoutePhase::Tunnel });
			pointerRoute.push_back({ targetWidget, SdInteractionRoutePhase::Target });
			pointerRoute.push_back({ targetWidget, SdInteractionRoutePhase::Bubble });
		}

		const SdInteractionState& GetState() const noexcept
		{
			return state;
		}

		const std::vector<SdInteractionRouteNode>& GetPointerRoute() const noexcept
		{
			return pointerRoute;
		}

		void SetPointerCapturePolicy(SdPointerCapturePolicy policy) noexcept
		{
			capturePolicy = policy;
		}

		SdPointerCapturePolicy GetPointerCapturePolicy() const noexcept
		{
			return capturePolicy;
		}

		void SetFocusScope(SdWidgetId rootWidgetId) noexcept
		{
			state.focusScopeRootWidget = rootWidgetId;
		}

		void SetModalScope(SdWidgetId rootWidgetId) noexcept
		{
			state.modalScopeRootWidget = rootWidgetId;
		}

		void ClearModalScope(SdWidgetId rootWidgetId) noexcept
		{
			if (state.modalScopeRootWidget == rootWidgetId)
				state.modalScopeRootWidget = 0;
		}

		void CapturePointer(SdWidgetId widgetId, SdMouseButton button = SdMouseButton::Left) noexcept
		{
			if (!IsButtonIndexValid(button))
				return;
			mouseButtons[static_cast<SdSize>(button)].capturedWidget = widgetId;
			if (button == SdMouseButton::Left)
				state.capturedWidget = widgetId;
		}

		void ReleasePointerCapture(SdWidgetId widgetId, SdMouseButton button = SdMouseButton::Left) noexcept
		{
			if (!IsButtonIndexValid(button))
				return;
			SdMouseButtonInteraction& buttonState = mouseButtons[static_cast<SdSize>(button)];
			if (buttonState.capturedWidget == widgetId)
				buttonState.capturedWidget = 0;
			if (button == SdMouseButton::Left && state.capturedWidget == widgetId)
				state.capturedWidget = 0;
		}

		bool IsHovered(SdWidgetId widgetId) const noexcept
		{
			return state.hoveredWidget == widgetId;
		}

		bool IsPressed(SdWidgetId widgetId) const noexcept
		{
			return state.activeWidget == widgetId || state.capturedWidget == widgetId;
		}

		bool IsPressed(SdWidgetId widgetId, SdMouseButton button) const noexcept
		{
			if (!IsButtonIndexValid(button))
				return false;
			const SdMouseButtonInteraction& buttonState = mouseButtons[static_cast<SdSize>(button)];
			return buttonState.activeWidget == widgetId || buttonState.capturedWidget == widgetId;
		}

		bool WasClicked(SdWidgetId widgetId) const noexcept
		{
			return state.clickedWidget == widgetId;
		}

		bool WasClicked(SdWidgetId widgetId, SdMouseButton button) const noexcept
		{
			return IsButtonIndexValid(button) && mouseButtons[static_cast<SdSize>(button)].clickedWidget == widgetId;
		}

		bool WasDoubleClicked(SdWidgetId widgetId, SdMouseButton button = SdMouseButton::Left) const noexcept
		{
			return IsButtonIndexValid(button) && mouseButtons[static_cast<SdSize>(button)].doubleClickedWidget == widgetId;
		}

		bool WasLongPressed(SdWidgetId widgetId, SdMouseButton button = SdMouseButton::Left) const noexcept
		{
			return IsButtonIndexValid(button) && mouseButtons[static_cast<SdSize>(button)].longPressedWidget == widgetId;
		}

		bool WasOutsideClicked(SdWidgetId widgetId) const noexcept
		{
			return state.outsideClickWidget == widgetId;
		}

		bool IsWheelTarget(SdWidgetId widgetId) const noexcept
		{
			return state.wheelWidget == widgetId;
		}

		bool IsKeyboardTarget(SdWidgetId widgetId) const noexcept
		{
			return state.keyboardWidget == widgetId;
		}

		bool IsDragSource(SdWidgetId widgetId) const noexcept
		{
			return state.dragSourceWidget == widgetId && (state.dragActive || state.dragStarted || state.dragReleased);
		}

		bool IsDragTarget(SdWidgetId widgetId) const noexcept
		{
			return state.dragTargetWidget == widgetId;
		}

		bool IsDropTarget(SdWidgetId widgetId) const noexcept
		{
			return state.dropTargetWidget == widgetId;
		}

		bool IsFocused(SdWidgetId widgetId) const noexcept
		{
			return state.focusedWidget == widgetId;
		}

		bool WasPressed(SdWidgetId widgetId) const noexcept
		{
			return state.pressedWidget == widgetId;
		}

		bool WasReleased(SdWidgetId widgetId) const noexcept
		{
			return state.releasedWidget == widgetId;
		}

		bool IsCaptured(SdWidgetId widgetId) const noexcept
		{
			return state.capturedWidget == widgetId;
		}
	};
}
