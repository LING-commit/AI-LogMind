# ── 示例插件 ──
#
# 三件套刚好覆盖流水线的三个环节：Collector → Analyzer → Alert。
# 编出来的 .so 装到 lib/logmind/plugins/，守护进程默认会从那里加载。

function(logmind_add_plugin name)
  add_library(${name} MODULE ${ARGN})
  target_link_libraries(${name} PRIVATE logmind::plugin_sdk)
  set_target_properties(${name} PROPERTIES
    PREFIX ""                 # 输出 syslog_collector.so 而不是 libsyslog_collector.so
    CXX_VISIBILITY_PRESET hidden
  )
  # create_plugin / destroy_plugin / plugin_abi_version 必须导出，其余符号隐藏，
  # 避免多个插件之间的符号互相污染
  target_compile_definitions(${name} PRIVATE LOGMIND_PLUGIN=1)
  install(TARGETS ${name} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/logmind/plugins)
endfunction()

logmind_add_plugin(syslog_collector    syslog_collector/syslog_collector.cpp)
logmind_add_plugin(threshold_analyzer  threshold_analyzer/threshold_analyzer.cpp)
logmind_add_plugin(stdout_alert        stdout_alert/stdout_alert.cpp)
