# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(observer)

target_sources(app PRIVATE
  src/main.c
  src/observer.c
  src/lib/mylib/isc_msgs.c
)
zephyr_include_directories(src/lib/mylib)
