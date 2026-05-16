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
option(ENABLE_VCAM "Enable virtual camera output (requires vcam driver installed)" OFF)

if(ENABLE_VCAM)
    FetchContent_Declare(vcam
        GIT_REPOSITORY https://github.com/mycrl/vcam.git
        GIT_TAG main
        GIT_SHALLOW ON)
endif()
FetchContent_MakeAvailable(SFML cpr json)
if(ENABLE_VCAM)
    FetchContent_MakeAvailable(vcam)
endif()

add_executable(mtg-sim
    src/main.cpp
    src/card.cpp
    src/ui.cpp
    src/pile_viewer.cpp
    src/game_state.cpp
    src/game_action.cpp
    src/hand_window.cpp
    src/playmat_window.cpp
    src/playmat_menus.cpp
    src/playmat_draw.cpp
    src/playmat_tick.cpp
    src/card_requester.cpp
)

target_compile_features(mtg-sim PRIVATE cxx_std_20)
target_link_libraries(mtg-sim PRIVATE
    SFML::Graphics
    cpr::cpr
    nlohmann_json::nlohmann_json
)
target_include_directories(mtg-sim PRIVATE src)

if(ENABLE_VCAM)
    target_compile_definitions(mtg-sim PRIVATE ENABLE_VCAM)
    target_link_libraries(mtg-sim PRIVATE vcam-driver)
    target_include_directories(mtg-sim PRIVATE "${vcam_SOURCE_DIR}/driver" "${vcam_SOURCE_DIR}/service")
endif()

# Make mtg-sim the default startup project in Visual Studio.
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT mtg-sim)
