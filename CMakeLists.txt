cmake_minimum_required(VERSION 3.14)
project(CentralExchange VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Development mode - enables dev_otp in auth responses
option(DEV_MODE "Enable development features (dev_otp, etc)" ON)
if(DEV_MODE)
    add_compile_definitions(DEV_MODE)
    message(STATUS "DEV_MODE enabled - dev_otp will be returned in auth responses")
endif()

# Native GUI client option
option(BUILD_GUI_CLIENT "Build native Dear ImGui trading client" OFF)

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

# ForexConnect SDK
option(FXCM_ENABLED "Enable FXCM ForexConnect integration" ON)
if(FXCM_ENABLED)
    if(WIN32)
        set(FXCM_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/fxcm")
    else()
        # Linux SDK
        set(FXCM_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/fxcm-linux/ForexConnectAPI-1.6.5-Linux-x86_64")
    endif()
    
    if(EXISTS "${FXCM_SDK_DIR}/include/ForexConnect.h")
        message(STATUS "ForexConnect SDK found at ${FXCM_SDK_DIR}")
        target_include_directories(central_exchange PRIVATE 
            "${FXCM_SDK_DIR}/include"
        )
        target_link_directories(central_exchange PRIVATE 
            "${FXCM_SDK_DIR}/lib"
        )
        if(WIN32)
            target_link_libraries(central_exchange PRIVATE ForexConnect)
        else()
            # Linux needs pthread and dl
            target_link_libraries(central_exchange PRIVATE
                ForexConnect
                pricehistorymgr
                quotesmgr2
                pthread
                dl
            )
            # Set rpath for runtime library loading
            set_target_properties(central_exchange PROPERTIES
                BUILD_RPATH "${FXCM_SDK_DIR}/lib"
                INSTALL_RPATH "${FXCM_SDK_DIR}/lib"
            )
        endif()
        target_compile_definitions(central_exchange PRIVATE FXCM_ENABLED)
    else()
        message(WARNING "FXCM_ENABLED=ON but ForexConnect SDK not found at ${FXCM_SDK_DIR}")
    endif()
endif()

# =============================================================================
# NATIVE GUI CLIENT (Dear ImGui + GLFW + OpenGL)
# =============================================================================
if(BUILD_GUI_CLIENT)
    message(STATUS "Building native GUI client (cre_client)")
    
    # Fetch Dear ImGui
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.90.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(imgui)
    
    # Fetch GLFW
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.9
        GIT_SHALLOW TRUE
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
    
    # Find OpenGL
    find_package(OpenGL REQUIRED)
    
    # ImGui sources (core + backends)
    set(IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    
    # GUI client executable
    add_executable(cre_client
        src/client_main.cpp
        ${IMGUI_SOURCES}
    )
    
    target_include_directories(cre_client PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    
    target_link_libraries(cre_client PRIVATE
        glfw
        OpenGL::GL
        nlohmann_json::nlohmann_json
    )
    
    if(WIN32)
        target_link_libraries(cre_client PRIVATE opengl32)
    endif()
    
    if(UNIX AND NOT APPLE)
        target_link_libraries(cre_client PRIVATE dl)
    endif()
    
    message(STATUS "GUI client will be built: cre_client")
endif()

# =============================================================================
# TESTS (GoogleTest)
# =============================================================================
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
