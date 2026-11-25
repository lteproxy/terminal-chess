cmake_minimum_required(VERSION 3.20)
project(terminal_chess VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2)

add_library(chess_rules
    src/game.cpp
    src/commands.cpp
)
target_include_directories(chess_rules PUBLIC include third_party/chess-library)
target_compile_options(chess_rules PRIVATE -Wall -Wextra -Wpedantic)

add_executable(terminal-chess
    src/main.cpp
    src/gui.cpp
)
target_link_libraries(terminal-chess PRIVATE chess_rules SDL2::SDL2)
target_compile_options(terminal-chess PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
if(BUILD_TESTING)
    add_executable(chess-tests tests/game_tests.cpp)
    target_link_libraries(chess-tests PRIVATE chess_rules)
    target_compile_options(chess-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME chess-tests COMMAND chess-tests)
endif()

install(TARGETS terminal-chess RUNTIME DESTINATION bin)
