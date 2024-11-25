cmake_minimum_required(VERSION 3.16.0)

#
## ESP32 OTA APP版本号设置 https://blog.csdn.net/toopoo/article/details/99693181
#
#set(PROJECT_VER "v1.0.0") # Override this PROJECT_VER or use "git describe" by default
list(APPEND EXTRA_COMPONENT_DIRS cached_components)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_ghota_example)
