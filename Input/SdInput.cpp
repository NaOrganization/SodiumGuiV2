#include "Input/SdInput.h"

namespace Sodium
{
	namespace
	{
		template<typename T, SdSize N>
		T& GetArrayEntry(std::array<T, N>& entries, SdSize index)
		{
			return entries[index];
		}

		template<typename T, SdSize N>
		const T& GetArrayEntry(const std::array<T, N>& entries, SdSize index)
		{
			return entries[index];
		}

		bool HasModifier(SdModifierMask mask, SdModifierMask bit)
		{
			return (mask & bit) == bit;
		}

		bool IsButtonPressed(const SdButtonState& button, SdTimePoint currentTime, SdTimePoint previousTime, SdDuration interval)
		{
			if (button.wentDown)
				return true;
			if (!button.isDown || interval <= SdDuration::zero() || currentTime <= button.lastTransitionTime)
				return false;

			const SdDuration currentElapsed = currentTime - button.lastTransitionTime;
			if (currentElapsed < interval)
				return false;

			SdDuration previousElapsed = {};
			if (previousTime > button.lastTransitionTime)
				previousElapsed = previousTime - button.lastTransitionTime;
			if (previousElapsed > currentElapsed)
				previousElapsed = currentElapsed;

			return currentElapsed / interval > previousElapsed / interval;
		}
	}

	bool SdInputSnapshot::IsMouseButtonDown(SdMouseButton button) const
	{
		return GetArrayEntry(mouse.buttons, static_cast<SdSize>(button)).wentDown;
	}

	bool SdInputSnapshot::IsMouseButtonHeld(SdMouseButton button) const
	{
		return GetArrayEntry(mouse.buttons, static_cast<SdSize>(button)).isDown;
	}

	bool SdInputSnapshot::IsMouseButtonPressed(SdMouseButton button, SdDuration interval) const
	{
		return IsButtonPressed(GetArrayEntry(mouse.buttons, static_cast<SdSize>(button)), currentTime, previousTime, interval);
	}

	bool SdInputSnapshot::IsMouseButtonUp(SdMouseButton button) const
	{
		return GetArrayEntry(mouse.buttons, static_cast<SdSize>(button)).wentUp;
	}

	bool SdInputSnapshot::IsKeyDown(SdKeyCode keyCode) const
	{
		return GetArrayEntry(keyboard.keys, static_cast<SdSize>(keyCode)).wentDown;
	}

	bool SdInputSnapshot::IsKeyHeld(SdKeyCode keyCode) const
	{
		return GetArrayEntry(keyboard.keys, static_cast<SdSize>(keyCode)).isDown;
	}

	bool SdInputSnapshot::IsKeyPressed(SdKeyCode keyCode, SdDuration interval) const
	{
		return IsButtonPressed(GetArrayEntry(keyboard.keys, static_cast<SdSize>(keyCode)), currentTime, previousTime, interval);
	}

	bool SdInputSnapshot::IsKeyUp(SdKeyCode keyCode) const
	{
		return GetArrayEntry(keyboard.keys, static_cast<SdSize>(keyCode)).wentUp;
	}

	SdModifierMask SdInputSnapshot::GetModifierMask() const
	{
		SdModifierMask mask = SdModifierMask::None;
		if (IsKeyHeld(SdKeyCode::LeftCtrl) || IsKeyHeld(SdKeyCode::RightCtrl))
			mask |= SdModifierMask::Ctrl;
		if (IsKeyHeld(SdKeyCode::LeftShift) || IsKeyHeld(SdKeyCode::RightShift))
			mask |= SdModifierMask::Shift;
		if (IsKeyHeld(SdKeyCode::LeftAlt) || IsKeyHeld(SdKeyCode::RightAlt))
			mask |= SdModifierMask::Alt;
		if (IsKeyHeld(SdKeyCode::LeftWin) || IsKeyHeld(SdKeyCode::RightWin))
			mask |= SdModifierMask::Super;
		return mask;
	}

