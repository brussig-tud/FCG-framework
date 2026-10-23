# Framework manual

The framework manual combines the library overviews and module guides in public headers with the complete registered
public API. Class and method documentation stays beside declarations. Private members are hidden. Malformed
documentation, unresolved explicit references, and missing snippets fail the documentation build.

With Doxygen installed:

```sh
cmake --preset debug
cmake --build --preset debug --target fcg-docs
```

Open `build/debug/docs/html/index.html`. The shared preset writes to `build/debug-shared/docs/html/index.html`; every
build tree has its own `docs/Doxyfile` and `docs/html`. To invoke Doxygen directly, use the generated configuration,
e.g. `doxygen build/debug/docs/Doxyfile`.

`FCG_BUILD_DOCUMENTATION` defaults to ON for top-level builds and OFF when embedded. Set it to OFF to omit the target.
Doxygen is optional: if absent, configuration succeeds and reports that `fcg-docs` is unavailable. Documentation is
outside the default build and has no library or executable dependencies.

The `build` workflow (`.github/workflows/ci.yml`) builds `fcg-docs` on Linux and Windows for pull requests that change
files other than Markdown; Markdown-only changes do not trigger the workflow. Documentation warnings fail the build.
Its `deploy-docs` job publishes the Linux leg's `docs/html` to GitHub Pages on qualifying pushes to `master`.
It requires the repository's Pages source to be set to "GitHub Actions".


## Module guides

What library overviews and module guides contain, where they live, and how to write them is codified in `AGENTS.md`
(sections "Library overviews and module guides" and "API documentation content"). In short: one overview page per
library, one module guide (`\defgroup`, in group `fcg_modules`) per public header, written for inexperienced new users,
with snippets from compiled example sources.

Do not guess uninvestigated behavior; expand guides as contracts are investigated. The buffer guide in
`core/include/FCG/buffer.h` is a complete example, with snippets from `core/tests/buffers/buffer_examples.cpp`. The
normal build compiles these as `buffer-examples`. Register snippet directories even when their library binaries or
example executables are disabled; examples are never API inputs.

Update module guides, class/method contracts, and compiled snippets in the same change as their APIs. Validate both
parts: build the normal targets (including the `*-examples` targets), then build `fcg-docs` and review the affected
rendered pages. Doxygen detects broken references and snippets; compilation checks example/API compatibility. Neither
replaces review of lifetime, error, and ownership contracts.


## Cameras

Core provides `FCG/viewing.h`, `FCG/camera_focus.h`, `FCG/applet/orbit_camera.h`, and `FCG/applet/camera_2d.h`.
The [viewing module guide](../build/debug/docs/html/group__fcg__viewing.html) documents the shared
[`fcg::Camera`](../build/debug/docs/html/classfcg_1_1Camera.html) interface and
[`fcg::CameraParameters`](../build/debug/docs/html/structfcg_1_1CameraParameters.html). The
[focus-picking module guide](../build/debug/docs/html/group__fcg__camera__focus.html) documents the reusable
[`fcg::CameraFocus`](../build/debug/docs/html/classfcg_1_1CameraFocus.html) controller; both supplied camera applets
integrate it internally. The camera module guides and class contracts are the authority on behavior and integration.

The normal build compiles `camera-examples`; `camera-smoke` checks the CPU contracts and `camera-lifecycle`
checks controls, picking, cancellation, resizing, and redraw cleanup against real rendered depth.
For an interactive GUI review, run `FCG_CAMERA_PROBE_INTERACTIVE=1 build/debug/bin/camera-lifecycle`.


## Library registrations

Core lives in `core/` and exposes headers directly under `FCG/`, including its generated `FCG/export.h`.
Other framework libraries live in `libs/<lowercase-name>/` and expose headers under `FCG/<LibraryName>/`.
For example, Image lives in `libs/image/`, uses `<FCG/Image/image_loader.h>`, and generates `FCG/Image/export.h`.
Register additional library directories and targets in `libs/CMakeLists.txt`; packaging derives their include
directories from the targets. Libraries link dependency targets prepared by the root startup step described below.

Keep the framework-owned manifest in `docs/Documentation.cmake`, outside binary build-option branches:

