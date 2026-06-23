set(LOGMIND_CLIENT_SOURCES
  main.cpp
  dbus_client.h
  dbus_client.cpp
  main_window.h
  main_window.cpp
  models/log_model.h
  models/log_model.cpp
  models/log_filter_model.h
  models/log_filter_model.cpp
  widgets/log_view.h
  widgets/log_view.cpp
  widgets/search_bar.h
  widgets/search_bar.cpp
  widgets/plugin_panel.h
  widgets/plugin_panel.cpp
  widgets/alert_panel.h
  widgets/alert_panel.cpp
  widgets/status_bar_widget.h
  widgets/status_bar_widget.cpp
  widgets/log_detail_panel.h
  widgets/log_detail_panel.cpp
  widgets/dashboard_widget.h
  widgets/dashboard_widget.cpp
)

add_executable(logmind-client ${LOGMIND_CLIENT_SOURCES})
target_link_libraries(logmind-client PRIVATE
  logmind_compiler_settings
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  Qt6::DBus
)

qt6_add_resources(logmind-client "resources"
  PREFIX "/styles"
  FILES resources/style.qss resources/style_light.qss
)

install(TARGETS logmind-client RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
