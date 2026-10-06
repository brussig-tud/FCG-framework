# Framework manual

The framework manual combines guides in principal public headers with the complete registered public API. Class and
method documentation stays beside declarations. Private members are hidden. Missing narrative and parameter descriptions
are allowed during the transition; malformed documentation, unresolved explicit references, and missing snippets fail
the documentation build.

With Doxygen installed:

```sh
cmake --preset debug
cmake --build --preset debug --target fcg-docs
```

Open `build/debug/docs/html/index.html`. The shared preset writes to `build/debug-shared/docs/html/index.html`; every
build tree has its own `docs/Doxyfile` and `docs/html`. The complete buffer guide remains at `group__fcg__buffers.html`,
with its existing section anchors. To invoke Doxygen directly, use the generated configuration, e.g.
`doxygen build/debug/docs/Doxyfile`.

`FCG_BUILD_DOCUMENTATION` defaults to ON for top-level builds and OFF when embedded. Set it to OFF to omit the target.
Doxygen is optional: if absent, configuration succeeds and reports that `fcg-docs` is unavailable. Documentation is
outside the default build and has no library or executable dependencies. `buffer-docs` has been replaced by `fcg-docs`.


## Component guides

Define each component with `\defgroup` in its principal public header and attach it to `\ingroup fcg_components`. Use
stable IDs describing responsibilities (such as `fcg_buffers`), never library ownership. Preserve group IDs and section
anchors when files or library boundaries change. Group additional headers with `\addtogroup` and balanced `@{` / `@}`
blocks inside the declaration namespace; use explicit `\ingroup` where needed. Group all public declarations with their
logical component.

New guide stubs should include a meaningful overview with explicit API and related-component links, an explicit **Guide
incomplete** notice, and sections with these suffixes:

- `<group>_workflows`: Common workflows
- `<group>_lifetime`: Ownership and lifetime
- `<group>_errors`: Errors
- `<group>_examples`: Examples

Do not guess uninvestigated behavior. Expand the stub as contracts are investigated. The buffer guide in
`core/include/FCG/buffer.h` is a complete example, with snippets from `core/tests/buffers/buffer_examples.cpp`. The normal build
compiles these as `buffer-examples`. Register snippet directories even when their library binaries or example
executables are disabled; examples are never API inputs.

Update module guides, class/method contracts, and compiled snippets in the same change as their APIs. Validate both
parts: build the normal targets (including `buffer-examples`), then build `fcg-docs` and review the affected rendered
pages. Doxygen detects broken references and snippets; compilation checks example/API compatibility. Neither replaces
review of lifetime, error, and ownership contracts. For buffer ownership, distinguish optional absence from moved-from
objects and keep examples of delayed creation and replacement consistent with the factories.


## Library registrations

Keep the framework-owned manifest in `docs/Documentation.cmake`, outside binary build-option branches:

```cmake
fcg_register_documentation(LIBRARY Core
    LINK_TARGET FCG-framework::Core
    COMPONENTS fcg_buffers fcg_devices
    INPUTS "${PROJECT_SOURCE_DIR}/core/include/FCG"
    EXAMPLE_DIRS "${PROJECT_SOURCE_DIR}/core/tests/buffers"
    PREDEFINED FCG_FRAMEWORK_EXPORT=
)
```

`LINK_TARGET` is metadata and need not name an existing CMake target. Library names and component IDs must be
identifiers. Repeat registrations for a library to accumulate inputs, components, snippet paths, and definitions; the
link target must agree. Inputs are deduplicated, and component ownership conflicts are configuration errors. Relative
paths resolve at the registration call's source directory, including subdirectories; absolute paths are useful in
included manifests.

Register public-header directories or individual `.h`, `.hh`, `.hpp`, or `.hxx` files. Directories are searched
recursively; implementation sources and nested `src`, `app`, `apps`, `applications`, `test`, `tests`, `dependencies`,
`third_party`, `vendor`, `_deps`, and build directories are excluded. The active binary tree is excluded too. Only
register public source locations; snippet paths are separate.

To add a library, add its manifest registration and public component groups. Add the guide links to `docs/main.dox`. The
generated library index lists its CMake target and links to component groups; library pages do not own the groups in
Doxygen's hierarchy. To split a library or move a component, change its ownership registration and any necessary input
paths. Keep the component's group ID and anchors unchanged. No changes to the documentation target are needed.

The reusable builder is `cmake/Documentation.cmake`. After all registrations, call
`fcg_add_documentation(TEMPLATE ... MAIN_PAGE ...)` once, as the framework root does. The template and landing page are
explicit so independent fixtures can use the same builder without framework inputs.

Run `ctest --preset debug -R documentation` for the registration/manual fixture. It needs Doxygen for HTML checks and
still checks configuration behavior when Doxygen is unavailable.

Library registrations may include `GUIDE <page_id>` to link an authored library guide from the generated library
page. The guide belongs in a registered public header, alongside the component guides; repeated registrations must
agree on its ID. Image's full example is in `image/include/FCG/image_loader.h`, with compiled snippets in
`image/tests/image_examples.cpp`. Link `FCG-framework::Image` for CPU image loading. See its guide for source codec
configuration through upstream `SDLIMAGE_*` options and automatic registration in static/shared builds.
