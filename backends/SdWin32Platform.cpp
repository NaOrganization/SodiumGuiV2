#include "SdWin32Platform.h"

#include <WindowsX.h>
#include <imm.h>
#include <xinput.h>

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <cstring>
#include <string>

#pragma comment(lib, "imm32.lib")

#ifndef DBT_DEVNODES_CHANGED
#define DBT_DEVNODES_CHANGED 0x0007
#endif

namespace Sodium::Backends
{
	namespace
	{
		constexpr UINT SdWmUnichar = 0x0109;
		constexpr WPARAM SdUnicodeNoChar = 0xFFFF;

		SdKeyCode Win32KeyCodeToSdKeyCode(WPARAM wParam, LPARAM lParam)
		{
			switch (wParam)
			{
			case VK_ESCAPE: return SdKeyCode::Esc;
			case VK_F1: return SdKeyCode::F1;
			case VK_F2: return SdKeyCode::F2;
			case VK_F3: return SdKeyCode::F3;
			case VK_F4: return SdKeyCode::F4;
			case VK_F5: return SdKeyCode::F5;
			case VK_F6: return SdKeyCode::F6;
			case VK_F7: return SdKeyCode::F7;
			case VK_F8: return SdKeyCode::F8;
			case VK_F9: return SdKeyCode::F9;
			case VK_F10: return SdKeyCode::F10;
			case VK_F11: return SdKeyCode::F11;
			case VK_F12: return SdKeyCode::F12;
			case VK_SNAPSHOT: return SdKeyCode::PrintScreen;
			case VK_SCROLL: return SdKeyCode::ScrollLock;
			case VK_PAUSE: return SdKeyCode::PauseBreak;
			case VK_OEM_3: return SdKeyCode::Tilde;
			case '1': return SdKeyCode::Num1;
			case '2': return SdKeyCode::Num2;
			case '3': return SdKeyCode::Num3;
			case '4': return SdKeyCode::Num4;
			case '5': return SdKeyCode::Num5;
			case '6': return SdKeyCode::Num6;
			case '7': return SdKeyCode::Num7;
			case '8': return SdKeyCode::Num8;
			case '9': return SdKeyCode::Num9;
			case '0': return SdKeyCode::Num0;
			case VK_OEM_MINUS: return SdKeyCode::Minus;
			case VK_OEM_PLUS: return SdKeyCode::Plus;
			case VK_BACK: return SdKeyCode::Backspace;
			case VK_INSERT: return SdKeyCode::Insert;
			case VK_HOME: return SdKeyCode::Home;
			case VK_PRIOR: return SdKeyCode::PageUp;
			case VK_NUMLOCK: return SdKeyCode::NumPadLock;
			case VK_DIVIDE: return SdKeyCode::NumPadSlash;
			case VK_MULTIPLY: return SdKeyCode::NumPadStar;
			case VK_SUBTRACT: return SdKeyCode::NumPadMinus;
			case VK_TAB: return SdKeyCode::Tab;
			case 'Q': return SdKeyCode::Q;
			case 'W': return SdKeyCode::W;
			case 'E': return SdKeyCode::E;
			case 'R': return SdKeyCode::R;
			case 'T': return SdKeyCode::T;
			case 'Y': return SdKeyCode::Y;
			case 'U': return SdKeyCode::U;
			case 'I': return SdKeyCode::I;
			case 'O': return SdKeyCode::O;
			case 'P': return SdKeyCode::P;
			case VK_OEM_4: return SdKeyCode::LeftBracket;
			case VK_OEM_6: return SdKeyCode::RightBracket;
			case VK_OEM_5: return SdKeyCode::Backslash;
			case VK_DELETE: return SdKeyCode::Delete;
			case VK_END: return SdKeyCode::End;
			case VK_NEXT: return SdKeyCode::PageDown;
			case VK_NUMPAD7: return SdKeyCode::NumPad7;
			case VK_NUMPAD8: return SdKeyCode::NumPad8;
			case VK_NUMPAD9: return SdKeyCode::NumPad9;
			case VK_ADD: return SdKeyCode::NumPadPlus;
			case VK_CAPITAL: return SdKeyCode::CapsLock;
			case 'A': return SdKeyCode::A;
			case 'S': return SdKeyCode::S;
			case 'D': return SdKeyCode::D;
			case 'F': return SdKeyCode::F;
			case 'G': return SdKeyCode::G;
			case 'H': return SdKeyCode::H;
			case 'J': return SdKeyCode::J;
			case 'K': return SdKeyCode::K;
			case 'L': return SdKeyCode::L;
			case VK_OEM_1: return SdKeyCode::Semicolon;
			case VK_OEM_7: return SdKeyCode::Quote;
			case VK_RETURN:
				return (HIWORD(lParam) & KF_EXTENDED) ? SdKeyCode::NumPadEnter : SdKeyCode::Enter;
			case VK_NUMPAD4: return SdKeyCode::NumPad4;
			case VK_NUMPAD5: return SdKeyCode::NumPad5;
			case VK_NUMPAD6: return SdKeyCode::NumPad6;
			case VK_SHIFT:
			{
				const UINT scancode = static_cast<UINT>((lParam >> 16) & 0xFFu);
				const UINT vk = ::MapVirtualKeyW(scancode, MAPVK_VSC_TO_VK_EX);
				return vk == VK_RSHIFT ? SdKeyCode::RightShift : SdKeyCode::LeftShift;
			}
			case 'Z': return SdKeyCode::Z;
			case 'X': return SdKeyCode::X;
			case 'C': return SdKeyCode::C;
			case 'V': return SdKeyCode::V;
			case 'B': return SdKeyCode::B;
			case 'N': return SdKeyCode::N;
			case 'M': return SdKeyCode::M;
			case VK_OEM_COMMA: return SdKeyCode::Comma;
			case VK_OEM_PERIOD: return SdKeyCode::Period;
			case VK_OEM_2: return SdKeyCode::Slash;
			case VK_UP: return SdKeyCode::UpArrow;
			case VK_NUMPAD1: return SdKeyCode::NumPad1;
			case VK_NUMPAD2: return SdKeyCode::NumPad2;
			case VK_NUMPAD3: return SdKeyCode::NumPad3;
			case VK_CONTROL: return (HIWORD(lParam) & KF_EXTENDED) ? SdKeyCode::RightCtrl : SdKeyCode::LeftCtrl;
			case VK_LWIN: return SdKeyCode::LeftWin;
			case VK_MENU: return (HIWORD(lParam) & KF_EXTENDED) ? SdKeyCode::RightAlt : SdKeyCode::LeftAlt;
			case VK_SPACE: return SdKeyCode::Space;
			case VK_RWIN: return SdKeyCode::RightWin;
			case VK_APPS: return SdKeyCode::Menu;
			case VK_LEFT: return SdKeyCode::LeftArrow;
			case VK_DOWN: return SdKeyCode::DownArrow;
			case VK_RIGHT: return SdKeyCode::RightArrow;
			case VK_NUMPAD0: return SdKeyCode::NumPad0;
			case VK_DECIMAL: return SdKeyCode::NumPadPeriod;
			default: return SdKeyCode::Count;
			}
		}

