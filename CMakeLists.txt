cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(rlcd_agent_monitor)

# A failed LVGL assertion, such as an allocation that came back empty, must be a
# panic with a core dump and a reboot rather than the default while(1): that
# default once parked the render thread for good with the task watchdog firing
# every five seconds and nothing on the console to say why. The Kconfig offers
# only the header to include; the handler body has to come from here.
idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
# The semicolon is escaped because an unescaped one is a CMake list separator.
target_compile_definitions(${lvgl_lib} PUBLIC "LV_ASSERT_HANDLER=abort()\;")
