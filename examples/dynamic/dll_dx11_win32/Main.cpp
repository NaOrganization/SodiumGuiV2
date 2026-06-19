#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

#include <SodiumGUI.h>
#include <backends/FreeTypeFontBackend.h>
#include <backends/SdDx11Renderer.h>
#include <backends/SdWin32Platform.h>

#include <detours/detours.h>

#pragma comment(lib, "d3d11.lib")

namespace SodiumDynamicExample
{
	inline HMODULE selfModule = nullptr;
	inline std::atomic_bool unloadRequested = false;
}

namespace SodiumDynamicExample
{
	using Microsoft::WRL::ComPtr;
	using namespace std::chrono_literals;

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

	struct Dx11HostContext final
	{
		ComPtr<IDXGISwapChain> swapChain = {};
		ComPtr<ID3D11Device> device = {};
		ComPtr<ID3D11DeviceContext> deviceContext = {};
		ComPtr<ID3D11RenderTargetView> renderTargetView = {};
		HWND windowHandle = nullptr;
		Sodium::SdVec2 displaySize = {};

		bool Initialize(IDXGISwapChain* hostSwapChain)
		{
			if (!hostSwapChain)
				return false;

			swapChain = hostSwapChain;
			if (FAILED(swapChain->GetDevice(IID_PPV_ARGS(device.ReleaseAndGetAddressOf()))))
				return false;

			device->GetImmediateContext(deviceContext.ReleaseAndGetAddressOf());

			DXGI_SWAP_CHAIN_DESC desc = {};
			if (FAILED(swapChain->GetDesc(&desc)))
				return false;

			windowHandle = desc.OutputWindow;
			return RefreshDisplaySize() && CreateRenderTarget();
		}

