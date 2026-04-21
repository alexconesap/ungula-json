cmake_minimum_required(VERSION 3.16)
project(ungula_json_tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FETCHCONTENT_BASE_DIR ${CMAKE_CURRENT_LIST_DIR}/vendor)

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

set(LIB_JSON_SRC      ${CMAKE_CURRENT_LIST_DIR}/../src)
set(LIB_GENERIC_SRC   ${CMAKE_CURRENT_LIST_DIR}/../../lib/src)

include_directories(
    ${LIB_JSON_SRC}
    ${LIB_GENERIC_SRC}
    ${CMAKE_CURRENT_LIST_DIR}/ArduinoStub
    ${CMAKE_CURRENT_LIST_DIR}
)

add_executable(test_json
    test_json.cpp
    ${LIB_JSON_SRC}/json/json.cpp
    ${LIB_JSON_SRC}/json/json_utils.cpp
)
target_link_libraries(test_json gtest_main)

enable_testing()
include(GoogleTest)
gtest_discover_tests(test_json)
