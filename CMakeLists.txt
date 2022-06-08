cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTS "Build tests" OFF)
option(BUILD_DOCS "Build documentation" OFF)
option(BUILD_COVERAGE "Build code coverage" OFF)

include("cmake/HunterGate.cmake")

HunterGate(
        URL "https://github.com/cpp-pm/hunter/archive/v0.23.297.tar.gz"
        SHA1 "3319fe6a3b08090df7df98dee75134d68e2ef5a3"
)

project(CrawlerProject)
string(APPEND CMAKE_CXX_FLAGS " -pedantic -Wall -Wextra")
string(APPEND CMAKE_CXX_FLAGS " -Wno-unused-command-line-argument")
string(APPEND CMAKE_CXX_FLAGS " -Wshadow -Wnon-virtual-dtor")

hunter_add_package(GTest)
#hunter_add_package(Boost COMPONENTS program_options)
hunter_add_package(gumbo)
#hunter_add_package(OpenSSL)
#hunter_add_package(CURL)

find_package(CURL CONFIG REQUIRED)
#find_package(OpenSSL REQUIRED)
find_package(gumbo CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(Threads)
find_package(Boost COMPONENTS program_options REQUIRED)

add_library(${PROJECT_NAME} STATIC
  # enum your files and delete this comment
  #${CMAKE_CURRENT_SOURCE_DIR}/sources/example.cpp
  # ${CMAKE_CURRENT_SOURCE_DIR}/include/crawler.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/sources/crawler.cpp
)

add_executable(crawler
  ${CMAKE_CURRENT_SOURCE_DIR}/demo/main.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>"
  "${BOOST_ROOT}/include"
  "${GUMBO_ROOT}/include"
  "${CURL_ROOT}/include"
)

target_link_libraries(crawler ${PROJECT_NAME}
        Threads::Threads
        Boost::program_options
        gumbo::gumbo
        CURL::libcurl
)

if(BUILD_TESTS)
  add_executable(tests
    # TODO: enum your files and delete this comment
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/example.cpp
  )
  target_link_libraries(tests ${PROJECT_NAME}
          GTest::gtest_main
          Boost::program_options
          Threads::Threads
          gumbo::gumbo
          CURL::libcurl
  )
  enable_testing()
  add_test(NAME unit_tests COMMAND tests)
endif()

if(BUILD_COVERAGE)
  set(ENABLE_COVERAGE ON CACHE BOOL "Enable coverage build." FORCE)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tools/coverage/cmake")
  find_package(codecov)
  string(APPEND CMAKE_CXX_FLAGS " --coverage")
  add_coverage(${PROJECT_NAME})
  add_coverage(tests)
  coverage_evaluate()
endif()