	bool SdInputSnapshot::IsChordPressed(SdModifierMask modifiers, SdKeyCode triggerKey, SdDuration interval) const
	{
		if (!IsKeyPressed(triggerKey, interval))
			return false;

		const SdModifierMask currentMask = GetModifierMask();
		if (HasModifier(modifiers, SdModifierMask::Ctrl) && !HasModifier(currentMask, SdModifierMask::Ctrl))
			return false;
		if (HasModifier(modifiers, SdModifierMask::Shift) && !HasModifier(currentMask, SdModifierMask::Shift))
			return false;
		if (HasModifier(modifiers, SdModifierMask::Alt) && !HasModifier(currentMask, SdModifierMask::Alt))
			return false;
		if (HasModifier(modifiers, SdModifierMask::Super) && !HasModifier(currentMask, SdModifierMask::Super))
			return false;
		return true;
	}

	bool SdInputSnapshot::IsGamepadConnected(SdUInt32 index) const
	{
		return index < gamepads.size() && gamepads[index].connected;
	}

	bool SdInputSnapshot::IsGamepadButtonDown(SdUInt32 index, SdGamepadButton button) const
	{
		return IsGamepadConnected(index) && GetArrayEntry(gamepads[index].buttons, static_cast<SdSize>(button)).wentDown;
	}

	bool SdInputSnapshot::IsGamepadButtonHeld(SdUInt32 index, SdGamepadButton button) const
	{
		return IsGamepadConnected(index) && GetArrayEntry(gamepads[index].buttons, static_cast<SdSize>(button)).isDown;
	}

	bool SdInputSnapshot::IsGamepadButtonPressed(SdUInt32 index, SdGamepadButton button, SdDuration interval) const
	{
		return IsGamepadConnected(index) && IsButtonPressed(GetArrayEntry(gamepads[index].buttons, static_cast<SdSize>(button)), currentTime, previousTime, interval);
	}

	bool SdInputSnapshot::IsGamepadButtonUp(SdUInt32 index, SdGamepadButton button) const
	{
		return IsGamepadConnected(index) && GetArrayEntry(gamepads[index].buttons, static_cast<SdSize>(button)).wentUp;
	}

	SdVec2 SdInputSnapshot::GetGamepadLeftStick(SdUInt32 index) const
	{
		return IsGamepadConnected(index) ? gamepads[index].leftStick : SdVec2();
	}

	SdVec2 SdInputSnapshot::GetGamepadRightStick(SdUInt32 index) const
	{
		return IsGamepadConnected(index) ? gamepads[index].rightStick : SdVec2();
	}

	float SdInputSnapshot::GetGamepadLeftTrigger(SdUInt32 index) const
	{
		return IsGamepadConnected(index) ? gamepads[index].leftTrigger : 0.0f;
	}

	float SdInputSnapshot::GetGamepadRightTrigger(SdUInt32 index) const
	{
		return IsGamepadConnected(index) ? gamepads[index].rightTrigger : 0.0f;
	}

	SdInt32 SdInputSnapshot::GetLastActiveGamepadIndex() const
	{
		SdInt32 bestIndex = -1;
		SdUInt64 bestFrame = 0;
		for (SdUInt32 index = 0; index < static_cast<SdUInt32>(gamepads.size()); ++index)
		{
			if (!gamepads[index].connected)
				continue;
			if (gamepads[index].lastActiveFrame >= bestFrame)
			{
				bestFrame = gamepads[index].lastActiveFrame;
				bestIndex = static_cast<SdInt32>(index);
			}
		}
		return bestIndex;
	}

	SdInputBuffer::SdInputBuffer(SdSize bufferCapacity)
		: capacity(bufferCapacity)
	{
		events.reserve(capacity);
	}

	void SdInputBuffer::Reset()
	{
		events.clear();
		droppedEventCount = 0;
	}

	bool SdInputBuffer::Push(const SdInputEvent& event)
	{
		if (events.size() >= capacity)
		{
			++droppedEventCount;
			return false;
		}
		events.push_back(event);
		return true;
	}

