#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <iterator>
#include <limits>

#include <SodiumGUI.h>
#include <Backends/FreeType/FreeTypeFontBackend.h>
#include <Backends/Dx12/SdDx12Renderer.h>
#include <Backends/Win32/SdWin32Platform.h>

#include <Widget/SdBasicWidgets.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace
{
	using Microsoft::WRL::ComPtr;

	constexpr wchar_t kWindowClassName[] = L"SodiumGUI.Win32DirectX12.Example";
	constexpr UINT kInitialWindowWidth = 1024;
	constexpr UINT kInitialWindowHeight = 720;

	struct Dx12DeviceResources final
	{
		static constexpr UINT kFrameCount = 2;

		ComPtr<IDXGIFactory4> factory = {};
		ComPtr<ID3D12Device> device = {};
		ComPtr<ID3D12CommandQueue> commandQueue = {};
		ComPtr<IDXGISwapChain3> swapChain = {};
		std::array<ComPtr<ID3D12Resource>, kFrameCount> renderTargets = {};
		ComPtr<ID3D12Fence> fence = {};
		HANDLE fenceEvent = nullptr;
		UINT64 fenceValue = 1;
		UINT frameIndex = 0;
		UINT width = 0;
		UINT height = 0;
		DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		bool occluded = false;

		bool Create(HWND windowHandle, UINT initialWidth, UINT initialHeight)
		{
			width = initialWidth;
			height = initialHeight;

			UINT factoryFlags = 0;
#if defined(_DEBUG)
			ComPtr<ID3D12Debug> debugController = {};
			if (SUCCEEDED(::D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()))))
			{
				debugController->EnableDebugLayer();
				factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
#endif

			if (FAILED(::CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()))))
			{
				factoryFlags = 0;
				if (FAILED(::CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()))))
					return false;
			}

			if (!CreateDevice())
				return false;

			D3D12_COMMAND_QUEUE_DESC queueDesc = {};
			queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(commandQueue.ReleaseAndGetAddressOf()))))
				return false;

			DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
			swapChainDesc.Width = width;
			swapChainDesc.Height = height;
			swapChainDesc.Format = backBufferFormat;
			swapChainDesc.BufferCount = kFrameCount;
			swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapChainDesc.SampleDesc.Count = 1;
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

			ComPtr<IDXGISwapChain1> swapChain1 = {};
			if (FAILED(factory->CreateSwapChainForHwnd(commandQueue.Get(), windowHandle, &swapChainDesc, nullptr, nullptr, swapChain1.ReleaseAndGetAddressOf())))
				return false;

			factory->MakeWindowAssociation(windowHandle, DXGI_MWA_NO_ALT_ENTER);
			if (FAILED(swapChain1.As(&swapChain)))
				return false;

			frameIndex = swapChain->GetCurrentBackBufferIndex();
			if (!CreateRenderTargets())
				return false;

			if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.ReleaseAndGetAddressOf()))))
				return false;
			fenceEvent = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
			return fenceEvent != nullptr;
		}

		bool CreateDevice()
		{
			if (SUCCEEDED(::D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device.ReleaseAndGetAddressOf()))))
				return true;

			ComPtr<IDXGIAdapter> warpAdapter = {};
			if (FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(warpAdapter.GetAddressOf()))))
				return false;
			return SUCCEEDED(::D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device.ReleaseAndGetAddressOf())));
		}

		bool CreateRenderTargets()
		{
			for (UINT i = 0; i < kFrameCount; ++i)
			{
				if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(renderTargets[i].ReleaseAndGetAddressOf()))))
					return false;
			}
			return true;
		}

		void FillRenderTargetArray(std::array<ID3D12Resource*, kFrameCount>& targets) const noexcept
		{
			for (UINT i = 0; i < kFrameCount; ++i)
				targets[i] = renderTargets[i].Get();
		}

		bool Resize(UINT newWidth, UINT newHeight)
		{
			if (!swapChain || newWidth == 0 || newHeight == 0)
				return true;
			if (newWidth == width && newHeight == height)
				return true;

			WaitForGpu();
			for (ComPtr<ID3D12Resource>& renderTarget : renderTargets)
				renderTarget.Reset();

			width = newWidth;
			height = newHeight;
			if (FAILED(swapChain->ResizeBuffers(kFrameCount, width, height, backBufferFormat, 0)))
				return false;

			frameIndex = swapChain->GetCurrentBackBufferIndex();
			return CreateRenderTargets();
		}

		void Present()
		{
			const HRESULT hr = swapChain->Present(0, 0);
			occluded = hr == DXGI_STATUS_OCCLUDED;
			frameIndex = swapChain->GetCurrentBackBufferIndex();
		}

		void WaitForGpu()
		{
			if (!commandQueue || !fence || !fenceEvent)
				return;

			const UINT64 signalValue = fenceValue++;
			if (FAILED(commandQueue->Signal(fence.Get(), signalValue)))
				return;

			if (fence->GetCompletedValue() < signalValue)
			{
				if (SUCCEEDED(fence->SetEventOnCompletion(signalValue, fenceEvent)))
					::WaitForSingleObject(fenceEvent, INFINITE);
			}
		}

		void Shutdown()
		{
			WaitForGpu();
			for (ComPtr<ID3D12Resource>& renderTarget : renderTargets)
				renderTarget.Reset();
			swapChain.Reset();
			commandQueue.Reset();
			fence.Reset();
			device.Reset();
			factory.Reset();
			if (fenceEvent)
			{
				::CloseHandle(fenceEvent);
				fenceEvent = nullptr;
			}
		}
	};

	struct ExampleApplication final
	{
		HINSTANCE instance = nullptr;
		HWND windowHandle = nullptr;
		Dx12DeviceResources dx = {};
		UINT resizeWidth = 0;
		UINT resizeHeight = 0;
		bool running = true;
		bool optionEnabled = true;
		bool demoWindowOpen = true;
		bool popupOpen = true;
		bool lightTheme = false;
		int clickCount = 0;
		float sliderValue = 0.45f;
		Sodium::SdUtf8String textInputValue = "Edit SodiumGUI text";
		UINT64 frameCount = 0;
		double liveFps = 0.0;
		std::chrono::steady_clock::time_point previousFrameTime = {};

		Sodium::SdInstance gui = {};
		Sodium::Backends::SdWin32Platform platform = {};
		Sodium::Backends::SdDx12Renderer renderer = {};
		Sodium::Backends::SdFreeTypeFontBackend fontBackend = {};

		bool Initialize(HINSTANCE hInstance, int showCommand)
		{
			instance = hInstance;
			if (!RegisterWindowClass())
				return false;
			if (!CreateWindowInstance(showCommand))
				return false;
			if (!dx.Create(windowHandle, kInitialWindowWidth, kInitialWindowHeight))
				return false;
			if (!platform.Initialize(Sodium::Backends::SdWin32PlatformConfig(windowHandle)))
				return false;

			std::array<ID3D12Resource*, Dx12DeviceResources::kFrameCount> renderTargets = {};
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
			};
			for (const Sodium::SdFontRequest& request : fontRequests)
				fontBackend.LoadFont(request);

			ConfigureStyleSystem();
			return true;
		}

		int Run()
		{
			previousFrameTime = std::chrono::steady_clock::now();
			while (running)
			{
				MSG msg = {};
				while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
				{
					::TranslateMessage(&msg);
					::DispatchMessage(&msg);
					if (msg.message == WM_QUIT)
						running = false;
				}
				if (!running)
					break;
				if (dx.occluded)
					::Sleep(16);

				if (resizeWidth != 0 && resizeHeight != 0)
				{
					renderer.ReleaseRenderTargets();
					if (!dx.Resize(resizeWidth, resizeHeight))
						break;
					std::array<ID3D12Resource*, Dx12DeviceResources::kFrameCount> renderTargets = {};
					dx.FillRenderTargetArray(renderTargets);
					if (!renderer.SetRenderTargets(renderTargets.data(), static_cast<UINT>(renderTargets.size()), dx.backBufferFormat))
						break;
					resizeWidth = 0;
					resizeHeight = 0;
				}

				const auto frameStart = std::chrono::steady_clock::now();
				renderer.BeginFrame(dx.frameIndex, GetClearColor());
				gui.BeginFrame();
				DeclareDemoUi();
				gui.EndFrame();
				gui.Render();
				dx.Present();

				const auto frameEnd = std::chrono::steady_clock::now();
				const double frameSeconds = std::chrono::duration<double>(frameEnd - previousFrameTime).count();
				liveFps = frameSeconds > 0.0 ? 1.0 / frameSeconds : 0.0;
				previousFrameTime = frameEnd;
				++frameCount;
				(void)frameStart;
			}
			return 0;
		}

		void Shutdown()
		{
			gui.Shutdown();
			fontBackend.Shutdown();
			renderer.Shutdown();
			platform.Shutdown();
			dx.Shutdown();
			if (windowHandle)
			{
				::DestroyWindow(windowHandle);
				windowHandle = nullptr;
			}
			::UnregisterClassW(kWindowClassName, instance);
		}

		LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
		{
			platform.HandleMessage(hwnd, message, wParam, lParam, gui.GetInputSystem());
			if (message == WM_LBUTTONDOWN)
				::SetFocus(hwnd);
			if (message == WM_SIZE && dx.swapChain && wParam != SIZE_MINIMIZED)
			{
				resizeWidth = static_cast<UINT>(LOWORD(lParam));
				resizeHeight = static_cast<UINT>(HIWORD(lParam));
				return 0;
			}
			if (message == WM_DESTROY)
			{
				running = false;
				::PostQuitMessage(0);
			}
			return ::DefWindowProcW(hwnd, message, wParam, lParam);
		}

	private:
		void ConfigureStyleSystem()
		{
			Sodium::SdStyleSystem& styleSystem = gui.GetStyleSystem();
			styleSystem.SetColor(Sodium::SdDesignTokenIds::Text, Sodium::SdColorWhite);
			styleSystem.SetColor(Sodium::SdDesignTokenIds::ButtonText, Sodium::SdColorWhite);
			styleSystem.SetColor(Sodium::SdDesignTokenIds::Background, { 22, 27, 34, 242 });
			styleSystem.SetColor(Sodium::SdDesignTokenIds::WindowBg, { 22, 27, 34, 0 });
			styleSystem.SetColor(Sodium::SdDesignTokenIds::PanelBg, { 34, 42, 52, 235 });
			styleSystem.SetColor(Sodium::SdDesignTokenIds::ButtonBg, { 48, 75, 102, 255 });
			styleSystem.SetColor(Sodium::SdDesignTokenIds::ButtonBgHover, { 60, 104, 142, 255 });
			styleSystem.SetColor(Sodium::SdDesignTokenIds::ButtonBgPressed, { 70, 126, 168, 255 });
			styleSystem.SetColor(Sodium::SdDesignTokenIds::Accent, { 84, 179, 132, 255 });
			styleSystem.SetColor(Sodium::SdDesignTokenIds::Border, { 92, 111, 130, 255 });
			styleSystem.SetColor(Sodium::SdDesignTokenIds::BorderStrong, { 130, 156, 184, 255 });
			styleSystem.SetMetric(Sodium::SdDesignTokenIds::SpacingSmall, 10.0f);
			styleSystem.SetMetric(Sodium::SdDesignTokenIds::SpacingMedium, 18.0f);
			styleSystem.SetMetric(Sodium::SdDesignTokenIds::RadiusSmall, 8.0f);
		}

		std::array<float, 4> GetClearColor() const
		{
			if (lightTheme)
				return { 0.92f, 0.94f, 0.96f, 1.0f };
			return { 0.055f, 0.065f, 0.08f, 1.0f };
		}

		void DeclareDemoUi()
		{
			bool buttonClicked = false;
			char fpsLine[128] = {};
			std::snprintf(fpsLine, sizeof(fpsLine), "SodiumGUI DirectX 12 backend - %.1f FPS", liveFps);

			Sodium::SdWindowOptions windowOptions = {};
			windowOptions.position = { 44.0f, 42.0f };
			windowOptions.size = { 510.0f, 390.0f };
			windowOptions.backgroundBlurRadius = 12.0f;
			gui.ui.DeclareKeyed<Sodium::SdWindow>("dx12_demo_window", "Win32 + DirectX 12", demoWindowOpen, windowOptions, [this, fpsLine, &buttonClicked](Sodium::SdUi& ui)
			{
				ui.Declare<Sodium::SdText>(fpsLine);
				ui.Declare<Sodium::SdText>("The renderer records native DX12 commands for the swap-chain back buffer.");
				ui.DeclareKeyed<Sodium::SdButton>("dx12_button", "SdButton: increment", buttonClicked);
				ui.DeclareKeyed<Sodium::SdCheckBox>("dx12_option", "SdCheckBox: option enabled", optionEnabled);
				ui.DeclareKeyed<Sodium::SdCheckBox>("dx12_light_theme", "Clear color: light", lightTheme);
				ui.DeclareKeyed<Sodium::SdCheckBox>("dx12_popup", "Show popup", popupOpen);
				char clickLine[64] = {};
				std::snprintf(clickLine, sizeof(clickLine), "Clicks %d", clickCount);
				ui.Declare<Sodium::SdText>(clickLine);
				char sliderLabel[96] = {};
				std::snprintf(sliderLabel, sizeof(sliderLabel), "SdSliderFloat %.2f", sliderValue);
				ui.DeclareKeyed<Sodium::SdSliderFloat>("dx12_slider", sliderLabel, sliderValue, 0.0f, 1.0f);
				ui.DeclareKeyed<Sodium::SdTextInput>("dx12_text_input", textInputValue, "SdTextInput");
				ui.DeclareKeyed<Sodium::SdImageViewer>("dx12_font_atlas", fontBackend.GetAtlasTexture(), Sodium::SdVec2{ 128.0f, 32.0f });
			});

			gui.ui.DeclareKeyed<Sodium::SdWindow>("dx12_demo_window1", "Win32 + DirectX 12", demoWindowOpen, windowOptions, [this, fpsLine, &buttonClicked](Sodium::SdUi& ui)
				{
					ui.Declare<Sodium::SdText>(fpsLine);
					ui.Declare<Sodium::SdText>("The renderer records native DX12 commands for the swap-chain back buffer.");
					ui.DeclareKeyed<Sodium::SdButton>("dx12_button", "SdButton: increment", buttonClicked);
					ui.DeclareKeyed<Sodium::SdCheckBox>("dx12_option", "SdCheckBox: option enabled", optionEnabled);
					ui.DeclareKeyed<Sodium::SdCheckBox>("dx12_light_theme", "Clear color: light", lightTheme);
					ui.DeclareKeyed<Sodium::SdCheckBox>("dx12_popup", "Show popup", popupOpen);
					char clickLine[64] = {};
					std::snprintf(clickLine, sizeof(clickLine), "Clicks %d", clickCount);
					ui.Declare<Sodium::SdText>(clickLine);
					char sliderLabel[96] = {};
					std::snprintf(sliderLabel, sizeof(sliderLabel), "SdSliderFloat %.2f", sliderValue);
					ui.DeclareKeyed<Sodium::SdSliderFloat>("dx12_slider", sliderLabel, sliderValue, 0.0f, 1.0f);
					ui.DeclareKeyed<Sodium::SdTextInput>("dx12_text_input", textInputValue, "SdTextInput");
					ui.DeclareKeyed<Sodium::SdImageViewer>("dx12_font_atlas", fontBackend.GetAtlasTexture(), Sodium::SdVec2{ 128.0f, 32.0f });
				});

			gui.ui.DeclareKeyed<Sodium::SdWindow>("dx12_demo_window2", "Win32 + DirectX 12", demoWindowOpen, windowOptions, [this, fpsLine, &buttonClicked](Sodium::SdUi& ui)
				{
					ui.Declare<Sodium::SdText>(fpsLine);
					ui.Declare<Sodium::SdText>("The renderer records native DX12 commands for the swap-chain back buffer.");
					ui.DeclareKeyed<Sodium::SdButton>("dx12_button", "SdButton: increment", buttonClicked);
					ui.DeclareKeyed<Sodium::SdCheckBox>("dx12_option", "SdCheckBox: option enabled", optionEnabled);
					ui.DeclareKeyed<Sodium::SdCheckBox>("dx12_light_theme", "Clear color: light", lightTheme);
					ui.DeclareKeyed<Sodium::SdCheckBox>("dx12_popup", "Show popup", popupOpen);
					char clickLine[64] = {};
					std::snprintf(clickLine, sizeof(clickLine), "Clicks %d", clickCount);
					ui.Declare<Sodium::SdText>(clickLine);
					char sliderLabel[96] = {};
					std::snprintf(sliderLabel, sizeof(sliderLabel), "SdSliderFloat %.2f", sliderValue);
					ui.DeclareKeyed<Sodium::SdSliderFloat>("dx12_slider", sliderLabel, sliderValue, 0.0f, 1.0f);
					ui.DeclareKeyed<Sodium::SdTextInput>("dx12_text_input", textInputValue, "SdTextInput");
					ui.DeclareKeyed<Sodium::SdImageViewer>("dx12_font_atlas", fontBackend.GetAtlasTexture(), Sodium::SdVec2{ 128.0f, 32.0f });
				});

			if (buttonClicked)
				++clickCount;

			gui.ui.DeclareKeyed<Sodium::SdPopup>("dx12_popup_window", popupOpen, Sodium::SdVec2{ 586.0f, 86.0f }, [](Sodium::SdUi& ui)
			{
				ui.Declare<Sodium::SdText>("Popup rendered on the DX12 sample back buffer");
				ui.Declare<Sodium::SdButton>("Popup action");
			});
		}

		bool RegisterWindowClass()
		{
			WNDCLASSEXW windowClass = {};
			windowClass.cbSize = sizeof(windowClass);
			windowClass.style = CS_HREDRAW | CS_VREDRAW;
			windowClass.lpfnWndProc = WindowProc;
			windowClass.hInstance = instance;
			windowClass.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
			windowClass.lpszClassName = kWindowClassName;
			return ::RegisterClassExW(&windowClass) != 0;
		}

		bool CreateWindowInstance(int showCommand)
		{
			RECT rect = { 0, 0, static_cast<LONG>(kInitialWindowWidth), static_cast<LONG>(kInitialWindowHeight) };
			::AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
			windowHandle = ::CreateWindowExW(
				0,
				kWindowClassName,
				L"SodiumGUI Win32 DirectX12 Example",
				WS_OVERLAPPEDWINDOW,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				rect.right - rect.left,
				rect.bottom - rect.top,
				nullptr,
				nullptr,
				instance,
				this);
			if (!windowHandle)
				return false;
			::ShowWindow(windowHandle, showCommand);
			::UpdateWindow(windowHandle);
			return true;
		}

		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
		{
			ExampleApplication* app = nullptr;
			if (message == WM_NCCREATE)
			{
				const CREATESTRUCTW* createStruct = reinterpret_cast<const CREATESTRUCTW*>(lParam);
				app = static_cast<ExampleApplication*>(createStruct->lpCreateParams);
				::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
			}
			else
			{
				app = reinterpret_cast<ExampleApplication*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
			}
			if (app)
				return app->HandleMessage(hwnd, message, wParam, lParam);
			return ::DefWindowProcW(hwnd, message, wParam, lParam);
		}
	};
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int showCommand)
{
	ExampleApplication app = {};
	if (!app.Initialize(hInstance, showCommand))
	{
		::MessageBoxA(nullptr, "Failed to initialize SodiumGUI DirectX 12 example.", "SodiumGUI", MB_ICONERROR | MB_OK);
		app.Shutdown();
		return 1;
	}

	const int result = app.Run();
	app.Shutdown();
	return result;
}
