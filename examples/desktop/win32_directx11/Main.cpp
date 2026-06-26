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
#include <Backends/FreeType/FreeTypeFontBackend.h>
#include <Backends/Dx11/SdDx11Renderer.h>
#include <Backends/Win32/SdWin32Platform.h>

#include <Widget/SdBasicWidgets.h>

using namespace Sodium::Literals;

#pragma comment(lib, "d3d11.lib")

namespace
{
	using Microsoft::WRL::ComPtr;

	constexpr wchar_t kWindowClassName[] = L"SodiumGUI.Win32DirectX11.Example";
	constexpr UINT kInitialWindowWidth = 1024;
	constexpr UINT kInitialWindowHeight = 720;
	constexpr Sodium::SdStyleClassId kExampleAccentTextClass = "Sodium.Example.Text.Accent"_SdId;
	constexpr Sodium::SdStyleClassId kExampleWarningTextClass = "Sodium.Example.Text.Warning"_SdId;
	constexpr Sodium::SdStyleClassId kExamplePaddedTextClass = "Sodium.Example.Text.Padded"_SdId;
	constexpr Sodium::SdStyleClassId kExampleDemoWindowClass = "Sodium.Example.Window.Demo"_SdId;
	constexpr Sodium::SdStyleClassId kExampleBasicPanelClass = "Sodium.Example.Panel.Basic"_SdId;
	constexpr Sodium::SdStyleClassId kExampleBasicScrollClass = "Sodium.Example.Scroll.Basic"_SdId;
	constexpr Sodium::SdStyleScopeId kExampleDemoWindowScope = "Sodium.Example.Scope.DemoWindow.Root"_SdId;
	constexpr Sodium::SdStyleScopeId kExampleDemoTextScope = "Sodium.Example.Scope.DemoWindow"_SdId;
	constexpr Sodium::SdStyleScopeId kExampleDemoPanelScope = "Sodium.Example.Scope.DemoPanel"_SdId;
	constexpr Sodium::SdStyleScopeId kExampleDemoScrollScope = "Sodium.Example.Scope.DemoScroll"_SdId;

	void ConfigureBuiltInThemeTransitions(Sodium::SdStyleSystem& styleSystem)
	{
		(void)styleSystem;
	}

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
		bool metricsWindowOpen = true;
		bool toolsWindowOpen = true;
		bool popupOpen = true;
		bool contextMenuOpen = true;
		bool tooltipVisible = true;
		bool lightTheme = false;
		bool compactTheme = false;
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
		bool themeInitialized = false;
		bool appliedLightTheme = false;
		bool appliedCompactTheme = false;
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

		void DeclareFloatingBuiltInWidgets(Sodium::SdTextureHandle fontAtlasTexture)
		{
			Sodium::SdWindowOptions mediaWindowOptions = {};
			mediaWindowOptions.position = { 594.0f, 54.0f };
			mediaWindowOptions.size = { 360.0f, 190.0f };
			mediaWindowOptions.backgroundBlurRadius = 12.0f;
			gui.ui.DeclareKeyed<Sodium::SdWindow>("sample_media_window", "Media Window", demoControls.mediaWindowOpen, mediaWindowOptions, [fontAtlasTexture](Sodium::SdUi& ui)
			{
				ui.Declare<Sodium::SdText>("Floating built-in window");
				ui.Declare<Sodium::SdButton>("Media action");
				ui.Declare<Sodium::SdCheckBox>("Media option");
				ui.Declare<Sodium::SdImageViewer>(fontAtlasTexture, Sodium::SdVec2{ 96.0f, 28.0f });
			});

			Sodium::SdWindowOptions metricsWindowOptions = {};
			metricsWindowOptions.shadowColor = { 255, 0, 0, 128 };
			metricsWindowOptions.position = { 638.0f, 118.0f };
			metricsWindowOptions.size = { 330.0f, 172.0f };
			metricsWindowOptions.backgroundBlurRadius = 12.0f;
			gui.ui.DeclareKeyed<Sodium::SdWindow>("sample_metrics_window", "Metrics Window", demoControls.metricsWindowOpen, metricsWindowOptions, [this](Sodium::SdUi& ui)
			{
				char frameLine[96] = {};
				std::snprintf(frameLine, sizeof(frameLine), "Frame %llu", static_cast<unsigned long long>(frameCount));
				char fpsLine[96] = {};
				std::snprintf(fpsLine, sizeof(fpsLine), "Live FPS %.1f", std::max(0.0, liveFps));
				ui.Declare<Sodium::SdText>(frameLine);
				ui.Declare<Sodium::SdText>(fpsLine);
				ui.Declare<Sodium::SdText>("This window overlaps the media window");
			});

			Sodium::SdWindowOptions toolsWindowOptions = {};
			toolsWindowOptions.position = { 680.0f, 182.0f };
			toolsWindowOptions.size = { 310.0f, 178.0f };
			toolsWindowOptions.backgroundBlurRadius = 12.0f;
			gui.ui.DeclareKeyed<Sodium::SdWindow>("sample_tools_window", "Tools Window", demoControls.toolsWindowOpen, toolsWindowOptions, [](Sodium::SdUi& ui)
			{
				ui.Declare<Sodium::SdText>("Independent SdWindow instance");
				ui.Declare<Sodium::SdButton>("Tool button");
				ui.Declare<Sodium::SdCheckBox>("Tool toggle");
			});

			gui.ui.DeclareKeyed<Sodium::SdPopup>("sample_builtin_popup", demoControls.popupOpen, Sodium::SdVec2{ 594.0f, 270.0f }, [](Sodium::SdUi& ui)
			{
				ui.Declare<Sodium::SdText>("SdPopup appears above content");
				ui.Declare<Sodium::SdButton>("Popup action");
			});

			gui.ui.DeclareKeyed<Sodium::SdContextMenu>("sample_builtin_context_menu", demoControls.contextMenuOpen, demoControls.contextMenuPosition, [](Sodium::SdUi& ui)
			{
				ui.Declare<Sodium::SdText>("SdContextMenu");
				ui.Declare<Sodium::SdButton>("First command");
				ui.Declare<Sodium::SdButton>("Second command");
			});

			gui.ui.DeclareKeyed<Sodium::SdTooltip>(
				"sample_builtin_tooltip",
				demoControls.tooltipVisible,
				gui.GetInput().GetMousePosition(),
				"SdTooltip follows the overlay layer");
		}

