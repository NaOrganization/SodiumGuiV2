# SodiumGUI

SodiumGUI 是一个基于 C++20 的轻量级 GUI 框架。它使用每帧声明式 API 描述界面，同时在内部保留稳定的 widget 状态、布局缓存、输入捕获、动画、图层、字体图集和渲染资源。

项目当前重点是 Windows 桌面、DirectX 11 和 DirectX 12 场景，仓库中包含 Win32 平台 backend、DirectX 11 renderer backend、DirectX 12 renderer backend、FreeType 字体 backend，以及桌面应用和 DLL overlay 示例。

![SodiumGUI Win32 DirectX11 Example](img/sodiumgui-win32-directx11-example.png)

## 特性

- C++20 API，公共类型位于 `Sodium` 命名空间。
- 使用 `SdUi::Declare<T>()` 声明 widget，应用代码不需要维护传统对象树。
- 内部保留 widget 状态，支持进入、离开和布局过渡动画。
- 分层渲染模型，支持普通窗口、浮动窗口、弹出层和上下文菜单。
- 样式系统覆盖尺寸、间距、颜色、圆角、边框、字体和动画属性。
- 内置基础 widget，包括文本、按钮、复选框、面板、滚动区域、窗口、弹出层、上下文菜单和图片查看器。
- RenderList 每帧重建，后端可以保留 GPU 缓冲和纹理资源。
- UTF-8 字符串模型，字体栅格化由 FreeType backend 提供。

## 目录结构

```text
.
├── SodiumGUI.h                  # 主聚合头文件
├── Sd*.h / Sd*.cpp              # 核心运行时、输入、样式、布局、绘制和 widget
├── backends/                    # 平台、渲染和字体 backend
│   ├── SdWin32Platform.*
│   ├── SdDx11Renderer.*
│   ├── SdDx12Renderer.*
│   └── FreeTypeFontBackend.*
├── docs/                        # 架构设计文档
├── examples/                    # Visual Studio 示例工程
│   ├── desktop/win32_directx11
│   └── dynamic/dll_dx11_win32
├── scripts/Verify.ps1           # 测试和示例构建脚本
└── tests/CoreTests.cpp          # 核心测试
```

## 环境要求

- Windows 10 或更新版本。
- Visual Studio 2022 或 Build Tools，包含 MSVC v143、MSBuild 和 Windows SDK。
- C++20 编译支持。
- DirectX 11 / DirectX 12 SDK 头文件和库，通常由 Windows SDK 提供。
- FreeType 开发库，需在你的工程环境中提供 `ft2build.h`、相关 include 路径和链接库。

## 构建示例

打开示例解决方案：

```powershell
start .\examples\examples.sln
```

或使用验证脚本构建核心测试和示例：

```powershell
.\scripts\Verify.ps1 -Configuration Debug -Platform x64
```

只构建并运行核心测试：

```powershell
.\scripts\Verify.ps1 -Configuration Debug -Platform x64 -SkipExamples
```

示例解决方案包含三个项目：

- `win32_directx11`：普通 Win32 + DirectX 11 桌面窗口示例。
- `win32_directx12`：普通 Win32 + DirectX 12 桌面窗口示例，使用原生 D3D12 command list 将 GUI 绘制提交到 D3D12 command queue。
- `dll_dx11_win32`：基于现有 DXGI swap chain 的动态库 overlay 示例。

## 快速使用

包含主头文件：

```cpp
#include <SodiumGUI.h>
#include <Backends/Win32/SdWin32Platform.h>
#include <Backends/Dx11/SdDx11Renderer.h>
#include <Backends/Dx12/SdDx12Renderer.h>
#include <Backends/FreeType/FreeTypeFontBackend.h>
```

初始化实例和 backend：

```cpp
Sodium::SdInstance gui = {};
Sodium::Backends::SdWin32Platform platform = {};
Sodium::Backends::SdDx11Renderer renderer = {};
Sodium::Backends::SdFreeTypeFontBackend fontBackend = {};

platform.Initialize(Sodium::Backends::SdWin32PlatformConfig(windowHandle));
renderer.Initialize(Sodium::Backends::SdDx11RendererConfig(device, deviceContext));
fontBackend.Initialize();
gui.Initialize(platform, renderer, fontBackend);
```

每帧声明 UI：

```cpp
gui.BeginFrame();

gui.ui.Declare<Sodium::SdText>("Hello SodiumGUI");
gui.ui.Declare<Sodium::SdButton>("Apply");
gui.ui.Declare<Sodium::SdCheckBox>("Enable option");

gui.EndFrame();
```

关闭时按相反顺序释放：

```cpp
gui.Shutdown();
fontBackend.Shutdown();
renderer.Shutdown();
platform.Shutdown();
```

更完整的初始化、消息循环、渲染提交和 widget 用法请参考：

- `examples/desktop/win32_directx11/Main.cpp`
- `examples/desktop/win32_directx12/Main.cpp`
- `examples/dynamic/dll_dx11_win32/Main.cpp`
- `docs/SdGUI_Architecture.md`
- `docs/SdGUI_Architecture.zh-CN.md`

## 设计模型

SodiumGUI 的外部模型是 typed declaration API，内部模型是 persistent widget state。应用每帧通过 `SdUi::Declare<T>()` 重新声明当前 UI，而运行时负责维护 ID、状态、布局、样式、动画和渲染数据。

这种模型允许 widget 在当前帧没有被声明时仍短暂保留内部状态，例如执行离场动画或布局过渡。它也让应用可以显式拥有一个或多个独立的 `SdInstance`。

## 许可证

Copyright 2026 SodiumGUI contributors.

本项目采用 Apache License 2.0 协议发布。详见 `LICENSE`。

## 致谢

感谢 GPT 在项目开发过程中提供的辅助，包括架构梳理、文档编写、代码组织建议和日常开发问题分析。