		bool CreateRenderTarget()
		{
			ComPtr<ID3D11Texture2D> backBuffer = {};
			if (!swapChain || FAILED(swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()))))
				return false;
			return SUCCEEDED(device->CreateRenderTargetView(backBuffer.Get(), nullptr, renderTargetView.ReleaseAndGetAddressOf()));
		}

		void ReleaseRenderTarget()
		{
			if (deviceContext)
				deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
			renderTargetView.Reset();
		}

		bool RefreshDisplaySize()
		{
			DXGI_SWAP_CHAIN_DESC desc = {};
			if (!swapChain || FAILED(swapChain->GetDesc(&desc)))
				return false;

			displaySize = {
				static_cast<float>(desc.BufferDesc.Width),
				static_cast<float>(desc.BufferDesc.Height)
			};

			if (displaySize.x <= 0.0f || displaySize.y <= 0.0f)
			{
				RECT rect = {};
				if (windowHandle && ::GetClientRect(windowHandle, &rect))
				{
					displaySize.x = static_cast<float>(rect.right - rect.left);
					displaySize.y = static_cast<float>(rect.bottom - rect.top);
				}
			}

			return displaySize.x > 0.0f && displaySize.y > 0.0f;
		}

		void Shutdown()
		{
			ReleaseRenderTarget();
			deviceContext.Reset();
			device.Reset();
			swapChain.Reset();
			windowHandle = nullptr;
			displaySize = {};
		}
	};

	struct DemoControlsState final
	{
		bool optionEnabled = true;
		bool mediaWindowOpen = true;
		bool popupOpen = true;
		bool contextMenuOpen = true;
		bool tooltipVisible = true;
		int clickCount = 0;
		float sliderValue = 0.45f;
		Sodium::SdUtf8String textInputValue = "Edit SodiumGUI text";
		Sodium::SdVec2 contextMenuPosition = { 560.0f, 300.0f };
	};

	struct SodiumOverlay final
	{
		Dx11HostContext dx = {};
		Sodium::SdInstance gui = {};
		Sodium::Backends::SdWin32Platform platform = {};
		Sodium::Backends::SdDx11Renderer renderer = {};
		Sodium::Backends::SdFreeTypeFontBackend fontBackend = {};
		bool overlayWindowOpen = true;
		bool initialized = false;
		DemoControlsState controls = {};
		Sodium::SdFrameIndex frameCount = 0;
		double liveFps = 0.0;
		std::chrono::steady_clock::time_point previousFrameTime = {};

		class OverlayWindow final : public Sodium::SdWidgetTag
		{
		public:
			struct State
			{
				bool hovered = false;
				int clickCount = 0;
				Sodium::SdVec2 mouse = {};
			};

			void OnUpdate(Sodium::SdUpdateContext& context, bool& windowOpen, DemoControlsState& controls, Sodium::SdFrameIndex frameCount, double liveFps, Sodium::SdTextureHandle fontAtlasTexture)
			{
				(void)fontAtlasTexture;
				(void)frameCount;
				(void)liveFps;
				if (context.input.IsKeyDown(Sodium::SdKeyCode::Esc))
					windowOpen = false;
				State& state = context.State<State>();
				state.hovered = context.IsHovered();
				state.mouse = context.input.GetMousePosition();
				if (context.WasClicked())
					++controls.clickCount;
				state.clickCount = controls.clickCount;
			}

			void OnLayout(Sodium::SdLayoutContext& context)
			{
				context.SetDesiredSize({ 410.0f, 150.0f });
				context.widgetState.manualLayout = true;
				context.widgetState.manualRect = { 42.0f, 42.0f, 452.0f, 192.0f };
				context.widgetState.layerPriority = Sodium::SdLayerPriority::Overlay;
				context.widgetState.styleClass = Sodium::SdStyleWidgetClass::Panel;
			}

			void OnPaint(Sodium::SdPaintContext& context)
			{
				const State& state = context.State<State>();
				const Sodium::SdTheme& theme = context.instance.GetStyleSystem().GetTheme();
				const Sodium::SdColor panel = theme.GetColor(Sodium::SdStyleToken::ColorWindowBg);
				const Sodium::SdColor accent = theme.GetColor(Sodium::SdStyleToken::ColorAccent);
				const Sodium::SdRect rect = context.animatedRect;
				context.renderList.AddRectFilled(rect, Sodium::SdColor(panel.r, panel.g, panel.b, static_cast<Sodium::SdUInt8>(panel.a * context.opacity)), context.clipRect, 5.0f);
				context.renderList.AddRect(rect, accent, context.clipRect, 1.0f, 5.0f);

				char line[160] = {};
				std::snprintf(line, sizeof(line), "SodiumGUI DLL overlay smoke");
				context.renderList.AddText(line, { rect.min.x + 14.0f, rect.min.y + 14.0f }, Sodium::SdColorWhite, context.clipRect);
				std::snprintf(line, sizeof(line), "Frame %llu  Mouse %.0f, %.0f", static_cast<unsigned long long>(context.instance.GetFrameIndex()), state.mouse.x, state.mouse.y);
				context.renderList.AddText(line, { rect.min.x + 14.0f, rect.min.y + 42.0f }, Sodium::SdColorWhite, context.clipRect);
				std::snprintf(line, sizeof(line), "Clicks %d  Hover %s", state.clickCount, state.hovered ? "yes" : "no");
				context.renderList.AddText(line, { rect.min.x + 14.0f, rect.min.y + 70.0f }, Sodium::SdColorWhite, context.clipRect);
			}
		};

		bool Initialize(IDXGISwapChain* hostSwapChain)
		{
			if (initialized)
				return true;
			if (!dx.Initialize(hostSwapChain))
				return false;
			if (!platform.Initialize(Sodium::Backends::SdWin32PlatformConfig(dx.windowHandle)))
				return false;
			if (!renderer.Initialize({ dx.device.Get(), dx.deviceContext.Get() }))
				return false;
			if (!fontBackend.Initialize())
				return false;

			gui.SetRendererBackend(&renderer);
			gui.SetFontBackend(&fontBackend);
			const Sodium::SdFontRequest fontRequests[] =
			{
				{ L"C:\\Windows\\Fonts\\segoeui.ttf", "Segoe UI" },
				{ L"C:\\Windows\\Fonts\\arial.ttf", "Arial" },
				{ L"C:\\Windows\\Fonts\\msyh.ttc", "Microsoft YaHei" },
				{ L"C:\\Windows\\Fonts\\simhei.ttf", "SimHei" },
				{ L"C:\\Windows\\Fonts\\simsun.ttc", "SimSun" },
				{ L"C:\\Windows\\Fonts\\Deng.ttf", "DengXian" }
			};
			for (const Sodium::SdFontRequest& request : fontRequests)
			{
				fontBackend.LoadFont(request);
			}
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
			gui.BeginFrame(platform, dx.displaySize);
			gui.ui.Declare<OverlayWindow>(overlayWindowOpen, controls, frameCount, liveFps, fontBackend.GetAtlasTexture());
			gui.EndFrame();

			ID3D11RenderTargetView* target = dx.renderTargetView.Get();
			dx.deviceContext->OMSetRenderTargets(1, &target, nullptr);
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

		void OnBeforeResize()
		{
			if (initialized)
				dx.ReleaseRenderTarget();
		}

		void OnAfterResize()
		{
			if (!initialized || unloadRequested.load())
				return;
			dx.RefreshDisplaySize();
			dx.CreateRenderTarget();
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

	SD_HOOK(HRESULT, Present, IDXGISwapChain* swapChain, UINT syncInterval, UINT flags)
	{
		if (!unloadRequested.load() && !overlay.initialized)
		{
			if (!overlay.Initialize(swapChain))
				unloadRequested.store(true);
			else if (!hookWndProc.IsAttached())
				hookWndProc.Attach(reinterpret_cast<void*>(::GetWindowLongPtrW(overlay.dx.windowHandle, GWLP_WNDPROC)));
		}

		overlay.Render();
		return oPresent(swapChain, syncInterval, flags);
	}

	SD_HOOK(HRESULT, ResizeBuffers, IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags)
	{
		overlay.OnBeforeResize();
		const HRESULT result = oResizeBuffers(swapChain, bufferCount, width, height, newFormat, swapChainFlags);
		if (SUCCEEDED(result))
			overlay.OnAfterResize();
		return result;
	}

	bool FindDxgiSwapChainVTable(void**& vTable)
	{
		constexpr wchar_t kClassName[] = L"SodiumGUI.Dynamic.Dx11Probe";

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
			L"SodiumGUI Dynamic DX11 Probe",
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

		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		swapChainDesc.BufferDesc.Width = 100;
		swapChainDesc.BufferDesc.Height = 100;
		swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
		swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 1;
		swapChainDesc.OutputWindow = window;
		swapChainDesc.Windowed = TRUE;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
		const D3D_FEATURE_LEVEL featureLevels[] =
		{
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0
		};

		ComPtr<IDXGISwapChain> swapChain = {};
		ComPtr<ID3D11Device> device = {};
		ComPtr<ID3D11DeviceContext> deviceContext = {};

		const HRESULT result = ::D3D11CreateDeviceAndSwapChain(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			0,
			featureLevels,
			static_cast<UINT>(std::size(featureLevels)),
			D3D11_SDK_VERSION,
			&swapChainDesc,
			swapChain.GetAddressOf(),
			device.GetAddressOf(),
			&featureLevel,
			deviceContext.GetAddressOf());

		if (SUCCEEDED(result))
			vTable = *reinterpret_cast<void***>(swapChain.Get());

		::DestroyWindow(window);
		::UnregisterClassW(kClassName, windowClass.hInstance);
		return SUCCEEDED(result) && vTable;
	}

	bool InitializeHooks()
	{
		void** swapChainVTable = nullptr;
		if (!FindDxgiSwapChainVTable(swapChainVTable))
			return false;

		return hookPresent.Attach(swapChainVTable[8])
			&& hookResizeBuffers.Attach(swapChainVTable[13]);
	}

	void ShutdownHooks()
	{
		hookWndProc.Detach();
		hookResizeBuffers.Detach();
		hookPresent.Detach();
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
		SodiumDynamicExample::selfModule = module;
		SodiumDynamicExample::unloadRequested.store(false);
		::DisableThreadLibraryCalls(module);
		::CreateThread(nullptr, 0, SodiumDynamicExample::MainThread, nullptr, 0, nullptr);
	}
	return TRUE;
}