		void ConfigureStyleSystem()
		{
			Sodium::SdStyleSystem& styleSystem = gui.GetStyleSystem();
			ConfigureBuiltInThemeTransitions(styleSystem);
			styleSystem.RootRule(Sodium::SdPanel::StyleId)
				.Scope(kExampleDemoWindowScope)
				.Class(kExampleDemoWindowClass)
				.Set(&Sodium::SdBoxStyle::padding, Sodium::SdStyleValue::FromSpacing({ 14.0f, 108.0f, 14.0f, 12.0f }))
				.Set(&Sodium::SdBoxStyle::gap, Sodium::SdLength::Pixels(5.0f));
			styleSystem.RootRule(Sodium::SdText::StyleId)
				.Scope(kExampleDemoTextScope)
				.Set(&Sodium::SdBoxStyle::fontSize, 17.0f)
				.Set(&Sodium::SdBoxStyle::lineHeight, 22.0f)
				.Set(&Sodium::SdBoxStyle::padding, Sodium::SdStyleValue::FromSpacing({ 2.0f, 0.0f, 2.0f, 0.0f }));
			styleSystem.RootRule(Sodium::SdText::StyleId)
				.Scope(kExampleDemoTextScope)
				.Class(kExampleAccentTextClass)
				.Set(&Sodium::SdBoxStyle::color, Sodium::DesignColor(Sodium::SdDesignTokenIds::Accent));
			styleSystem.RootRule(Sodium::SdText::StyleId)
				.Scope(kExampleDemoTextScope)
				.Class(kExampleWarningTextClass)
				.Set(&Sodium::SdBoxStyle::color, Sodium::SdColor{ 255, 176, 92, 255 });
			styleSystem.RootRule(Sodium::SdText::StyleId)
				.Scope(kExampleDemoTextScope)
				.Class(kExamplePaddedTextClass)
				.Set(&Sodium::SdBoxStyle::padding, Sodium::SdStyleValue::FromSpacing({ 2.0f, 1.0f, 2.0f, 1.0f }));
			styleSystem.RootRule(Sodium::SdPanel::StyleId)
				.Scope(kExampleDemoPanelScope)
				.Class(kExampleBasicPanelClass)
				.Set(&Sodium::SdBoxStyle::width, Sodium::SdLength::Pixels(492.0f))
				.Set(&Sodium::SdBoxStyle::height, Sodium::SdLength::Pixels(72.0f))
				.Set(&Sodium::SdBoxStyle::padding, Sodium::SdStyleValue::FromSpacing({ 8.0f, 8.0f, 8.0f, 8.0f }))
				.Set(&Sodium::SdBoxStyle::gap, Sodium::SdLength::Pixels(3.0f));
			styleSystem.RootRule(Sodium::SdScrollView::StyleId)
				.Scope(kExampleDemoScrollScope)
				.Class(kExampleBasicScrollClass)
				.Set(&Sodium::SdBoxStyle::width, Sodium::SdLength::Pixels(492.0f))
				.Set(&Sodium::SdBoxStyle::height, Sodium::SdLength::Pixels(86.0f))
				.Set(&Sodium::SdBoxStyle::padding, Sodium::SdStyleValue::FromSpacing({ 8.0f, 8.0f, 8.0f, 8.0f }))
				.Set(&Sodium::SdBoxStyle::gap, Sodium::SdLength::Pixels(3.0f));
		}