	bool SdInputBuffer::Push(SdInputEvent&& event)
	{
		if (events.size() >= capacity)
		{
			++droppedEventCount;
			return false;
		}
		events.push_back(std::move(event));
		return true;
	}

	SdInputSystem::SdInputSystem(SdSize eventCapacity)
		: pendingEvents(eventCapacity)
	{
		snapshot.text.committedTextThisFrame.reserve(64);
		snapshot.text.compositionText.reserve(64);
	}

	void SdInputSystem::BeginFrame(SdUInt64 newFrameIndex)
	{
		BeginFrame(newFrameIndex, SdTimePoint(SdDuration(newFrameIndex)));
	}

	void SdInputSystem::BeginFrame(SdUInt64 newFrameIndex, SdTimePoint newFrameTime)
	{
		frameIndex = newFrameIndex;
		snapshot.previousTime = snapshot.currentTime;
		snapshot.currentTime = newFrameTime;
		ResetFrameTransientState();
	}

	void SdInputSystem::FinalizeFrame()
	{
		for (const SdInputEvent& event : pendingEvents.GetEvents())
			HandleEvent(event);
		snapshot.droppedEventCount = pendingEvents.GetDroppedEventCount();
		pendingEvents.Reset();
	}

	bool SdInputSystem::PushEvent(const SdInputEvent& event)
	{
		return pendingEvents.Push(event);
	}

	bool SdInputSystem::PushEvent(SdInputEvent&& event)
	{
		return pendingEvents.Push(std::move(event));
	}

	void SdInputSystem::SetTextInputTarget(SdTextInputTargetId targetId, const SdRect& caretRect, float lineHeight)
	{
		textInputTarget.id = targetId;
		textInputTarget.caretRect = caretRect;
		textInputTarget.lineHeight = lineHeight;
		if (snapshot.text.activeTarget == targetId)
			snapshot.text.candidateAnchorRect = caretRect;
	}

	void SdInputSystem::ActivateTextInput(SdTextInputTargetId targetId)
	{
		if (textInputTarget.id == targetId)
		{
			textInputTarget.active = true;
			snapshot.text.activeTarget = targetId;
			snapshot.text.candidateAnchorRect = textInputTarget.caretRect;
		}
	}

	void SdInputSystem::DeactivateTextInput(SdTextInputTargetId targetId)
	{
		if (snapshot.text.activeTarget == targetId)
		{
			snapshot.text.activeTarget = 0;
			snapshot.text.candidateAnchorRect = {};
		}
		if (textInputTarget.id == targetId)
			textInputTarget.active = false;
	}

	void SdInputSystem::ClearTextInputTarget(SdTextInputTargetId targetId)
	{
		if (textInputTarget.id != targetId)
			return;
		textInputTarget = {};
		if (snapshot.text.activeTarget == targetId)
		{
			snapshot.text.activeTarget = 0;
			snapshot.text.candidateAnchorRect = {};
		}
	}

	const SdTextInputTarget* SdInputSystem::GetActiveTextInputTarget() const noexcept
	{
		if (textInputTarget.active && textInputTarget.id == snapshot.text.activeTarget)
			return &textInputTarget;
		return nullptr;
	}

	void SdInputSystem::ResetFrameTransientState()
	{
		snapshot.mouse.delta = {};
		snapshot.mouse.wheelDelta = {};
		for (SdButtonState& button : snapshot.mouse.buttons)
		{
			button.wentDown = false;
			button.wentUp = false;
			button.repeatCountThisFrame = 0;
		}
		for (SdButtonState& key : snapshot.keyboard.keys)
		{
			key.wentDown = false;
			key.wentUp = false;
			key.repeatCountThisFrame = 0;
		}
		for (SdGamepadState& gamepad : snapshot.gamepads)
		{
			for (SdButtonState& button : gamepad.buttons)
			{
				button.wentDown = false;
				button.wentUp = false;
				button.repeatCountThisFrame = 0;
			}
		}
		snapshot.text.committedTextThisFrame.clear();
		snapshot.droppedEventCount = 0;
	}

