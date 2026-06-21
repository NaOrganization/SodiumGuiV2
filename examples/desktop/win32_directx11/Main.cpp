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

#include <SdBasicWidgets.h>

#pragma comment(lib, "d3d11.lib")

namespace
{
	using Microsoft::WRL::ComPtr;

	constexpr wchar_t kWindowClassName[] = L"SodiumGUI.Win32DirectX11.Example";
	constexpr UINT kInitialWindowWidth = 1024;
	constexpr UINT kInitialWindowHeight = 720;
	constexpr Sodium::SdStyleClassId kExampleAccentTextClass = Sodium::SdStyleClassLiteral("Sodium.Example.Text.Accent");
	constexpr Sodium::SdStyleClassId kExampleWarningTextClass = Sodium::SdStyleClassLiteral("Sodium.Example.Text.Warning");
	constexpr Sodium::SdStyleClassId kExampleBasicPanelClass = Sodium::SdStyleClassLiteral("Sodium.Example.Panel.Basic");
	constexpr Sodium::SdStyleScopeId kExampleDemoTextScope = Sodium::SdStyleScopeLiteral("Sodium.Example.Scope.DemoWindow");
	constexpr Sodium::SdStyleScopeId kExampleDemoPanelScope = Sodium::SdStyleScopeLiteral("Sodium.Example.Scope.DemoPanel");

