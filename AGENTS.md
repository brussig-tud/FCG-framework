## Preamble

The rules and guidelines laid out in this document might not be reflected accurately in the existing code base, as some of it was and still is being crafted by hand. Such instances should be considered outliers, the rules in this document are binding.



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

Avoid sections other than these unless there is a very good reason.

The very first section is always separated by 2 blank lines after the include guard in case of `.h` headers, and just a single blank first line in case of `.cpp` and `.inl` files. All following sections are always separated by 3 blank lines after the last content of the preceding section. The closing `#endif` of the include guard at the end of a header is only separated by 2 blank lines after the last content of the final section.

The following sections have special rules and/or mandatory content:


### *Includes*:

Group individual `#include` statements according to library, with the library being named by a short preciding comment, e.g.
```C++
// C++ STL
#include <print>
#include <vector>

// GLM library
#include <glm/glm.hpp>
```

The block `// C++ STL` should always be included if no STL headers are needed, just put `/* nothing here yet */`, e.g.:
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

Always prefer C-style casts unless there is a pressing reason to use a C++ cast.

### Formatting function signatures

Always separate the argument list with a single space ` ` before the opening `(` from the function/method name, *unless* it is a constructor or destructor. For constructors/destructors, the opening `(` should follow the name without any space in between.

Make use of `[[nodiscard]]` where appropriate. Always use trailing return types unless the function/method returns `void`. Put trailing return types including the `->` indented on the next line if it would make the signature too long.

Function signatures with too many arguments to fit on a line should open their argument list with `(` on the same line as the function name, with all arguments starting indented on the next line, then linebreak and unindent to put the closing `)`. A trailing return type may follow on the same line as the closing `)`.


### Bodies of functions/methods, loops and `if`/`else if`/`else` clauses

Avoid use of braces around single-statement bodies where it is allowed.

If the body cannot be made single-statement, put the opening `{` on the same line if the body is 5 lines or less. If it is more, the opening `{` should go on its own line.


### Class definitions

Note that these instructions concern complex ("actual") classes that define at least one method (i.e., those that go into a `Classes` section). They do not apply to simple structs that go under the `Structs and enums` section).

Put the opening class brace on its own line. Align access labels (`public:`, `protected:`, `private:`) with the `class` keyword. Apply the same relative indentation to nested classes.

Group all members using two-line intra-class section headers, indented with the members:
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



## Documenting the code

### Doc comments

Always use triple-slash `///` doc comments.

Always typeset mentions of types, enum values, functions/methods, globals and inline snippets as code. Use `\c` when possible, fall back to `<tt>`/`</tt>` instead when using `\c` is impractical, e.g. for whitespace-containing snippets or when `\c` would consume punctuation (full stops, commas, colons, semicolons). In free-text documentation such as library and module overviews or inside guides, when such a mention (*except* code snippets, no matter if block or inline) concerns an item from this project, directly link to the corresponding API documentation in addition to typesetting as code. When referring to individual functions or methods, do not add paranthesis `()`. When using an individual function or method in an inline snipped, do add `()` (plus required arguments if any) when the snipped constitutes a function call.

Always observe the following ordering of Doxygen paragraph-level commands (skipping those that are not applicable):
1. `\brief`
2. `\pre`
3. `\note`
4. `\tparam`
5. `\param`
6. `\return`
7. `\todo`

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
