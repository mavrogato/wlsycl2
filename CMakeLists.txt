
cmake_minimum_required(VERSION 3.14)

set(PROJ "wlsycl2")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_COMPILER "icpx")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS_DEBUG "-Od -g3")

project(${PROJ})

find_package(IntelDPCPP REQUIRED)
include_directories(
  /opt/intel/oneapi/compiler/2022.0.2/linux/include/sycl)

add_executable(${PROJ}
  main.cc)

target_compile_options(${PROJ}
  PRIVATE
  -Wall
  -std=c++20
  -fcoroutines-ts)

target_link_libraries(${PROJ}
  PRIVATE
  wayland-client)

add_custom_target(run
  DEPENDS ${PROJ}
  COMMAND WAYLAND_DEBUG=1 ./${PROJ})