	void ConfigureBuiltInThemeTransitions(Sodium::SdStyleSystem& styleSystem)
	{
		const auto themeTransition = std::chrono::milliseconds(360);
		const Sodium::SdAnimationEasing easing = Sodium::SdAnimationEasing::OutCubic;
		styleSystem.Rule<Sodium::SdSliderFloat>()
			.Transition(&Sodium::SdSliderFloat::Style::radius, themeTransition, easing);
		styleSystem.Rule<Sodium::SdTextInput>()
			.Transition(&Sodium::SdTextInput::Style::radius, themeTransition, easing);
		styleSystem.Rule<Sodium::SdWindow>()
			.Transition(&Sodium::SdWindow::Style::radius, themeTransition, easing);
		styleSystem.Rule<Sodium::SdImageViewer>()
			.Transition(&Sodium::SdImageViewer::Style::radius, themeTransition, easing);
		styleSystem.Rule<Sodium::SdScrollView>()
			.Transition(&Sodium::SdScrollView::Style::radius, themeTransition, easing);
		styleSystem.Rule<Sodium::SdPopup>()
			.Transition(&Sodium::SdPopup::Style::radius, themeTransition, easing);
		styleSystem.Rule<Sodium::SdContextMenu>()
			.Transition(&Sodium::SdContextMenu::Style::radius, themeTransition, easing);
		styleSystem.Rule<Sodium::SdTooltip>()
			.Transition(&Sodium::SdTooltip::Style::radius, themeTransition, easing);
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
				if (context.input.IsKeyDown(Sodium::SdKeyCode::Esc))
					windowOpen = false;
				State& state = context.State<State>();
				state.hovered = context.IsHovered();
				state.mouse = context.input.GetMousePosition();
				liveFps = std::max(0.0, liveFps);

				const bool warningPhase = ((frameCount / 120) % 2) != 0;
				const Sodium::SdStyleClassId statusClasses[] =
				{
					warningPhase ? kExampleWarningTextClass : kExampleAccentTextClass
				};
				const Sodium::SdStyleIdentity statusIdentity{
					Sodium::SdSpan<const Sodium::SdStyleClassId>(statusClasses, 1),
					kExampleDemoTextScope
				};
				char styledStatus[160] = {};
				std::snprintf(styledStatus, sizeof(styledStatus), "Class/scope style text - %.1f FPS", liveFps);
				context.ui.DeclareStyledKeyed<Sodium::SdText>("style_status", statusIdentity, nullptr, styledStatus);

				Sodium::SdText::Style inlineTextStyle = {};
				inlineTextStyle.padding = { 2.0f, 1.0f, 2.0f, 1.0f };
				context.ui.DeclareStyledKeyed<Sodium::SdText>("style_inline", &inlineTextStyle, "Inline target style text");

				if (!context.input.IsKeyHeld(Sodium::SdKeyCode::W))
					context.ui.DeclareKeyed<Sodium::SdText>("test_text", "中文");

				if (context.input.IsKeyDown(Sodium::SdKeyCode::A))
					context.ui.ConfigureModel<Sodium::SdText>("test_text", [](Sodium::SdText::Model& model) { model.SetText("English"); });
				if (context.input.IsKeyDown(Sodium::SdKeyCode::S))
					context.ui.ConfigureModel<Sodium::SdText>("test_text", [](Sodium::SdText::Model& model) { model.ClearText(); });
				if (context.input.IsKeyDown(Sodium::SdKeyCode::D))
					context.ui.ConfigureModel<Sodium::SdText>("test_text", [](Sodium::SdText::Model& model) { model.SetText("中文"); });

				if (!context.input.IsKeyHeld(Sodium::SdKeyCode::W))
					context.ui.DeclareKeyed<Sodium::SdText>("test_text2", "English");

				if (context.input.IsKeyDown(Sodium::SdKeyCode::A))
					context.ui.ConfigureModel<Sodium::SdText>("test_text2", [](Sodium::SdText::Model& model) { model.SetText("中文"); });
				if (context.input.IsKeyDown(Sodium::SdKeyCode::S))
					context.ui.ConfigureModel<Sodium::SdText>("test_text2", [](Sodium::SdText::Model& model) { model.ClearText(); });
				if (context.input.IsKeyDown(Sodium::SdKeyCode::D))
					context.ui.ConfigureModel<Sodium::SdText>("test_text2", [](Sodium::SdText::Model& model) { model.SetText("English"); });

				const Sodium::SdStyleClassId panelClasses[] = { kExampleBasicPanelClass };
				const Sodium::SdStyleIdentity panelIdentity{
					Sodium::SdSpan<const Sodium::SdStyleClassId>(panelClasses, 1),
					kExampleDemoPanelScope
				};
				Sodium::SdPanel::Style panelStyle = {};
				panelStyle.childSpacing = 3.0f;
				context.ui.DeclareStyledKeyed<Sodium::SdPanel>("basic_panel", panelIdentity, &panelStyle, [](Sodium::SdUi& ui)
				{
					ui.Declare<Sodium::SdText>("SdPanel contains regular tagged children");
					ui.Declare<Sodium::SdText>("Layout, clip, background and border come from the widget");
				});

				bool buttonClicked = false;
				context.ui.DeclareKeyed<Sodium::SdButton>("basic_button", "SdButton: increment counter", buttonClicked);
				if (buttonClicked)
					++controls.clickCount;
				state.clickCount = controls.clickCount;

				context.ui.DeclareKeyed<Sodium::SdCheckBox>("basic_checkbox", "SdCheckBox: option enabled", controls.optionEnabled);
				context.ui.DeclareKeyed<Sodium::SdCheckBox>("theme_light", "Theme demo: light palette", controls.lightTheme);
				context.ui.DeclareKeyed<Sodium::SdCheckBox>("theme_compact", "Theme demo: compact metrics", controls.compactTheme);
				context.ui.DeclareKeyed<Sodium::SdCheckBox>("toggle_window", "Show SdWindow", controls.mediaWindowOpen);
				context.ui.DeclareKeyed<Sodium::SdCheckBox>("toggle_popup", "Show SdPopup", controls.popupOpen);
				context.ui.DeclareKeyed<Sodium::SdCheckBox>("toggle_menu", "Show SdContextMenu / SdTooltip", controls.contextMenuOpen);
				controls.tooltipVisible = controls.contextMenuOpen;

				char sliderLabel[96] = {};
				std::snprintf(sliderLabel, sizeof(sliderLabel), "SdSliderFloat %.2f", controls.sliderValue);
				context.ui.DeclareKeyed<Sodium::SdSliderFloat>("basic_slider", sliderLabel, controls.sliderValue, 0.0f, 1.0f);
				context.ui.DeclareKeyed<Sodium::SdTextInput>("basic_text_input", controls.textInputValue, "SdTextInput");
				context.ui.DeclareKeyed<Sodium::SdImageViewer>("basic_image", fontAtlasTexture, Sodium::SdVec2{ 104.0f, 34.0f });

				Sodium::SdScrollView::Style scrollStyle = {};
				scrollStyle.width = 492.0f;
				scrollStyle.height = 86.0f;
				scrollStyle.padding = { 8.0f, 8.0f, 8.0f, 8.0f };
				scrollStyle.childSpacing = 3.0f;
				context.ui.DeclareStyledKeyed<Sodium::SdScrollView>("basic_scroll", &scrollStyle, [](Sodium::SdUi& ui)
				{
					ui.Declare<Sodium::SdText>("SdScrollView clips children and reacts to wheel input");
					ui.Declare<Sodium::SdButton>("Focusable child");
					ui.Declare<Sodium::SdText>("Additional row");
				});
			}

