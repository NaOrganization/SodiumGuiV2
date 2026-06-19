#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cwchar>
#include <iterator>
#include <limits>
#include <fstream>
#include <string>
#include <vector>

#include <SodiumGUI.h>
#include <backends/FreeTypeFontBackend.h>
#include <backends/SdDx11Renderer.h>
#include <backends/SdWin32Platform.h>

#pragma comment(lib, "d3d11.lib")

namespace
{
	using Microsoft::WRL::ComPtr;

	constexpr wchar_t kWindowClassName[] = L"SodiumGUI.Win32DirectX11.Example";
	constexpr UINT kInitialWindowWidth = 1024;
	constexpr UINT kInitialWindowHeight = 720;

	struct Dx11DeviceResources final
	{
		ComPtr<ID3D11Device> device = {};
		ComPtr<ID3D11DeviceContext> context = {};
		ComPtr<IDXGISwapChain> swapChain = {};
		ComPtr<ID3D11RenderTargetView> renderTargetView = {};
		UINT width = 0;
		UINT height = 0;
		bool occluded = false;

		bool Create(HWND windowHandle, UINT initialWidth, UINT initialHeight)
		{
			width = initialWidth;
			height = initialHeight;

			DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
			swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			swapChainDesc.SampleDesc.Count = 1;
			swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapChainDesc.BufferCount = 2;
			swapChainDesc.OutputWindow = windowHandle;
			swapChainDesc.Windowed = TRUE;
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

			UINT flags = 0;
#if defined(_DEBUG)
			flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

			D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
			const D3D_FEATURE_LEVEL featureLevels[] =
			{
				D3D_FEATURE_LEVEL_11_1,
				D3D_FEATURE_LEVEL_11_0,
				D3D_FEATURE_LEVEL_10_1,
				D3D_FEATURE_LEVEL_10_0
			};

			HRESULT hr = ::D3D11CreateDeviceAndSwapChain(
				nullptr,
				D3D_DRIVER_TYPE_HARDWARE,
				nullptr,
				flags,
				featureLevels,
				static_cast<UINT>(std::size(featureLevels)),
				D3D11_SDK_VERSION,
				&swapChainDesc,
				swapChain.ReleaseAndGetAddressOf(),
				device.ReleaseAndGetAddressOf(),
				&featureLevel,
				context.ReleaseAndGetAddressOf());

#if defined(_DEBUG)
			if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING)
			{
				flags &= ~D3D11_CREATE_DEVICE_DEBUG;
				hr = ::D3D11CreateDeviceAndSwapChain(
					nullptr,
					D3D_DRIVER_TYPE_HARDWARE,
					nullptr,
					flags,
					featureLevels,
					static_cast<UINT>(std::size(featureLevels)),
					D3D11_SDK_VERSION,
					&swapChainDesc,
					swapChain.ReleaseAndGetAddressOf(),
					device.ReleaseAndGetAddressOf(),
					&featureLevel,
					context.ReleaseAndGetAddressOf());
			}
#endif

			if (FAILED(hr))
				return false;
			return CreateRenderTarget();
		}

