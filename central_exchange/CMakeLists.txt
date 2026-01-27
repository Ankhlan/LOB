cmake_minimum_required(VERSION 3.16)
project(CentralExchange VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# === BUILD OPTIONS ===
option(BUILD_WITH_FXCM "Build with ForexConnect SDK" ON)
option(BUILD_TESTS "Build unit tests" OFF)

# === FOREXCONNECT SDK (from cortex_daemon) ===
if(BUILD_WITH_FXCM)
    set(FOREXCONNECT_SDK "C:/Program Files/Candleworks/ForexConnectAPI")
    
    if(EXISTS "${FOREXCONNECT_SDK}")
        message(STATUS "ForexConnect SDK found at: ${FOREXCONNECT_SDK}")
        add_definitions(-DFXCM_ENABLED)
        include_directories("${FOREXCONNECT_SDK}/include")
        link_directories("${FOREXCONNECT_SDK}/lib")
    else()
        message(WARNING "ForexConnect SDK not found - building without FXCM")
        set(BUILD_WITH_FXCM OFF)
    endif()
endif()

# === DEPENDENCIES ===
# JSON for REST API
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

# httplib for embedded HTTP server
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.3
)
FetchContent_MakeAvailable(httplib)

# === SOURCE FILES ===
set(EXCHANGE_SOURCES
    src/main.cpp
    src/exchange.cpp
    src/matching_engine.cpp
    src/order_book.cpp
    src/product_catalog.cpp
    src/position_manager.cpp
    src/hedging_engine.cpp
    src/fxcm_feed.cpp
    src/http_server.cpp
    src/websocket_server.cpp
)

# === MAIN EXECUTABLE ===
add_executable(central_exchange ${EXCHANGE_SOURCES})

target_include_directories(central_exchange PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(central_exchange PRIVATE
    nlohmann_json::nlohmann_json
    httplib::httplib
)

if(BUILD_WITH_FXCM)
    target_link_libraries(central_exchange PRIVATE
        ForexConnect
        fxmsg
    )
endif()

# Windows-specific
if(WIN32)
    target_link_libraries(central_exchange PRIVATE
        ws2_32
        winhttp
    )
endif()

# === INSTALL ===
install(TARGETS central_exchange
    RUNTIME DESTINATION bin
)

# Copy ForexConnect DLLs for Windows
if(WIN32 AND BUILD_WITH_FXCM)
    file(GLOB FXCM_DLLS "${FOREXCONNECT_SDK}/bin/*.dll")
    install(FILES ${FXCM_DLLS} DESTINATION bin)
endif()
