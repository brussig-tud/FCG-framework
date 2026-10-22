# FCG Framework

The FCG Framework is a lightweight C++23 graphics framework built around the *SDL3 GPU* API, a modern rendering middleware with backends for Vulkan, Direct3D 12 and Metal. It serves as the common application framework for platform abstraction used in exercises of the TU Dresden Computer Graphics and Visualization chair.

Course pages:
> https://tu-dresden.de/ing/informatik/smt/cgv/studium/lehrveranstaltungen

This readme only provides a coarse overview. The full framework documentation is located at:
> https://brussig-tud.github.io/FCG-framework/


# Building

The framework requires CMake 3.31 or newer and a C++23-capable compiler (a recent GCC, Clang or MSVC); Ninja is the recommended generator. SDL3 reuses a supplied target or a compatible installed package, with a pinned source build as fallback. On Linux, building that fallback with X11 support requires the X11 development packages. Doxygen is optional and only needed for building the documentation.

Configure, build and test using one of the provided CMake presets:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

The available presets are `debug`, `release`, `debug-shared` and `release-shared`; each configures its build tree under `build/<preset>`. The `*-shared` presets build the framework as shared libraries.

To build the framework manual (requires Doxygen):

```sh
cmake --build --preset debug --target fcg-docs
```

The manual is written to `build/debug/docs/html/index.html`.

To use the framework in your own project, add it as a CMake dependency (e.g. via CPM) and link against the library targets `FCG-Framework::Core`, `FCG-Framework::Image`, `FCG-Framework::Render` and `FCG-Framework::Extras` as needed. See the full framework documentation for details.


## Notable build options

| Option | Default | Description |
| --- | --- | --- |
| `FCG_SHARED_LIBS` | `BUILD_SHARED_LIBS`, else `OFF` | Build the framework libraries as shared libraries. |
| `FCG_USE_SHARED_SDL` | framework linkage | Link SDL3 and SDL_image as shared libraries. |
| `FCG_IMAGE_OPENEXR` | `OFF` | Enable OpenEXR image decoding; dependency resolution occurs only when enabled. |
| `FCG_BUILD_APPS` | `ON` | Build the demo apps in the `app/` directory. |
| `FCG_BUILD_APP_...` | `ON` | Toggle building individual demo apps (overridden when `FCG_BUILD_APPS` is `OFF`). |
| `FCG_INCLUDE_PACKAGING` | `ON` for top-level builds | Include install and packaging rules. |
| `FCG_BUILD_DOCUMENTATION` | `ON` for top-level builds | Enable the `fcg-docs` documentation target (requires Doxygen). |

The formats of a source-built *SDL_image* are configured through the upstream `SDLIMAGE_*` options; see `docs/DOCS.md` for details.
Radiance HDR and Portable Float Map loading are always included. OpenEXR is optional. All three retain floating-point
HDR values; source color encoding must be known before conversion or GPU upload.


# Overview

Framework functionality is grouped by purpose into four libraries that applications link against individually:

| Library | Description |
| --- | --- |
| [Core](https://brussig-tud.github.io/FCG-framework/fcg_core_overview.html) | The foundation: application lifecycle (applets driven by `fcg::run`), main window and GPU device, input events, GPU resources such as buffers, textures and fullscreen passes, render state shared between applets, ready-made camera controls, and an integrated Dear ImGui GUI. |
| [Image](https://brussig-tud.github.io/FCG-framework/fcg_image_overview.html) | Decodes image files into CPU pixel data via SDL_image (BMP, GIF, JPG, PNG, TGA, WebP and more) for inspection, processing or upload to the GPU – no GPU device or applet required. |
| [Render](https://brussig-tud.github.io/FCG-framework/fcg_render_overview.html) | Draws instanced primitives – currently just (optionally textured) quads and boxes – from user-definable attributes on the GPU; the quickest way to put many simple objects on the screen without writing your own pipelines and shaders. |
| [Extras](https://brussig-tud.github.io/FCG-framework/fcg_extras_overview.html) | Optional desktop conveniences on top of Core that only some platforms or applications need, currently just native file/folder dialogs. |


## Examples

Example applications using various parts of the Framework are in the `app/` directory. These are still WIP, exact descriptions to follow at a later date.
