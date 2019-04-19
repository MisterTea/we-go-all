cmake_minimum_required (VERSION 3.0.2)
project (WeGoAll VERSION 0.0.1)

option(BUILD_TEST "Build all unit test" ON)
option(CODE_COVERAGE "Enable code coverage" OFF)

# Enable C++-11
if(UNIX)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")
endif()

# Enable debug info
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

IF(APPLE)
SET(OPENSSL_ROOT_DIR "/usr/local/opt/openssl")

# Turn off address randomizing to get debug prints
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-pie")

ENDIF()

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWGA_VERSION='\"${PROJECT_VERSION}\"'")

include(CheckIncludeFileCXX)
SET(CMAKE_REQUIRED_INCLUDES "${CMAKE_SOURCE_DIR}/external/asio/asio/include")
SET(CMAKE_REQUIRED_DEFINITIONS "-DASIO_STANDALONE -D_WIN32_WINNT=_WIN32_WINNT_WIN7")
SET(USE_STANDALONE_ASIO ON CACHE BOOL "set ON to use standalone Asio instead of Boost.Asio")

IF(BUILD_TEST)
  add_subdirectory(${CMAKE_SOURCE_DIR}/external/googletest)
ENDIF(BUILD_TEST)
IF(CODE_COVERAGE)
  if(UNIX)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
  endif()
ENDIF(CODE_COVERAGE)

#Using FreeBSD?
if (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    set(FREEBSD TRUE)
endif (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")

#Using NetBSD?
if (CMAKE_SYSTEM_NAME MATCHES "NetBSD")
  set(NETBSD TRUE)
endif (CMAKE_SYSTEM_NAME MATCHES "NetBSD")

# Add cmake script directory.
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Required packages
find_package(Threads REQUIRED)
find_package(Sodium REQUIRED)

IF(MINGW)
set(CORE_LIBRARIES Ws2_32)
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
  external/cxxopts/include
  external/Optional
  external/asio/asio/include
  external/msgpack-c/include
  external/Catch2/single_include
  external/SimpleWebServer
  ${sodium_INCLUDE_DIR}
  )

add_library(
  wga-lib
  STATIC

  src/base/LocalIpFetcher.cpp
  src/base/BiDirectionalRpc.cpp
  src/base/UdpBiDirectionalRpc.cpp
  src/base/CryptoHandler.cpp
  src/base/LogHandler.cpp
  src/base/MultiEndpointHandler.cpp
  src/base/EncryptedMultiEndpointHandler.cpp
  src/base/PortMultiplexer.cpp
  src/base/TimeHandler.cpp
  src/base/EasyLoggingWrapper.cpp

  src/peer/MyPeer.cpp
  )

add_executable (
  examplepeer

  src/example/ExamplePeer.cpp
  )

target_link_libraries (
  examplepeer
  LINK_PUBLIC
  wga-lib
  ${sodium_LIBRARY_RELEASE}
  ${CMAKE_THREAD_LIBS_INIT}
  ${CORE_LIBRARIES}
  )

add_executable (
  examplelobby

  src/example/ExampleLobby.cpp
  )

target_link_libraries (
  examplelobby
  LINK_PUBLIC
  wga-lib
  ${sodium_LIBRARY_RELEASE}
  ${CMAKE_THREAD_LIBS_INIT}
  ${CORE_LIBRARIES}
  )
  
if(BUILD_TEST) # Build unit tests.
  message(STATUS "Enabling test for ${PROJECT_NAME}")
  enable_testing()

  set(GTEST_PATH ${PROJECT_SOURCE_DIR}/test)

  add_executable(
    wga-test
  
    test/Main.cpp

    test/ChronoMapTest.cpp
    test/FlakyRpcTest.cpp
    test/PeerTest.cpp
    )
  
  add_dependencies(
    wga-test
    wga-lib
    gtest
    gtest_main
    gmock
    gmock_main
    )
  
  target_link_libraries(
    wga-test
    wga-lib
    ${sodium_LIBRARY_RELEASE}
    ${CMAKE_THREAD_LIBS_INIT}
    ${CORE_LIBRARIES}
    gtest
    gtest_main
    gmock
    gmock_main
    )
  add_test(
    wga-test
    wga-test
    )
  
endif()
