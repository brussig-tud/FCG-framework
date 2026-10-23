# Maintaining the framework documentation

This guide covers authoring, registering, building, validating, and publishing the framework documentation.


## Building and publishing the manual

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

What library overviews and module guides contain, where they live, and how to write them is codified in
[AGENTS.md](../AGENTS.md) (sections "Library overviews and module guides" and "API documentation content"). In short:
one overview page per
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


## Library registrations

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
The caller resolves Doxygen before invoking the builder; the framework does this during
[dependency startup](../cmake/DEPENDENCIES.md#startup-and-maintenance).

Run `ctest --preset debug -R documentation` for the registration/manual fixture. It needs Doxygen for HTML checks and
still checks configuration behavior when Doxygen is unavailable.

Library registrations may include `OVERVIEW <page_id>` to link the library's overview page from the generated library
page. The overview lives in a registered public header — in the library's umbrella header if one exists, otherwise in
its most principal public header; repeated registrations must agree on its ID. Image's overview is in
`libs/image/include/FCG/Image/image.h`, with compiled snippets in `libs/image/tests/image_examples.cpp`.
