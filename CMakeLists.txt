cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTS "Build tests" OFF)
#option(BUILD_DOCS "Build documentation" OFF)
#option(BUILD_COVERAGE "Build code coverage" OFF)

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
hunter_add_package(CURL)

find_package(CURL CONFIG REQUIRED)
find_package(gumbo CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(Threads)
find_package(Boost COMPONENTS program_options REQUIRED)

add_library(${PROJECT_NAME} STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/sources/crawler.cpp
)

add_executable(crawler
        ${CMAKE_CURRENT_SOURCE_DIR}/main/main.cpp
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
          ${CMAKE_CURRENT_SOURCE_DIR}/tests/tests.cpp
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
