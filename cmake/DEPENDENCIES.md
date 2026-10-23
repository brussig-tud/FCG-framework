# Maintaining framework dependencies

This reference covers dependency startup, build overrides, linkage, and optional codec troubleshooting. For ordinary
image loading and enabling additional formats, start with the
[SDL_image module guide](https://brussig-tud.github.io/FCG-framework/group__fcg__sdl__image.html).


## Startup and maintenance

The root establishes build options and includes [Dependencies.cmake](Dependencies.cmake) before adding libraries or
applications. This coordinator loads CPM, includes package scripts from [Dependencies/](Dependencies/), and checks
required targets and commands. Package scripts own target reuse, package lookup, source fallback, scoped defaults,
and target setup. Libraries consume these targets; they must not discover or fetch packages themselves.

To add a dependency, add its package script, include it from the coordinator after its prerequisites, and extend the
required-target or required-command checks. Keep defaults scoped and respect both normal variables and cache entries
set by callers. Invoke scripts through the coordinator in framework builds; independent test fixtures establish their
own startup prerequisites. Packaging and tests consume the scripts' root-scope discovery results.

SDL3, SDL_image, GLM, and ImGui reuse supplied targets, otherwise preferring compatible installed packages before
source fallback. glslang, SDL_shadercross, cpp-embedlib, and the independent stb decoder use pinned source checkouts.
A supplied `glslang-standalone` target must provide the version pinned in its package script. Exact package versions
and source pins belong to the scripts in [Dependencies/](Dependencies/).

Doxygen and the WebP package used by optional codec tests are discovered during startup. Neither is fetched or
required. Documentation and test setup consume these discovery results.


## Overrides and linkage

Set options and source overrides before adding the framework. Use upstream package options; SDL_image's defaults
respect normal and cache `SDLIMAGE_*` variables. Supplied targets and installed packages retain their existing codecs
and linkage; source-build options cannot reconfigure them. Third-party packages manage their own transitive
dependencies.

`FCG_SHARED_LIBS` selects framework linkage, defaulting to `BUILD_SHARED_LIBS` or otherwise OFF. `FCG_USE_SHARED_SDL`
selects source-built SDL3 and SDL_image linkage and defaults to the framework choice. Shared SDL_image requires shared
SDL3. Codec dependencies default to static position-independent builds; `SDLIMAGE_DEPS_SHARED` and codec-specific
choices can override their linkage. Dynamic codec loading requires shared codec libraries to be available at runtime.

For local or offline source resolution, supply `CPM_<name>_SOURCE`. Set `CPM_DOWNLOAD_<name>=ON` when forcing source
resolution rather than package lookup. Relevant image package names are `SDL3_image`, `fcg_stb`, `OpenEXR`, `Imath`,
`libavif`, and `TIFF`; for example, `CPM_TIFF_SOURCE` supplies a libtiff checkout. See each package script for its name
and prerequisites. Source-built packages can require additional upstream dependencies or codec submodules; supplying
one checkout alone does not make all transitive dependencies available offline.

Build-tree consumers receive dependency targets transitively. Installed framework targets require external
dependencies to be supplied separately. Internal adapters preserve backend build order and static codec linkage;
packaging changes must retain those properties. Bundled decoder license notices accompany the framework installation.
Installed Image headers do not require stb or OpenEXR headers.


## Optional OpenEXR

Set `FCG_IMAGE_OPENEXR=ON` before adding the framework. When OFF, startup does not discover or fetch OpenEXR or its
dependencies. When ON, [OpenEXR.cmake](Dependencies/OpenEXR.cmake) reuses `OpenEXR::OpenEXR`, otherwise preferring a
compatible installed package before source fallback. Imath follows the same target/package/source policy. OpenEXR
manages its compression dependencies.

The OpenEXR and Imath source fallbacks use static position-independent libraries, independently of framework linkage.
OpenEXR tools, examples, tests, Python bindings, and upstream installation default to disabled; explicit upstream
options are respected. Supplied shared packages remain externally managed.


## SDL_image codec configuration

The [SDL_image module guide](https://brussig-tud.github.io/FCG-framework/group__fcg__sdl__image.html#fcg_sdl_image_workflows_enabling)
contains the basic enable-format recipe. The following details apply to source-built SDL_image; supplied targets and
installed backends retain their existing capabilities. Resolution is implemented in
[SDL3_image.cmake](Dependencies/SDL3_image.cmake).

### Vendoring and strict dependency checks

The source fallback uses built-in PNG and JPEG decoding. AVIF, JPEG XL, TIFF, WebP, libpng, and codec vendoring default
to OFF. Set the corresponding upstream `SDLIMAGE_*` options before adding the framework. `SDLIMAGE_STRICT=ON` makes
missing dependencies a configuration error instead of silently disabling a requested codec.

`SDLIMAGE_VENDORED=ON` uses SDL_image's codec submodules; enabled submodules are fetched with its checkout. Vendored
codec linkage defaults to static (`SDLIMAGE_DEPS_SHARED=OFF`). Non-vendored WebP, JPEG XL, and libpng require their
codec dependencies to be supplied or installed. AVIF and TIFF have the additional framework resolution described below.

### AVIF

For `SDLIMAGE_AVIF=ON` without vendoring, [libavif.cmake](Dependencies/libavif.cmake) reuses an `avif` target or a
compatible installed libavif, otherwise building its source fallback. Explicit `AVIF_CODEC_*` selections are respected.
Automatic decoder selection reuses a supplied or installed libaom, dav1d, or libgav1; if none is available, it builds
libaom locally.

Local libaom requires Perl. Git for Windows' bundled Perl is discovered automatically. On x86 without yasm or nasm,
the local build uses libaom's portable implementation unless `AOM_TARGET_CPU` is explicitly selected. Check these
prerequisites when diagnosing local codec builds.

AVIF saving, libavif tools, tests, examples, and optional libyuv default to OFF. `SDLIMAGE_AVIF_SAVE=ON` also requests
an encoder for the backend. `SDLIMAGE_AVIF_SHARED` takes precedence over `SDLIMAGE_DEPS_SHARED`. Explicitly disabled
libavif discovery or an applicable explicit runtime library path leaves resolution to SDL_image.

### TIFF

For `SDLIMAGE_TIF=ON` without vendoring, [libtiff.cmake](Dependencies/libtiff.cmake) reuses `TIFF::tiff`, `TIFF::TIFF`,
or `tiff`, otherwise preferring an installed libtiff before source fallback. Source-built libtiff discovers installed
compression libraries; FCG does not fetch them. Deflate, JPEG, and other compression support therefore depends on
available libraries and the caller's upstream libtiff options. Tools, tests, contributed programs, documentation, the
C++ wrapper, OpenGL support, and upstream installation default to disabled.

`SDLIMAGE_TIF_SHARED` takes precedence over `SDLIMAGE_DEPS_SHARED`; explicit `tiff-static` settings are respected.
A shared TIFF codec request requires a shared libtiff. FCG installs its source-built shared TIFF runtime alongside
the framework. Apple's default ImageIO backend and an explicitly enabled Windows WIC backend bypass libtiff
resolution. Disabled TIFF discovery and an applicable explicit dynamic-library path retain SDL_image's own handling.

### Declaring capabilities for supplied backends

Resolved source or package capabilities determine the handler's advertised formats. If a supplied target or package
has no trustworthy format metadata, set `FCG_SDL_IMAGE_FORMATS` before adding the framework to the codec identifiers
its existing backend supports:

```cmake
set(FCG_SDL_IMAGE_FORMATS "PNG;JPG;WEBP")
```

Accepted identifiers are `ANI;AVIF;BMP;GIF;JPG;JXL;LBM;PCX;PNG;PNM;QOI;SVG;TGA;TIF;WEBP;XCF;XPM;XV`. An explicitly
empty list is valid. Unknown identifiers and a missing declaration when metadata is unavailable fail configuration.
This declaration describes an existing backend; it does not enable codecs. ANI is animation-only and omitted from
still-image metadata; BMP also advertises ICO and CUR. Advertised dynamic codecs can still fail when their runtime
libraries are unavailable.


## Validation

After building, run `ctest --preset debug -R dependencies` and the corresponding `debug-shared` preset. When
registered, the dependency fixture reuses resolved SDKs and source checkouts without test-time downloads. It exercises
supplied targets, source defaults, caller overrides, and all four framework/backend static/shared combinations.
Image decoder and upload checks are described in the [image fixture notes](../libs/image/tests/fixtures/README.md).
