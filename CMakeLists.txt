cmake_minimum_required(VERSION 3.20)
project(logmind VERSION 0.1.0 LANGUAGES CXX)

# ── Qt MOC / UIC / RCC ──
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Export project root directory ──
set(LOGMIND_PROJECT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# ── Compiler settings ──
if(COMMAND cmake_policy)
  cmake_policy(SET CMP0091 NEW)
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
  message(FATAL_ERROR "MSVC not supported; require GCC >= 11 or Clang >= 14")
endif()
add_library(logmind_compiler_settings INTERFACE)
target_compile_options(logmind_compiler_settings INTERFACE
  $<$<CXX_COMPILER_ID:GNU>:
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
    -Wshadow -Wnon-virtual-dtor -Wold-style-cast
    -Wcast-align -Wunused -Woverloaded-virtual
    -Wformat=2 -Wmisleading-indentation -Wduplicated-cond
    -Wduplicated-branches -Wlogical-op -Wnull-dereference
    -Wuseless-cast -Wdouble-promotion
  >
  $<$<CXX_COMPILER_ID:Clang>:
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
    -Wshadow -Wnon-virtual-dtor -Wold-style-cast
    -Wcast-align -Wunused -Woverloaded-virtual
    -Wformat=2 -Wnull-dereference -Wdouble-promotion
  >
)
# LTO 要先探测工具链是否支持，直接置位会让部分交叉工具链构建失败
include(CheckIPOSupported)
check_ipo_supported(RESULT LOGMIND_IPO_SUPPORTED OUTPUT LOGMIND_IPO_ERROR)
if(LOGMIND_IPO_SUPPORTED)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
else()
  message(STATUS "LTO 不可用，已跳过: ${LOGMIND_IPO_ERROR}")
endif()

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
if(ENABLE_ASAN)
  target_compile_options(logmind_compiler_settings INTERFACE -fsanitize=address -fno-omit-frame-pointer)
  target_link_options(logmind_compiler_settings INTERFACE -fsanitize=address)
endif()
if(ENABLE_UBSAN)
  target_compile_options(logmind_compiler_settings INTERFACE -fsanitize=undefined)
  target_link_options(logmind_compiler_settings INTERFACE -fsanitize=undefined)
endif()
if(ENABLE_TSAN)
  target_compile_options(logmind_compiler_settings INTERFACE -fsanitize=thread)
  target_link_options(logmind_compiler_settings INTERFACE -fsanitize=thread)
endif()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ── Build options ──
option(BUILD_CLIENT "Build Qt6 client" OFF)
option(BUILD_TESTS "Build tests" OFF)

# ── Dependencies ──
find_package(Qt6 6.2 REQUIRED COMPONENTS Core DBus)
find_package(nlohmann_json 3 REQUIRED)
find_package(Threads REQUIRED)

# 优先用 pkg-config 的 IMPORTED_TARGET：它会一并带上 include/library dirs，
# 裸库名容易在交叉编译时链到宿主机的库上。
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(SQLITE3 IMPORTED_TARGET sqlite3)
endif()
if(TARGET PkgConfig::SQLITE3)
  add_library(logmind_sqlite3 INTERFACE)
  target_link_libraries(logmind_sqlite3 INTERFACE PkgConfig::SQLITE3)
else()
  find_path(SQLITE3_INCLUDE_DIR sqlite3.h)
  find_library(SQLITE3_LIBRARY sqlite3)
  if(NOT SQLITE3_INCLUDE_DIR OR NOT SQLITE3_LIBRARY)
    message(FATAL_ERROR "sqlite3 not found; install libsqlite3-dev")
  endif()
  add_library(logmind_sqlite3 INTERFACE)
  target_include_directories(logmind_sqlite3 INTERFACE ${SQLITE3_INCLUDE_DIR})
  target_link_libraries(logmind_sqlite3 INTERFACE ${SQLITE3_LIBRARY})
endif()

# ── Plugin SDK (headers only) ──
add_library(logmind_plugin_sdk INTERFACE)
add_library(logmind::plugin_sdk ALIAS logmind_plugin_sdk)
target_include_directories(logmind_plugin_sdk INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(logmind_plugin_sdk INTERFACE
  nlohmann_json::nlohmann_json
  logmind_compiler_settings
)

# ── Daemon ──
set(DAEMON_SOURCES
  src/daemon/main.cpp
  src/daemon/daemon_core.h
  src/daemon/daemon_core.cpp
  src/daemon/plugin_api.cpp
  src/daemon/plugin_loader.cpp
  src/daemon/pipeline.cpp
  src/daemon/log_buffer.cpp
  src/daemon/file_watcher.cpp
  src/daemon/dbus_service.h
  src/daemon/dbus_service.cpp
  src/daemon/config_mgr.cpp
  src/daemon/vector_store.cpp
)

add_executable(logmind-daemon ${DAEMON_SOURCES})
target_link_libraries(logmind-daemon PRIVATE
  logmind_compiler_settings
  logmind::plugin_sdk
  Qt6::Core
  Qt6::DBus
  logmind_sqlite3
  Threads::Threads
  ${CMAKE_DL_LIBS}
)

# 关键：插件 SDK 是 header-only，ICollector/IAnalyzer/IAlert 的 typeinfo 与
# 离线虚函数都编在守护进程这个可执行文件里。不导出动态符号的话，插件 .so
# 一律 dlopen 失败（undefined symbol: _ZTIN7logmind10ICollectorE），
# 而且跨 .so 边界的 dynamic_cast 也需要 typeinfo 只有一份。
set_target_properties(logmind-daemon PROPERTIES ENABLE_EXPORTS ON)

# ── Installation ──
include(GNUInstallDirs)

install(TARGETS logmind-daemon RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY include/logmind
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h"
)

# ── Plugin scanning ──
if(IS_DIRECTORY "${PROJECT_SOURCE_DIR}/plugins")
  add_subdirectory(plugins)
endif()

# ── Client ──
if(BUILD_CLIENT)
  find_package(Qt6 6.2 REQUIRED COMPONENTS Gui Widgets)
  add_subdirectory(src/client)
endif()

# ── Tests ──
if(BUILD_TESTS AND IS_DIRECTORY "${PROJECT_SOURCE_DIR}/tests")
  enable_testing()
  add_subdirectory(tests)
endif()

# ── Summary ──
message(STATUS "── LogMind v${PROJECT_VERSION} (plugin ABI v2) ──")
message(STATUS "Qt6:            ${Qt6_DIR}")
message(STATUS "nlohmann_json:  ${nlohmann_json_DIR}")
message(STATUS "LTO:            ${LOGMIND_IPO_SUPPORTED}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Build client:   ${BUILD_CLIENT}")
message(STATUS "Build tests:    ${BUILD_TESTS}")
