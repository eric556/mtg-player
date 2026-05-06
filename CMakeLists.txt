cmake_minimum_required(VERSION 3.28)
project(mtg-sim LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Configure dependencies for static linking
set(SFML_STATIC_LIBRARIES ON CACHE BOOL "" FORCE)
set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CPR_USE_SYSTEM_CURL OFF CACHE BOOL "" FORCE)
set(CPR_ENABLE_SSL ON CACHE BOOL "" FORCE)

if(WIN32)
    # Use Windows' native Schannel for HTTPS instead of requiring OpenSSL
    set(SELECT_CRYPTO_BACKEND "Schannel" CACHE STRING "" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.1.0
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_Declare(cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG 1.11.0
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL)
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(SFML cpr json)

add_executable(mtg-sim
    src/main.cpp
    src/card.cpp
    src/ui.cpp
    src/game_state.cpp
    src/hand_window.cpp
    src/playmat_window.cpp
    src/card_requester.cpp
    src/vcam/vcam_ffmpeg.cpp
)

target_compile_features(mtg-sim PRIVATE cxx_std_20)
target_link_libraries(mtg-sim PRIVATE SFML::Graphics cpr::cpr nlohmann_json::nlohmann_json)
target_include_directories(mtg-sim PRIVATE src)

if(WIN32)
    target_sources(mtg-sim PRIVATE src/vcam/vcam_win32.cpp)
    target_link_libraries(mtg-sim PRIVATE mfsensorgroup mfplat mfuuid ole32 oleaut32)

    # COM DLL: the in-process media source loaded by the Windows Frame Server
    add_library(mtg-sim-vcam SHARED
        src/vcam/win32_source/dllmain.cpp
        src/vcam/win32_source/media_source.cpp
        src/vcam/win32_source/media_stream.cpp
        src/vcam/win32_source/vcam_source.def
    )
    target_compile_features(mtg-sim-vcam PRIVATE cxx_std_20)
    target_include_directories(mtg-sim-vcam PRIVATE src/vcam/win32_source src/vcam src)
    target_link_libraries(mtg-sim-vcam PRIVATE mfplat mfuuid ole32 oleaut32 shlwapi)
    # Place the DLL alongside the exe so the app can find it at runtime
    set_target_properties(mtg-sim-vcam PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()

# Make mtg-sim the default startup project in Visual Studio.
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT mtg-sim)