```cmake
fcg_register_documentation(LIBRARY Core
    LINK_TARGET FCG-Framework::Core
    OVERVIEW fcg_core_overview
    MODULES fcg_buffers fcg_devices
    INPUTS "${PROJECT_SOURCE_DIR}/core/include/FCG"
    EXAMPLE_DIRS "${PROJECT_SOURCE_DIR}/core/tests/buffers"
    PREDEFINED FCG_FRAMEWORK_EXPORT=
)
```

`LINK_TARGET` is metadata and need not name an existing CMake target. Library names and module IDs must be
identifiers. Repeat registrations for a library to accumulate inputs, modules, snippet paths, and definitions; the
link target must agree. Inputs are deduplicated, and module ownership conflicts are configuration errors. Relative
paths resolve at the registration call's source directory, including subdirectories; absolute paths are useful in
included manifests.

Register public-header directories or individual `.h`, `.hh`, `.hpp`, `.hxx`, or `.dox` documentation files. Directories
are searched recursively; implementation sources and nested `src`, `app`, `apps`, `applications`, `test`, `tests`,
`dependencies`, `third_party`, `vendor`, `_deps`, and build directories are excluded. The active binary tree is excluded
too. Only register public source locations; snippet paths are separate.

To add a library, add its manifest registration and public module groups. Add the overview link to `docs/main.dox`. The
generated library pages list the CMake target, the overview, and the module guides; library pages do not own the groups
in Doxygen's hierarchy. To split a library or move a module, change its ownership registration and any necessary input
paths. Keep the module's group ID and anchors unchanged. No changes to the documentation target are needed.

The reusable builder is `cmake/Documentation.cmake`. After all registrations, call
`fcg_add_documentation(TEMPLATE ... MAIN_PAGE ...)` once, as the framework root does. The template and landing page are
explicit so independent fixtures can use the same builder without framework inputs.
The caller resolves Doxygen before invoking the builder; the framework does this during dependency startup.

Run `ctest --preset debug -R documentation` for the registration/manual fixture. It needs Doxygen for HTML checks and
still checks configuration behavior when Doxygen is unavailable.

Library registrations may include `OVERVIEW <page_id>` to link the library's overview page from the generated library
page. The overview lives in a registered public header — in the library's umbrella header if one exists, otherwise in
its most principal public header; repeated registrations must agree on its ID. Image's overview is in
`libs/image/include/FCG/Image/image.h`, with compiled snippets in `libs/image/tests/image_examples.cpp`. Link
`FCG-Framework::Image` for CPU image loading. See "SDL_image formats and codecs" below for source codec configuration
through upstream `SDLIMAGE_*` options and automatic registration in static/shared builds.


## Dependency startup

The root establishes build options and includes `cmake/Dependencies.cmake` before adding any libraries or applications.
This coordinator loads CPM, includes explicitly ordered scripts from `cmake/Dependencies/`, and verifies the required
targets and commands. Each package's script owns its existing-target checks, version, lookup/fetch policy, scoped
options, and target setup. Add new dependencies by adding a package script, placing its include after its prerequisites,
and extending the coordinator's required-target or required-command checks. Invoke scripts through the coordinator in
normal framework builds; independently configured test fixtures establish their own startup prerequisites.

SDL3, SDL_image, GLM, and ImGui reuse supplied targets, otherwise preferring compatible installed packages before source
fallback. glslang, SDL_shadercross, and cpp-embedlib retain their source pins. A supplied `glslang-standalone` target
replaces glslang's build and must provide the pinned version; the dependency test reuses the parent's tool this way.
Configure source overrides and codec options
before adding the framework. Source defaults respect normal and cache `SDLIMAGE_*` overrides; third-party packages still
manage their own transitive dependencies. Source-built SDL3 and SDL_image follow `FCG_USE_SHARED_SDL`, while the framework
follows `FCG_SHARED_LIBS` or `BUILD_SHARED_LIBS`.

Libraries must not call package discovery or fetching themselves. Image links the internal `fcg-sdl-image` adapter,
which preserves backend build ordering and codec linkage while keeping dependency details out of installed exports.
Dependency scripts produce the ordinary root-scope `FCG_SDL_IMAGE_BUILT_HERE` and `FCG_IMGUI_BUILT_HERE` values used by
packaging and tests; intermediate library directories do not forward this information.

Doxygen and the WebP package used by optional codec tests are also discovered during startup. Neither is fetched or
required. Documentation and test setup consume the discovery results. Run `ctest --preset debug -R dependencies` and
the corresponding shared preset to exercise supplied targets, source defaults, caller overrides, and all four
framework/backend static/shared combinations without downloading dependencies during the tests.