		SdGamepadButton XInputButtonToSdGamepadButton(WORD button)
		{
			switch (button)
			{
			case XINPUT_GAMEPAD_DPAD_UP: return SdGamepadButton::DPadUp;
			case XINPUT_GAMEPAD_DPAD_DOWN: return SdGamepadButton::DPadDown;
			case XINPUT_GAMEPAD_DPAD_LEFT: return SdGamepadButton::DPadLeft;
			case XINPUT_GAMEPAD_DPAD_RIGHT: return SdGamepadButton::DPadRight;
			case XINPUT_GAMEPAD_START: return SdGamepadButton::Start;
			case XINPUT_GAMEPAD_BACK: return SdGamepadButton::Back;
			case XINPUT_GAMEPAD_LEFT_THUMB: return SdGamepadButton::LeftThumb;
			case XINPUT_GAMEPAD_RIGHT_THUMB: return SdGamepadButton::RightThumb;
			case XINPUT_GAMEPAD_LEFT_SHOULDER: return SdGamepadButton::LeftBumper;
			case XINPUT_GAMEPAD_RIGHT_SHOULDER: return SdGamepadButton::RightBumper;
			case XINPUT_GAMEPAD_A: return SdGamepadButton::FaceDown;
			case XINPUT_GAMEPAD_B: return SdGamepadButton::FaceRight;
			case XINPUT_GAMEPAD_X: return SdGamepadButton::FaceLeft;
			case XINPUT_GAMEPAD_Y: return SdGamepadButton::FaceUp;
			default: return SdGamepadButton::Count;
			}
		}

