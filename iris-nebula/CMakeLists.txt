cmake_minimum_required(VERSION 3.16)
project(IrisNebula LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(IrisNebula WIN32
    src/main.cpp
    src/assets.cpp
    src/layout.cpp
    src/renderer.cpp
    src/settings.cpp
    resources.rc
)

# Screensavers are normal programs with a .scr ending.
set_target_properties(IrisNebula PROPERTIES SUFFIX ".scr")
target_compile_definitions(IrisNebula PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(IrisNebula PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(IrisNebula PRIVATE d3d11 dxgi d3dcompiler windowscodecs ole32 advapi32 user32 gdi32)

if(MINGW)
    # One self-contained file: no extra DLLs needed.
    target_link_options(IrisNebula PRIVATE -static -municode)
endif()

# Rebuild when the packed files change.
set_source_files_properties(resources.rc PROPERTIES OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/iris.jpg;${CMAKE_CURRENT_SOURCE_DIR}/assets/background.png;${CMAKE_CURRENT_SOURCE_DIR}/assets/stars.bin;${CMAKE_CURRENT_SOURCE_DIR}/src/shaders.hlsl;${CMAKE_CURRENT_SOURCE_DIR}/src/app.manifest")
