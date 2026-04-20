require "json"

package = JSON.parse(File.read(File.join(__dir__, "package.json")))

Pod::Spec.new do |s|
  s.name         = "TurboDB"
  s.version      = package["version"]
  s.summary      = package["description"]
  s.homepage     = package["homepage"]
  s.license      = package["license"]
  s.authors      = package["author"]

  s.platforms    = { :ios => min_ios_version_supported }
  s.source       = { :git => ".git", :tag => "#{s.version}" }

  s.source_files = "ios/**/*.{h,m,mm}", "cpp/**/*.{hpp,cpp,c,h}"
  s.private_header_files = "ios/**/*.h"

  # Exclude generated codegen files and dead ThreadPool (replaced by DBScheduler)
  s.exclude_files = "ios/generated/**/*", "cpp/ThreadPool.cpp"

  s.dependency 'libsodium'

  # Folly config defines — FOLLY_HAS_COROUTINES=0 prevents 'folly/coro/Coroutine.h' not found
  # error on Xcode 26 Beta where the SDK does not ship that header yet.
  # std=c++20 required by DBScheduler (priority_queue comparator) and WALManager 2-pass recovery.
  s.pod_target_xcconfig = {
    "OTHER_CPLUSPLUSFLAGS" => "-DFOLLY_NO_CONFIG -DFOLLY_MOBILE=1 -DFOLLY_USE_LIBCPP=1 -DFOLLY_HAS_COROUTINES=0 -DSODIUM_STATIC=1 -std=c++20",
    "CLANG_CXX_LANGUAGE_STANDARD" => "c++20",
    "HEADER_SEARCH_PATHS" => "\"$(PODS_TARGET_SRCROOT)/cpp\" \"$(PODS_ROOT)/libsodium/src/libsodium/include/sodium\""
  }

  install_modules_dependencies(s)
end