cmake_minimum_required(VERSION 3.20)

project(samp_probe_asi C)

add_library(samp_probe MODULE
  src/samp_probe_asi.c
)

set_target_properties(samp_probe PROPERTIES
  PREFIX ""
  SUFFIX ".asi"
  OUTPUT_NAME "samp_probe"
)

target_compile_definitions(samp_probe PRIVATE
  WIN32_LEAN_AND_MEAN
)

if(MINGW)
  target_compile_options(samp_probe PRIVATE
    -Wall
    -Wextra
    -Wno-cast-function-type
    $<$<CONFIG:Debug>:-O0>
    $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Debug>:-ggdb>
    $<$<CONFIG:RelWithDebInfo>:-g3>
    $<$<CONFIG:RelWithDebInfo>:-ggdb>
    $<$<CONFIG:Release>:-fno-asynchronous-unwind-tables>
    $<$<CONFIG:Release>:-fno-unwind-tables>
    $<$<CONFIG:Release>:-fno-ident>
  )

  target_link_options(samp_probe PRIVATE
    -Wl,--subsystem,windows
    -Wl,--exclude-all-symbols
    -Wl,-Map,$<TARGET_FILE_DIR:samp_probe>/samp_probe.map
    -static-libgcc
  )
endif()
