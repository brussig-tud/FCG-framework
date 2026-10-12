## Preamble

The rules and guidelines laid out in this document might not be reflected accurately in the existing code base, as some of it was and still is being crafted by hand or by agents who only knew an older version of this document. Such instances should be considered outliers, the rules in this document are binding.



## C++ file style conventions

Allways leave the first line of a file blank.

All C++ sources (including headers `.h`, translation unit source files `.cpp` and inline files `.inl`) use section banners of the form
```C++
//////
//
// $Section
//
```

where `$Section` can be one of the following, typically in this order:
* `Library documentation` (`.h` files only, only in the header that hosts the library overview)
* `Module documentation` (`.h` files only, only in a module's principal header)
* `Includes`
* `Forward declarations`
* `Module-private symbols` (`.cpp` and `.inl` files only)
* `Namespaces open` (`.h` files only)
* `Module namespace open` (`.cpp` and `.inl` files only)
* `Structs and enums`
* `Classes`
* `Class implementations`
* `Functions`
* `Function implementations`
* `Class implementations` (`.cpp` and `.inl` files only)
* `Namespaces close` (`.h` files only)
* `Module namespace close` (`.cpp` and `.inl` files only)

Avoid sections other than these unless there is a good reason.

The very first section is always separated by 2 blank lines after the include guard in case of `.h` headers, and just a single blank first line in case of `.cpp` and `.inl` files. All following sections are always separated by 3 blank lines after the last content of the preceding section. The closing `#endif` of the include guard at the end of a header is only separated by 2 blank lines after the last content of the final section.

Look to the rest of the code base for guidance on how to lay out and format these sections.

The following sections have explicit special rules and/or mandatory content:


### *Includes*:

Group individual `#include` statements according to library, with the library being named by a short preciding comment, e.g.
```C++
// C++ STL
#include <print>
#include <vector>

// GLM library
#include <glm/glm.hpp>
```

The block `// C++ STL` should always be included. If no STL headers are needed, just put `/* nothing here yet */`, e.g.:
```C++
// C++ STL
/* nothing here yet */
```


### *Module-private symbols*:
must open and close an anonymous namespace like so:
```C++
// Anonymous namespace begin
namespace {
```
followed, after all module-private symbols, by
```C++
// Anonymous namespace end
}
```


### *Namespaces open*:

Put a consistent doc-comment briefly characterizing each namespace you are opening, for example:
```C++
/// The library top-level namespace.
namespace fcg {
```

Prefer to open nested namespaces individually in order to make sure each level get a proper doc comment. Never use
indentation from namespaces. For example:
```C++
/// The library top-level namespace.
namespace fcg {

/// The library's namespace for assorted utilities.
namespace util {
```


### *Module namespace open*:

Put a short (non-doc) comment characterizing where the namespace you are opening is coming from, for example: 
```C++
// The library top-level namespace.
namespace fcg {
```


### *Class implementations*:

Group method and static field definitions per class they belong to, using subsection headers:
```C++
////
// FooClass

[[nodiscard]] auto FooClass::isFoo () -> bool {
	return true;
}

[[nodiscard]] auto FooClass::isBar () -> bool {
	return false;
}


////
// BarClass

[[nodiscard]] auto BarClass::isFoo () -> bool {
	return false;
}

[[nodiscard]] auto BarClass::isBar () -> bool {
	return true;
}
```



## Header files

Always use `#define`-based include guards. Infer the appropriate guard macro name from the rest of the codebase, using other headers defined in the current CMake target as priority references.



## C++ coding style conventions

### Formatting function signatures

Always separate the argument list with a single space ` ` before the opening `(` from the function/method name, *unless* it is a constructor or destructor. For constructors/destructors, the opening `(` should follow the name without any space in between.

Make use of `[[nodiscard]]` where appropriate. Always use trailing return types unless the function/method returns `void`. Put trailing return types including the `->` indented on the next line if it would make the signature too long.

Function signatures with too many arguments to fit on a line should open their argument list with `(` on the same line as the function name, with all arguments starting indented on the next line, then linebreak and unindent to put the closing `)`. A trailing return type may follow on the same line as the closing `)`.


### Bodies of functions/methods, loops and `if`/`else if`/`else` clauses

Avoid use of braces around single-statement bodies where it is allowed.

If the body cannot be made single-statement, put the opening `{` on the same line if the body is 5 lines or less. If it is more, the opening `{` should go on its own line.


### Class definitions

Note that these instructions concern complex ("actual") classes that define at least one method (i.e., those that go into a `Classes` section). They do not apply to simple structs that go under the `Structs and enums` section, or similarly, simple nested structs defined in a class. Such "simple" structs should just observe the general documentation/commenting rules.

Put the opening class brace on its own line. Align access labels (`public:`, `protected:`, `private:`) with the `class` keyword. Apply the same relative indentation to nested classes.

Group all members using two-line intra-class section headers, indented with the members. For a example in a top-level/not-nested class:
```C++
	////
	// $ClassSection
```

Leave one blank line after a class-section header and between documented members, and two blank lines before each subsequent class-section. At an access change, put the two blank lines before the access label and one blank line between the label and the next class-section header.

Normally put the public interface first, followed by protected and private implementation details. `Friend declarations` and internal `Types` class-sections may precede the first `public:` label, using the default private access. If there is no need for such a first private access block, the initial `public:` should immediatly follow under the first line after the opening `{`.
Within each access block, use the following class-sections as applicable, normally in this order; omit empty ones:
* `Types`: nested classes, structs, enums and `using` aliases.
* `Constants`: named constants, when they warrant a separate section.
* `Object construction/destruction`: constructors, static factories, destructor, and copy/move constructors and assignment operators, including `= default` and `= delete` declarations. `Object construction` is suitable for a section containing only constructors.
* `Interface: BaseType`: overrides grouped by the interface they implement, one section per base interface. Use `override` on overriding methods.
* `Accessors`: queries and simple access to object state.
* `Methods`: other operations and implementation helpers. Descriptive groups such as `Transfers` or `Bindings` may replace this heading when useful.
* `Fields`: data members, normally after methods. Keep related fields together, but always put a documentation comment before each individual field. Never group fields under a single doc comment. Provide in-class initializers for fixed defaults.

Keep related overloads together. Short accessors and forwarding methods may be defined directly in the class; larger non-template implementations belong outside the definition, under the corresponding source file's `Class implementations` section. Header-only classes or locally defined classes inside a `.cpp` file always inline all method definitions inside the class definition.

Document the class and every member with preceding triple-slash `///` Doxygen comments, following the documentation rules below. This includes non-public members, nested types and aliases, enum values, and defaulted or deleted special members. Document why each friend needs access in its `Friend declarations` section. Section headers use ordinary comments, not Doxygen comments.


### Enum and (simple) struct definitions

Whether the opening brace is on the same line as the struct name depends on how long the struct or enum is going to be. If it is at least 6 lines including doc comments, the opening `{` should be on its own line. Definitions 5 lines or shorter should have the `{` behind the struct or enum name separated by a single whitespace ` `.



## Documenting the code

### Doc comments

Always use triple-slash `///` doc comments. Documented items (such as classes, structs, enums, functions, members, etc.) must always be separated by at least one blank line.

Always typeset mentions of types, enum values, functions/methods, globals and inline snippets as code. Use `\c` when possible, fall back to `<tt>`/`</tt>` instead when using `\c` is impractical, e.g. for whitespace-containing snippets or when `\c` would consume punctuation (full stops, commas, colons, semicolons). In free-text documentation such as library overviews and module guides, when such a mention (*except* code snippets, no matter if block or inline) concerns an item from this project, directly link to the corresponding API documentation in addition to typesetting as code. When referring to individual functions or methods, do not add paranthesis `()`. When using an individual function or method in an inline snipped, do add `()` (plus required arguments if any) when the snipped constitutes a function call.

Always observe the following ordering of Doxygen paragraph-level commands (skipping those that are not applicable):
1. `\brief`
2. `\pre`
3. `\note`
4. `\tparam`
5. `\param`
6. `\return`
7. `\throws`
8. `\todo`

Doxygen paragraph-level commands that are not included in above list can be inserted anywhere in the documentation block at your discretion, with one exception:
Long descriptions always follow the `\brief` paragraph; no other paragraph-level commands must be inserted between `\brief` and the long description.
Long descriptions may be multi-paragraph.

Between paragraphs, there should always be a blank line inside the doc comment (i.e., just `///`) *unless* these paragraphs are started with identical Doxygegn commands to form logical blocks – for example, 2 or more `\tparam`, `\param` or `\todo` commands must not have a blank `///` line between them, to visually inform readers of their block nature.

If the contents of a Doxygen paragraph-level command are too long, then the command should be on its own line, with the paragraph contents starting indented on the next line. For example:
```C++
/// \brief
/// 	Convenience function for the longer-form equivalent that omits this and that parameter, for which some
/// 	reasonable defaults are assumed.
```

If such a multi-line paragraph-level command takes an argument (e.g., `\param` and `\tparam`), the argument goes on the same line as the command:
```C++
/// \param length
/// 	The number of elements to allocate. The actual memory allocated in terms of bytes can exceed this to meet
/// 	alignment requirenments.
```

A doc comment consisting of more than one paragraph must always start with an explicit `\brief` paragraph. Always use
`\return`, never `\returns`. Never use Markdown backticks for code inside doc comments.


### API documentation content

API-level doc comments address users of the framework – application developers and students who know C++ and basic GPU
programming concepts, but were not involved in developing the framework and do not know its internals. Write them in
full sentences. Each public doc comment answers, as far as applicable:
* what the item is or does, and when or where to use it (e.g. "Call once per frame from \c Applet::update.");
* what the caller must guarantee (`\pre`): lifetimes, threading, call order, valid ranges;
* what is returned, the meaning of each error, and every exception type intentionally thrown (`\throws`);
* ownership and lifetime of inputs, outputs and borrowed references;
* units, coordinate conventions, defaults, clamping ranges and tolerances;
* user-visible side effects (e.g. consuming an event, blocking or not blocking).

Every public function or method documents each parameter with `\param`, each template parameter with `\tparam`, any
non-void result with `\return` and each intentionally thrown exception type with `\throws`. Trivial accessors may use a
single sentence if their result is self-evident.

Never put any of the following into public doc comments:
* development history or versioning ("formerly", "now", "no longer", "renamed", "initial", "yet", "future");
* design rationale or solved internal problems ("because …", "so that …", "the main reason for …");
* internal mechanisms, private member or type names, and internal vocabulary not defined by the public API;
* reviewer-style negative statements ("does not map, submit or synchronize") – unless the negative is a guarantee users
  rely on, such as never blocking;
* `\todo` items that are design musings.

A backend quirk is only documented if knowing about it genuinely benefits a user; describe each such quirk in user terms
in exactly one `\note` paragraph.

Doc comments on private members and module-private symbols are aimed at maintainers and may describe internals, but
they show up in IDE documentation previews and must therefore still be clear, accurate and useful. Friend declarations
appear in the generated documentation, so their doc comments must read sensibly to users as well.


### Library overviews and module guides

Narrative documentation comes in exactly two kinds:

* A **library overview** is a Doxygen page (`\page fcg_<library>_overview "<Library> library overview"`, one per
  library) giving a broad overview of the library's purpose and scope, and use-case-driven pointers on where to start
  reading the module guides.
* A **module guide** is a Doxygen group (`\defgroup`, essentially one per public header, all belonging to
  `\ingroup fcg_modules`) containing the actual usage guidance for that module.

These blocks address inexperienced new users of the framework who seek usage guidance and have no knowledge about or
interest in its internals. Write them on top of, and consistent with, the API documentation content rules above; the
beginner audience rule is paramount in any conflict.

**Placement and structure.**
* The overview lives in the library's umbrella header if one exists (e.g. `FCG/Extras/all.h`), otherwise in the
  library's most principal public header, the one defining its central interface (e.g. `image.h`, `applet.h`). Under a
  `Library documentation` section banner.
* The module guide is defined with `\defgroup` in the module's principal header, under a `Module documentation` section
  banner. It carries `\ingroup fcg_modules` directly after the group line, followed by an explicit `\brief` paragraph.
* Auxiliary headers that belong to a module join it via a `\addtogroup <group>` / `@{` … `@}` membership block inside
  the declaration namespace; use explicit `\ingroup` where needed.
* Strive to keep a 1:1 relationship between modules and headers. Split-off headers that do not concern users directly
  (e.g. `frame.h`) or are meaningless in isolation may share a module; this should remain the exception.
* Group IDs (e.g. `fcg_buffers`) and section anchors are stable once published. Rename them only deliberately, and
  update every `\ref` in the same change.

**Library overview content.**
* What the library is for and its scope, in user terms.
* How to use it: the CMake link target (`FCG-Framework::<Library>`) and the include line.
* "Where to start": use-case-driven pointers into the module guides (e.g. "To draw many textured quads, start with
  \ref fcg_primitives").
* Optionally short task-oriented subsections (e.g. enabling additional image formats). Full build-system and dependency
  reference belongs to the maintainer documentation, not the overview.

**Module guide skeleton** (section anchor IDs use the full group ID):
1. `\brief` one-liner.
2. Intro paragraph: what the module is for, when you need it, and its principal types with links.
3. `\section <group>_usage` "Getting started": mandatory. A minimal compiled snippet whenever users write code with the
   module.
4. `\section <group>_workflows` "Common tasks": mandatory.
5. `\section <group>_lifetime` "Ownership and lifetime": optional. Only for genuinely module-wide rules; individual
   contracts belong into the class or method documentation.
6. `\section <group>_errors` "Handling errors": optional.
7. `\section <group>_examples` "More examples": optional.

`\subsection`s within sections are free. Explain concepts before details and show the typical call sequence in the
order a user writes it.

**Never put any of the following into overviews or guides:**
* development history or versioning ("former", "renamed", "this version", "future", "initial", "source compatibility");
* design rationale or solved internal problems;
* internal mechanisms and vocabulary (e.g. tickets, staging pages, tokens, snapshots, fence retirement, registration
  internals);
* reviewer-style negative statements (e.g. "no logger, no cache, no background task is introduced");
* build-tree or source-layout paths (e.g. "Core lives in `core/`");
* backend details, unless knowing them genuinely benefits a user, then as exactly one `\note` paragraph in user terms.

**Snippets.** C++ examples are `\snippet`s from compiled example sources registered via `EXAMPLE_DIRS` in
`fcg_register_documentation`; never uncompiled `\code` blocks. CMake snippets in `\code{.cmake}` are allowed. The
normal build compiles all example sources.