### Floating-point image formats

Radiance HDR (`hdr`, `rgbe`) and Portable Float Map (`pfm`) handlers are always built. Their outputs retain binary32
HDR values. They register with the global loader at priority -90, before SDL_image at -100. Image's public feature
header provides numeric `FCG_IMAGE_HAS_OPENEXR` for conditional C++ usage.

Set `FCG_IMAGE_OPENEXR=ON` before adding the framework to enable EXR. With the option OFF, startup never discovers or
fetches OpenEXR or its dependencies. With it ON, FCG reuses `OpenEXR::OpenEXR`, prefers an installed OpenEXR >= 3.4.14,
then falls back to source-pinned 3.4.14. Source fallbacks use static PIC libraries and disable tools, examples, tests,
Python, and upstream installation by default. Explicit upstream options are respected. Imath reuses its target/package,
with source fallback v3.2.2. OpenEXR manages compression dependencies, including its vendored libdeflate/OpenJPH fallbacks.

The independent stb source is pinned to commit `a7d4c2175630e1ea5b6f8dd00ab96fed0b97b96d`. Its implementation has internal
linkage and only HDR enabled. SDL_image's private stb implementation and applications' own copies remain independent.
For offline builds use `CPM_fcg_stb_SOURCE`, `CPM_OpenEXR_SOURCE`, and `CPM_Imath_SOURCE`; use the corresponding
`CPM_DOWNLOAD_<name>=ON` settings when forcing source resolution. No dependencies are downloaded by the tests.

PFM supports conventional bottom-to-top RGB/grayscale, LF/CRLF headers, both byte orders, and scale multiplication.
Radiance supports RGBE flat/scanline RLE in `-Y height +X width` order. EXR supports full-resolution HALF/FLOAT RGB/Y
with optional alpha in single-part non-deep scanline/tiled files, using the data window. See the module guides for
the supported subsets. EXR alpha association and HDR stored exposure are preserved. Unidentified source color encoding
is completed by default. `ImageLoaderOptions` independently controls assumptions for missing primaries,
transfer and range, plus `ImageHDRMetadataOptions` for missing SDR white and headroom. All requests default to enabled;
`ImageLoaderOptions::conservative()` disables optional assumptions and scans. Extracted declarations, format-defined
facts and backend-supplied labels remain available. Identified custom encodings take precedence over fallbacks.
PFM and missing EXR components use conventional linear BT.709 assumptions. EXR chromaticities establish primaries,
not an explicit transfer declaration. SDL_image's supplied metadata is treated as factual declarations, including
its default labels and brightness properties.
Missing QOI transfer comes from the header; PGM/PPM transfer is format-defined. Other missing SDL_image components use
BT.709 (BT.2020 for identified PQ/HLG), float-linear or integer/palette-sRGB transfer, and full range as assumptions.

Radiance missing primary inference uses its conventional RGB/equal-energy-white gamut, then converts to BT.709/D65.
Declared numeric gamuts unsupported by color-managed upload are converted even in conservative mode. Gamma is decoded
before gamut conversion; range and decoded linear transfer are format-defined. Conversion uses double-precision
RGB/XYZ matrices and Bradford adaptation without clipping or extra exposure/color-correction adjustments. Original
source records are retained, including EXR `whiteLuminance` and `sampToNits` calibration, independently of current
metadata. Neither calibration value is assigned directly to SDR white.

`ImagePixelInfo::provenance` has independent records for primaries/chromatic white, transfer, range, SDR white level
and headroom. Each record contains `ImageMetadataOrigin`, `adapted`, `dependsOnAssumptions`, and an owned explanation.
Origins distinguish Unknown, Declared, FormatDefined, Assumed, Derived and Assigned. Declared covers explicit source
declarations and decoder-supplied values. Assigned describes an explicit output definition; adaptation and dependence
on assumed inputs remain independent. Derived headroom can therefore depend on assumed white.

Records use private copyable numeric/string SDL properties bound to their associated current value. Surface moves,
adoption and independent copies retain records. Raw mismatches suppress stale evidence defensively; supported changes
permanently invalidate records. Unnotified external changes cannot be tracked. Writable pixel borrowing clears content
headroom before exposing storage. The preferred `editSurface(changes)` guard reuses the invalidation implementation
of `notifySurfaceChanged(changes)` and owns framework exclusivity, not an SDL lock. Pixel changes clear headroom;
color-metadata changes additionally clear affected provenance. Raw SDL users must notify before another framework
operation relies on changed state. Apply replacement headroom after notification.

