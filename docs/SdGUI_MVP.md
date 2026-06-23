# SdGUI MVP

## Scope

This MVP implements the first usable slice of the architecture in `SdGUI_Architecture.md`.

Implemented:

1. `Sodium::SdInstance` as the user-owned runtime instance.
2. `SdUi::Declare<T>()` as the public typed declaration API.
3. `SdWidgetTag` as a zero-state compile-time declaration tag.
4. Persistent internal widget records keyed by `SdWidgetId`.
5. `Entering / Alive / Leaving / Dead` widget lifecycle.
6. Typed per-widget user state through `context.State<T>()`.
7. Phase contexts: create, update, layout, paint.
8. Frame-local `SdRenderData` and `SdRenderList`.
9. Persistent DX11 vertex/index buffers in `SdDx11Renderer`.
10. UTF-8 validation and codepoint decoding.
11. Basic built-in tagged widgets: `SdText`, `SdPanel`, `SdButton`, `SdCheckBox`.
12. Win32 input integration through the existing `SdInputSystem`.
13. FreeType font backend with an internal ProggyClean fallback font.
14. Text rendering through glyph atlas uploads and textured DX11 quads.

Out of scope for this MVP:

1. Full layout tree solving.
2. Full style selector/token preprocessing.
3. Popup/modal root-layer routing beyond the stored root layer field.
4. Complex renderer batching across texture/scissor/shader state.
5. Complex shaping, kerning, RTL layout, and multi-font fallback chains.

## Migrated From SodiumGUI

The following parts of the original `G:\Alchemy\Sodium\SodiumGUI` project were reused or adapted because they fit the v0.4 architecture:

1. Base type direction and naming style.
2. Existing `SdInputSystem` and Win32 input backend.
3. Win32 IME synchronization behavior.
4. DX11 renderer resource model: persistent buffers that grow only when capacity is insufficient.
5. Lightweight layout/style/animation data concepts, rewritten without the old enum widget tree.
6. FreeType glyph parsing, atlas packing, and text quad generation concepts.

The old `SdUiCore` and `SdRuntime` were not copied directly because they are centered on enum-based widget declarations and old component functions, while the v0.4 architecture requires `Declare<T>()` with declarable widget tag types.

## MVP Example

The Win32 DirectX 11 example now runs this frame flow:

```cpp
gui.BeginFrame(platform, displaySize);
gui.ui.Declare<DemoWindow>(optionEnabled, clickCount);
gui.EndFrame();
gui.Render();
```

`DemoWindow` is a normal type derived from `SdWidgetTag`. It declares a panel, real rendered text, a button, and a checkbox through the same typed API.

The default fallback font is ProggyClean from the Proggy Fonts project, embedded as compressed TTF bytes and decompressed in memory during FreeType backend initialization.

## Build

Verified command:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Professional\MSBuild\Current\Bin\MSBuild.exe' `
  'G:\Alchemy\Sodium\SodiumGuiV2\examples\desktop\win32_directx11\win32_directx11.vcxproj' `
  /p:Configuration=Debug /p:Platform=x64 /m
```

Result:

```text
Build succeeded.
0 warnings
0 errors
```
