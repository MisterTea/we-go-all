cmake_minimum_required (VERSION 3.0.2)
project (WeGoAll VERSION 0.0.1)

option(BUILD_TEST "Build all unit test" OFF)
option(CODE_COVERAGE "Enable code coverage" OFF)


SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWGA_VERSION='\"${CMAKE_PROJECT_VERSION}\"'")
# For easylogging, disable default log file, enable crash log, ensure thread safe, and catch c++ exceptions
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DELPP_NO_DEFAULT_LOG_FILE -DELPP_FEATURE_CRASH_LOG -DELPP_THREAD_SAFE -DELPP_HANDLE_SIGABRT")
# Enable standalone asio
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DASIO_STANDALONE")

IF(BUILD_TEST)
  add_subdirectory(${CMAKE_SOURCE_DIR}/external/googletest)
ENDIF(BUILD_TEST)
IF(CODE_COVERAGE)
  if(UNIX)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
  endif()
ENDIF(CODE_COVERAGE)

# Enable C++-11
if(UNIX)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")
endif()

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
find_package(GFlags REQUIRED)

IF(FREEBSD)
  set(CORE_LIBRARIES util execinfo)
ELSEIF(NETBSD)
  set(CORE_LIBRARIES util resolv execinfo)
ELSE()
  set(CORE_LIBRARIES util resolv)
ENDIF()

include_directories(
  src/base
  src/external
  src/external/asio/asio/include
  ${GFLAGS_INCLUDE_DIRS}
  )

add_library(
  wga-lib
  STATIC

  src/base/BiDirectionalRpc.cpp
  src/base/LogHandler.cpp
  src/external/easylogging++.cc
  )

add_executable (
  peer

  src/peer/Main.cpp
  )

target_link_libraries (
  peer
  LINK_PUBLIC
  wga-lib
  ${CMAKE_THREAD_LIBS_INIT}
  ${GFLAGS_LIBRARIES}
  ${CORE_LIBRARIES}
  )

if(BUILD_TEST) # Build unit tests.
  message(STATUS "Enabling test for ${PROJECT_NAME}")
  enable_testing()

  set(GTEST_PATH ${PROJECT_SOURCE_DIR}/test)

  file(GLOB TEST_SRCS test/*Test.cpp )
  add_executable(
    wga-test
  
    ${TEST_SRCS}
    test/Main.cpp
    )
  
  add_dependencies(
    wga-test
    wga-lib
    gtest
    gtest_main
    gmock
    gmock_main)
  
  target_link_libraries(
    wga-test
    wga-lib
    ${CMAKE_THREAD_LIBS_INIT}
    ${GFLAGS_LIBRARIES}
    ${CORE_LIBRARIES}
    gtest
    gtest_main
    gmock
    gmock_main
    resolv
    util
    )
  add_test(
    wga-test
    wga-test
    )
  
endif()