	void SdInputSystem::HandleEvent(const SdInputEvent& event)
	{
		switch (event.type)
		{
		case SdInputEventType::MouseMove:
		{
			const SdMouseMoveEvent& move = std::get<SdMouseMoveEvent>(event.payload);
			snapshot.mouse.delta += move.position - snapshot.mouse.position;
			snapshot.mouse.position = move.position;
			snapshot.lastInputDevice = SdInputDevice::Mouse;
			return;
		}
		case SdInputEventType::MouseButton:
			HandleMouseButtonEvent(std::get<SdMouseButtonEvent>(event.payload));
			snapshot.lastInputDevice = SdInputDevice::Mouse;
			return;
		case SdInputEventType::MouseWheel:
			snapshot.mouse.wheelDelta += std::get<SdMouseWheelEvent>(event.payload).delta;
			snapshot.lastInputDevice = SdInputDevice::Mouse;
			return;
		case SdInputEventType::Key:
			HandleKeyEvent(std::get<SdKeyEvent>(event.payload));
			snapshot.lastInputDevice = SdInputDevice::Keyboard;
			return;
		case SdInputEventType::TextCommit:
			snapshot.text.committedTextThisFrame += std::get<SdTextCommitEvent>(event.payload).text;
			snapshot.lastInputDevice = SdInputDevice::Text;
			return;
		case SdInputEventType::TextCompositionStart:
			snapshot.text.composing = true;
			snapshot.text.compositionText.clear();
			snapshot.text.compositionCursorByteOffset = 0;
			snapshot.text.candidateWindowVisible = true;
			return;
		case SdInputEventType::TextCompositionUpdate:
		{
			const SdTextCompositionEvent& composition = std::get<SdTextCompositionEvent>(event.payload);
			snapshot.text.composing = true;
			snapshot.text.compositionText = composition.text;
			snapshot.text.compositionCursorByteOffset = composition.cursorByteOffset;
			snapshot.text.candidateWindowVisible = composition.candidateVisible;
			snapshot.lastInputDevice = SdInputDevice::Text;
			return;
		}
		case SdInputEventType::TextCompositionEnd:
			snapshot.text.composing = false;
			snapshot.text.compositionText.clear();
			snapshot.text.compositionCursorByteOffset = 0;
			snapshot.text.candidateWindowVisible = false;
			return;
		case SdInputEventType::WindowFocus:
			snapshot.display.focused = std::get<SdWindowFocusEvent>(event.payload).focused;
			if (!snapshot.display.focused)
			{
				for (SdButtonState& button : snapshot.mouse.buttons)
					button = {};
				for (SdButtonState& key : snapshot.keyboard.keys)
					key = {};
				for (SdGamepadState& gamepad : snapshot.gamepads)
				{
					for (SdButtonState& button : gamepad.buttons)
						button = {};
				}
				snapshot.text.composing = false;
				snapshot.text.compositionText.clear();
				snapshot.text.compositionCursorByteOffset = 0;
				snapshot.text.candidateWindowVisible = false;
			}
			return;
		case SdInputEventType::WindowResize:
			snapshot.display.size = std::get<SdWindowResizeEvent>(event.payload).size;
			return;
		case SdInputEventType::WindowMove:
			snapshot.display.position = std::get<SdWindowMoveEvent>(event.payload).position;
			return;
		case SdInputEventType::GamepadConnection:
		{
			const SdGamepadConnectionEvent& connection = std::get<SdGamepadConnectionEvent>(event.payload);
			if (connection.index >= snapshot.gamepads.size())
				return;
			SdGamepadState& gamepad = snapshot.gamepads[connection.index];
			gamepad.connected = connection.connected;
			if (!connection.connected)
			{
				gamepad.leftStick = {};
				gamepad.rightStick = {};
				gamepad.leftTrigger = 0.0f;
				gamepad.rightTrigger = 0.0f;
				for (SdButtonState& button : gamepad.buttons)
					button = {};
			}
			return;
		}
		case SdInputEventType::GamepadButton:
			HandleGamepadButtonEvent(std::get<SdGamepadButtonEvent>(event.payload));
			snapshot.lastInputDevice = SdInputDevice::Gamepad;
			return;
		case SdInputEventType::GamepadAxis:
			HandleGamepadAxisEvent(std::get<SdGamepadAxisEvent>(event.payload));
			snapshot.lastInputDevice = SdInputDevice::Gamepad;
			return;
		case SdInputEventType::QuitRequest:
			snapshot.quitRequested = true;
			return;
		default:
			return;
		}
	}

