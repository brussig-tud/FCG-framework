
#ifndef __FCG_UTIL_H__
#define __FCG_UTIL_H__


//////
//
// Includes
//

// C++ STL
#include <concepts>
#include <functional>
#include <optional>
#include <utility>



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {



//////
//
// Interfaces
//

template<typename F, typename T>
concept MapFunction = std::invocable<F, const T&>;



//////
//
// Functions
//

/// Utility for mapping the contained value of an \ref std::optional to another.
template<typename T, typename F> requires MapFunction<F, T>
inline auto map (const std::optional<T> &opt, F&& f) -> std::optional<std::invoke_result_t<F, const T&>> {
	opt ? std::invoke(std::forward<F>(f), *opt) : std::nullopt;
}



//////
//
// Namespaces close
//

// namespace FCG
}


#endif  // ifndef __FCG_UTIL_H__
