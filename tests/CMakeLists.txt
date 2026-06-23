# LogMind Test Infrastructure
# Usage: cmake -DBUILD_TESTS=ON ..
#
# Individual unit tests will be added as Google Test / QtTest targets
# in a future phase.  For now the stress test is built as a standalone
# executable that exercises LogBuffer / Pipeline / PluginLoader directly.

option(BUILD_STRESS_TESTS "Build performance / stress tests" ON)

if(BUILD_STRESS_TESTS)
  add_executable(logmind-stress-test
    stress/stress_test.cpp
    ../src/daemon/log_buffer.cpp
    ../src/daemon/plugin_api.cpp
    ../src/daemon/pipeline.cpp
    ../src/daemon/plugin_loader.cpp
  )
  target_include_directories(logmind-stress-test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
  )
  target_link_libraries(logmind-stress-test PRIVATE
    logmind_compiler_settings
    logmind::plugin_sdk
    ${CMAKE_DL_LIBS}
    pthread
  )
  target_compile_options(logmind-stress-test PRIVATE
    -Wno-unused-function -Wno-unused-variable
  )

  # Register with CTest
  add_test(NAME stress-test
    COMMAND logmind-stress-test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )
endif()

# Dead-simple smoke test that verifies the core headers compile
add_executable(logmind-smoke-test
  smoke/smoke_test.cpp
  ../src/daemon/log_buffer.cpp
  ../src/daemon/plugin_api.cpp
  ../src/daemon/pipeline.cpp
  ../src/daemon/plugin_loader.cpp
)
target_include_directories(logmind-smoke-test PRIVATE
  ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(logmind-smoke-test PRIVATE
  logmind_compiler_settings
  logmind::plugin_sdk
  ${CMAKE_DL_LIBS}
  pthread
)
add_test(NAME smoke-test
  COMMAND logmind-smoke-test
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

# RISC-V cross-compilation note (not built by default)
# Use: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/riscv64-toolchain.cmake ..
message(STATUS "Tests built: stress=${BUILD_STRESS_TESTS}, smoke=ON")
