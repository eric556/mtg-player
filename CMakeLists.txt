cmake_minimum_required(VERSION 3.28)
project(mtg-sim LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)
FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.1.0
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(SFML)

add_executable(mtg-sim
    src/main.cpp
    src/card.cpp
    src/zone.cpp
    src/game_state.cpp
    src/hand_window.cpp
    src/playmat_window.cpp
)

target_compile_features(mtg-sim PRIVATE cxx_std_17)
target_link_libraries(mtg-sim PRIVATE SFML::Graphics)
target_include_directories(mtg-sim PRIVATE src)

# Make mtg-sim the default startup project in Visual Studio.
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT mtg-sim)
