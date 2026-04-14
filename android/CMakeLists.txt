cmake_minimum_required(VERSION 3.4.1)
project(SecureDB)

set (CMAKE_VERBOSE_MAKEFILE ON)

add_library(
    react-native-secure-db    STATIC
    ../cpp/SecureDBImpl.cpp
)

set_target_properties(
    react-native-secure-db PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_include_directories(
    react-native-secure-db    PUBLIC
    ../cpp
)

target_link_libraries(
    react-native-secure-db    jsi
    reactnative
    react_codegen_SecureDBSpec
)
