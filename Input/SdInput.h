#pragma once

#include "Core/SdCore.h"

#include <array>
#include <vector>
#include <variant>

namespace Sodium
{
	enum class SdInputDevice : SdUInt8
	{
		None,
		Mouse,
		Keyboard,
		Text,
		Gamepad,
		Display
	};

	enum class SdMouseButton : SdUInt8
	{
		Left,
		Right,
		Middle,
		X1,
		X2,
		Count
	};

	enum class SdGamepadButton : SdUInt8
	{
		FaceDown,
		FaceRight,
		FaceLeft,
		FaceUp,
		LeftBumper,
		RightBumper,
		Back,
		Start,
		LeftThumb,
		RightThumb,
		DPadUp,
		DPadDown,
		DPadLeft,
		DPadRight,
		Count
	};

	enum class SdGamepadAxis : SdUInt8
	{
		LeftStick,
		RightStick,
		LeftTrigger,
		RightTrigger
	};

	enum class SdKeyCode : SdUInt16
	{
		Esc, F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, PrintScreen, ScrollLock, PauseBreak,
		Tilde, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9, Num0, Minus, Plus, Backspace, Insert, Home, PageUp, NumPadLock, NumPadSlash, NumPadStar, NumPadMinus,
		Tab, Q, W, E, R, T, Y, U, I, O, P, LeftBracket, RightBracket, Backslash, Delete, End, PageDown, NumPad7, NumPad8, NumPad9, NumPadPlus,
		CapsLock, A, S, D, F, G, H, J, K, L, Semicolon, Quote, Enter, NumPad4, NumPad5, NumPad6,
		LeftShift, Z, X, C, V, B, N, M, Comma, Period, Slash, RightShift, UpArrow, NumPad1, NumPad2, NumPad3, NumPadEnter,
		LeftCtrl, LeftWin, LeftAlt, Space, RightAlt, RightWin, Menu, RightCtrl, LeftArrow, DownArrow, RightArrow, NumPad0, NumPadPeriod,
		Count
	};

	enum class SdModifierMask : SdUInt8
	{
		None = 0,
		Ctrl = 1 << 0,
		Shift = 1 << 1,
		Alt = 1 << 2,
		Super = 1 << 3
	};

	inline constexpr SdModifierMask operator|(SdModifierMask left, SdModifierMask right)
	{
		return static_cast<SdModifierMask>(static_cast<SdUInt8>(left) | static_cast<SdUInt8>(right));
	}

	inline constexpr SdModifierMask operator&(SdModifierMask left, SdModifierMask right)
	{
		return static_cast<SdModifierMask>(static_cast<SdUInt8>(left) & static_cast<SdUInt8>(right));
	}

	inline constexpr SdModifierMask& operator|=(SdModifierMask& left, SdModifierMask right)
	{
		left = left | right;
		return left;
	}

	struct SdKeyChord final
	{
		SdModifierMask modifiers = SdModifierMask::None;
		SdKeyCode triggerKey = SdKeyCode::Esc;
	};

	inline constexpr SdDuration SdInputDefaultPressInterval = std::chrono::milliseconds(100);

	struct SdButtonState final
	{
		bool isDown = false;
		bool wentDown = false;
		bool wentUp = false;
		SdUInt16 repeatCountThisFrame = 0;
		SdUInt64 lastTransitionFrame = 0;
		SdTimePoint lastTransitionTime = {};
	};

	struct SdMouseState final
	{
		SdVec2 position = {};
		SdVec2 delta = {};
		SdVec2 wheelDelta = {};
		std::array<SdButtonState, static_cast<SdSize>(SdMouseButton::Count)> buttons = {};
	};

	struct SdKeyboardState final
	{
		std::array<SdButtonState, static_cast<SdSize>(SdKeyCode::Count)> keys = {};
	};

	struct SdTextInputTarget final
	{
		SdTextInputTargetId id = 0;
		SdRect caretRect = {};
		float lineHeight = 0.0f;
		bool active = false;
	};