`refreshMetadata` refreshes exactly SDR diffuse-white level, HDR headroom and their provenance. Eligible full-range
linear binary16/binary32 RGB receives a missing assumed white of 1. Peak measurement uses visible stored RGB, including
RGB at zero alpha, ignores alpha itself, and computes max(1, peak / white) in double precision. Negative samples do
not increase the peak. Nonfinite RGB, missing/invalid white or an unrepresentable ratio prevents a finite result and
is reported in `ImageMetadataRefreshResult::diagnostic`. Metadata completion retains pixels and never tone-maps.
Loader finalization fills missing properties, retaining supplied headroom including zero; explicit refresh replaces
headroom on successful measurement. Unavailable explicit measurement clears an old derived value and retains supplied
metadata. Ordinary `converted` retains headroom only on paths known to preserve RGB and white interpretation; other
paths clear it without an extra scan. Users must refresh manually when a current peak is needed, or select
`convertedWithMetadata`. Its refresh has the same precisely documented brightness-only scope. Metadata queries never
scan pixels.

`ImageUploadOptions` selects normal color management, stored-sample preservation, or preservation only for unidentified
encoding. Its optional fallback colorspace fills only missing components and permits color-managed fallback when an
applicable preservation path is unavailable. Applicable preservation is attempted before filling metadata. An identified
custom or unsupported encoding is distinct from missing metadata: guessing respects its declaration, and conditional
upload preservation treats it as established; unconditional preservation uploads its numeric samples. Fallback encodings
are validated even when not used.
Stored integer samples are normalized without sRGB decoding; palettes expand to colors. Raw YUV planes are outside
this path. Color-managed conversion and upload reject unsupported SDL transforms instead of accepting metadata that
SDL cannot actually convert. All upload
policies leave the image unchanged. The viewer uses `load` followed by strict color-managed upload.

Custom handlers override the protected `decode(bytes, normalizedHint, options)` hook and return an owned nonempty
image. Final public `load` wrappers normalize arguments and run shared brightness completion after decoding, for both
direct calls and registry dispatch. The protected `resolveMetadata(image, options, candidates, provenance)` helper
merges evidence-bearing candidates into genuinely missing RGB components without replacing custom encodings. Known
candidate declarations/facts, including decoder-supplied declarations, do not require assumption permission. Completed
source/adaptation provenance takes precedence over matching forwarded records. Format-specific sample adaptations
happen before final peak measurement.

The existing installed-target contract still requires external dependencies to be supplied separately. Source-built
EXR fallbacks are static; supplied shared packages remain externally managed. Bundled decoder license notices accompany
the framework installation. Installed public headers never require stb/OpenEXR headers.

Run `ctest --preset release -R 'image-|texture-smoke|install|documentation'` and the corresponding `release-shared`
configuration after building. Check both EXR settings; enabling EXR includes `image-exr-smoke` and `image-exr-assets`.
`image-exr-smoke` uses generated fixtures. The separately registered `image-exr-assets` test checks EXR/PFM sample
equality and RGBE tolerance when all three local Memorial assets exist, and skips an absent or incomplete set.
`texture-smoke` also validates whichever local Memorial assets its enabled handlers support, comparing uploaded samples
across guessing, upload fallback, and preservation policies. These tests never download the teaching assets.

### SDL_image formats and codecs

`FCG_SHARED_LIBS` selects framework linkage; `FCG_USE_SHARED_SDL` selects SDL linkage, defaulting to the framework
choice. Source-built SDL_image follows the latter because a shared SDL_image requires shared SDL3. SDL3_image
3.4.4 is the pinned source fallback. Compatible system packages and previously provided `SDL3_image::SDL3_image`
targets are reused as-is; source configuration cannot change their codecs or linkage. Installed targets defer full
third-party dependency exports. CPM/`add_subdirectory` consumers receive build-time dependency targets transitively.

The source fallback enables built-in formats and avoids external codec dependencies by default. AVIF, JPEG XL,
TIFF, WebP, libpng, and codec vendoring default to `OFF`. PNG uses SDL's built-in decoder and JPEG uses SDL_image's
stb backend. Change the upstream `SDLIMAGE_*` variables before adding the framework; FCG respects normal variables
and cache entries and introduces no bespoke codec switches. To decode WebP with installed codec dependencies:

