cmake_minimum_required(VERSION 3.20)

project(samp_re C)

add_library(samp_re_rpc178_logic STATIC
  src/rpc178_logic.c
)
target_include_directories(samp_re_rpc178_logic PUBLIC include)
target_compile_features(samp_re_rpc178_logic PUBLIC c_std_11)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(samp_re_rpc178_logic PRIVATE
    -Wall
    -Wextra
    -Werror
  )
endif()

add_library(samp_re_rpc175_logic STATIC
  src/rpc175_logic.c
)
target_include_directories(samp_re_rpc175_logic PUBLIC include)
target_compile_features(samp_re_rpc175_logic PUBLIC c_std_11)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(samp_re_rpc175_logic PRIVATE
    -Wall
    -Wextra
    -Werror
  )
endif()

if(WIN32)
  add_library(samp_re MODULE
    src/samp_re_asi.c
  )
  target_link_libraries(samp_re PRIVATE
    samp_re_rpc175_logic
    samp_re_rpc178_logic
    advapi32
  )
  target_include_directories(samp_re PRIVATE include)
  target_compile_definitions(samp_re PRIVATE
    WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0600
  )
  set_target_properties(samp_re PROPERTIES
    PREFIX ""
    SUFFIX ".asi"
    OUTPUT_NAME "samp_re"
  )

  if(MINGW)
    target_compile_options(samp_re PRIVATE
      -Wall
      -Wextra
      -Werror
      -O2
      -fno-asynchronous-unwind-tables
      -fno-unwind-tables
      -fno-ident
    )
    target_link_options(samp_re PRIVATE
      -Wl,--subsystem,windows
      -Wl,--exclude-all-symbols
      -Wl,-Map,$<TARGET_FILE_DIR:samp_re>/samp_re.map
      -static-libgcc
    )
  endif()
endif()

include(CTest)
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
  add_executable(samp_re_rpc178_logic_tests
    tests/test_rpc178_logic.c
  )
  target_link_libraries(samp_re_rpc178_logic_tests PRIVATE
    samp_re_rpc178_logic
  )
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(samp_re_rpc178_logic_tests PRIVATE
      -Wall
      -Wextra
      -Werror
    )
  endif()
  add_test(NAME samp_re_rpc178_logic COMMAND samp_re_rpc178_logic_tests)

  add_executable(samp_re_rpc175_logic_tests
    tests/test_rpc175_logic.c
  )
  target_link_libraries(samp_re_rpc175_logic_tests PRIVATE
    samp_re_rpc175_logic
  )
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(samp_re_rpc175_logic_tests PRIVATE
      -Wall
      -Wextra
      -Werror
    )
  endif()
  add_test(NAME samp_re_rpc175_logic COMMAND samp_re_rpc175_logic_tests)
endif()