	struct SdTextInputState final
	{
		SdUtf8String committedTextThisFrame = {};
		SdUtf8String compositionText = {};
		SdUInt32 compositionCursorByteOffset = 0;
		bool composing = false;
		bool imeEnabled = false;
		bool candidateWindowVisible = false;
		SdRect candidateAnchorRect = {};
		SdTextInputTargetId activeTarget = 0;
	};

	struct SdDisplayState final
	{
		SdVec2 position = {};
		SdVec2 size = {};
		bool focused = false;
	};

	struct SdGamepadState final
	{
		bool connected = false;
		std::array<SdButtonState, static_cast<SdSize>(SdGamepadButton::Count)> buttons = {};
		SdVec2 leftStick = {};
		SdVec2 rightStick = {};
		float leftTrigger = 0.0f;
		float rightTrigger = 0.0f;
		SdUInt32 lastPacket = 0;
		SdUInt64 lastActiveFrame = 0;
	};

	struct SdInputSnapshot final
	{
		bool quitRequested = false;
		SdTimePoint currentTime = {};
		SdTimePoint previousTime = {};
		SdInputDevice lastInputDevice = SdInputDevice::None;
		SdDisplayState display = {};
		SdMouseState mouse = {};
		SdKeyboardState keyboard = {};
		SdTextInputState text = {};
		std::array<SdGamepadState, 4> gamepads = {};
		SdUInt32 droppedEventCount = 0;

		bool IsMouseButtonDown(SdMouseButton button) const;
		bool IsMouseButtonHeld(SdMouseButton button) const;
		bool IsMouseButtonPressed(SdMouseButton button, SdDuration interval = SdInputDefaultPressInterval) const;
		bool IsMouseButtonUp(SdMouseButton button) const;
		SdVec2 GetMousePosition() const noexcept { return mouse.position; }
		SdVec2 GetMouseDelta() const noexcept { return mouse.delta; }
		SdVec2 GetMouseWheelDelta() const noexcept { return mouse.wheelDelta; }

		bool IsKeyDown(SdKeyCode keyCode) const;
		bool IsKeyHeld(SdKeyCode keyCode) const;
		bool IsKeyPressed(SdKeyCode keyCode, SdDuration interval = SdInputDefaultPressInterval) const;
		bool IsKeyUp(SdKeyCode keyCode) const;
		SdModifierMask GetModifierMask() const;
		bool IsChordPressed(SdModifierMask modifiers, SdKeyCode triggerKey, SdDuration interval = SdInputDefaultPressInterval) const;
		bool IsChordPressed(const SdKeyChord& chord, SdDuration interval = SdInputDefaultPressInterval) const { return IsChordPressed(chord.modifiers, chord.triggerKey, interval); }

		SdUtf8StringView GetCommittedText() const noexcept { return text.committedTextThisFrame; }
		SdUtf8StringView GetCompositionText() const noexcept { return text.compositionText; }
		bool IsComposing() const noexcept { return text.composing; }
		SdUInt32 GetCompositionCursorByteOffset() const noexcept { return text.compositionCursorByteOffset; }

		bool IsGamepadConnected(SdUInt32 index) const;
		bool IsGamepadButtonDown(SdUInt32 index, SdGamepadButton button) const;
		bool IsGamepadButtonHeld(SdUInt32 index, SdGamepadButton button) const;
		bool IsGamepadButtonPressed(SdUInt32 index, SdGamepadButton button, SdDuration interval = SdInputDefaultPressInterval) const;
		bool IsGamepadButtonUp(SdUInt32 index, SdGamepadButton button) const;
		SdVec2 GetGamepadLeftStick(SdUInt32 index) const;
		SdVec2 GetGamepadRightStick(SdUInt32 index) const;
		float GetGamepadLeftTrigger(SdUInt32 index) const;
		float GetGamepadRightTrigger(SdUInt32 index) const;
		SdInt32 GetLastActiveGamepadIndex() const;
	};

