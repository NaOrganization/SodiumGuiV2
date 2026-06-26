#include <Windows.h>

#include <array>
#include <cstdio>

#include <SodiumGUI.h>
#include <Backends/FreeType/FreeTypeFontBackend.h>
#include <Backends/Dx9/SdDx9Renderer.h>
#include <Backends/Win32/SdWin32Platform.h>

#include <Widget/SdBasicWidgets.h>

namespace
{
	constexpr wchar_t kWindowClassName[] = L"SodiumGUI.Win32DirectX9.Example";
	constexpr UINT kInitialWindowWidth = 1024;
	constexpr UINT kInitialWindowHeight = 720;

	struct DemoControlsState final
	{
		bool windowOpen = true;
		bool optionEnabled = true;
		bool tooltipVisible = true;
		int clickCount = 0;
		float sliderValue = 0.45f;
		Sodium::SdUtf8String textInputValue = "Edit SodiumGUI text";
	};

	struct ExampleApplication final
	{
		HINSTANCE instance = nullptr;
		HWND windowHandle = nullptr;
		UINT resizeWidth = 0;
		UINT resizeHeight = 0;
		bool running = true;
		DemoControlsState controls = {};

		Sodium::SdInstance gui = {};
		Sodium::Backends::SdWin32Platform platform = {};
		Sodium::Backends::SdDx9Renderer renderer = {};
		Sodium::Backends::SdFreeTypeFontBackend fontBackend = {};

		bool Initialize(HINSTANCE hInstance, int showCommand)
		{
			instance = hInstance;
			if (!RegisterWindowClass())
				return false;
			if (!CreateWindowInstance(showCommand))
				return false;
			if (!platform.Initialize(Sodium::Backends::SdWin32PlatformConfig(windowHandle)))
				return false;

			Sodium::Backends::SdDx9RendererConfig rendererConfig = {};
			rendererConfig.swapchain =
			{
				windowHandle,
				kInitialWindowWidth,
				kInitialWindowHeight,
				Sodium::Rhi::SdTextureFormat::Bgra8Unorm,
				1,
				Sodium::Rhi::SdPresentMode::Immediate
			};
			if (!renderer.Initialize(rendererConfig))
				return false;
			if (!fontBackend.Initialize())
				return false;
			if (!gui.Initialize(platform, renderer, fontBackend))
				return false;

			const Sodium::SdFontRequest fontRequests[] =
			{
				{ L"C:\\Windows\\Fonts\\segoeui.ttf", "Segoe UI" },
				{ L"C:\\Windows\\Fonts\\seguiemj.ttf", "Segoe UI Emoji" },
				{ L"C:\\Windows\\Fonts\\msyh.ttc", "Microsoft YaHei" }
			};
			for (const Sodium::SdFontRequest& request : fontRequests)
				fontBackend.LoadFont(request);

			return true;
		}

		int Run()
		{
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

				if (resizeWidth != 0 && resizeHeight != 0)
				{
					if (!renderer.Resize(resizeWidth, resizeHeight))
						break;
					resizeWidth = 0;
					resizeHeight = 0;
				}

				if (renderer.IsOccluded())
				{
					::Sleep(16);
					continue;
				}

				const std::array<float, 4> clearColor = { 0.055f, 0.065f, 0.08f, 1.0f };
				if (!renderer.BeginFrame(clearColor))
				{
					::Sleep(16);
					continue;
				}

				gui.BeginFrame();
				DeclareDemoUi();
				gui.EndFrame();
				gui.Render();

				if (!renderer.Present())
					break;
			}
			return 0;
		}

		void Shutdown()
		{
			gui.Shutdown();
			fontBackend.Shutdown();
			renderer.Shutdown();
			platform.Shutdown();
			if (windowHandle)
			{
				::DestroyWindow(windowHandle);
				windowHandle = nullptr;
			}
			if (instance)
				::UnregisterClassW(kWindowClassName, instance);
		}

		LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
		{
			platform.HandleMessage(hwnd, message, wParam, lParam, gui.GetInputSystem());
			if (message == WM_LBUTTONDOWN)
				::SetFocus(hwnd);
			if (message == WM_SIZE && wParam != SIZE_MINIMIZED)
			{
				resizeWidth = static_cast<UINT>(LOWORD(lParam));
				resizeHeight = static_cast<UINT>(HIWORD(lParam));
				return 0;
			}
			if (message == WM_DESTROY)
			{
				running = false;
				::PostQuitMessage(0);
				return 0;
			}
			return ::DefWindowProcW(hwnd, message, wParam, lParam);
		}

	private:
		void DeclareDemoUi()
		{
			Sodium::SdWindowOptions windowOptions = {};
			windowOptions.position = { 48.0f, 48.0f };
			windowOptions.size = { 460.0f, 360.0f };
			windowOptions.backgroundBlurRadius = 0.0f;
			windowOptions.shadowRadius = 0.0f;

			gui.ui.DeclareKeyed<Sodium::SdWindow>("dx9_demo_window", "SodiumGUI DirectX9", controls.windowOpen, windowOptions, [this](Sodium::SdUi& ui)
			{
				ui.Declare<Sodium::SdText>("Win32 + Direct3D9 backend smoke test");
				if (ui.Declare<Sodium::SdButton>("Increment counter").clickedThisFrame)
					++controls.clickCount;

				char counterLine[96] = {};
				std::snprintf(counterLine, sizeof(counterLine), "Counter: %d", controls.clickCount);
				ui.Declare<Sodium::SdText>(counterLine);
				ui.Declare<Sodium::SdCheckBox>("Option enabled", controls.optionEnabled);
				ui.Declare<Sodium::SdSliderFloat>("Slider", controls.sliderValue, 0.0f, 1.0f);
				ui.Declare<Sodium::SdTextInput>("Text", controls.textInputValue, "Type here");
				ui.Declare<Sodium::SdImageViewer>(fontBackend.GetAtlasTexture(), Sodium::SdVec2{ 128.0f, 32.0f });
			});

			gui.ui.DeclareKeyed<Sodium::SdTooltip>(
				"dx9_demo_tooltip",
				controls.tooltipVisible,
				gui.GetInput().GetMousePosition(),
				"DX9 renderer uses the same SodiumGUI draw packet.");
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
				L"SodiumGUI Win32 DirectX9 Example",
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
		::MessageBoxA(nullptr, "Failed to initialize SodiumGUI DirectX 9 example.", "SodiumGUI", MB_ICONERROR | MB_OK);
		app.Shutdown();
		return 1;
	}

	const int result = app.Run();
	app.Shutdown();
	return result;
}
