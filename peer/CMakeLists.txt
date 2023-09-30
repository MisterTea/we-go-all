cmake_minimum_required(VERSION 3.15.0)

# Enable vcpkg
set(CMAKE_TOOLCHAIN_FILE
  "${CMAKE_SOURCE_DIR}/external/vcpkg/scripts/buildsystems/vcpkg.cmake")

# Set the triplet
if(WIN32)
  set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "VCPKG Triplet")
elseif(APPLE)
  execute_process(
    COMMAND uname -m
    COMMAND tr -d '\n'
    OUTPUT_VARIABLE VCPKG_ARCHITECTURE)
  message(STATUS "Architecture: ${VCPKG_ARCHITECTURE}")

  if(${VCPKG_ARCHITECTURE} STREQUAL "arm64") # Apple Silicon
    set(VCPKG_TARGET_TRIPLET "arm64-osx" CACHE STRING "VCPKG Triplet")
  else()
    set(VCPKG_TARGET_TRIPLET "x64-osx" CACHE STRING "VCPKG Triplet")
  endif()
else()
  execute_process(
    COMMAND uname -m
    COMMAND tr -d '\n'
    OUTPUT_VARIABLE VCPKG_ARCHITECTURE)
  message(STATUS "Architecture: ${VCPKG_ARCHITECTURE}")

  if(${VCPKG_ARCHITECTURE} STREQUAL "x86_64")
    set(VCPKG_TARGET_TRIPLET "x64-linux" CACHE STRING "VCPKG Triplet")
  elseif(${VCPKG_ARCHITECTURE} STREQUAL "x86_86")
    set(VCPKG_TARGET_TRIPLET "x86-linux" CACHE STRING "VCPKG Triplet")
  elseif(${VCPKG_ARCHITECTURE} STREQUAL "aarch64")
    set(VCPKG_TARGET_TRIPLET "arm64-linux" CACHE STRING "VCPKG Triplet")
  elseif(${VCPKG_ARCHITECTURE} STREQUAL "arm64")
    set(VCPKG_TARGET_TRIPLET "arm64-linux" CACHE STRING "VCPKG Triplet")
  elseif(${VCPKG_ARCHITECTURE} MATCHES "arm.*")
    set(VCPKG_TARGET_TRIPLET "arm-linux" CACHE STRING "VCPKG Triplet")
  else()
    set(VCPKG_TARGET_TRIPLET "${VCPKG_ARCHITECTURE}-linux" CACHE STRING "VCPKG Triplet")
  endif()
endif()

# Some hacks to fix vcpkg
if(WIN32)
  set(ZLIB_LIBRARY_RELEASE
    "${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib/zlib.lib"
  )
  set(ZLIB_LIBRARY_DEBUG
    "${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/debug/lib/zlibd.lib"
  )
else()
  # Make BoostConfig.cmake able to check its inputs
  cmake_policy(SET CMP0057 NEW)

  # Honor the visibility properties for all target types, including
  # object libraries and static libraries.
  cmake_policy(SET CMP0063 NEW)

  # Don't ignore <PackageName>_ROOT variables
  cmake_policy(SET CMP0074 NEW)
endif()

project(WeGoAll VERSION 0.0.1)

SET(OPENSSL_USE_STATIC_LIBS TRUE)
SET(OPENSSL_ROOT_DIR "${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}")

# Enable C++-17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/external/sanitizers-cmake/cmake" ${CMAKE_MODULE_PATH})
FIND_PACKAGE(Sanitizers)

option(CODE_COVERAGE "Enable code coverage" OFF)

if(UNIX)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

  # Enable debug info
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
endif()

# Flags for miniupnpc
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMINIUPNPC_SET_SOCKET_TIMEOUT -DMINIUPNPC_GET_SRC_ADDR -DMINIUPNP_STATICLIB -DSTATICLIB -DSODIUM_STATIC -DASIO_STANDALONE")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DMINIUPNPC_SET_SOCKET_TIMEOUT -DMINIUPNPC_GET_SRC_ADDR -DMINIUPNP_STATICLIB -DSTATICLIB -DSODIUM_STATIC -DASIO_STANDALONE")

# Flags for libnat-pmp
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DENABLE_STRNATPMPERR")

IF(APPLE)
  SET(OPENSSL_ROOT_DIR "/usr/local/opt/openssl")

  # Turn off address randomizing to get debug prints
  SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-pie")
ENDIF()

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWGA_VERSION='\"${PROJECT_VERSION}\"'")

IF(CODE_COVERAGE)
  if(UNIX)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
  endif()
ENDIF(CODE_COVERAGE)

# Using FreeBSD?
if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
  set(FREEBSD TRUE)
endif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")

# Using NetBSD?
if(CMAKE_SYSTEM_NAME MATCHES "NetBSD")
  set(NETBSD TRUE)
endif(CMAKE_SYSTEM_NAME MATCHES "NetBSD")

# Add cmake script directory.
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Required packages
find_package(Threads REQUIRED)
find_package(sodium MODULE REQUIRED)
FIND_PACKAGE(OpenSSL MODULE REQUIRED)

IF(MINGW)
  set(CORE_LIBRARIES Ws2_32 iphlpapi)
ELSEIF(WIN32)
  set(
    CORE_LIBRARIES

    Ws2_32
    iphlpapi
    Shlwapi
    dbghelp)
ELSEIF(FREEBSD)
  set(CORE_LIBRARIES util execinfo)
ELSEIF(NETBSD)
  set(CORE_LIBRARIES util resolv execinfo)
ELSE()
  set(CORE_LIBRARIES util resolv)
ENDIF()

include_directories(
  src/base
  src/peer
  external
  external/asio/asio/include
  external/json/include
  external/msgpack-c/include
  external/Eigen
  ${sodium_INCLUDE_DIR}
)