			void OnLayout(Sodium::SdLayoutContext& context)
			{
				context.SetDesiredSize({ 520.0f, 620.0f });
				context.widgetState.manualLayout = true;
				context.widgetState.manualRect = { 48.0f, 42.0f, 568.0f, 662.0f };
				context.widgetState.arrangeChildren = true;
				context.widgetState.clipChildren = true;
				context.widgetState.childPadding = { 14.0f, 108.0f, 14.0f, 12.0f };
				context.widgetState.childSpacing = 5.0f;
				context.widgetState.targetTypeId = Sodium::SdWidgetTargetIds::Panel;
			}

			void OnPaint(Sodium::SdPaintContext& context)
			{
				const State& state = context.State<State>();
				const Sodium::SdRect rect = context.animatedRect;
				const Sodium::SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
				const Sodium::SdColor background = Sodium::SdColor(
					presentation.backgroundColor.r,
					presentation.backgroundColor.g,
					presentation.backgroundColor.b,
					static_cast<Sodium::SdUInt8>(presentation.backgroundColor.a * context.opacity));
				const Sodium::SdColor border = Sodium::SdColor(
					presentation.border.left.color.r,
					presentation.border.left.color.g,
					presentation.border.left.color.b,
					static_cast<Sodium::SdUInt8>(presentation.border.left.color.a * context.opacity));
				const Sodium::SdColor text = Sodium::SdColor(
					presentation.color.r,
					presentation.color.g,
					presentation.color.b,
					static_cast<Sodium::SdUInt8>(presentation.color.a * context.opacity));
				const float radius = Sodium::SdResolveLength(presentation.radius, rect.Width());
				context.renderList.AddRectFilled(rect, background, context.clipRect, radius);
				context.renderList.AddRect(rect, border, context.clipRect, 1.0f, radius);

				char line[160] = {};
				std::snprintf(line, sizeof(line), "SodiumGUI style system sample");
				context.renderList.AddText(line, { rect.min.x + 14.0f, rect.min.y + 16.0f }, text, context.clipRect);
				std::snprintf(line, sizeof(line), "Frame %llu  Mouse %.0f, %.0f", static_cast<unsigned long long>(context.instance.GetFrameIndex()), state.mouse.x, state.mouse.y);
				context.renderList.AddText(line, { rect.min.x + 14.0f, rect.min.y + 44.0f }, text, context.clipRect);
				std::snprintf(line, sizeof(line), "Clicks %d  Hover %s", state.clickCount, state.hovered ? "yes" : "no");
				context.renderList.AddText(line, { rect.min.x + 14.0f, rect.min.y + 72.0f }, text, context.clipRect);
			}
		};

		void DeclareFloatingBuiltInWidgets()
		{
			Sodium::SdWindowOptions windowOptions = {};
			windowOptions.position = { 594.0f, 54.0f };
			windowOptions.size = { 360.0f, 190.0f };
			gui.ui.DeclareKeyed<Sodium::SdWindow>("sample_builtin_window", "SdWindow", demoControls.mediaWindowOpen, windowOptions, [](Sodium::SdUi& ui)
			{
				ui.Declare<Sodium::SdText>("Floating built-in window");
				ui.Declare<Sodium::SdButton>("Window child button");
				ui.Declare<Sodium::SdCheckBox>("Window checkbox");
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
				Sodium::SdVec2{ 594.0f, 456.0f },
				"SdTooltip follows the overlay layer");
		}

		void ConfigureStyleSystem()
		{
			Sodium::SdStyleSystem& styleSystem = gui.GetStyleSystem();
			ConfigureBuiltInThemeTransitions(styleSystem);
			styleSystem.RootRule(Sodium::SdText::TargetTypeId)
				.Scope(kExampleDemoTextScope)
				.Set(&Sodium::SdBoxStyle::fontSize, 17.0f)
				.Set(&Sodium::SdBoxStyle::lineHeight, 22.0f);
			styleSystem.Rule<Sodium::SdText>()
				.Scope(kExampleDemoTextScope)
				.Set(&Sodium::SdText::Style::padding, Sodium::SdSpacing{ 2.0f, 0.0f, 2.0f, 0.0f });
			styleSystem.RootRule(Sodium::SdText::TargetTypeId)
				.Scope(kExampleDemoTextScope)
				.Class(kExampleAccentTextClass)
				.Set(&Sodium::SdBoxStyle::color, Sodium::ThemeColor("accent"));
			styleSystem.RootRule(Sodium::SdText::TargetTypeId)
				.Scope(kExampleDemoTextScope)
				.Class(kExampleWarningTextClass)
				.Set(&Sodium::SdBoxStyle::color, Sodium::SdColor{ 255, 176, 92, 255 });
			styleSystem.RootRule(Sodium::SdPanel::TargetTypeId)
				.Scope(kExampleDemoPanelScope)
				.Class(kExampleBasicPanelClass)
				.Set(&Sodium::SdBoxStyle::width, Sodium::SdLength::Pixels(492.0f))
				.Set(&Sodium::SdBoxStyle::height, Sodium::SdLength::Pixels(72.0f))
				.Set(&Sodium::SdBoxStyle::padding, Sodium::SdStyleValue::FromSpacing({ 8.0f, 8.0f, 8.0f, 8.0f }));
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
				styleSystem.SetColorVariable("text", { 28, 34, 42, 255 });
				styleSystem.SetColorVariable("button.text", { 28, 34, 42, 255 });
				styleSystem.SetColorVariable("background", { 236, 241, 246, 255 });
				styleSystem.SetColorVariable("window.bg", { 250, 252, 255, 245 });
				styleSystem.SetColorVariable("panel.bg", { 226, 234, 242, 242 });
				styleSystem.SetColorVariable("button.bg", { 214, 228, 241, 255 });
				styleSystem.SetColorVariable("button.bg.hover", { 190, 215, 238, 255 });
				styleSystem.SetColorVariable("button.bg.pressed", { 158, 198, 230, 255 });
				styleSystem.SetColorVariable("accent", { 30, 132, 112, 255 });
				styleSystem.SetColorVariable("border", { 126, 145, 164, 255 });
				styleSystem.SetColorVariable("border.strong", { 82, 102, 122, 255 });
				styleSystem.SetColorVariable("danger", { 180, 70, 76, 255 });
				styleSystem.SetColorVariable("selection", { 30, 132, 112, 90 });
			}
			else
			{
				styleSystem.SetColorVariable("text", Sodium::SdColorWhite);
				styleSystem.SetColorVariable("button.text", Sodium::SdColorWhite);
				styleSystem.SetColorVariable("background", { 24, 30, 39, 242 });
				styleSystem.SetColorVariable("window.bg", { 24, 30, 39, 242 });
				styleSystem.SetColorVariable("panel.bg", { 35, 42, 52, 235 });
				styleSystem.SetColorVariable("button.bg", { 48, 72, 96, 255 });
				styleSystem.SetColorVariable("button.bg.hover", { 62, 100, 138, 255 });
				styleSystem.SetColorVariable("button.bg.pressed", { 68, 118, 160, 255 });
				styleSystem.SetColorVariable("accent", { 82, 170, 128, 255 });
				styleSystem.SetColorVariable("border", { 91, 109, 128, 255 });
				styleSystem.SetColorVariable("border.strong", { 128, 154, 180, 255 });
				styleSystem.SetColorVariable("danger", { 164, 66, 66, 255 });
				styleSystem.SetColorVariable("selection", { 82, 170, 128, 96 });
			}

			styleSystem.SetMetricVariable("spacing.small", demoControls.compactTheme ? 4.0f : 6.0f);
			styleSystem.SetMetricVariable("spacing.medium", demoControls.compactTheme ? 7.0f : 10.0f);
			styleSystem.SetMetricVariable("radius.small", demoControls.compactTheme ? 3.0f : 5.0f);
			styleSystem.SetMetricVariable("duration.fast", 0.36f);
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

				dx.Resize(resizeWidth, resizeHeight);

				ApplyGlobalTheme();
				const std::array<float, 4> clearColor = GetClearColor();
				dx.BeginFrame(clearColor);
				const Sodium::SdVec2 displaySize = { static_cast<float>(dx.width), static_cast<float>(dx.height) };
				gui.BeginFrame(platform, displaySize);
				gui.ui.Declare<DemoWindow>(demoWindowOpen, demoControls, frameCount, liveFps, fontBackend.GetAtlasTexture());
				DeclareFloatingBuiltInWidgets();
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
