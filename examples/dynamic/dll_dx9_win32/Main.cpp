#include <Windows.h>
#include <d3d9.h>
#include <wrl/client.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

#include <SodiumGUI.h>
#include <Backends/FreeType/FreeTypeFontBackend.h>
#include <Backends/Dx9/SdDx9Renderer.h>
#include <Backends/Win32/SdWin32Platform.h>

#include <Widget/SdBasicWidgets.h>

#include <detours/detours.h>

#pragma comment(lib, "d3d9.lib")

namespace SodiumDynamicDx9Example
{
	using Microsoft::WRL::ComPtr;
	using namespace std::chrono_literals;

	inline HMODULE selfModule = nullptr;
	inline std::atomic_bool unloadRequested = false;

	class DetourHook final
	{
	public:
		DetourHook(void* detour, void** original)
			: detour(detour), original(original)
		{
		}

		bool Attach(void* target)
		{
			if (!target || !detour || !original)
				return false;

			*original = target;
			if (::DetourTransactionBegin() != NO_ERROR)
				return false;
			if (::DetourUpdateThread(::GetCurrentThread()) != NO_ERROR)
			{
				::DetourTransactionAbort();
				return false;
			}
			if (::DetourAttach(original, detour) != NO_ERROR)
			{
				::DetourTransactionAbort();
				return false;
			}
			if (::DetourTransactionCommit() != NO_ERROR)
				return false;

			attached = true;
			return true;
		}

		bool Detach()
		{
			if (!attached)
				return true;

			if (::DetourTransactionBegin() != NO_ERROR)
				return false;
			if (::DetourUpdateThread(::GetCurrentThread()) != NO_ERROR)
			{
				::DetourTransactionAbort();
				return false;
			}
			if (::DetourDetach(original, detour) != NO_ERROR)
			{
				::DetourTransactionAbort();
				return false;
			}
			if (::DetourTransactionCommit() != NO_ERROR)
				return false;

			attached = false;
			return true;
		}

		bool IsAttached() const noexcept
		{
			return attached;
		}