	enum class SdInputEventType : SdUInt8
	{
		MouseMove,
		MouseButton,
		MouseWheel,
		Key,
		TextCommit,
		TextCompositionStart,
		TextCompositionUpdate,
		TextCompositionEnd,
		WindowFocus,
		WindowResize,
		WindowMove,
		GamepadConnection,
		GamepadButton,
		GamepadAxis,
		QuitRequest
	};

	struct SdMouseMoveEvent final
	{
		SdVec2 position = {};
	};

	struct SdMouseButtonEvent final
	{
		SdMouseButton button = SdMouseButton::Left;
		bool isDown = false;
	};

	struct SdMouseWheelEvent final
	{
		SdVec2 delta = {};
	};

	struct SdKeyEvent final
	{
		SdKeyCode keyCode = SdKeyCode::Esc;
		bool isDown = false;
		bool isRepeat = false;
	};

	struct SdTextCommitEvent final
	{
		SdUtf8String text = {};
	};

	struct SdTextCompositionEvent final
	{
		SdUtf8String text = {};
		SdUInt32 cursorByteOffset = 0;
		bool candidateVisible = false;
	};

	struct SdWindowFocusEvent final
	{
		bool focused = false;
	};

	struct SdWindowResizeEvent final
	{
		SdVec2 size = {};
	};

	struct SdWindowMoveEvent final
	{
		SdVec2 position = {};
	};

	struct SdGamepadConnectionEvent final
	{
		SdUInt32 index = 0;
		bool connected = false;
	};

	struct SdGamepadButtonEvent final
	{
		SdUInt32 index = 0;
		SdGamepadButton button = SdGamepadButton::FaceDown;
		bool isDown = false;
	};

	struct SdGamepadAxisEvent final
	{
		SdUInt32 index = 0;
		SdGamepadAxis axis = SdGamepadAxis::LeftStick;
		SdVec2 stickValue = {};
		float triggerValue = 0.0f;
	};

	struct SdQuitRequestEvent final
	{
	};

	using SdInputEventPayload = std::variant<
		SdMouseMoveEvent,
		SdMouseButtonEvent,
		SdMouseWheelEvent,
		SdKeyEvent,
		SdTextCommitEvent,
		SdTextCompositionEvent,
		SdWindowFocusEvent,
		SdWindowResizeEvent,
		SdWindowMoveEvent,
		SdGamepadConnectionEvent,
		SdGamepadButtonEvent,
		SdGamepadAxisEvent,
		SdQuitRequestEvent>;

	struct SdInputEvent final
	{
		SdInputEventType type = SdInputEventType::MouseMove;
		SdInputDevice device = SdInputDevice::None;
		SdInputEventPayload payload = SdMouseMoveEvent{};
	};

	class SdInputBuffer final
	{
	private:
		SdSize capacity = 0;
		std::vector<SdInputEvent> events = {};
		SdUInt32 droppedEventCount = 0;
	public:
		explicit SdInputBuffer(SdSize bufferCapacity = 512);

		void Reset();
		bool Push(const SdInputEvent& event);
		bool Push(SdInputEvent&& event);

		std::span<const SdInputEvent> GetEvents() const noexcept { return events; }
		SdUInt32 GetDroppedEventCount() const noexcept { return droppedEventCount; }
	};

	class SdInputSystem final
	{
	private:
		SdInputBuffer pendingEvents{ 512 };
		SdInputSnapshot snapshot = {};
		SdTextInputTarget textInputTarget = {};
		SdUInt64 frameIndex = 0;
	public:
		explicit SdInputSystem(SdSize eventCapacity = 512);

		void BeginFrame(SdUInt64 newFrameIndex);
		void BeginFrame(SdUInt64 newFrameIndex, SdTimePoint newFrameTime);
		void FinalizeFrame();

		bool PushEvent(const SdInputEvent& event);
		bool PushEvent(SdInputEvent&& event);

		const SdInputSnapshot& GetSnapshot() const noexcept { return snapshot; }
		SdInputSnapshot& GetMutableSnapshot() noexcept { return snapshot; }