	void SdInputSystem::HandleMouseButtonEvent(const SdMouseButtonEvent& event)
	{
		SdButtonState& button = snapshot.mouse.buttons[static_cast<SdSize>(event.button)];
		if (event.isDown)
		{
			if (!button.isDown)
			{
				button.isDown = true;
				button.wentDown = true;
				button.lastTransitionFrame = frameIndex;
				button.lastTransitionTime = snapshot.currentTime;
			}
		}
		else if (button.isDown)
		{
			button.isDown = false;
			button.wentUp = true;
			button.lastTransitionFrame = frameIndex;
			button.lastTransitionTime = snapshot.currentTime;
		}
	}

	void SdInputSystem::HandleKeyEvent(const SdKeyEvent& event)
	{
		SdButtonState& key = snapshot.keyboard.keys[static_cast<SdSize>(event.keyCode)];
		if (event.isDown)
		{
			if (!key.isDown)
			{
				key.isDown = true;
				key.wentDown = true;
				key.lastTransitionFrame = frameIndex;
				key.lastTransitionTime = snapshot.currentTime;
			}
			else
			{
				++key.repeatCountThisFrame;
			}
		}
		else if (key.isDown)
		{
			key.isDown = false;
			key.wentUp = true;
			key.lastTransitionFrame = frameIndex;
			key.lastTransitionTime = snapshot.currentTime;
		}
	}

	void SdInputSystem::HandleGamepadButtonEvent(const SdGamepadButtonEvent& event)
	{
		if (event.index >= snapshot.gamepads.size())
			return;
		SdGamepadState& gamepad = snapshot.gamepads[event.index];
		if (!gamepad.connected)
			return;

		SdButtonState& button = gamepad.buttons[static_cast<SdSize>(event.button)];
		if (event.isDown)
		{
			if (!button.isDown)
			{
				button.isDown = true;
				button.wentDown = true;
				button.lastTransitionFrame = frameIndex;
				button.lastTransitionTime = snapshot.currentTime;
				gamepad.lastActiveFrame = frameIndex;
			}
		}
		else if (button.isDown)
		{
			button.isDown = false;
			button.wentUp = true;
			button.lastTransitionFrame = frameIndex;
			button.lastTransitionTime = snapshot.currentTime;
			gamepad.lastActiveFrame = frameIndex;
		}
	}

	void SdInputSystem::HandleGamepadAxisEvent(const SdGamepadAxisEvent& event)
	{
		if (event.index >= snapshot.gamepads.size())
			return;
		SdGamepadState& gamepad = snapshot.gamepads[event.index];
		if (!gamepad.connected)
			return;

		switch (event.axis)
		{
		case SdGamepadAxis::LeftStick:
			gamepad.leftStick = event.stickValue;
			break;
		case SdGamepadAxis::RightStick:
			gamepad.rightStick = event.stickValue;
			break;
		case SdGamepadAxis::LeftTrigger:
			gamepad.leftTrigger = event.triggerValue;
			break;
		case SdGamepadAxis::RightTrigger:
			gamepad.rightTrigger = event.triggerValue;
			break;
		}
		gamepad.lastActiveFrame = frameIndex;
	}
}