	private:
		void* detour = nullptr;
		void** original = nullptr;
		bool attached = false;
	};

#define SD_RAW_HOOK(returnType, functionName, ...) \
	returnType h##functionName(__VA_ARGS__); \
	using functionName##Fn = decltype(&h##functionName); \
	inline functionName##Fn o##functionName = nullptr; \
	inline DetourHook hook##functionName = DetourHook( \
		reinterpret_cast<void*>(h##functionName), \
		reinterpret_cast<void**>(&o##functionName))

#define SD_HOOK(returnType, functionName, ...) \
	SD_RAW_HOOK(returnType, functionName, __VA_ARGS__); \
	inline returnType h##functionName(__VA_ARGS__)

	struct Dx9HostContext final
	{
		ComPtr<IDirect3DDevice9> device = {};
		HWND windowHandle = nullptr;
		Sodium::SdVec2 displaySize = {};

		bool Initialize(IDirect3DDevice9* hostDevice)
		{
			if (!hostDevice)
				return false;

			device = hostDevice;
			D3DDEVICE_CREATION_PARAMETERS creationParameters = {};
			if (SUCCEEDED(device->GetCreationParameters(&creationParameters)))
				windowHandle = creationParameters.hFocusWindow;
			if (!windowHandle)
				windowHandle = ::GetForegroundWindow();
			return windowHandle && RefreshDisplaySize();
		}

		bool RefreshDisplaySize()
		{
			D3DVIEWPORT9 viewport = {};
			if (device && SUCCEEDED(device->GetViewport(&viewport)) && viewport.Width > 0 && viewport.Height > 0)
			{
				displaySize = { static_cast<float>(viewport.Width), static_cast<float>(viewport.Height) };
				return true;
			}

			RECT rect = {};
			if (windowHandle && ::GetClientRect(windowHandle, &rect) && rect.right > rect.left && rect.bottom > rect.top)
			{
				displaySize =
				{
					static_cast<float>(rect.right - rect.left),
					static_cast<float>(rect.bottom - rect.top)
				};
				return true;
			}
			return false;
		}

		void Shutdown()
		{
			device.Reset();
			windowHandle = nullptr;
			displaySize = {};
		}
	};

	struct DemoControlsState final
	{
		bool windowOpen = true;
		bool optionEnabled = true;
		bool tooltipVisible = true;
		int clickCount = 0;
		float sliderValue = 0.45f;
		Sodium::SdUtf8String textInputValue = "DX9 overlay text";
	};

	struct SodiumOverlay final
	{
		Dx9HostContext dx = {};
		Sodium::SdInstance gui = {};
		Sodium::Backends::SdWin32Platform platform = {};
		Sodium::Backends::SdDx9Renderer renderer = {};
		Sodium::Backends::SdFreeTypeFontBackend fontBackend = {};
		DemoControlsState controls = {};
		bool initialized = false;
		Sodium::SdFrameIndex frameCount = 0;
		double liveFps = 0.0;
		std::chrono::steady_clock::time_point previousFrameTime = {};

		bool Initialize(IDirect3DDevice9* hostDevice)
		{
			if (initialized)
				return true;
			if (!dx.Initialize(hostDevice))
				return false;
			if (!platform.Initialize(Sodium::Backends::SdWin32PlatformConfig(dx.windowHandle)))
				return false;

			Sodium::Backends::SdDx9RendererConfig rendererConfig = {};
			rendererConfig.device = dx.device.Get();
			if (!renderer.Initialize(rendererConfig))
				return false;
			if (!fontBackend.Initialize())
				return false;
			if (!gui.Initialize(platform, renderer, fontBackend))
				return false;

			const Sodium::SdFontRequest fontRequests[] =
			{
				{ L"C:\\Windows\\Fonts\\segoeui.ttf", "Segoe UI" },
				{ L"C:\\Windows\\Fonts\\arial.ttf", "Arial" },
				{ L"C:\\Windows\\Fonts\\msyh.ttc", "Microsoft YaHei" }
			};
			for (const Sodium::SdFontRequest& request : fontRequests)
				fontBackend.LoadFont(request);

			initialized = true;
			return true;
		}

		void Render()
		{
			if (!initialized || unloadRequested.load())
				return;
			if (!dx.RefreshDisplaySize())
				return;

			const auto frameStart = std::chrono::steady_clock::now();
			gui.BeginFrame();
			DeclareDemoUi();
			gui.EndFrame();
			gui.Render();

			const auto frameEnd = std::chrono::steady_clock::now();
			const std::chrono::steady_clock::time_point frameReference = previousFrameTime.time_since_epoch().count() != 0
				? previousFrameTime
				: frameStart;
			const double frameSeconds = std::chrono::duration<double>(frameEnd - frameReference).count();
			liveFps = frameSeconds > 0.0 ? 1.0 / frameSeconds : 0.0;
			previousFrameTime = frameEnd;
			++frameCount;
		}

		void OnBeforeReset()
		{
			if (initialized)
				renderer.OnLostDevice();
		}

		void OnAfterReset(IDirect3DDevice9* hostDevice)
		{
			if (!initialized || unloadRequested.load())
				return;
			if (hostDevice && hostDevice != dx.device.Get())
				dx.device = hostDevice;
			dx.RefreshDisplaySize();
			renderer.OnResetDevice();
		}

		void Shutdown()
		{
			if (!initialized)
				return;

			gui.Shutdown();
			fontBackend.Shutdown();
			renderer.Shutdown();
			platform.Shutdown();
			dx.Shutdown();
			initialized = false;
		}

	private:
		void DeclareDemoUi()
		{
			Sodium::SdWindowOptions windowOptions = {};
			windowOptions.position = { 42.0f, 42.0f };
			windowOptions.size = { 420.0f, 330.0f };
			windowOptions.backgroundBlurRadius = 0.0f;
			windowOptions.shadowRadius = 0.0f;

			gui.ui.DeclareKeyed<Sodium::SdWindow>("dx9_overlay_window", "SodiumGUI DX9 Overlay", controls.windowOpen, windowOptions, [this](Sodium::SdUi& ui)
			{
				char status[128] = {};
				std::snprintf(status, sizeof(status), "Frame %llu  FPS %.1f", static_cast<unsigned long long>(frameCount), liveFps);
				ui.Declare<Sodium::SdText>(status);

				if (ui.Declare<Sodium::SdButton>("Overlay button").clickedThisFrame)
					++controls.clickCount;
				char clicks[64] = {};
				std::snprintf(clicks, sizeof(clicks), "Clicks: %d", controls.clickCount);
				ui.Declare<Sodium::SdText>(clicks);
				ui.Declare<Sodium::SdCheckBox>("Option enabled", controls.optionEnabled);
				ui.Declare<Sodium::SdSliderFloat>("Slider", controls.sliderValue, 0.0f, 1.0f);
				ui.Declare<Sodium::SdTextInput>("Text", controls.textInputValue, "Type here");
				ui.Declare<Sodium::SdImageViewer>(fontBackend.GetAtlasTexture(), Sodium::SdVec2{ 128.0f, 32.0f });
			});

			gui.ui.DeclareKeyed<Sodium::SdTooltip>(
				"dx9_overlay_tooltip",
				controls.tooltipVisible,
				Sodium::SdVec2{ 42.0f, 390.0f },
				"Press Home to unload the DX9 overlay sample.");
		}
	};

	inline SodiumOverlay overlay = {};

	SD_HOOK(LRESULT CALLBACK, WndProc, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (message == WM_KEYDOWN && wParam == VK_HOME)
			unloadRequested.store(true);

		if (overlay.initialized)
			overlay.platform.HandleMessage(hwnd, message, wParam, lParam, overlay.gui.GetInputSystem());

		return ::CallWindowProcW(oWndProc, hwnd, message, wParam, lParam);
	}

	SD_HOOK(HRESULT, EndScene, IDirect3DDevice9* device)
	{
		if (!unloadRequested.load() && !overlay.initialized)
		{
			if (!overlay.Initialize(device))
				unloadRequested.store(true);
			else if (!hookWndProc.IsAttached())
				hookWndProc.Attach(reinterpret_cast<void*>(::GetWindowLongPtrW(overlay.dx.windowHandle, GWLP_WNDPROC)));
		}

		overlay.Render();
		return oEndScene(device);
	}

	SD_HOOK(HRESULT, Reset, IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* presentationParameters)
	{
		overlay.OnBeforeReset();
		const HRESULT result = oReset(device, presentationParameters);
		if (SUCCEEDED(result))
			overlay.OnAfterReset(device);
		return result;
	}

	bool FindD3D9DeviceVTable(void**& vTable)
	{
		constexpr wchar_t kClassName[] = L"SodiumGUI.Dynamic.Dx9Probe";

		WNDCLASSEXW windowClass = {};
		windowClass.cbSize = sizeof(windowClass);
		windowClass.style = CS_HREDRAW | CS_VREDRAW;
		windowClass.lpfnWndProc = ::DefWindowProcW;
		windowClass.hInstance = ::GetModuleHandleW(nullptr);
		windowClass.lpszClassName = kClassName;

		if (!::RegisterClassExW(&windowClass))
			return false;

		HWND window = ::CreateWindowExW(
			0,
			kClassName,
			L"SodiumGUI Dynamic DX9 Probe",
			WS_OVERLAPPEDWINDOW,
			0,
			0,
			100,
			100,
			nullptr,
			nullptr,
			windowClass.hInstance,
			nullptr);
		if (!window)
		{
			::UnregisterClassW(kClassName, windowClass.hInstance);
			return false;
		}

		ComPtr<IDirect3D9> direct3d = {};
		direct3d.Attach(::Direct3DCreate9(D3D_SDK_VERSION));
		if (!direct3d)
		{
			::DestroyWindow(window);
			::UnregisterClassW(kClassName, windowClass.hInstance);
			return false;
		}

		D3DPRESENT_PARAMETERS params = {};
		params.Windowed = TRUE;
		params.SwapEffect = D3DSWAPEFFECT_DISCARD;
		params.BackBufferFormat = D3DFMT_UNKNOWN;
		params.hDeviceWindow = window;

		ComPtr<IDirect3DDevice9> device = {};
		const HRESULT result = direct3d->CreateDevice(
			D3DADAPTER_DEFAULT,
			D3DDEVTYPE_HAL,
			window,
			D3DCREATE_SOFTWARE_VERTEXPROCESSING,
			&params,
			device.GetAddressOf());

		if (SUCCEEDED(result))
			vTable = *reinterpret_cast<void***>(device.Get());

		::DestroyWindow(window);
		::UnregisterClassW(kClassName, windowClass.hInstance);
		return SUCCEEDED(result) && vTable;
	}

	bool InitializeHooks()
	{
		void** deviceVTable = nullptr;
		if (!FindD3D9DeviceVTable(deviceVTable))
			return false;

		return hookReset.Attach(deviceVTable[16])
			&& hookEndScene.Attach(deviceVTable[42]);
	}

	void ShutdownHooks()
	{
		hookWndProc.Detach();
		hookReset.Detach();
		hookEndScene.Detach();
	}

	DWORD WINAPI MainThread(LPVOID)
	{
		if (!InitializeHooks())
			unloadRequested.store(true);

		while (!unloadRequested.load())
			std::this_thread::sleep_for(16ms);

		ShutdownHooks();
		overlay.Shutdown();
		::FreeLibraryAndExitThread(selfModule, 0);
		return 0;
	}
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		SodiumDynamicDx9Example::selfModule = module;
		SodiumDynamicDx9Example::unloadRequested.store(false);
		::DisableThreadLibraryCalls(module);
		::CreateThread(nullptr, 0, SodiumDynamicDx9Example::MainThread, nullptr, 0, nullptr);
	}
	return TRUE;
}