		bool IsMouseButtonDown(SdMouseButton button) const { return snapshot.IsMouseButtonDown(button); }
		bool IsMouseButtonHeld(SdMouseButton button) const { return snapshot.IsMouseButtonHeld(button); }
		bool IsMouseButtonPressed(SdMouseButton button, SdDuration interval = SdInputDefaultPressInterval) const { return snapshot.IsMouseButtonPressed(button, interval); }
		bool IsMouseButtonUp(SdMouseButton button) const { return snapshot.IsMouseButtonUp(button); }
		bool IsKeyDown(SdKeyCode keyCode) const { return snapshot.IsKeyDown(keyCode); }
		bool IsKeyHeld(SdKeyCode keyCode) const { return snapshot.IsKeyHeld(keyCode); }
		bool IsKeyPressed(SdKeyCode keyCode, SdDuration interval = SdInputDefaultPressInterval) const { return snapshot.IsKeyPressed(keyCode, interval); }
		bool IsKeyUp(SdKeyCode keyCode) const { return snapshot.IsKeyUp(keyCode); }
		bool IsChordPressed(SdModifierMask modifiers, SdKeyCode triggerKey, SdDuration interval = SdInputDefaultPressInterval) const { return snapshot.IsChordPressed(modifiers, triggerKey, interval); }
		bool IsChordPressed(const SdKeyChord& chord, SdDuration interval = SdInputDefaultPressInterval) const { return snapshot.IsChordPressed(chord, interval); }
		SdModifierMask GetModifierMask() const { return snapshot.GetModifierMask(); }
		SdUtf8StringView GetCommittedText() const noexcept { return snapshot.GetCommittedText(); }
		SdUtf8StringView GetCompositionText() const noexcept { return snapshot.GetCompositionText(); }
		bool IsComposing() const noexcept { return snapshot.IsComposing(); }
		SdUInt32 GetCompositionCursorByteOffset() const noexcept { return snapshot.GetCompositionCursorByteOffset(); }
		bool IsGamepadConnected(SdUInt32 index) const { return snapshot.IsGamepadConnected(index); }
		bool IsGamepadButtonDown(SdUInt32 index, SdGamepadButton button) const { return snapshot.IsGamepadButtonDown(index, button); }
		bool IsGamepadButtonHeld(SdUInt32 index, SdGamepadButton button) const { return snapshot.IsGamepadButtonHeld(index, button); }
		bool IsGamepadButtonPressed(SdUInt32 index, SdGamepadButton button, SdDuration interval = SdInputDefaultPressInterval) const { return snapshot.IsGamepadButtonPressed(index, button, interval); }
		bool IsGamepadButtonUp(SdUInt32 index, SdGamepadButton button) const { return snapshot.IsGamepadButtonUp(index, button); }
		SdVec2 GetGamepadLeftStick(SdUInt32 index) const { return snapshot.GetGamepadLeftStick(index); }
		SdVec2 GetGamepadRightStick(SdUInt32 index) const { return snapshot.GetGamepadRightStick(index); }
		float GetGamepadLeftTrigger(SdUInt32 index) const { return snapshot.GetGamepadLeftTrigger(index); }
		float GetGamepadRightTrigger(SdUInt32 index) const { return snapshot.GetGamepadRightTrigger(index); }
		SdInt32 GetLastActiveGamepadIndex() const { return snapshot.GetLastActiveGamepadIndex(); }

		void SetTextInputTarget(SdTextInputTargetId targetId, const SdRect& caretRect, float lineHeight);
		void ActivateTextInput(SdTextInputTargetId targetId);
		void DeactivateTextInput(SdTextInputTargetId targetId);
		void ClearTextInputTarget(SdTextInputTargetId targetId);
		const SdTextInputTarget* GetActiveTextInputTarget() const noexcept;
	private:
		void ResetFrameTransientState();
		void HandleEvent(const SdInputEvent& event);
		void HandleMouseButtonEvent(const SdMouseButtonEvent& event);
		void HandleKeyEvent(const SdKeyEvent& event);
		void HandleGamepadButtonEvent(const SdGamepadButtonEvent& event);
		void HandleGamepadAxisEvent(const SdGamepadAxisEvent& event);
	};
}
