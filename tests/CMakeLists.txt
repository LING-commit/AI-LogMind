# LogMind Test Infrastructure
# Usage: cmake -DBUILD_TESTS=ON ..

option(BUILD_STRESS_TESTS "Build performance / stress tests" ON)

# 三个测试可执行文件都直接编 daemon 侧的核心源码（不含 Qt 部分）
set(LOGMIND_TEST_CORE
  ../src/daemon/log_buffer.cpp
  ../src/daemon/plugin_api.cpp
  ../src/daemon/pipeline.cpp
  ../src/daemon/plugin_loader.cpp
)

function(logmind_add_test target source)
  add_executable(${target} ${source} ${LOGMIND_TEST_CORE})
  target_include_directories(${target} PRIVATE ${PROJECT_SOURCE_DIR}/include)
  target_link_libraries(${target} PRIVATE
    logmind_compiler_settings
    logmind::plugin_sdk
    Threads::Threads
    ${CMAKE_DL_LIBS}
  )
  # 与守护进程同理：测试程序也要导出动态符号，否则被 dlopen 的插件
  # 解析不到 ICollector 等接口的 typeinfo
  set_target_properties(${target} PROPERTIES ENABLE_EXPORTS ON)
endfunction()

# ── 供插件加载/卸载测试用的 mock 插件 ──
# 原先这个文件躺在 tests/plugins/ 里但从未被编译，所以插件相关的压测
# 根本没有插件可加载。
add_library(mock_collector MODULE plugins/mock_collector.cpp)
target_link_libraries(mock_collector PRIVATE logmind::plugin_sdk)
set_target_properties(mock_collector PROPERTIES
  PREFIX ""
  CXX_VISIBILITY_PRESET hidden
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/plugins
)

# ── smoke ──
logmind_add_test(logmind-smoke-test smoke/smoke_test.cpp)
add_test(NAME smoke-test
  COMMAND logmind-smoke-test
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

# ── stress ──
if(BUILD_STRESS_TESTS)
  logmind_add_test(logmind-stress-test stress/stress_test.cpp)
  target_compile_options(logmind-stress-test PRIVATE
    -Wno-unused-function -Wno-unused-variable
  )
  add_dependencies(logmind-stress-test mock_collector)
  add_test(NAME stress-test
    COMMAND logmind-stress-test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )
  # 压测有真实耗时，给足超时
  set_tests_properties(stress-test PROPERTIES TIMEOUT 300)
endif()

# ── riscv（跨架构冒烟；在本机也能跑）──
# README 里提到过这个可执行文件，但它之前没有被任何 CMake 目标构建。
logmind_add_test(logmind-riscv-test stress/riscv_test.cpp)
target_compile_options(logmind-riscv-test PRIVATE
  -Wno-unused-function -Wno-unused-variable
)
add_test(NAME riscv-test
  COMMAND logmind-riscv-test
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

message(STATUS "Tests built: smoke=ON, stress=${BUILD_STRESS_TESTS}, riscv=ON")