		bool CreateRenderTarget()
		{
			ComPtr<ID3D11Texture2D> backBuffer = {};
			if (FAILED(swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()))))
				return false;
			return SUCCEEDED(device->CreateRenderTargetView(backBuffer.Get(), nullptr, renderTargetView.ReleaseAndGetAddressOf()));
		}

		void CleanupRenderTarget()
		{
			renderTargetView.Reset();
		}

		bool Resize(UINT newWidth, UINT newHeight)
		{
			if (!swapChain || newWidth == 0 || newHeight == 0)
				return true;
			if (newWidth == width && newHeight == height)
				return true;

			width = newWidth;
			height = newHeight;
			CleanupRenderTarget();
			context->OMSetRenderTargets(0, nullptr, nullptr);
			if (FAILED(swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0)))
				return false;
			return CreateRenderTarget();
		}

		void BeginFrame(const std::array<float, 4>& clearColor)
		{
			ID3D11RenderTargetView* target = renderTargetView.Get();
			context->OMSetRenderTargets(1, &target, nullptr);
			context->ClearRenderTargetView(renderTargetView.Get(), clearColor.data());
		}

		void Present()
		{
			const HRESULT hr = swapChain->Present(0, 0);
			occluded = hr == DXGI_STATUS_OCCLUDED;
		}

		void Shutdown()
		{
			CleanupRenderTarget();
			swapChain.Reset();
			context.Reset();
			device.Reset();
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

	struct ExampleApplication final
	{
		HINSTANCE instance = nullptr;
		HWND windowHandle = nullptr;
		Dx11DeviceResources dx = {};
		UINT resizeWidth = 0, resizeHeight = 0;
		bool running = true;
		bool demoWindowOpen = true;
		DemoControlsState demoControls = {};
		UINT64 frameCount = 0;
		double liveFps = 0.0;
		double accumulatedFrameSeconds = 0.0;
		double minFrameSeconds = std::numeric_limits<double>::max();
		double maxFrameSeconds = 0.0;
		std::chrono::steady_clock::time_point statsStart = {};
		std::chrono::steady_clock::time_point previousFrameTime = {};

		Sodium::SdInstance gui = {};
		Sodium::Backends::SdWin32Platform platform = {};
		Sodium::Backends::SdDx11Renderer renderer = {};
		Sodium::Backends::SdFreeTypeFontBackend fontBackend = {};

		class DemoWindow final : public Sodium::SdWidgetTag
		{
		public:
			struct State
			{
				bool hovered = false;
				int clickCount = 0;
				Sodium::SdVec2 mouse = {};
			};

			void OnUpdate(Sodium::SdUpdateContext& context, bool& windowOpen, DemoControlsState& controls, UINT64 frameCount, double liveFps, Sodium::SdTextureHandle fontAtlasTexture)
			{
				(void)fontAtlasTexture;
				if (context.input.IsKeyDown(Sodium::SdKeyCode::Esc))
					windowOpen = false;
				State& state = context.State<State>();
				state.hovered = context.IsHovered();
				state.mouse = context.input.GetMousePosition();
				if (context.WasClicked())
					++controls.clickCount;
				state.clickCount = controls.clickCount;
				controls.sliderValue = static_cast<float>((frameCount % 240) / 239.0);
				liveFps = std::max(0.0, liveFps);
			}

			void OnLayout(Sodium::SdLayoutContext& context)
			{
				context.SetDesiredSize({ 430.0f, 220.0f });
				context.widgetState.manualLayout = true;
				context.widgetState.manualRect = { 48.0f, 42.0f, 478.0f, 262.0f };
				context.widgetState.styleClass = Sodium::SdStyleWidgetClass::Panel;
			}

			void OnPaint(Sodium::SdPaintContext& context)
			{
				const State& state = context.State<State>();
				const Sodium::SdTheme& theme = context.instance.GetStyleSystem().GetTheme();
				const Sodium::SdColor panel = theme.GetColor(Sodium::SdStyleToken::ColorPanelBg);
				const Sodium::SdColor accent = theme.GetColor(Sodium::SdStyleToken::ColorAccent);
				const Sodium::SdRect rect = context.animatedRect;
				context.renderList.AddRectFilled(rect, Sodium::SdColor(panel.r, panel.g, panel.b, static_cast<Sodium::SdUInt8>(panel.a * context.opacity)), context.clipRect, 5.0f);
				context.renderList.AddRect(rect, accent, context.clipRect, 1.0f, 5.0f);

				char line[160] = {};
				std::snprintf(line, sizeof(line), "SodiumGUI core smoke - no built-in widgets");
				context.renderList.AddText(line, { rect.min.x + 14.0f, rect.min.y + 16.0f }, Sodium::SdColorWhite, context.clipRect);
				std::snprintf(line, sizeof(line), "Frame %llu  Mouse %.0f, %.0f", static_cast<unsigned long long>(context.instance.GetFrameIndex()), state.mouse.x, state.mouse.y);
				context.renderList.AddText(line, { rect.min.x + 14.0f, rect.min.y + 44.0f }, Sodium::SdColorWhite, context.clipRect);
				std::snprintf(line, sizeof(line), "Clicks %d  Hover %s", state.clickCount, state.hovered ? "yes" : "no");
				context.renderList.AddText(line, { rect.min.x + 14.0f, rect.min.y + 72.0f }, Sodium::SdColorWhite, context.clipRect);
			}
		};

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
			if (!renderer.Initialize(Sodium::Backends::SdDx11RendererConfig(dx.device.Get(), dx.context.Get())))
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
			return true;
		}

		int Run()
		{
			const std::array<float, 4> clearColor = { 0.055f, 0.065f, 0.08f, 1.0f };
			statsStart = std::chrono::steady_clock::now();
			previousFrameTime = statsStart;
			while (running)
			{
				const auto frameStart = std::chrono::steady_clock::now();
				MSG msg;
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

				dx.Resize(resizeWidth, resizeHeight);

				dx.BeginFrame(clearColor);
				const Sodium::SdVec2 displaySize = { static_cast<float>(dx.width), static_cast<float>(dx.height) };
				gui.BeginFrame(platform, displaySize);
				gui.ui.Declare<DemoWindow>(demoWindowOpen, demoControls, frameCount, liveFps, fontBackend.GetAtlasTexture());
				gui.EndFrame();
				gui.Render();
				dx.Present();

				const auto frameEnd = std::chrono::steady_clock::now();
				const double frameSeconds = std::chrono::duration<double>(frameEnd - frameStart).count();
				++frameCount;
				accumulatedFrameSeconds += frameSeconds;
				minFrameSeconds = std::min(minFrameSeconds, frameSeconds);
				maxFrameSeconds = std::max(maxFrameSeconds, frameSeconds);
				liveFps = frameSeconds > 0.0 ? 1.0 / frameSeconds : 0.0;
				previousFrameTime = frameEnd;
			}
			OutputPerformanceStats();
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
				resizeWidth = (UINT)LOWORD(lParam); // Queue resize
				resizeHeight = (UINT)HIWORD(lParam);
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
		void OutputPerformanceStats()
		{
			const auto statsEnd = std::chrono::steady_clock::now();
			const double totalSeconds = std::chrono::duration<double>(statsEnd - statsStart).count();
			const double averageFrameSeconds = frameCount > 0
				? accumulatedFrameSeconds / static_cast<double>(frameCount)
				: 0.0;
			const double averageFps = totalSeconds > 0.0
				? static_cast<double>(frameCount) / totalSeconds
				: 0.0;
			if (frameCount == 0)
				minFrameSeconds = 0.0;

			const Sodium::SdFrameDiagnostics& diagnostics = gui.GetDiagnostics();
			const Sodium::SdRenderStats& renderStats = gui.GetRenderStats();

			char message[1024] = {};
			std::snprintf(
				message,
				sizeof(message),
				"SodiumGUI performance summary\n"
				"Frames: %llu\n"
				"Elapsed: %.3f s\n"
				"Average FPS: %.2f\n"
				"Average frame: %.3f ms\n"
				"Min frame: %.3f ms\n"
				"Max frame: %.3f ms\n"
				"Widgets: submitted=%u live=%u entering=%u leaving=%u removed=%u\n"
				"Draw: commands=%u batches=%u vertices=%u indices=%u uploads=%u\n"
				"Runtime: hitTests=%u activeAnimations=%u renderListGrows=%u aaScratchGrows=%u\n",
				static_cast<unsigned long long>(frameCount),
				totalSeconds,
				averageFps,
				averageFrameSeconds * 1000.0,
				minFrameSeconds * 1000.0,
				maxFrameSeconds * 1000.0,
				diagnostics.submittedWidgetCount,
				diagnostics.liveWidgetCount,
				diagnostics.enteringWidgetCount,
				diagnostics.leavingWidgetCount,
				diagnostics.removedWidgetCount,
				diagnostics.drawCommandCount,
				diagnostics.drawBatchCount,
				diagnostics.drawVertexCount,
				diagnostics.drawIndexCount,
				diagnostics.resourceUploadCount,
				diagnostics.hitTestRecordCount,
				diagnostics.activeAnimationChannelCount,
				renderStats.renderListGrowCount,
				renderStats.aaScratchGrowCount);

			::OutputDebugStringA(message);
			{
				std::ofstream output("SodiumGUI_Performance.txt", std::ios::out | std::ios::trunc);
				if (output)
					output << message;
			}
			::MessageBoxA(nullptr, message, "SodiumGUI Performance", MB_OK | MB_ICONINFORMATION | MB_TASKMODAL | MB_SETFOREGROUND);
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
				L"SodiumGUI Win32 DirectX11 Example",
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
		::MessageBoxA(nullptr, "Failed to initialize SodiumGUI DirectX 11 example.", "SodiumGUI", MB_ICONERROR | MB_OK);
		app.Shutdown();
		return 1;
	}

	const int result = app.Run();
	app.Shutdown();
	return result;
}
