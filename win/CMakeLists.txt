cmake_minimum_required(VERSION 3.20)
project(SkiffWin LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

get_filename_component(CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../entry/src/main/cpp" ABSOLUTE)
get_filename_component(LIBSSH2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libssh2-install" ABSOLUTE)
get_filename_component(COMPAT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/compat" ABSOLUTE)

set(CORE_SOURCES
  ${CORE_DIR}/command/command.cpp
  ${CORE_DIR}/jsonAnalyse/jsonAnalyse.cpp
  ${CORE_DIR}/protocol/SessionFactory/SessionFactory.cpp
  ${CORE_DIR}/protocol/SessionManager/SessionManager.cpp
  ${CORE_DIR}/protocol/proxy/httpConnectClient.cpp
  ${CORE_DIR}/protocol/proxy/proxyClient.cpp
  ${CORE_DIR}/protocol/proxy/socks5Client.cpp
  ${CORE_DIR}/protocol/ssh/LogManager.cpp
  ${CORE_DIR}/protocol/ssh/SftpSession.cpp
  ${CORE_DIR}/protocol/ssh/SshPortForward.cpp
  ${CORE_DIR}/protocol/ssh/SshSession.cpp
  ${CORE_DIR}/protocol/telnetSession/telnetSession.cpp
)

add_executable(Skiff WIN32
  src/main.cpp
  src/ui/Render.cpp
  src/ui/Chrome.cpp
  src/ui/Views.cpp
  src/ui/Terminal.cpp
  src/compat/Platform.cpp
  src/compat/Crypto.cpp
  src/core/SerialSessionWin.cpp
  src/core/SshService.cpp
  src/app/KeyGen.cpp
  src/app/Config.cpp
  res/app.rc
  ${CORE_DIR}/jsonAnalyse/cJSON.c
  ${CORE_SOURCES}
)

target_include_directories(Skiff BEFORE PRIVATE
  ${COMPAT_DIR}
  ${CORE_DIR}
  ${CORE_DIR}/include
  ${CORE_DIR}/protocol
  ${CORE_DIR}/command
  ${CORE_DIR}/jsonAnalyse
  ${LIBSSH2_DIR}/include
)

target_include_directories(Skiff PRIVATE src)

target_compile_definitions(Skiff PRIVATE
  UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS
  _WINSOCK_DEPRECATED_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS
)

set_source_files_properties(${CORE_SOURCES} "${CMAKE_CURRENT_SOURCE_DIR}/src/core/SerialSessionWin.cpp" PROPERTIES
  COMPILE_OPTIONS "/FI${COMPAT_DIR}/Platform.h"
)

target_link_libraries(Skiff PRIVATE
  d2d1 dwrite user32 gdi32 ole32 shell32 shcore windowscodecs
  ws2_32 bcrypt crypt32
  "${LIBSSH2_DIR}/lib/libssh2.lib"
)

set_target_properties(Skiff PROPERTIES
  OUTPUT_NAME "Skiff"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/bin"
)

if(MSVC)
  target_compile_options(Skiff PRIVATE /utf-8 /W1 /EHsc /MP /permissive-)
  target_compile_options(Skiff PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/wd4996 /wd4267 /wd4244 /wd4018 /wd4100>)
  target_link_options(Skiff PRIVATE "/MANIFESTUAC:level='asInvoker' uiAccess='false'")
endif()