add_library(
  upnp
  STATIC

  external/libnatpmp-20150609/declspec.h
  external/libnatpmp-20150609/getgateway.c
  external/libnatpmp-20150609/getgateway.h
  external/libnatpmp-20150609/natpmp.c
  external/libnatpmp-20150609/natpmp.h
  external/libnatpmp-20150609/wingettimeofday.c
  external/libnatpmp-20150609/wingettimeofday.h

  external/miniupnpc-2.1.20190408/miniwget_private.h
  external/miniupnpc-2.1.20190408/miniupnpc_declspec.h
  external/miniupnpc-2.1.20190408/upnpcommands.h
  external/miniupnpc-2.1.20190408/minixml.h
  external/miniupnpc-2.1.20190408/miniupnpc_socketdef.h
  external/miniupnpc-2.1.20190408/upnperrors.h
  external/miniupnpc-2.1.20190408/igd_desc_parse.h
  external/miniupnpc-2.1.20190408/minissdpc.h
  external/miniupnpc-2.1.20190408/miniupnpc.h
  external/miniupnpc-2.1.20190408/miniupnpctypes.h
  external/miniupnpc-2.1.20190408/minisoap.h
  external/miniupnpc-2.1.20190408/portlistingparse.h
  external/miniupnpc-2.1.20190408/connecthostport.h
  external/miniupnpc-2.1.20190408/miniwget.h
  external/miniupnpc-2.1.20190408/codelength.h
  external/miniupnpc-2.1.20190408/receivedata.h
  external/miniupnpc-2.1.20190408/upnpreplyparse.h
  external/miniupnpc-2.1.20190408/upnpdev.h
  external/miniupnpc-2.1.20190408/minisoap.c
  external/miniupnpc-2.1.20190408/portlistingparse.c
  external/miniupnpc-2.1.20190408/connecthostport.c
  external/miniupnpc-2.1.20190408/upnpreplyparse.c
  external/miniupnpc-2.1.20190408/receivedata.c
  external/miniupnpc-2.1.20190408/miniwget.c
  external/miniupnpc-2.1.20190408/upnpdev.c
  external/miniupnpc-2.1.20190408/upnpcommands.c
  external/miniupnpc-2.1.20190408/upnperrors.c
  external/miniupnpc-2.1.20190408/minixml.c
  external/miniupnpc-2.1.20190408/minissdpc.c
  external/miniupnpc-2.1.20190408/miniupnpc.c
  external/miniupnpc-2.1.20190408/igd_desc_parse.c
)

# SingleGameServer does some non-thread-safe actions, so don't run it with TSAN
add_library(
  wga-singlegameserver
  STATIC

  src/peer/SingleGameServer.cpp
)

add_library(
  wga-lib
  STATIC

  src/base/ClockSynchronizer.hpp
  src/base/ClockSynchronizer.cpp

  src/base/PidController.hpp
  src/base/PidController.cpp

  src/base/LocalIpFetcher.hpp
  src/base/LocalIpFetcher.cpp

  src/base/BiDirectionalRpc.hpp
  src/base/BiDirectionalRpc.cpp

  src/base/UdpBiDirectionalRpc.hpp
  src/base/UdpBiDirectionalRpc.cpp

  src/base/CryptoHandler.hpp
  src/base/CryptoHandler.cpp

  src/base/LogHandler.hpp
  src/base/LogHandler.cpp

  src/base/MultiEndpointHandler.hpp
  src/base/MultiEndpointHandler.cpp

  src/base/EncryptedMultiEndpointHandler.hpp
  src/base/EncryptedMultiEndpointHandler.cpp

  src/base/PortMultiplexer.hpp
  src/base/PortMultiplexer.cpp

  src/base/PortMappingHandler.hpp
  src/base/PortMappingHandler.cpp

  src/base/RpcServer.hpp
  src/base/RpcServer.cpp

  src/base/TimeHandler.hpp
  src/base/TimeHandler.cpp

  src/base/StunClient.hpp
  src/base/StunClient.cpp

  src/base/EasyLoggingWrapper.cpp

  src/peer/MyPeer.cpp
  src/peer/SingleGameServer.cpp
)
add_sanitizers(wga-lib)

add_executable(
  examplepeer

  src/example/ExamplePeer.cpp
)
target_link_libraries(
  examplepeer
  LINK_PUBLIC
  wga-lib
  upnp
  ${sodium_LIBRARY_RELEASE}
  OpenSSL::SSL
  ${CMAKE_THREAD_LIBS_INIT}
  ${CORE_LIBRARIES}
)
add_sanitizers(examplepeer)

add_executable(
  examplelobby

  src/example/ExampleLobby.cpp
)
target_link_libraries(
  examplelobby
  LINK_PUBLIC
  wga-lib
  upnp
  ${sodium_LIBRARY_RELEASE}
  OpenSSL::SSL
  ${CMAKE_THREAD_LIBS_INIT}
  ${CORE_LIBRARIES}
)
add_sanitizers(examplelobby)

enable_testing()

add_executable(
  wga-test

  test/Main.cpp

  test/ChronoMapTest.cpp
  test/ClockSynchronizerTest.cpp
  test/FlakyRpcTest.cpp
  test/PeerTest.cpp
  test/StunTest.cpp
)
add_dependencies(
  wga-test
  wga-lib
  upnp
)
target_link_libraries(
  wga-test
  wga-lib
  upnp
  ${sodium_LIBRARY_RELEASE}
  OpenSSL::SSL
  ${CMAKE_THREAD_LIBS_INIT}
  ${CORE_LIBRARIES}
)
add_test(
  wga-test
  wga-test
)
add_sanitizers(wga-test)
