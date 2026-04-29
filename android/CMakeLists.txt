cmake_minimum_required(VERSION 3.4.1)
project(TurboDB)

set(PACKAGE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Libsodium configuration for Android
add_library(sodium SHARED IMPORTED)
set_target_properties(sodium PROPERTIES IMPORTED_LOCATION 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libsodium.so)

add_library(react-native-turbo-db SHARED
    src/main/cpp/TurboDBModule.cpp
    ../cpp/BinarySerializer.cpp
    ../cpp/BufferedBTree.cpp
    ../cpp/DBEngine.cpp
    ../cpp/DBScheduler.cpp
    ../cpp/Compactor.cpp
    ../cpp/MMapRegion.cpp
    ../cpp/PersistentBPlusTree.cpp
    ../cpp/WALManager.cpp
    ../cpp/SodiumCryptoContext.cpp
    ../cpp/CachedCryptoContext.cpp
    ../cpp/LazyRecordProxy.cpp
    ../cpp/ValueCache.cpp
    ../cpp/Compression.cpp
    # ThreadPool.cpp removed — replaced by DBScheduler
)

set_target_properties(
    react-native-turbo-db PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

set(REACT_NATIVE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../node_modules/react-native")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/generated/jni
    ${CMAKE_CURRENT_SOURCE_DIR}/generated/jni/react/renderer/components/TurboDBSpec
    ${REACT_NATIVE_DIR}/ReactCommon/jsi
    ${REACT_NATIVE_DIR}/ReactCommon
    ${REACT_NATIVE_DIR}/React
)

# Find system libraries
find_library(LOG_LIB log)
find_library(ANDROID_LIB android)
find_library(ZLIB_LIB z)

# React Native prefab targets
find_package(ReactAndroid REQUIRED CONFIG)

target_link_libraries(react-native-turbo-db
    ${LOG_LIB}
    ${ANDROID_LIB}
    ${ZLIB_LIB}
    sodium
    ReactAndroid::jsi
    ReactAndroid::reactnative
)
