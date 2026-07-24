cmake_minimum_required(VERSION 3.20)

project(reloop_control C)

add_library(reloop_control MODULE src/reloop_control.c)
set_target_properties(reloop_control PROPERTIES PREFIX "" SUFFIX ".asi" OUTPUT_NAME "reloop_control")
target_compile_definitions(reloop_control PRIVATE WIN32_LEAN_AND_MEAN)

if(MINGW)
  target_compile_options(reloop_control PRIVATE -Wall -Wextra -Werror -O2)
  target_link_libraries(reloop_control PRIVATE ws2_32)
  target_link_options(reloop_control PRIVATE
    -Wl,--subsystem,windows
    -Wl,--exclude-all-symbols
    -Wl,-Map,$<TARGET_FILE_DIR:reloop_control>/reloop_control.map
    -static-libgcc
  )
endif()
