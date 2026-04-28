#pragma once
// Compatibility stub for React Native environments where the ReactNativeDependencies
// pod ships folly headers but NOT folly/coro. folly/Expected.h gates its
// coroutine machinery behind FOLLY_HAS_COROUTINES; when that flag auto-detects
// to 1 (C++20 + <coroutine> present) but the coro/ directory is absent this
// stub satisfies the include so the build does not fail.
// When actual coroutine support is needed this file should be replaced by the
// real folly/coro/Coroutine.h from a full folly installation.
