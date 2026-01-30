# WASM Build Configuration for Central Exchange
# 
# Build: emcmake cmake -DWASM_BUILD=ON .. && emmake make
#
# Produces:
# - exchange_core.wasm
# - exchange_core.js

cmake_minimum_required(VERSION 3.16)

# When building for WASM
if(WASM_BUILD)
    message(STATUS "=== WASM BUILD MODE ===")
    
    # Disable networking dependencies
    set(FXCM_ENABLED OFF CACHE BOOL "" FORCE)
    set(QPAY_ENABLED OFF CACHE BOOL "" FORCE)
    set(DATABASE_ENABLED OFF CACHE BOOL "" FORCE)
    
    # WASM-specific compile flags
    add_compile_definitions(
        WASM_BUILD=1
        NO_NETWORKING=1
        NO_THREADING=1
    )
    
    # Emscripten flags
    set(CMAKE_CXX_FLAGS " -s WASM=1 -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap']")
    set(CMAKE_CXX_FLAGS " -s MODULARIZE=1 -s EXPORT_NAME='ExchangeCore'")
    set(CMAKE_CXX_FLAGS " -s ALLOW_MEMORY_GROWTH=1")
    set(CMAKE_CXX_FLAGS " -O3")  # Optimize for speed
    
    # WASM-compatible sources only
    set(WASM_SOURCES
        src/wasm_exports.cpp
    )
    
    # WASM-compatible headers (no networking)
    set(WASM_HEADERS
        src/order_book.h
        src/matching_engine.h
        src/position_manager.h
        src/product_catalog.h
        src/risk_engine.h
        src/interfaces.h
    )
    
    add_executable(exchange_core )
    target_include_directories(exchange_core PRIVATE src)
    
    # Output .wasm and .js to web folder
    set_target_properties(exchange_core PROPERTIES
        SUFFIX ".js"
        RUNTIME_OUTPUT_DIRECTORY "/src/web/wasm"
    )
    
endif()
