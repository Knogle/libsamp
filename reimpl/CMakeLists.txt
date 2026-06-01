cmake_minimum_required(VERSION 3.20)

project(sampdll_reimpl C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(SAMPDLL_BUILD_TESTS "Build host-side tests" ON)
option(SAMPDLL_ENABLE_RAKNET_KNOGLE "Integrate vendored Knogle RakNet variant" ON)
option(SAMPDLL_ENABLE_LEGACY_SAMP_CLIENT_TRANSPORT "Enable SA-MP legacy client UDP datagram transform" ON)

set(SAMPDLL_RAKNET_KNOGLE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/raknet-knogle")

set(SAMPDLL_NET_SOURCES
  src/net/dual_stack.c
  src/net/legacy_ipv4_compat.c
  src/net/tcp_bootstrap_manager.c
)

if(SAMPDLL_ENABLE_RAKNET_KNOGLE AND EXISTS "${SAMPDLL_RAKNET_KNOGLE_DIR}/CMakeLists.txt")
  set(RAKNET_BUILD_SAMP_GLUE OFF CACHE BOOL "Build SAMPRakNet.cpp glue module" FORCE)
  set(RAKNET_BUILD_TOOLS OFF CACHE BOOL "Build probe tools" FORCE)
  set(RAKNET_REQUIRE_OMP_SDK OFF CACHE BOOL "Link against OMP-SDK target" FORCE)
  add_subdirectory("${SAMPDLL_RAKNET_KNOGLE_DIR}" "${CMAKE_BINARY_DIR}/raknet-knogle-build" EXCLUDE_FROM_ALL)
  if(SAMPDLL_ENABLE_LEGACY_SAMP_CLIENT_TRANSPORT)
    target_compile_definitions(raknet PUBLIC RAKNET_ENABLE_LEGACY_SAMP_CLIENT_TRANSPORT=1)
  endif()

  list(APPEND SAMPDLL_NET_SOURCES
    src/net/raknet_bridge.cpp
    src/net/raknet_client_adapter.cpp
    ../samp/raknet/SAMP/samp_auth.cpp
  )
elseif(SAMPDLL_ENABLE_RAKNET_KNOGLE)
  message(WARNING "SAMPDLL_ENABLE_RAKNET_KNOGLE=ON but vendored path not found: ${SAMPDLL_RAKNET_KNOGLE_DIR}")
endif()

if(NOT TARGET raknet)
  list(APPEND SAMPDLL_NET_SOURCES
    src/net/raknet_client_adapter_stub.c
  )
endif()

add_library(sampdll_net STATIC ${SAMPDLL_NET_SOURCES})

target_include_directories(sampdll_net
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(WIN32)
  if(TARGET raknet)
    target_link_libraries(sampdll_net PUBLIC ws2_32)
  else()
    target_link_libraries(sampdll_net PUBLIC wsock32)
  endif()
else()
  target_link_libraries(sampdll_net PUBLIC pthread)
endif()

if(TARGET raknet)
  target_link_libraries(sampdll_net PUBLIC raknet)
endif()

if(WIN32)
  add_library(samp SHARED
    src/dllmain.c
    src/runtime_bridge.c
    src/runtime_hook_bridge.c
    src/samp.rc
  )

  target_include_directories(samp
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

  target_link_libraries(samp PRIVATE sampdll_net gdi32)

  set_target_properties(samp PROPERTIES
    OUTPUT_NAME "samp"
    PREFIX ""
  )

  if(MINGW)
    target_compile_options(samp PRIVATE
      -fno-asynchronous-unwind-tables
      -fno-unwind-tables
      -fno-ident
      -ffunction-sections
      -fdata-sections
    )
    target_compile_options(sampdll_net PRIVATE
      -fno-asynchronous-unwind-tables
      -fno-unwind-tables
      -fno-ident
      -ffunction-sections
      -fdata-sections
    )

    # Match legacy PE profile where possible: fixed image base, no exports, GUI subsystem.
    target_link_options(samp PRIVATE
      -Wl,--subsystem,windows
      -Wl,--image-base,0x10000000
      -Wl,--file-alignment,0x1000
      -Wl,--disable-dynamicbase
      -Wl,--disable-nxcompat
      -Wl,--stack,0x100000
      -Wl,--heap,0x100000
      -Wl,--exclude-all-symbols
      -Wl,--gc-sections
      -static-libgcc
      -static-libstdc++
    )
    # RakNet/C++ thread usage can pull winpthread dynamically; force static link to avoid runtime DLL dependency.
    target_link_libraries(samp PRIVATE -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic)
  endif()
endif()

if(SAMPDLL_BUILD_TESTS)
  include(CTest)
  enable_testing()

  add_executable(test_dual_stack
    tests/test_dual_stack.c
  )
  target_include_directories(test_dual_stack
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  target_link_libraries(test_dual_stack PRIVATE sampdll_net)

  add_test(NAME dual_stack COMMAND test_dual_stack)

  add_executable(test_legacy_ipv4_compat
    tests/test_legacy_ipv4_compat.c
  )
  target_include_directories(test_legacy_ipv4_compat
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  target_link_libraries(test_legacy_ipv4_compat PRIVATE sampdll_net)

  add_test(NAME legacy_ipv4_compat COMMAND test_legacy_ipv4_compat)

  add_executable(test_tcp_bootstrap_manager
    tests/test_tcp_bootstrap_manager.c
  )
  target_include_directories(test_tcp_bootstrap_manager
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  target_link_libraries(test_tcp_bootstrap_manager PRIVATE sampdll_net)

  add_test(NAME tcp_bootstrap_manager COMMAND test_tcp_bootstrap_manager)

  if(TARGET raknet)
    add_executable(test_samp_client_transport
      tests/test_samp_client_transport.cpp
    )
    target_link_libraries(test_samp_client_transport PRIVATE sampdll_net)

    add_test(NAME samp_client_transport COMMAND test_samp_client_transport)

    add_executable(test_raknet_bridge
      tests/test_raknet_bridge.c
    )
    target_include_directories(test_raknet_bridge
      PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_raknet_bridge PRIVATE sampdll_net)

    add_test(NAME raknet_bridge COMMAND test_raknet_bridge)
  endif()
endif()
