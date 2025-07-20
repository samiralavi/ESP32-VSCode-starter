cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# add the component directories that we want to use
set(EXTRA_COMPONENT_DIRS
  "components/"
)

set(
  COMPONENTS
  # TODO: add additional esp-idf and espp components you want to use to the line below:
  main esptool_py activecpp wifi_provisioning esp_http_server cxx 
  app_trace bt esp_gdbstub esp_http_client esp_https_ota espcoredump
  esp_psram
  CACHE STRING
  "List of components to include"
  )


project(esp32_starter)

set(CMAKE_CXX_STANDARD 20)
