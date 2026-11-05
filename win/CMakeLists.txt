cmake_minimum_required(VERSION 3.20)
project(SkiffWin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(Skiff WIN32
  src/main.cpp
  src/ui/Render.cpp
  src/ui/Chrome.cpp
  src/ui/Views.cpp
  src/ui/Terminal.cpp
)

target_include_directories(Skiff PRIVATE src)

target_compile_definitions(Skiff PRIVATE
  UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS
)

target_link_libraries(Skiff PRIVATE
  d2d1 dwrite user32 gdi32 ole32 shell32 shcore windowscodecs
)

set_target_properties(Skiff PROPERTIES
  OUTPUT_NAME "Skiff"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/bin"
)

if(MSVC)
  target_compile_options(Skiff PRIVATE /utf-8 /W3 /EHsc /MP)
  target_link_options(Skiff PRIVATE "/MANIFESTUAC:level='asInvoker' uiAccess='false'")
endif()
