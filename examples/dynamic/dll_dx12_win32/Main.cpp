#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#include <SodiumGUI.h>
#include <Backends/FreeType/FreeTypeFontBackend.h>
#include <Backends/Dx12/SdDx12Renderer.h>
#include <Backends/Win32/SdWin32Platform.h>

#include <Widget/SdBasicWidgets.h>

#include <detours/detours.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace SodiumDynamicDx12Example
{
	using Microsoft::WRL::ComPtr;
	using namespace std::chrono_literals;

	inline HMODULE selfModule = nullptr;
	inline std::atomic_bool unloadRequested = false;
	inline ComPtr<ID3D12CommandQueue> capturedCommandQueue = {};

	bool IsDirectCommandQueue(ID3D12CommandQueue* commandQueue)
	{
		return commandQueue && commandQueue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT;
	}

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

	void CaptureCommandQueue(ID3D12CommandQueue* commandQueue)
	{
		if (!IsDirectCommandQueue(commandQueue) || capturedCommandQueue)
			return;
		capturedCommandQueue = commandQueue;
	}

	void CaptureCommandQueue(IUnknown* candidate)
	{
		if (!candidate || capturedCommandQueue)
			return;

		ComPtr<ID3D12CommandQueue> commandQueue = {};
		if (SUCCEEDED(candidate->QueryInterface(IID_PPV_ARGS(commandQueue.GetAddressOf()))))
			CaptureCommandQueue(commandQueue.Get());
	}

	struct Dx12HostContext final
	{
		ComPtr<IDXGISwapChain3> swapChain = {};
		ComPtr<ID3D12Device> device = {};
		ComPtr<ID3D12CommandQueue> commandQueue = {};
		std::vector<ComPtr<ID3D12Resource>> renderTargets = {};
		HWND windowHandle = nullptr;
		Sodium::SdVec2 displaySize = {};
		DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		UINT renderTargetCount = 0;

		bool Initialize(IDXGISwapChain* hostSwapChain, ID3D12CommandQueue* hostCommandQueue)
		{
			if (!hostSwapChain || !hostCommandQueue)
				return false;

			if (FAILED(hostSwapChain->QueryInterface(IID_PPV_ARGS(swapChain.ReleaseAndGetAddressOf()))))
				return false;
			if (FAILED(swapChain->GetDevice(IID_PPV_ARGS(device.ReleaseAndGetAddressOf()))))
				return false;

			commandQueue = hostCommandQueue;
			return RefreshSwapChainDesc() && CreateRenderTargets();
		}

		bool Matches(IDXGISwapChain* hostSwapChain) const noexcept
		{
			return hostSwapChain && swapChain.Get() == hostSwapChain;
		}

		UINT GetFrameIndex() const noexcept
		{
			return swapChain ? swapChain->GetCurrentBackBufferIndex() : 0;
		}

		bool RefreshSwapChainDesc()
		{
			if (!swapChain)
				return false;

			DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
			if (FAILED(swapChain->GetDesc(&swapChainDesc)))
				return false;
			windowHandle = swapChainDesc.OutputWindow;

			DXGI_SWAP_CHAIN_DESC1 desc = {};
			if (SUCCEEDED(swapChain->GetDesc1(&desc)))
			{
				displaySize = { static_cast<float>(desc.Width), static_cast<float>(desc.Height) };
				backBufferFormat = desc.Format == DXGI_FORMAT_UNKNOWN ? backBufferFormat : desc.Format;
				renderTargetCount = desc.BufferCount;
			}
			else
			{
				displaySize = {
					static_cast<float>(swapChainDesc.BufferDesc.Width),
					static_cast<float>(swapChainDesc.BufferDesc.Height)
				};
				backBufferFormat = swapChainDesc.BufferDesc.Format == DXGI_FORMAT_UNKNOWN ? backBufferFormat : swapChainDesc.BufferDesc.Format;
				renderTargetCount = swapChainDesc.BufferCount;
			}

			if (displaySize.x <= 0.0f || displaySize.y <= 0.0f)
			{
				RECT rect = {};
				if (windowHandle && ::GetClientRect(windowHandle, &rect))
				{
					displaySize.x = static_cast<float>(rect.right - rect.left);
					displaySize.y = static_cast<float>(rect.bottom - rect.top);
				}
			}

			if (renderTargetCount == 0)
				renderTargetCount = 2;
			return displaySize.x > 0.0f && displaySize.y > 0.0f;
		}

		bool CreateRenderTargets()
		{
			if (!swapChain || renderTargetCount == 0)
				return false;

			renderTargets.clear();
			renderTargets.resize(renderTargetCount);
			for (UINT i = 0; i < renderTargetCount; ++i)
			{
				if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(renderTargets[i].ReleaseAndGetAddressOf()))))
				{
					ReleaseRenderTargets();
					return false;
				}
			}
			return true;
		}

		void FillRenderTargetArray(std::vector<ID3D12Resource*>& targets) const
		{
			targets.clear();
			targets.reserve(renderTargets.size());
			for (const ComPtr<ID3D12Resource>& renderTarget : renderTargets)
				targets.push_back(renderTarget.Get());
		}

		void ReleaseRenderTargets()
		{
			for (ComPtr<ID3D12Resource>& renderTarget : renderTargets)
				renderTarget.Reset();
			renderTargets.clear();
		}

		void Shutdown()
		{
			ReleaseRenderTargets();
			commandQueue.Reset();
			device.Reset();
			swapChain.Reset();
			windowHandle = nullptr;
			displaySize = {};
			renderTargetCount = 0;
		}
	};

	struct DemoControlsState final
	{
		bool optionEnabled = true;
		bool demoWindowOpen = true;
		bool popupOpen = true;
		bool lightTheme = false;
		int clickCount = 0;
		float sliderValue = 0.45f;
		Sodium::SdUtf8String textInputValue = "Edit SodiumGUI text";
	};

	struct SodiumOverlay final
	{
		Dx12HostContext dx = {};
		Sodium::SdInstance gui = {};
		Sodium::Backends::SdWin32Platform platform = {};
		Sodium::Backends::SdDx12Renderer renderer = {};
		Sodium::Backends::SdFreeTypeFontBackend fontBackend = {};
		DemoControlsState controls = {};
		bool initialized = false;
		bool themeInitialized = false;
		bool appliedLightTheme = false;
		UINT64 frameCount = 0;
		double liveFps = 0.0;
		std::chrono::steady_clock::time_point previousFrameTime = {};

		bool Initialize(IDXGISwapChain* hostSwapChain, ID3D12CommandQueue* hostCommandQueue)
		{
			if (initialized)
				return true;
			if (!dx.Initialize(hostSwapChain, hostCommandQueue))
				return false;
			if (!platform.Initialize(Sodium::Backends::SdWin32PlatformConfig(dx.windowHandle)))
				return false;

			std::vector<ID3D12Resource*> renderTargets = {};
			dx.FillRenderTargetArray(renderTargets);
			if (!renderer.Initialize({ dx.device.Get(), dx.commandQueue.Get(), renderTargets.data(), static_cast<UINT>(renderTargets.size()), dx.backBufferFormat }))
				return false;
			if (!fontBackend.Initialize())
				return false;
			if (!gui.Initialize(platform, renderer, fontBackend))
				return false;

			const Sodium::SdFontRequest fontRequests[] =
			{
				{ L"C:\\Windows\\Fonts\\segoeui.ttf", "Segoe UI" },
				{ L"C:\\Windows\\Fonts\\seguiemj.ttf", "Segoe UI Emoji" },
				{ L"C:\\Windows\\Fonts\\msyh.ttc", "Microsoft YaHei" },
				{ L"C:\\Windows\\Fonts\\simhei.ttf", "SimHei" },
			};
			for (const Sodium::SdFontRequest& request : fontRequests)
				fontBackend.LoadFont(request);

			ApplyTheme();
			initialized = true;
			return true;
		}

		void Render(IDXGISwapChain* hostSwapChain)
		{
			if (!initialized || unloadRequested.load() || !dx.Matches(hostSwapChain))
				return;
			if (!dx.RefreshSwapChainDesc())
				return;

			const auto frameStart = std::chrono::steady_clock::now();
			ApplyTheme();
			renderer.BeginFrame(dx.GetFrameIndex());
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

		void OnBeforeResize(IDXGISwapChain* hostSwapChain)
		{
			if (!initialized || !dx.Matches(hostSwapChain))
				return;

			renderer.ReleaseRenderTargets();
			dx.ReleaseRenderTargets();
		}

		void OnAfterResize(IDXGISwapChain* hostSwapChain)
		{
			if (!initialized || unloadRequested.load() || !dx.Matches(hostSwapChain))
				return;
			if (!dx.RefreshSwapChainDesc() || !dx.CreateRenderTargets())
			{
				unloadRequested.store(true);
				return;
			}

			std::vector<ID3D12Resource*> renderTargets = {};
			dx.FillRenderTargetArray(renderTargets);
			if (!renderer.SetRenderTargets(renderTargets.data(), static_cast<UINT>(renderTargets.size()), dx.backBufferFormat))
				unloadRequested.store(true);
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
		void ApplyTheme()
		{
			if (themeInitialized && appliedLightTheme == controls.lightTheme)
				return;

			Sodium::SdStyleSystem& styleSystem = gui.GetStyleSystem();
			if (controls.lightTheme)
			{
				styleSystem.SetColor(Sodium::SdDesignTokenIds::Text, { 28, 34, 42, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::ButtonText, { 28, 34, 42, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::Background, { 236, 241, 246, 0 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::WindowBg, { 250, 252, 255, 0 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::PanelBg, { 226, 234, 242, 232 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::ButtonBg, { 214, 228, 241, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::ButtonBgHover, { 190, 215, 238, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::ButtonBgPressed, { 158, 198, 230, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::Accent, { 30, 132, 112, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::Border, { 126, 145, 164, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::BorderStrong, { 82, 102, 122, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::Selection, { 30, 132, 112, 90 });
			}
			else
			{
				styleSystem.SetColor(Sodium::SdDesignTokenIds::Text, Sodium::SdColorWhite);
				styleSystem.SetColor(Sodium::SdDesignTokenIds::ButtonText, Sodium::SdColorWhite);
				styleSystem.SetColor(Sodium::SdDesignTokenIds::Background, { 24, 30, 39, 0 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::WindowBg, { 24, 30, 39, 0 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::PanelBg, { 35, 42, 52, 232 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::ButtonBg, { 48, 72, 96, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::ButtonBgHover, { 62, 100, 138, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::ButtonBgPressed, { 68, 118, 160, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::Accent, { 82, 170, 128, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::Border, { 91, 109, 128, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::BorderStrong, { 128, 154, 180, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::Selection, { 82, 170, 128, 96 });
			}

			styleSystem.SetMetric(Sodium::SdDesignTokenIds::SpacingSmall, 8.0f);
			styleSystem.SetMetric(Sodium::SdDesignTokenIds::SpacingMedium, 14.0f);
			styleSystem.SetMetric(Sodium::SdDesignTokenIds::RadiusSmall, 6.0f);
			appliedLightTheme = controls.lightTheme;
			themeInitialized = true;
		}

		void DeclareDemoUi()
		{
			bool buttonClicked = false;
			char fpsLine[160] = {};
			std::snprintf(fpsLine, sizeof(fpsLine), "SodiumGUI DX12 DLL overlay - %.1f FPS", liveFps);

			Sodium::SdWindowOptions windowOptions = {};
			windowOptions.position = { 44.0f, 42.0f };
			windowOptions.size = { 520.0f, 390.0f };
			windowOptions.backgroundBlurRadius = 12.0f;
			gui.ui.DeclareKeyed<Sodium::SdWindow>("dx12_dll_window", "DLL + Win32 + DirectX 12", controls.demoWindowOpen, windowOptions, [this, fpsLine, &buttonClicked](Sodium::SdUi& ui)
			{
				ui.Declare<Sodium::SdText>(fpsLine);
				ui.Declare<Sodium::SdText>("Home requests unload. The overlay renders on the host swap-chain back buffer.");
				ui.DeclareKeyed<Sodium::SdButton>("dx12_dll_button", "SdButton: increment", buttonClicked);
				ui.DeclareKeyed<Sodium::SdCheckBox>("dx12_dll_option", "SdCheckBox: option enabled", controls.optionEnabled);
				ui.DeclareKeyed<Sodium::SdCheckBox>("dx12_dll_light_theme", "Overlay palette: light", controls.lightTheme);
				ui.DeclareKeyed<Sodium::SdCheckBox>("dx12_dll_popup", "Show popup", controls.popupOpen);
				char clickLine[64] = {};
				std::snprintf(clickLine, sizeof(clickLine), "Clicks %d", controls.clickCount);
				ui.Declare<Sodium::SdText>(clickLine);
				char sliderLabel[96] = {};
				std::snprintf(sliderLabel, sizeof(sliderLabel), "SdSliderFloat %.2f", controls.sliderValue);
				ui.DeclareKeyed<Sodium::SdSliderFloat>("dx12_dll_slider", sliderLabel, controls.sliderValue, 0.0f, 1.0f);
				ui.DeclareKeyed<Sodium::SdTextInput>("dx12_dll_text_input", controls.textInputValue, "SdTextInput");
				ui.DeclareKeyed<Sodium::SdImageViewer>("dx12_dll_font_atlas", fontBackend.GetAtlasTexture(), Sodium::SdVec2{ 128.0f, 32.0f });
			});

			if (buttonClicked)
				++controls.clickCount;

			gui.ui.DeclareKeyed<Sodium::SdPopup>("dx12_dll_popup_window", controls.popupOpen, Sodium::SdVec2{ 590.0f, 86.0f }, [](Sodium::SdUi& ui)
			{
				ui.Declare<Sodium::SdText>("Popup rendered by the DX12 DLL sample");
				ui.Declare<Sodium::SdButton>("Popup action");
			});
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

	SD_HOOK(void STDMETHODCALLTYPE, ExecuteCommandLists, ID3D12CommandQueue* commandQueue, UINT commandListCount, ID3D12CommandList* const* commandLists)
	{
		CaptureCommandQueue(commandQueue);
		oExecuteCommandLists(commandQueue, commandListCount, commandLists);
	}

	SD_HOOK(HRESULT STDMETHODCALLTYPE, CreateSwapChain, IDXGIFactory* factory, IUnknown* device, DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** swapChain)
	{
		CaptureCommandQueue(device);
		return oCreateSwapChain(factory, device, desc, swapChain);
	}

	SD_HOOK(HRESULT STDMETHODCALLTYPE, CreateSwapChainForHwnd, IDXGIFactory2* factory, IUnknown* device, HWND hwnd, const DXGI_SWAP_CHAIN_DESC1* desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreenDesc, IDXGIOutput* restrictToOutput, IDXGISwapChain1** swapChain)
	{
		CaptureCommandQueue(device);
		return oCreateSwapChainForHwnd(factory, device, hwnd, desc, fullscreenDesc, restrictToOutput, swapChain);
	}

	SD_HOOK(HRESULT STDMETHODCALLTYPE, Present, IDXGISwapChain* swapChain, UINT syncInterval, UINT flags)
	{
		if ((flags & DXGI_PRESENT_TEST) != 0)
			return oPresent(swapChain, syncInterval, flags);

		if (!unloadRequested.load() && !overlay.initialized && capturedCommandQueue)
		{
			if (!overlay.Initialize(swapChain, capturedCommandQueue.Get()))
			{
				unloadRequested.store(true);
			}
			else if (!hookWndProc.IsAttached())
			{
				hookWndProc.Attach(reinterpret_cast<void*>(::GetWindowLongPtrW(overlay.dx.windowHandle, GWLP_WNDPROC)));
			}
		}

		overlay.Render(swapChain);
		return oPresent(swapChain, syncInterval, flags);
	}

	SD_HOOK(HRESULT STDMETHODCALLTYPE, ResizeBuffers, IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags)
	{
		overlay.OnBeforeResize(swapChain);
		const HRESULT result = oResizeBuffers(swapChain, bufferCount, width, height, newFormat, swapChainFlags);
		if (SUCCEEDED(result))
			overlay.OnAfterResize(swapChain);
		return result;
	}

	struct HookTargets final
	{
		void** factoryVTable = nullptr;
		void** swapChainVTable = nullptr;
		void** commandQueueVTable = nullptr;
	};

	bool FindDx12VTables(HookTargets& targets)
	{
		constexpr wchar_t kClassName[] = L"SodiumGUI.Dynamic.Dx12Probe";

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
			L"SodiumGUI Dynamic DX12 Probe",
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

		ComPtr<IDXGIFactory4> factory = {};
		ComPtr<ID3D12Device> device = {};
		ComPtr<ID3D12CommandQueue> commandQueue = {};
		ComPtr<IDXGISwapChain1> swapChain1 = {};
		ComPtr<IDXGISwapChain3> swapChain = {};

		bool initialized = false;
		if (SUCCEEDED(::CreateDXGIFactory2(0, IID_PPV_ARGS(factory.GetAddressOf()))))
		{
			HRESULT deviceResult = ::D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device.GetAddressOf()));
			if (FAILED(deviceResult))
			{
				ComPtr<IDXGIAdapter> warpAdapter = {};
				if (SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(warpAdapter.GetAddressOf()))))
					deviceResult = ::D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device.ReleaseAndGetAddressOf()));
			}

			if (SUCCEEDED(deviceResult))
			{
				D3D12_COMMAND_QUEUE_DESC queueDesc = {};
				queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
				queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
				if (SUCCEEDED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(commandQueue.GetAddressOf()))))
				{
					DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
					swapChainDesc.Width = 100;
					swapChainDesc.Height = 100;
					swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
					swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
					swapChainDesc.BufferCount = 2;
					swapChainDesc.SampleDesc.Count = 1;
					swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
					initialized = SUCCEEDED(factory->CreateSwapChainForHwnd(commandQueue.Get(), window, &swapChainDesc, nullptr, nullptr, swapChain1.GetAddressOf()))
						&& SUCCEEDED(swapChain1.As(&swapChain));
				}
			}
		}

		if (initialized)
		{
			targets.factoryVTable = *reinterpret_cast<void***>(factory.Get());
			targets.swapChainVTable = *reinterpret_cast<void***>(swapChain.Get());
			targets.commandQueueVTable = *reinterpret_cast<void***>(commandQueue.Get());
		}

		::DestroyWindow(window);
		::UnregisterClassW(kClassName, windowClass.hInstance);
		return initialized
			&& targets.factoryVTable
			&& targets.swapChainVTable
			&& targets.commandQueueVTable;
	}

	bool InitializeHooks()
	{
		HookTargets targets = {};
		if (!FindDx12VTables(targets))
			return false;

		if (!hookCreateSwapChain.Attach(targets.factoryVTable[10]))
			return false;
		if (!hookCreateSwapChainForHwnd.Attach(targets.factoryVTable[15]))
			return false;
		if (!hookExecuteCommandLists.Attach(targets.commandQueueVTable[10]))
			return false;
		if (!hookPresent.Attach(targets.swapChainVTable[8]))
			return false;
		if (!hookResizeBuffers.Attach(targets.swapChainVTable[13]))
			return false;
		return true;
	}

	void ShutdownHooks()
	{
		hookWndProc.Detach();
		hookResizeBuffers.Detach();
		hookPresent.Detach();
		hookExecuteCommandLists.Detach();
		hookCreateSwapChainForHwnd.Detach();
		hookCreateSwapChain.Detach();
	}

	DWORD WINAPI MainThread(LPVOID)
	{
		if (!InitializeHooks())
			unloadRequested.store(true);

		while (!unloadRequested.load())
			std::this_thread::sleep_for(16ms);

		ShutdownHooks();
		overlay.Shutdown();
		capturedCommandQueue.Reset();
		::FreeLibraryAndExitThread(selfModule, 0);
		return 0;
	}
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		SodiumDynamicDx12Example::selfModule = module;
		SodiumDynamicDx12Example::unloadRequested.store(false);
		::DisableThreadLibraryCalls(module);
		HANDLE thread = ::CreateThread(nullptr, 0, SodiumDynamicDx12Example::MainThread, nullptr, 0, nullptr);
		if (thread)
			::CloseHandle(thread);
	}
	return TRUE;
}
