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

	struct SodiumOverlay final
	{
		Dx11HostContext dx = {};
		Sodium::SdInstance gui = {};
		Sodium::Backends::SdWin32Platform platform = {};
		Sodium::Backends::SdDx11Renderer renderer = {};
		Sodium::Backends::SdFreeTypeFontBackend fontBackend = {};
		bool enabled = true;
		bool overlayWindowOpen = true;
		bool initialized = false;
		int clickCount = 0;
		Sodium::SdFrameIndex frameCount = 0;

		class OverlayWindow final : public Sodium::SdWidgetTag
		{
		public:
			void OnUpdate(Sodium::SdUpdateContext& context, bool& windowOpen, bool& enabled, int& clickCount, Sodium::SdFrameIndex frameCount)
			{
				if (!windowOpen)
					return;

				Sodium::SdWindowOptions windowOptions = {};
				windowOptions.initialPosition = { 52.0f, 48.0f };
				windowOptions.initialSize = { 380.0f, 260.0f };
				context.ui.Declare<Sodium::SdWindow>("SodiumGUI Overlay", windowOpen, windowOptions, [&](Sodium::SdUi& ui)
				{
					ui.Declare<Sodium::SdPanel>();
					ui.Declare<Sodium::SdText>("SodiumGUI dynamic DX11 sample");

					char frameText[96] = {};
					std::snprintf(frameText, sizeof(frameText), "Frame: %llu  HOME: unload", static_cast<unsigned long long>(frameCount));
					ui.Declare<Sodium::SdText>(frameText);

					Sodium::SdButton& button = ui.Declare<Sodium::SdButton>("Increment");
					if (button.WasClicked())
						++clickCount;

					ui.Declare<Sodium::SdCheckBox>(enabled);
					ui.Declare<Sodium::SdText>(enabled ? "Checkbox: enabled" : "Checkbox: disabled");

					char clickText[64] = {};
					std::snprintf(clickText, sizeof(clickText), "Button clicks: %d", clickCount);
					ui.Declare<Sodium::SdText>(clickText);
				});
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
			initialized = true;
			return true;
		}

		void Render()
		{
			if (!initialized || unloadRequested.load())
				return;
			if (!dx.RefreshDisplaySize())
				return;

			gui.BeginFrame(platform, dx.displaySize);
			gui.ui.Declare<OverlayWindow>(overlayWindowOpen, enabled, clickCount, frameCount);
			gui.EndFrame();

			ID3D11RenderTargetView* target = dx.renderTargetView.Get();
			dx.deviceContext->OMSetRenderTargets(1, &target, nullptr);
			gui.Render();
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
