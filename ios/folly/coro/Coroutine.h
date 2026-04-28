#pragma once
// ── folly/coro/Coroutine.h compatibility stub ──────────────────────────────
// ReactNativeDependencies ships folly headers but NOT folly/coro/*.
// folly/Expected.h and folly/Optional.h conditionally include this file when
// FOLLY_HAS_COROUTINES is truthy (auto-detected from C++20 <coroutine>).
//
// Strategy A — suppression (ideal):
//   Our podspec sets -DFOLLY_HAS_COROUTINES=0 via pod_target_xcconfig and
//   user_target_xcconfig. When that flag reaches the compiler before folly
//   portability headers run, FOLLY_HAS_COROUTINES is forced to 0 and this
//   file's body is never reached.
//
// Strategy B — shim (fallback):
//   For any pod compiled without our flag (e.g. ReactNativeDependencies), the
//   minimal folly::coro namespace below satisfies all usages in Expected.h
//   and Optional.h by aliasing the standard C++20 coroutine primitives.

#if defined(FOLLY_HAS_COROUTINES) && FOLLY_HAS_COROUTINES
#include <coroutine>

namespace folly {
namespace coro {

// Alias std C++20 coroutine primitives into folly::coro so that
// folly/Expected.h and folly/Optional.h can use folly::coro::coroutine_handle
// without the real folly/coro/ implementation being present.
template <typename Promise = void>
using coroutine_handle = std::coroutine_handle<Promise>;

using suspend_never  = std::suspend_never;
using suspend_always = std::suspend_always;

} // namespace coro
} // namespace folly

#endif // FOLLY_HAS_COROUTINES
