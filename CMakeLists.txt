cmake_minimum_required(VERSION 3.10)

project(watchdogd VERSION 1.0.0 DESCRIPTION "Basic watchdog daemon")

option(WITHOUT_WATCHDOG "Don't open watchdog device" OFF)
option(ENABLE_DEBUG "Enable debug output" OFF)

add_compile_options(
   -std=gnu11
  -O3 
  -Wall 
  -Wextra 
  -Werror
  -g 
  -D_GNU_SOURCE 
)

if(WITHOUT_WATCHDOG)
  add_compile_options(-DWITHOUT_WATCHDOG=1)
endif()

if(ENABLE_DEBUG)
  add_compile_options(-DDEBUG=1)
endif()

include(GNUInstallDirs)

find_library(UBOX ubox)
find_package(ubus_utils REQUIRED)

SET(SOURCES 
  src/watchdogd.c
  src/iterate_executables.c
  src/utils.c
  src/watchdog_fd.c
  include/debug.h
  include/iterate_executables.h
  include/utils.h
  include/watchdog_fd.h
)

add_executable(${PROJECT_NAME} ${SOURCES})

add_dependencies(${PROJECT_NAME} 
  ubus_utils
)

target_include_directories(${PROJECT_NAME}
  PRIVATE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
)

target_link_libraries(${PROJECT_NAME}
  ubus_utils
  ${UBOX}
)

set_target_properties(${PROJECT_NAME} 
  PROPERTIES 
    VERSION ${PROJECT_VERSION}
    OUTPUT_NAME ${PROJECT_NAME}
)

install(TARGETS ${PROJECT_NAME} 
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

