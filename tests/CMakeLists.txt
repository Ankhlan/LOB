cmake_minimum_required(VERSION 3.14)

# GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# OrderBook unit tests (no heavy dependencies)
add_executable(test_order_book test_order_book.cpp)
target_include_directories(test_order_book PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_order_book PRIVATE
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(test_order_book)
