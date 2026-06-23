cmake_minimum_required(VERSION 3.20)
project(logmind VERSION 0.1.0 LANGUAGES CXX)

# ── Qt MOC / UIC / RCC ──
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Export project root directory ──
set(LOGMIND_PROJECT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# ── Compiler settings ──
include(cmake/CompilerSettings.cmake)

# ── Build options ──
option(BUILD_CLIENT "Build Qt6 client" OFF)
option(BUILD_TESTS "Build tests" OFF)

# ── Dependencies ──
find_package(Qt6 6.2 REQUIRED COMPONENTS Core DBus)
find_package(nlohmann_json 3 REQUIRED)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(SQLITE3 sqlite3)
endif()
if(NOT SQLITE3_FOUND)
  find_path(SQLITE3_INCLUDE_DIRS sqlite3.h)
  find_library(SQLITE3_LIBRARIES sqlite3)
  if(NOT SQLITE3_INCLUDE_DIRS OR NOT SQLITE3_LIBRARIES)
    message(FATAL_ERROR "sqlite3 not found; install libsqlite3-dev")
  endif()
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
  ${SQLITE3_LIBRARIES}
)

if(WIN32)
  set_target_properties(logmind-daemon PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_include_directories(logmind-daemon PRIVATE
  ${SQLITE3_INCLUDE_DIRS}
)

# ── Installation ──
include(GNUInstallDirs)

install(TARGETS logmind-daemon RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY include/logmind
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h"
)

install(FILES cmake/PluginScan.cmake
  DESTINATION ${CMAKE_INSTALL_DATADIR}/logmind/cmake
)

if(CMAKE_INSTALL_SYSTEMD_SYSTEMDIR)
  install(FILES docs/logmind-daemon.service
    DESTINATION ${CMAKE_INSTALL_SYSTEMD_SYSTEMDIR}
  )
endif()

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
message(STATUS "── LogMind v${PROJECT_VERSION} ──")
message(STATUS "Qt6:         ${Qt6_DIR}")
message(STATUS "nlohmann_json: ${nlohmann_json_DIR}")
message(STATUS "SQLite3:     ${SQLITE3_LIBRARIES}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Build tests: ${BUILD_TESTS}")
