#pragma once

#include "../SodiumGUI.h"

#include <Windows.h>
#include <xinput.h>

#include <string>

namespace Sodium::Backends
{
	struct SdWin32PlatformConfig final
	{
		HWND windowHandle = nullptr;
		float gamepadStickDeadZone = 0.1f;
		float gamepadTriggerDeadZone = 0.05f;

		SdWin32PlatformConfig() = default;
		explicit SdWin32PlatformConfig(HWND hwnd) : windowHandle(hwnd) {}
	};

	class SdWin32Platform final : public ISdPlatformBackend
	{
	public:
		using Config = SdWin32PlatformConfig;
	private:
		using XInputGetCapabilitiesProc = decltype(&::XInputGetCapabilities);
		using XInputGetStateProc = decltype(&::XInputGetState);

		HWND windowHandle = nullptr;
		bool running = false;
		bool imeComposing = false;
		std::wstring pendingImeResultChars = {};
		float gamepadStickDeadZone = 0.1f;
		float gamepadTriggerDeadZone = 0.05f;
		LARGE_INTEGER performanceFrequency = {};
		LARGE_INTEGER performanceStart = {};
		HMODULE xinputModule = nullptr;
		XInputGetCapabilitiesProc xinputGetCapabilities = nullptr;
		XInputGetStateProc xinputGetState = nullptr;
		bool xinputConnected[4] = {};
		SdUInt32 xinputPackets[4] = {};
	public:
		bool Initialize(const Config& config);
		void Shutdown();
		bool IsInitialized() const noexcept override { return IsRunning(); }
		SdUtf8String RequestClipboardText();
		void SetClipboardText(SdUtf8StringView text);
		bool HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, Sodium::SdInputSystem& targetInputSystem);
		void StartFrame(Sodium::SdInputSystem& targetInputSystem) override;

		bool IsRunning() const noexcept { return running && windowHandle && ::IsWindow(windowHandle); }
		HWND GetWindowHandle() const noexcept { return windowHandle; }
	private:
		void LoadXInput();
		void RefreshGamepadsList(SdInputSystem& targetInputSystem);
		void PollGamepads(Sodium::SdInputSystem& targetInputSystem);
		void SyncWindowState(Sodium::SdInputSystem& targetInputSystem);
		void SyncMouseState(Sodium::SdInputSystem& targetInputSystem);
		void SyncImeWindow(Sodium::SdInputSystem& targetInputSystem);
		bool TryConsumeDuplicateImeChar(WPARAM character);
	};

	using SdPlatform = SdWin32Platform;
}
