cmake_minimum_required(VERSION 3.20)
project(lob_forex_service VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ═══════════════════════════════════════════════════════════════
# ForexConnect SDK
# ═══════════════════════════════════════════════════════════════

set(FOREXCONNECT_SDK "C:/Candleworks/ForexConnectAPIx64")
set(FOREXCONNECT_INCLUDE "${FOREXCONNECT_SDK}/include")
set(FOREXCONNECT_LIB "${FOREXCONNECT_SDK}/lib")

# ═══════════════════════════════════════════════════════════════
# Source files
# ═══════════════════════════════════════════════════════════════

set(SOURCES
    src/main.cpp
    src/fxcm_session.cpp
    src/http_server.cpp
    src/product_catalog.cpp
)

set(HEADERS
    src/fxcm_session.h
    src/http_server.h
    src/product_catalog.h
    src/logger.h
)

# ═══════════════════════════════════════════════════════════════
# Target
# ═══════════════════════════════════════════════════════════════

add_executable(lob_forex_service ${SOURCES} ${HEADERS})

target_include_directories(lob_forex_service PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${FOREXCONNECT_INCLUDE}
)

# Link ForexConnect
target_link_directories(lob_forex_service PRIVATE ${FOREXCONNECT_LIB})
target_link_libraries(lob_forex_service PRIVATE ForexConnect)

# Windows-specific (Winsock2)
if(WIN32)
    target_link_libraries(lob_forex_service PRIVATE ws2_32)
    target_compile_definitions(lob_forex_service PRIVATE
        _WIN32_WINNT=0x0601
        NOMINMAX
    )
endif()

# ═══════════════════════════════════════════════════════════════
# Post-build: Copy ForexConnect DLLs
# ═══════════════════════════════════════════════════════════════

add_custom_command(TARGET lob_forex_service POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${FOREXCONNECT_SDK}/bin"
    "$<TARGET_FILE_DIR:lob_forex_service>"
)

# ═══════════════════════════════════════════════════════════════
# Summary
# ═══════════════════════════════════════════════════════════════

message(STATUS "=== LOB FOREX SERVICE v0.1.0 ===")
message(STATUS "ForexConnect SDK: ${FOREXCONNECT_SDK}")
message(STATUS "Purpose: Resell FXCM products to Mongolian market")