```cmake
set(SDLIMAGE_WEBP ON CACHE BOOL "Enable WebP decoding")
set(SDLIMAGE_STRICT ON CACHE BOOL "Require requested codec dependencies")
CPMAddPackage(NAME FCG SOURCE_DIR "/path/to/framework")
target_link_libraries(my_app PRIVATE FCG-Framework::Image)
```

Replace the source path with your framework checkout, or use your pinned `GITHUB_REPOSITORY`/`GIT_TAG` in CPM.
To enable AVIF loading, set `SDLIMAGE_AVIF=ON` before adding the framework. For a source-built, non-vendored SDL_image,
CPM reuses a supplied `avif` target or a compatible installed libavif package (minimum version 1.0); otherwise it
fetches libavif 1.4.2. Explicit `AVIF_CODEC_*` settings are respected. Automatic decoder selection reuses an installed
libaom, dav1d, or libgav1, or builds libaom locally when none is available. Without an x86 assembler, local libaom
uses its portable implementation unless `AOM_TARGET_CPU` is explicitly selected. Local libaom requires Perl;
Git for Windows' bundled Perl is discovered automatically. AVIF saving, libavif tools, tests,
examples, and optional libyuv default to `OFF`. Enable `SDLIMAGE_AVIF_SAVE` to also request an encoder. Codec linkage
defaults to static with position-independent code; explicit `SDLIMAGE_AVIF_SHARED` or `SDLIMAGE_DEPS_SHARED` choices
are respected. Supplied or installed SDL_image backends retain their existing capabilities and do not trigger this
codec resolution. Normal variables, cache entries, and standard CPM source/download overrides remain supported.

To enable TIFF loading, set `SDLIMAGE_TIF=ON` before adding the framework. For a source-built, non-vendored SDL_image
using libtiff, FCG reuses a supplied `TIFF::tiff`, `TIFF::TIFF`, or `tiff` target, otherwise preferring an installed
libtiff package (minimum version 4.0) before fetching libtiff 4.7.2. Use `CPM_TIFF_SOURCE` to supply a source checkout
and `CPM_DOWNLOAD_TIFF=ON` to force source resolution. Source-built libtiff detects installed compression libraries;
FCG does not fetch them. Deflate, JPEG, and other optional compression formats therefore depend on available
libraries and the caller's upstream libtiff options. Tools, tests, contributed programs, documentation, the C++
wrapper, OpenGL support, and upstream installation default to disabled. Codec linkage defaults to static with
position-independent code; `SDLIMAGE_TIF_SHARED` takes precedence over `SDLIMAGE_DEPS_SHARED`, and explicit
`tiff-static` settings are respected. Dynamic loading requires a shared libtiff. FCG installs its source-built shared
TIFF runtime alongside the framework. SDL_image's native ImageIO backend (enabled by default on Apple platforms) and
explicitly enabled Windows WIC backend do not need libtiff and skip this resolution. Supplied or installed SDL_image,
disabled TIFF discovery, and an explicit dynamic TIFF library path also retain SDL_image's own handling.

Set `SDLIMAGE_VENDORED=ON` to use SDL_image's own codec submodules instead; enabled submodules are fetched with the
pinned SDL_image checkout. Enable `SDLIMAGE_JXL` or `SDLIMAGE_PNG_LIBPNG` similarly.
`SDLIMAGE_STRICT` makes missing dependencies a configuration error rather than silently disabling a requested codec.
Vendored codecs link statically by default (`SDLIMAGE_DEPS_SHARED=OFF`), keeping them inside SDL_image.
Options also work with `-D` on the command line. Prebuilt packages may support more or fewer formats.

Resolved source/package capabilities determine the shipped handler's advertised formats. If a supplied target or
package has no trustworthy metadata, set `FCG_SDL_IMAGE_FORMATS` to an explicit list of SDL codec identifiers:
`ANI;AVIF;BMP;GIF;JPG;JXL;LBM;PCX;PNG;PNM;QOI;SVG;TGA;TIF;WEBP;XCF;XPM;XV`. An explicitly empty list is valid;
unknown identifiers and missing declarations fail configuration. This declaration describes the existing backend,
rather than changing its codec configuration. Configured dynamic codecs can still fail when runtime libraries are
unavailable. ANI is animation-only and is omitted from still-image metadata; BMP also advertises ICO and CUR.