		void ApplyGlobalTheme()
		{
			if (themeInitialized
				&& appliedLightTheme == demoControls.lightTheme
				&& appliedCompactTheme == demoControls.compactTheme)
			{
				return;
			}

			Sodium::SdStyleSystem& styleSystem = gui.GetStyleSystem();
			if (demoControls.lightTheme)
			{
				styleSystem.SetColor(Sodium::SdDesignTokenIds::Text, { 28, 34, 42, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::ButtonText, { 28, 34, 42, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::Background, { 236, 241, 246, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::WindowBg, { 250, 252, 255, 0 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::PanelBg, { 226, 234, 242, 242 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::ButtonBg, { 214, 228, 241, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::ButtonBgHover, { 190, 215, 238, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::ButtonBgPressed, { 158, 198, 230, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::Accent, { 30, 132, 112, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::Border, { 126, 145, 164, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::BorderStrong, { 82, 102, 122, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::Danger, { 180, 70, 76, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::Selection, { 30, 132, 112, 90 });
			}
			else
			{
				styleSystem.SetColor(Sodium::SdDesignTokenIds::Text, Sodium::SdColorWhite);
				styleSystem.SetColor(Sodium::SdDesignTokenIds::ButtonText, Sodium::SdColorWhite);
				styleSystem.SetColor(Sodium::SdDesignTokenIds::Background, { 24, 30, 39, 242 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::WindowBg, { 24, 30, 39, 0 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::PanelBg, { 35, 42, 52, 235 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::ButtonBg, { 48, 72, 96, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::ButtonBgHover, { 62, 100, 138, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::ButtonBgPressed, { 68, 118, 160, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::Accent, { 82, 170, 128, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::Border, { 91, 109, 128, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::BorderStrong, { 128, 154, 180, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::Danger, { 164, 66, 66, 255 });
				styleSystem.SetColor(Sodium::SdDesignTokenIds::Selection, { 82, 170, 128, 96 });
			}

			styleSystem.SetMetric(Sodium::SdDesignTokenIds::SpacingSmall, demoControls.compactTheme ? 4.0f : 10.0f);
			styleSystem.SetMetric(Sodium::SdDesignTokenIds::SpacingMedium, demoControls.compactTheme ? 7.0f : 20.0f);
			styleSystem.SetMetric(Sodium::SdDesignTokenIds::RadiusSmall, demoControls.compactTheme ? 3.0f : 10.0f);
			styleSystem.SetMetric(Sodium::SdDesignTokenIds::DurationFast, 0.36f);
			appliedLightTheme = demoControls.lightTheme;
			appliedCompactTheme = demoControls.compactTheme;
			themeInitialized = true;
		}

		std::array<float, 4> GetClearColor() const
		{
			if (demoControls.lightTheme)
				return { 0.92f, 0.94f, 0.96f, 1.0f };
			return { 0.055f, 0.065f, 0.08f, 1.0f };
		}

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
			if (!gui.Initialize(platform, renderer, fontBackend))
				return false;
			const Sodium::SdFontRequest fontRequests[] =
			{
				{ L"C:\\Windows\\Fonts\\segoeui.ttf", "Segoe UI" },
				{ L"C:\\Windows\\Fonts\\seguiemj.ttf", "Segoe UI Emoji" },
				{ L"C:\\Windows\\Fonts\\msyh.ttc", "Microsoft YaHei" },
			};
			for (const Sodium::SdFontRequest& request : fontRequests)
			{
				fontBackend.LoadFont(request);
			}
			ConfigureStyleSystem();
			ApplyGlobalTheme();
			return true;
		}

		int Run()
		{
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

				if (resizeWidth != 0 && resizeHeight != 0)
				{
					if (!dx.Resize(resizeWidth, resizeHeight))
						break;
					resizeWidth = 0;
					resizeHeight = 0;
				}

				ApplyGlobalTheme();
				const std::array<float, 4> clearColor = GetClearColor();
				dx.BeginFrame(clearColor);
				gui.BeginFrame();
				const Sodium::SdStyleClassId demoWindowClasses[] = { kExampleDemoWindowClass };
				DeclareFloatingBuiltInWidgets(fontBackend.GetAtlasTexture());
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
