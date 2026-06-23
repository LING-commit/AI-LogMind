# ── Plugin auto-discovery ──
# Usage: add_plugin_subdirs(plugins/ COLLECTORS analyzers alerts)
#
# Each plugin subdirectory must contain a CMakeLists.txt that builds a MODULE library.
# The MODULE type produces a .so (without "lib" prefix) suitable for dlopen.

function(add_plugin_subdirs plugin_root)
  set(one_value_args COLLECTORS ANALYZERS ALERTS)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "${one_value_args}")

  if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${plugin_root}")
    message(STATUS "Plugin directory not found: ${plugin_root}")
    return()
  endif()

  file(GLOB plugin_dirs RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/${plugin_root}"
       "${plugin_root}/*/")

  foreach(dir ${plugin_dirs})
    string(REGEX REPLACE "/$" "" dir "${dir}")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/CMakeLists.txt")
      add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/${dir}" "${CMAKE_CURRENT_BINARY_DIR}/${dir}")
      get_filename_component(plugin_name "${dir}" NAME)

      # Install the resulting .so
      install(TARGETS "${plugin_name}"
              LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/logmind/plugins")
    endif()
  endforeach()
endfunction()

# ── Plugin helper: define a MODULE library ──
# Plugin shared objects are named "parser_nginx.so" (no "lib" prefix).
function(add_logmind_plugin target)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SOURCES;LINK_LIBS")

  add_library(${target} MODULE ${ARG_SOURCES})
  set_target_properties(${target} PROPERTIES
    PREFIX ""
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
  )
  target_link_libraries(${target} PRIVATE
    logmind_compiler_settings
    logmind::plugin_sdk
    ${ARG_LINK_LIBS}
  )
  target_include_directories(${target} PRIVATE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  )
endfunction()
