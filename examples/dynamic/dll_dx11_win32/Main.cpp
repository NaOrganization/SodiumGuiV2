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

#include <SdBasicWidgets.h>

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

	constexpr Sodium::SdStyleClassId kOverlayAccentTextClass = Sodium::SdStyleClassLiteral("Sodium.DynamicExample.Text.Accent");
	constexpr Sodium::SdStyleClassId kOverlayMutedTextClass = Sodium::SdStyleClassLiteral("Sodium.DynamicExample.Text.Muted");
	constexpr Sodium::SdStyleClassId kOverlayBasicPanelClass = Sodium::SdStyleClassLiteral("Sodium.DynamicExample.Panel.Basic");
	constexpr Sodium::SdStyleClassId kOverlayBasicScrollClass = Sodium::SdStyleClassLiteral("Sodium.DynamicExample.Scroll.Basic");
	constexpr Sodium::SdStyleScopeId kOverlayTextScope = Sodium::SdStyleScopeLiteral("Sodium.DynamicExample.Scope.Overlay");
	constexpr Sodium::SdStyleScopeId kOverlayPanelScope = Sodium::SdStyleScopeLiteral("Sodium.DynamicExample.Scope.Panel");
	constexpr Sodium::SdStyleScopeId kOverlayScrollScope = Sodium::SdStyleScopeLiteral("Sodium.DynamicExample.Scope.Scroll");

	void ConfigureBuiltInThemeTransitions(Sodium::SdStyleSystem& styleSystem)
	{
		const auto themeTransition = 360ms;
		const Sodium::SdAnimationEasing easing = Sodium::SdAnimationEasing::OutCubic;
		styleSystem.Rule<Sodium::SdWindow>()
			.Transition(&Sodium::SdWindow::Style::radius, themeTransition, easing);
		styleSystem.Rule<Sodium::SdPopup>()
			.Transition(&Sodium::SdPopup::Style::radius, themeTransition, easing);
		styleSystem.Rule<Sodium::SdContextMenu>()
			.Transition(&Sodium::SdContextMenu::Style::radius, themeTransition, easing);
		styleSystem.Rule<Sodium::SdTooltip>()
			.Transition(&Sodium::SdTooltip::Style::radius, themeTransition, easing);
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
		bool lightTheme = false;
		bool compactTheme = false;
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
		bool styleConfigured = false;
		bool themeInitialized = false;
		bool appliedLightTheme = false;
		bool appliedCompactTheme = false;
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
				if (context.input.IsKeyDown(Sodium::SdKeyCode::Esc))
					windowOpen = false;
				State& state = context.State<State>();
				state.hovered = context.IsHovered();
				state.mouse = context.input.GetMousePosition();

				const bool accentPhase = ((frameCount / 90) % 2) == 0;
				const Sodium::SdStyleClassId statusClasses[] =
				{
					accentPhase ? kOverlayAccentTextClass : kOverlayMutedTextClass
				};
				const Sodium::SdStyleIdentity statusIdentity{
					Sodium::SdSpan<const Sodium::SdStyleClassId>(statusClasses, 1),
					kOverlayTextScope
				};
				char overlayStatus[160] = {};
				std::snprintf(overlayStatus, sizeof(overlayStatus), "Styled overlay text - %.1f FPS", liveFps);
				context.ui.DeclareStyledKeyed<Sodium::SdText>("overlay_style_status", statusIdentity, nullptr, overlayStatus);

				Sodium::SdText::Style inlineTextStyle = {};
				inlineTextStyle.padding = { 2.0f, 1.0f, 2.0f, 1.0f };
				context.ui.DeclareStyledKeyed<Sodium::SdText>("overlay_inline_style", &inlineTextStyle, "Inline target style");

				const Sodium::SdStyleClassId panelClasses[] = { kOverlayBasicPanelClass };
				const Sodium::SdStyleIdentity panelIdentity{
					Sodium::SdSpan<const Sodium::SdStyleClassId>(panelClasses, 1),
					kOverlayPanelScope
				};
				Sodium::SdPanel::Style panelStyle = {};
				panelStyle.childSpacing = 3.0f;
				context.ui.DeclareStyledKeyed<Sodium::SdPanel>("overlay_basic_panel", panelIdentity, &panelStyle, [](Sodium::SdUi& ui)
				{
					ui.Declare<Sodium::SdText>("SdPanel + SdText inside the DLL overlay");
					ui.Declare<Sodium::SdText>("Built-in widgets use the same runtime path");
				});

				bool buttonClicked = false;
				context.ui.DeclareKeyed<Sodium::SdButton>("overlay_basic_button", "SdButton: increment", buttonClicked);
				if (buttonClicked)
					++controls.clickCount;
				state.clickCount = controls.clickCount;

				context.ui.DeclareKeyed<Sodium::SdCheckBox>("overlay_option", "SdCheckBox: option enabled", controls.optionEnabled);
				context.ui.DeclareKeyed<Sodium::SdCheckBox>("overlay_theme_light", "Theme demo: light palette", controls.lightTheme);
				context.ui.DeclareKeyed<Sodium::SdCheckBox>("overlay_theme_compact", "Theme demo: compact metrics", controls.compactTheme);
				context.ui.DeclareKeyed<Sodium::SdCheckBox>("overlay_show_window", "Show SdWindow", controls.mediaWindowOpen);
				context.ui.DeclareKeyed<Sodium::SdCheckBox>("overlay_show_popup", "Show SdPopup / Tooltip", controls.popupOpen);
				controls.tooltipVisible = controls.popupOpen;

				char sliderLabel[96] = {};
				std::snprintf(sliderLabel, sizeof(sliderLabel), "SdSliderFloat %.2f", controls.sliderValue);
				context.ui.DeclareKeyed<Sodium::SdSliderFloat>("overlay_slider", sliderLabel, controls.sliderValue, 0.0f, 1.0f);
				context.ui.DeclareKeyed<Sodium::SdTextInput>("overlay_text_input", controls.textInputValue, "SdTextInput");
				context.ui.DeclareKeyed<Sodium::SdImageViewer>("overlay_image", fontAtlasTexture, Sodium::SdVec2{ 96.0f, 30.0f });

				const Sodium::SdStyleClassId scrollClasses[] = { kOverlayBasicScrollClass };
				const Sodium::SdStyleIdentity scrollIdentity{
					Sodium::SdSpan<const Sodium::SdStyleClassId>(scrollClasses, 1),
					kOverlayScrollScope
				};
				Sodium::SdScrollView::Style scrollStyle = {};
				scrollStyle.childSpacing = 3.0f;
				context.ui.DeclareStyledKeyed<Sodium::SdScrollView>("overlay_scroll", scrollIdentity, &scrollStyle, [](Sodium::SdUi& ui)
				{
					ui.Declare<Sodium::SdText>("SdScrollView row");
					ui.Declare<Sodium::SdButton>("Child button");
					ui.Declare<Sodium::SdText>("Clipped content row");
				});
			}

			void OnLayout(Sodium::SdLayoutContext& context)
			{
				context.SetDesiredSize({ 500.0f, 540.0f });
				context.widgetState.manualLayout = true;
				context.widgetState.manualRect = { 42.0f, 42.0f, 542.0f, 582.0f };
				context.widgetState.layerPriority = Sodium::SdLayerPriority::Overlay;
				context.widgetState.arrangeChildren = true;
				context.widgetState.clipChildren = true;
				context.widgetState.childPadding = { 14.0f, 96.0f, 14.0f, 10.0f };
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
				std::snprintf(line, sizeof(line), "SodiumGUI DLL overlay smoke");
				context.renderList.AddText(line, { rect.min.x + 14.0f, rect.min.y + 14.0f }, text, context.clipRect);
				std::snprintf(line, sizeof(line), "Frame %llu  Mouse %.0f, %.0f", static_cast<unsigned long long>(context.instance.GetFrameIndex()), state.mouse.x, state.mouse.y);
				context.renderList.AddText(line, { rect.min.x + 14.0f, rect.min.y + 42.0f }, text, context.clipRect);
				std::snprintf(line, sizeof(line), "Clicks %d  Hover %s", state.clickCount, state.hovered ? "yes" : "no");
				context.renderList.AddText(line, { rect.min.x + 14.0f, rect.min.y + 70.0f }, text, context.clipRect);
			}
		};

		void DeclareFloatingBuiltInWidgets()
		{
			Sodium::SdWindowOptions windowOptions = {};
			windowOptions.position = { 566.0f, 52.0f };
			windowOptions.size = { 330.0f, 176.0f };
			gui.ui.DeclareKeyed<Sodium::SdWindow>("overlay_builtin_window", "SdWindow", controls.mediaWindowOpen, windowOptions, [](Sodium::SdUi& ui)
			{
				ui.Declare<Sodium::SdText>("Floating window in overlay");
				ui.Declare<Sodium::SdButton>("Window button");
				ui.Declare<Sodium::SdCheckBox>("Window option");
			});

			gui.ui.DeclareKeyed<Sodium::SdPopup>("overlay_builtin_popup", controls.popupOpen, Sodium::SdVec2{ 566.0f, 250.0f }, [](Sodium::SdUi& ui)
			{
				ui.Declare<Sodium::SdText>("SdPopup");
				ui.Declare<Sodium::SdButton>("Popup action");
			});

			gui.ui.DeclareKeyed<Sodium::SdContextMenu>("overlay_builtin_context_menu", controls.contextMenuOpen, controls.contextMenuPosition, [](Sodium::SdUi& ui)
			{
				ui.Declare<Sodium::SdText>("SdContextMenu");
				ui.Declare<Sodium::SdButton>("Overlay command");
			});

			gui.ui.DeclareKeyed<Sodium::SdTooltip>(
				"overlay_builtin_tooltip",
				controls.tooltipVisible,
				Sodium::SdVec2{ 566.0f, 406.0f },
				"SdTooltip on overlay layer");
		}

		void ConfigureStyleSystem()
		{
			if (styleConfigured)
				return;

			Sodium::SdStyleSystem& styleSystem = gui.GetStyleSystem();
			ConfigureBuiltInThemeTransitions(styleSystem);
			styleSystem.RootRule(Sodium::SdText::TargetTypeId)
				.Scope(kOverlayTextScope)
				.Set(&Sodium::SdBoxStyle::fontSize, 15.0f)
				.Set(&Sodium::SdBoxStyle::lineHeight, 19.0f);
			styleSystem.Rule<Sodium::SdText>()
				.Scope(kOverlayTextScope)
				.Set(&Sodium::SdText::Style::padding, Sodium::SdSpacing{ 2.0f, 0.0f, 2.0f, 0.0f });
			styleSystem.RootRule(Sodium::SdText::TargetTypeId)
				.Scope(kOverlayTextScope)
				.Class(kOverlayAccentTextClass)
				.Set(&Sodium::SdBoxStyle::color, Sodium::ThemeColor("accent"));
			styleSystem.RootRule(Sodium::SdText::TargetTypeId)
				.Scope(kOverlayTextScope)
				.Class(kOverlayMutedTextClass)
				.Set(&Sodium::SdBoxStyle::color, Sodium::SdColor{ 178, 196, 214, 255 });
			styleSystem.RootRule(Sodium::SdPanel::TargetTypeId)
				.Scope(kOverlayPanelScope)
				.Class(kOverlayBasicPanelClass)
				.Set(&Sodium::SdBoxStyle::width, Sodium::SdLength::Pixels(472.0f))
				.Set(&Sodium::SdBoxStyle::height, Sodium::SdLength::Pixels(64.0f))
				.Set(&Sodium::SdBoxStyle::padding, Sodium::SdStyleValue::FromSpacing({ 8.0f, 8.0f, 8.0f, 8.0f }));
			styleSystem.RootRule(Sodium::SdScrollView::TargetTypeId)
				.Scope(kOverlayScrollScope)
				.Class(kOverlayBasicScrollClass)
				.Set(&Sodium::SdBoxStyle::width, Sodium::SdLength::Pixels(472.0f))
				.Set(&Sodium::SdBoxStyle::height, Sodium::SdLength::Pixels(74.0f))
				.Set(&Sodium::SdBoxStyle::padding, Sodium::SdStyleValue::FromSpacing({ 8.0f, 8.0f, 8.0f, 8.0f }));
			styleConfigured = true;
		}

		void ApplyGlobalTheme()
		{
			if (themeInitialized
				&& appliedLightTheme == controls.lightTheme
				&& appliedCompactTheme == controls.compactTheme)
			{
				return;
			}

			Sodium::SdStyleSystem& styleSystem = gui.GetStyleSystem();
			if (controls.lightTheme)
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

			styleSystem.SetMetricVariable("spacing.small", controls.compactTheme ? 4.0f : 6.0f);
			styleSystem.SetMetricVariable("spacing.medium", controls.compactTheme ? 7.0f : 10.0f);
			styleSystem.SetMetricVariable("radius.small", controls.compactTheme ? 3.0f : 5.0f);
			styleSystem.SetMetricVariable("duration.fast", 0.36f);
			appliedLightTheme = controls.lightTheme;
			appliedCompactTheme = controls.compactTheme;
			themeInitialized = true;
		}

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
			ConfigureStyleSystem();
			ApplyGlobalTheme();
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
			ApplyGlobalTheme();
			gui.BeginFrame(platform, dx.displaySize);
			gui.ui.Declare<OverlayWindow>(overlayWindowOpen, controls, frameCount, liveFps, fontBackend.GetAtlasTexture());
			DeclareFloatingBuiltInWidgets();
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
