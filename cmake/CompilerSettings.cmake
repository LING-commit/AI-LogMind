if(COMMAND cmake_policy)
  cmake_policy(SET CMP0091 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  message(FATAL_ERROR "MSVC is not supported; LogMind requires GCC >= 11 or Clang >= 14")
endif()

# --- Compiler warnings ---
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

# --- LTO (Release) ---
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

# --- Sanitizers (Debug) ---
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

# --- Position-independent code ---
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