		SdUtf8String WideToUtf8(const wchar_t* text, int length)
		{
			if (!text || length <= 0)
				return {};

			const int byteCount = ::WideCharToMultiByte(CP_UTF8, 0, text, length, nullptr, 0, nullptr, nullptr);
			if (byteCount <= 0)
				return {};

			SdUtf8String converted(static_cast<SdSize>(byteCount), '\0');
			::WideCharToMultiByte(CP_UTF8, 0, text, length, converted.data(), byteCount, nullptr, nullptr);
			return converted;
		}

		SdUtf8String WideCodepointToUtf8(wchar_t codepoint)
		{
			return WideToUtf8(&codepoint, 1);
		}

		std::wstring Utf8ToWide(SdUtf8StringView text)
		{
			if (text.empty())
				return {};

			const int wideCount = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
			if (wideCount <= 0)
				return {};

			std::wstring converted(static_cast<SdSize>(wideCount), L'\0');
			::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), converted.data(), wideCount);
			return converted;
		}

		float NormalizeTrigger(BYTE value, float deadZone)
		{
			float normalized = static_cast<float>(value) / 255.0f;
			return normalized < deadZone ? 0.0f : normalized;
		}

		SdVec2 NormalizeStick(SHORT x, SHORT y, float deadZone)
		{
			const float fx = static_cast<float>(x) / 32767.0f;
			const float fy = static_cast<float>(y) / 32767.0f;
			const float magnitude = std::sqrt((fx * fx) + (fy * fy));
			if (magnitude <= deadZone)
				return {};
			return { fx, -fy };
		}

		void PushSimpleEvent(SdInputSystem& targetInputSystem, SdInputEventType type, SdInputDevice device, SdInputEventPayload&& payload)
		{
			SdInputEvent event = {};
			event.type = type;
			event.device = device;
			event.payload = std::move(payload);
			targetInputSystem.PushEvent(std::move(event));
		}
	}

	bool SdWin32Platform::Initialize(const Config& config)
	{
		windowHandle = config.windowHandle;
		gamepadStickDeadZone = config.gamepadStickDeadZone;
		gamepadTriggerDeadZone = config.gamepadTriggerDeadZone;
		running = windowHandle && ::IsWindow(windowHandle);
		if (!running)
			return false;

		LoadXInput();
		::QueryPerformanceFrequency(&performanceFrequency);
		::QueryPerformanceCounter(&performanceStart);
		return true;
	}

	void SdWin32Platform::Shutdown()
	{
		if (xinputModule)
		{
			::FreeLibrary(xinputModule);
			xinputModule = nullptr;
		}
		xinputGetCapabilities = nullptr;
		xinputGetState = nullptr;
		windowHandle = nullptr;
		running = false;
		imeComposing = false;
		pendingImeResultChars.clear();
		for (bool& connected : xinputConnected)
			connected = false;
		for (SdUInt32& packet : xinputPackets)
			packet = 0;
		performanceFrequency = {};
		performanceStart = {};
	}

	SdUtf8String SdWin32Platform::RequestClipboardText()
	{
		if (!windowHandle || !::OpenClipboard(windowHandle))
			return {};

		HANDLE data = ::GetClipboardData(CF_UNICODETEXT);
		if (!data)
		{
			::CloseClipboard();
			return {};
		}

		const wchar_t* wideText = static_cast<const wchar_t*>(::GlobalLock(data));
		if (!wideText)
		{
			::CloseClipboard();
			return {};
		}

		const int length = static_cast<int>(std::wcslen(wideText));
		SdUtf8String result = WideToUtf8(wideText, length);
		::GlobalUnlock(data);
		::CloseClipboard();
		return result;
	}

	void SdWin32Platform::SetClipboardText(SdUtf8StringView text)
	{
		if (!windowHandle || !::OpenClipboard(windowHandle))
			return;

		const std::wstring wideText = Utf8ToWide(text);
		const SdSize bytes = (wideText.size() + 1) * sizeof(wchar_t);
		HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
		if (!memory)
		{
			::CloseClipboard();
			return;
		}

		void* target = ::GlobalLock(memory);
		if (!target)
		{
			::GlobalFree(memory);
			::CloseClipboard();
			return;
		}
		std::memcpy(target, wideText.c_str(), bytes);
		::GlobalUnlock(memory);

		::EmptyClipboard();
		::SetClipboardData(CF_UNICODETEXT, memory);
		::CloseClipboard();
	}

	bool SdWin32Platform::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, SdInputSystem& targetInputSystem)
	{
		if (hWnd != windowHandle)
			return false;

		switch (msg)
		{
		case WM_DESTROY:
			PushSimpleEvent(targetInputSystem, SdInputEventType::QuitRequest, SdInputDevice::Display, SdQuitRequestEvent{});
			running = false;
			return false;
		case WM_SETFOCUS:
			PushSimpleEvent(targetInputSystem, SdInputEventType::WindowFocus, SdInputDevice::Display, SdWindowFocusEvent{ true });
			return false;
		case WM_KILLFOCUS:
			PushSimpleEvent(targetInputSystem, SdInputEventType::WindowFocus, SdInputDevice::Display, SdWindowFocusEvent{ false });
			return false;
		case WM_MOVE:
			PushSimpleEvent(targetInputSystem, SdInputEventType::WindowMove, SdInputDevice::Display,
				SdWindowMoveEvent{ SdVec2(static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam))) });
			return false;
		case WM_SIZE:
			PushSimpleEvent(targetInputSystem, SdInputEventType::WindowResize, SdInputDevice::Display,
				SdWindowResizeEvent{ SdVec2(static_cast<float>(LOWORD(lParam)), static_cast<float>(HIWORD(lParam))) });
			return false;
		case WM_MOUSEMOVE:
			PushSimpleEvent(targetInputSystem, SdInputEventType::MouseMove, SdInputDevice::Mouse,
				SdMouseMoveEvent{ SdVec2(static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam))) });
			return false;
		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
			PushSimpleEvent(targetInputSystem, SdInputEventType::MouseButton, SdInputDevice::Mouse, SdMouseButtonEvent{ SdMouseButton::Left, true });
			return false;
		case WM_LBUTTONUP:
			PushSimpleEvent(targetInputSystem, SdInputEventType::MouseButton, SdInputDevice::Mouse, SdMouseButtonEvent{ SdMouseButton::Left, false });
			return false;
		case WM_RBUTTONDOWN:
		case WM_RBUTTONDBLCLK:
			PushSimpleEvent(targetInputSystem, SdInputEventType::MouseButton, SdInputDevice::Mouse, SdMouseButtonEvent{ SdMouseButton::Right, true });
			return false;
		case WM_RBUTTONUP:
			PushSimpleEvent(targetInputSystem, SdInputEventType::MouseButton, SdInputDevice::Mouse, SdMouseButtonEvent{ SdMouseButton::Right, false });
			return false;
		case WM_MBUTTONDOWN:
		case WM_MBUTTONDBLCLK:
			PushSimpleEvent(targetInputSystem, SdInputEventType::MouseButton, SdInputDevice::Mouse, SdMouseButtonEvent{ SdMouseButton::Middle, true });
			return false;
		case WM_MBUTTONUP:
			PushSimpleEvent(targetInputSystem, SdInputEventType::MouseButton, SdInputDevice::Mouse, SdMouseButtonEvent{ SdMouseButton::Middle, false });
			return false;
		case WM_XBUTTONDOWN:
		case WM_XBUTTONDBLCLK:
		{
			const SdMouseButton button = GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? SdMouseButton::X1 : SdMouseButton::X2;
			PushSimpleEvent(targetInputSystem, SdInputEventType::MouseButton, SdInputDevice::Mouse, SdMouseButtonEvent{ button, true });
			return false;
		}
		case WM_XBUTTONUP:
		{
			const SdMouseButton button = GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? SdMouseButton::X1 : SdMouseButton::X2;
			PushSimpleEvent(targetInputSystem, SdInputEventType::MouseButton, SdInputDevice::Mouse, SdMouseButtonEvent{ button, false });
			return false;
		}
		case WM_MOUSEWHEEL:
			PushSimpleEvent(targetInputSystem, SdInputEventType::MouseWheel, SdInputDevice::Mouse,
				SdMouseWheelEvent{ SdVec2(0.0f, static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA)) });
			return false;
		case WM_MOUSEHWHEEL:
			PushSimpleEvent(targetInputSystem, SdInputEventType::MouseWheel, SdInputDevice::Mouse,
				SdMouseWheelEvent{ SdVec2(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA), 0.0f) });
			return false;
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYUP:
		{
			const SdKeyCode keyCode = Win32KeyCodeToSdKeyCode(wParam, lParam);
			if (keyCode == SdKeyCode::Count)
				return false;

			const bool isDown = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
			const bool isRepeat = isDown && ((lParam & (1LL << 30)) != 0);
			PushSimpleEvent(targetInputSystem, SdInputEventType::Key, SdInputDevice::Keyboard, SdKeyEvent{ keyCode, isDown, isRepeat });
			return false;
		}
		case WM_CHAR:
			if (TryConsumeDuplicateImeChar(wParam))
				return false;
			if (!imeComposing && wParam >= 0x20)
				PushSimpleEvent(targetInputSystem, SdInputEventType::TextCommit, SdInputDevice::Text, SdTextCommitEvent{ WideCodepointToUtf8(static_cast<wchar_t>(wParam)) });
			return false;
		case SdWmUnichar:
			if (wParam == SdUnicodeNoChar)
				return true;
			if (wParam <= 0xFFFFu && TryConsumeDuplicateImeChar(wParam))
				return false;
			if (!imeComposing && wParam >= 0x20)
				PushSimpleEvent(targetInputSystem, SdInputEventType::TextCommit, SdInputDevice::Text, SdTextCommitEvent{ WideCodepointToUtf8(static_cast<wchar_t>(wParam)) });
			return false;
		case WM_INPUTLANGCHANGE:
			targetInputSystem.GetMutableSnapshot().text.imeEnabled = true;
			return false;
		case WM_IME_STARTCOMPOSITION:
			imeComposing = true;
			PushSimpleEvent(targetInputSystem, SdInputEventType::TextCompositionStart, SdInputDevice::Text, SdTextCompositionEvent{});
			return false;
		case WM_IME_ENDCOMPOSITION:
			imeComposing = false;
			PushSimpleEvent(targetInputSystem, SdInputEventType::TextCompositionEnd, SdInputDevice::Text, SdTextCompositionEvent{});
			return false;
		case WM_IME_COMPOSITION:
		{
			HIMC context = ::ImmGetContext(windowHandle);
			if (!context)
				return false;

			if ((lParam & GCS_RESULTSTR) != 0)
			{
				const LONG bytes = ::ImmGetCompositionStringW(context, GCS_RESULTSTR, nullptr, 0);
				if (bytes > 0)
				{
					std::wstring wideText(static_cast<SdSize>(bytes / sizeof(wchar_t)), L'\0');
					::ImmGetCompositionStringW(context, GCS_RESULTSTR, wideText.data(), bytes);
					pendingImeResultChars = wideText;
					PushSimpleEvent(targetInputSystem, SdInputEventType::TextCommit, SdInputDevice::Text, SdTextCommitEvent{ WideToUtf8(wideText.data(), static_cast<int>(wideText.size())) });
				}
			}

			if ((lParam & GCS_COMPSTR) != 0)
			{
				const LONG bytes = ::ImmGetCompositionStringW(context, GCS_COMPSTR, nullptr, 0);
				std::wstring wideText;
				if (bytes > 0)
				{
					wideText.resize(static_cast<SdSize>(bytes / sizeof(wchar_t)));
					::ImmGetCompositionStringW(context, GCS_COMPSTR, wideText.data(), bytes);
				}

				LONG cursorPos = ::ImmGetCompositionStringW(context, GCS_CURSORPOS, nullptr, 0);
				if (cursorPos < 0)
					cursorPos = 0;

				SdTextCompositionEvent composition = {};
				composition.text = WideToUtf8(wideText.data(), static_cast<int>(wideText.size()));
				const int clampedCursor = std::min<int>(cursorPos, static_cast<int>(wideText.size()));
				composition.cursorByteOffset = static_cast<SdUInt32>(WideToUtf8(wideText.data(), clampedCursor).size());
				composition.candidateVisible = !composition.text.empty();
				PushSimpleEvent(targetInputSystem, SdInputEventType::TextCompositionUpdate, SdInputDevice::Text, std::move(composition));
			}

			::ImmReleaseContext(windowHandle, context);
			return false;
		}
		case WM_DEVICECHANGE:
			if ((UINT)wParam == DBT_DEVNODES_CHANGED)
				RefreshGamepadsList(targetInputSystem);
			return false;
		default:
			return false;
		}
	}

	void SdWin32Platform::StartFrame(SdInputSystem& targetInputSystem)
	{
		if (!windowHandle || !::IsWindow(windowHandle))
		{
			PushSimpleEvent(targetInputSystem, SdInputEventType::QuitRequest, SdInputDevice::Display, SdQuitRequestEvent{});
			running = false;
			return;
		}

		SyncWindowState(targetInputSystem);
		SyncMouseState(targetInputSystem);
		PollGamepads(targetInputSystem);
		SyncImeWindow(targetInputSystem);
		pendingImeResultChars.clear();
	}

	bool SdWin32Platform::TryConsumeDuplicateImeChar(WPARAM character)
	{
		if (pendingImeResultChars.empty())
			return false;

		if (character <= 0xFFFFu && pendingImeResultChars.front() == static_cast<wchar_t>(character))
		{
			pendingImeResultChars.erase(pendingImeResultChars.begin());
			return true;
		}

		pendingImeResultChars.clear();
		return false;
	}

	void SdWin32Platform::LoadXInput()
	{
		const char* const kDllNames[] =
		{
			SODIUM_STRING("xinput1_4.dll"),
			SODIUM_STRING("xinput1_3.dll"),
			SODIUM_STRING("xinput9_1_0.dll"),
			SODIUM_STRING("xinput1_2.dll"),
			SODIUM_STRING("xinput1_1.dll")
		};

		for (const char* dllName : kDllNames)
		{
			HMODULE module = ::LoadLibraryA(dllName);
			if (!module)
				continue;

			xinputModule = module;
			xinputGetCapabilities = reinterpret_cast<XInputGetCapabilitiesProc>(::GetProcAddress(module, SODIUM_STRING("XInputGetCapabilities")));
			xinputGetState = reinterpret_cast<XInputGetStateProc>(::GetProcAddress(module, SODIUM_STRING("XInputGetState")));
			if (xinputGetCapabilities && xinputGetState)
				return;

			::FreeLibrary(module);
			xinputModule = nullptr;
			xinputGetCapabilities = nullptr;
			xinputGetState = nullptr;
		}
	}

	void SdWin32Platform::RefreshGamepadsList(SdInputSystem& targetInputSystem)
	{
		if (!xinputGetState || !xinputGetCapabilities)
			return;
		for (DWORD index = 0; index < XUSER_MAX_COUNT; index++)
		{
			XINPUT_CAPABILITIES caps = {};
			const bool connected = ERROR_SUCCESS == xinputGetCapabilities(index, XINPUT_FLAG_GAMEPAD, &caps);

			if (xinputConnected[index] != connected)
			{
				xinputConnected[index] = connected;
				PushSimpleEvent(targetInputSystem, SdInputEventType::GamepadConnection, SdInputDevice::Gamepad,
					SdGamepadConnectionEvent{ static_cast<SdUInt32>(index), connected });
			}
		}
	}

	void SdWin32Platform::PollGamepads(SdInputSystem& targetInputSystem)
	{
		if (!xinputGetState || !xinputGetCapabilities)
			return;
		for (DWORD index = 0; index < XUSER_MAX_COUNT; index++)
		{
			const bool connected = xinputConnected[index];
			if (!connected)
				continue;

			XINPUT_STATE state = {};
			const DWORD result = xinputGetState(index, &state);

			if (result != ERROR_SUCCESS)
				continue;

			if (xinputPackets[index] == state.dwPacketNumber)
				continue;

			const XINPUT_GAMEPAD& gamepad = state.Gamepad;
			const WORD buttonMask = gamepad.wButtons;

			for (WORD bit = 1; bit != 0; bit <<= 1)
			{
				const SdGamepadButton button = XInputButtonToSdGamepadButton(bit);
				if (button == SdGamepadButton::Count)
					continue;
				const bool previousDown = targetInputSystem.GetSnapshot().gamepads[index].buttons[static_cast<SdSize>(button)].isDown;
				const bool currentDown = (buttonMask & bit) != 0;
				if (previousDown != currentDown)
				{
					PushSimpleEvent(targetInputSystem, SdInputEventType::GamepadButton, SdInputDevice::Gamepad,
						SdGamepadButtonEvent{ static_cast<SdUInt32>(index), button, currentDown });
				}
			}

			const SdVec2 leftStick = NormalizeStick(gamepad.sThumbLX, gamepad.sThumbLY, gamepadStickDeadZone);
			const SdVec2 rightStick = NormalizeStick(gamepad.sThumbRX, gamepad.sThumbRY, gamepadStickDeadZone);
			const float leftTrigger = NormalizeTrigger(gamepad.bLeftTrigger, gamepadTriggerDeadZone);
			const float rightTrigger = NormalizeTrigger(gamepad.bRightTrigger, gamepadTriggerDeadZone);
			const SdGamepadState& existing = targetInputSystem.GetSnapshot().gamepads[index];

			if (leftStick.x != existing.leftStick.x || leftStick.y != existing.leftStick.y)
			{
				PushSimpleEvent(targetInputSystem, SdInputEventType::GamepadAxis, SdInputDevice::Gamepad,
					SdGamepadAxisEvent{ static_cast<SdUInt32>(index), SdGamepadAxis::LeftStick, leftStick, 0.0f });
			}
			if (rightStick.x != existing.rightStick.x || rightStick.y != existing.rightStick.y)
			{
				PushSimpleEvent(targetInputSystem, SdInputEventType::GamepadAxis, SdInputDevice::Gamepad,
					SdGamepadAxisEvent{ static_cast<SdUInt32>(index), SdGamepadAxis::RightStick, rightStick, 0.0f });
			}
			if (leftTrigger != existing.leftTrigger)
			{
				PushSimpleEvent(targetInputSystem, SdInputEventType::GamepadAxis, SdInputDevice::Gamepad,
					SdGamepadAxisEvent{ static_cast<SdUInt32>(index), SdGamepadAxis::LeftTrigger, {}, leftTrigger });
			}
			if (rightTrigger != existing.rightTrigger)
			{
				PushSimpleEvent(targetInputSystem, SdInputEventType::GamepadAxis, SdInputDevice::Gamepad,
					SdGamepadAxisEvent{ static_cast<SdUInt32>(index), SdGamepadAxis::RightTrigger, {}, rightTrigger });
			}

			xinputPackets[index] = state.dwPacketNumber;
		}
	}

	void SdWin32Platform::SyncWindowState(SdInputSystem& targetInputSystem)
	{
		RECT rect = {};
		::GetClientRect(windowHandle, &rect);
		PushSimpleEvent(targetInputSystem, SdInputEventType::WindowResize, SdInputDevice::Display,
			SdWindowResizeEvent{ SdVec2(static_cast<float>(rect.right - rect.left), static_cast<float>(rect.bottom - rect.top)) });

		POINT point = {};
		::ClientToScreen(windowHandle, &point);
		PushSimpleEvent(targetInputSystem, SdInputEventType::WindowMove, SdInputDevice::Display,
			SdWindowMoveEvent{ SdVec2(static_cast<float>(point.x), static_cast<float>(point.y)) });

		const bool focused = ::GetForegroundWindow() == windowHandle;
		PushSimpleEvent(targetInputSystem, SdInputEventType::WindowFocus, SdInputDevice::Display, SdWindowFocusEvent{ focused });
		HIMC context = ::ImmGetContext(windowHandle);
		targetInputSystem.GetMutableSnapshot().text.imeEnabled = context != nullptr;
		if (context)
			::ImmReleaseContext(windowHandle, context);
	}

	void SdWin32Platform::SyncMouseState(SdInputSystem& targetInputSystem)
	{
		POINT point = {};
		if (!::GetCursorPos(&point) || !::ScreenToClient(windowHandle, &point))
			return;

		PushSimpleEvent(targetInputSystem, SdInputEventType::MouseMove, SdInputDevice::Mouse,
			SdMouseMoveEvent{ SdVec2(static_cast<float>(point.x), static_cast<float>(point.y)) });
	}

	void SdWin32Platform::SyncImeWindow(SdInputSystem& targetInputSystem)
	{
		const SdTextInputTarget* target = targetInputSystem.GetActiveTextInputTarget();
		if (!target || !targetInputSystem.GetSnapshot().display.focused)
			return;

		HIMC context = ::ImmGetContext(windowHandle);
		if (!context)
			return;

		CANDIDATEFORM candidate = {};
		candidate.dwIndex = 0;
		candidate.dwStyle = CFS_CANDIDATEPOS;
		candidate.ptCurrentPos.x = static_cast<LONG>(target->caretRect.min.x);
		candidate.ptCurrentPos.y = static_cast<LONG>(target->caretRect.max.y);
		::ImmSetCandidateWindow(context, &candidate);

		COMPOSITIONFORM composition = {};
		composition.dwStyle = CFS_FORCE_POSITION;
		composition.ptCurrentPos.x = static_cast<LONG>(target->caretRect.min.x);
		composition.ptCurrentPos.y = static_cast<LONG>(target->caretRect.min.y);
		::ImmSetCompositionWindow(context, &composition);

		::ImmReleaseContext(windowHandle, context);
	}
}

