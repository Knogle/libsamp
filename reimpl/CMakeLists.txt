cmake_minimum_required(VERSION 3.20)

project(sampdll_reimpl C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(SAMPDLL_BUILD_TESTS "Build host-side tests" ON)
option(SAMPDLL_ENABLE_RAKNET_KNOGLE "Integrate vendored Knogle RakNet variant" ON)
option(SAMPDLL_ENABLE_SAMP_CLIENT_TRANSPORT "Enable SA-MP client UDP datagram transform" ON)

set(SAMPDLL_RAKNET_KNOGLE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/raknet-knogle")
set(SAMPDLL_REFERENCE_SAA "${CMAKE_CURRENT_SOURCE_DIR}/../analysis/metadata/installer_extract/samp.saa")

add_library(sampdll_archive STATIC
  src/archive/saa.c
)

target_include_directories(sampdll_archive
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(SAMPDLL_NET_SOURCES
  src/net/dual_stack.c
  src/net/ipv4_socket_compat.c
  src/net/tcp_bootstrap_manager.c
)

if(SAMPDLL_ENABLE_RAKNET_KNOGLE AND EXISTS "${SAMPDLL_RAKNET_KNOGLE_DIR}/CMakeLists.txt")
  set(RAKNET_BUILD_SAMP_GLUE OFF CACHE BOOL "Build SAMPRakNet.cpp glue module" FORCE)
  set(RAKNET_BUILD_TOOLS OFF CACHE BOOL "Build probe tools" FORCE)
  set(RAKNET_REQUIRE_OMP_SDK OFF CACHE BOOL "Link against OMP-SDK target" FORCE)
  add_subdirectory("${SAMPDLL_RAKNET_KNOGLE_DIR}" "${CMAKE_BINARY_DIR}/raknet-knogle-build" EXCLUDE_FROM_ALL)
  if(SAMPDLL_ENABLE_SAMP_CLIENT_TRANSPORT)
    target_compile_definitions(raknet PUBLIC RAKNET_ENABLE_LEGACY_SAMP_CLIENT_TRANSPORT=1)
  endif()

  set(SAMPDLL_RAKNET_KNOGLE_GLUE "${SAMPDLL_RAKNET_KNOGLE_DIR}/SAMPRakNet.cpp")
  set(SAMPDLL_RAKNET_AUTH_SOURCE "${CMAKE_BINARY_DIR}/raknet_knogle_auth_table.cpp")
  file(READ "${SAMPDLL_RAKNET_KNOGLE_GLUE}" SAMPDLL_RAKNET_KNOGLE_GLUE_TEXT)
  set(SAMPDLL_RAKNET_AUTH_BEGIN "} static const AuthTable[UCHAR_MAX + 1] = {")
  set(SAMPDLL_RAKNET_AUTH_END "\n};\n\nPair<uint8_t, StringView> SAMPRakNet::GenerateAuth")
  string(FIND "${SAMPDLL_RAKNET_KNOGLE_GLUE_TEXT}" "${SAMPDLL_RAKNET_AUTH_BEGIN}" SAMPDLL_RAKNET_AUTH_BEGIN_POS)
  string(FIND "${SAMPDLL_RAKNET_KNOGLE_GLUE_TEXT}" "${SAMPDLL_RAKNET_AUTH_END}" SAMPDLL_RAKNET_AUTH_END_POS)
  if(SAMPDLL_RAKNET_AUTH_BEGIN_POS EQUAL -1 OR SAMPDLL_RAKNET_AUTH_END_POS EQUAL -1)
    message(FATAL_ERROR "Could not locate Knogle RakNet SA-MP auth table in ${SAMPDLL_RAKNET_KNOGLE_GLUE}")
  endif()
  string(LENGTH "${SAMPDLL_RAKNET_AUTH_BEGIN}" SAMPDLL_RAKNET_AUTH_BEGIN_LEN)
  math(EXPR SAMPDLL_RAKNET_AUTH_ENTRIES_POS "${SAMPDLL_RAKNET_AUTH_BEGIN_POS} + ${SAMPDLL_RAKNET_AUTH_BEGIN_LEN}")
  math(EXPR SAMPDLL_RAKNET_AUTH_ENTRIES_LEN "${SAMPDLL_RAKNET_AUTH_END_POS} - ${SAMPDLL_RAKNET_AUTH_ENTRIES_POS}")
  string(SUBSTRING "${SAMPDLL_RAKNET_KNOGLE_GLUE_TEXT}" "${SAMPDLL_RAKNET_AUTH_ENTRIES_POS}" "${SAMPDLL_RAKNET_AUTH_ENTRIES_LEN}" SAMPDLL_RAKNET_AUTH_ENTRIES)
  file(WRITE "${SAMPDLL_RAKNET_AUTH_SOURCE}" "/* Generated at configure time from third_party/raknet-knogle/SAMPRakNet.cpp.
 * OPENMP_REF + INFERRED:
 * Knogle/RakNet is the vendored protocol source for this auth table. Keep this
 * generated instead of depending on local-only reverse-engineering workspaces.
 */
#include <cstring>

struct SampKnogleAuthEntry {
  const char *send;
  const char *recv;
};

static const SampKnogleAuthEntry kSampKnogleAuthTable[] = {${SAMPDLL_RAKNET_AUTH_ENTRIES}
};

extern \"C\" const char *samp_raknet_knogle_auth_response(const char *challenge) {
  if (challenge == nullptr || challenge[0] == '\\0') {
    return nullptr;
  }

  for (const auto &entry : kSampKnogleAuthTable) {
    if (entry.send != nullptr && std::strcmp(entry.send, challenge) == 0) {
      return entry.recv;
    }
  }

  return nullptr;
}
")

  list(APPEND SAMPDLL_NET_SOURCES
    src/net/raknet_bridge.cpp
    src/net/raknet_client_adapter.cpp
    "${SAMPDLL_RAKNET_AUTH_SOURCE}"
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
    src/archive/win32_archive_fs.c
    src/actor_facing_compat.c
    src/runtime_bridge.c
    src/runtime_hook_bridge.c
    src/samp.rc
  )

  target_include_directories(samp
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

  target_link_libraries(samp PRIVATE sampdll_archive sampdll_net advapi32 gdi32)

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

    # Match the reference PE profile where possible: fixed image base, no exports, GUI subsystem.
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
      -static
      -static-libgcc
      -static-libstdc++
    )
    # RakNet/C++ thread usage can pull winpthread dynamically; force static link to avoid runtime DLL dependency.
    target_link_libraries(samp PRIVATE -Wl,-Bstatic -lwinpthread)
  endif()
endif()

if(SAMPDLL_BUILD_TESTS)
  include(CTest)
  enable_testing()

  add_executable(test_actor_facing_compat
    tests/test_actor_facing_compat.c
    src/actor_facing_compat.c
  )
  target_include_directories(test_actor_facing_compat
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  add_test(NAME actor_facing_compat COMMAND test_actor_facing_compat)

  add_executable(test_saa
    tests/test_saa.c
  )
  target_link_libraries(test_saa PRIVATE sampdll_archive)

  if(EXISTS "${SAMPDLL_REFERENCE_SAA}")
    add_test(NAME saa COMMAND test_saa "${SAMPDLL_REFERENCE_SAA}")
  else()
    message(WARNING "Reference samp.saa absent; saa archive test is not registered")
  endif()

  add_executable(test_death_message_codec
    tests/test_death_message_codec.cpp
  )

  add_test(NAME death_message_codec COMMAND test_death_message_codec)

  add_executable(test_dual_stack
    tests/test_dual_stack.c
  )
  target_include_directories(test_dual_stack
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  target_link_libraries(test_dual_stack PRIVATE sampdll_net)

  add_test(NAME dual_stack COMMAND test_dual_stack)

  add_executable(test_ipv4_socket_compat
    tests/test_ipv4_socket_compat.c
  )
  target_include_directories(test_ipv4_socket_compat
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  target_link_libraries(test_ipv4_socket_compat PRIVATE sampdll_net)

  add_test(NAME ipv4_socket_compat COMMAND test_ipv4_socket_compat)

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

    add_executable(test_raknet_split_reassembly
      tests/test_raknet_split_reassembly.cpp
    )
    target_link_libraries(test_raknet_split_reassembly PRIVATE sampdll_net)

    add_test(NAME raknet_split_reassembly COMMAND test_raknet_split_reassembly)

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
