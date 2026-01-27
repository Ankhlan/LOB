cmake_minimum_required(VERSION 3.14)
project(CentralExchange VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(httplib)

# SQLite3 for order persistence
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
    # On Ubuntu, install libsqlite3-dev
    message(STATUS "SQLite3 not found via find_package, will use system package")
endif()

add_executable(central_exchange src/main.cpp)

target_include_directories(central_exchange PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${httplib_SOURCE_DIR}
)

target_link_libraries(central_exchange PRIVATE
    nlohmann_json::nlohmann_json
    sqlite3
)

if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(central_exchange PRIVATE Threads::Threads)
endif()

find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    target_link_libraries(central_exchange PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(central_exchange PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
endif()
