cmake_minimum_required(VERSION 3.14)
project(blocking_queue)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(blocking_queue_test
  BlockingQueue.hpp
  tests.cpp
)

target_link_libraries(
  blocking_queue_test
  GTest::gmock_main
)

include(GoogleTest)
gtest_discover_tests(blocking_queue_test)
