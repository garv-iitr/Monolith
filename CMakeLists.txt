cmake_minimum_required(VERSION 3.16)
project(risc201 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(RISC201_WORD_BITS 32 CACHE STRING "RISC201 data word width in bits (32 or 64)")
set_property(CACHE RISC201_WORD_BITS PROPERTY STRINGS 32 64)

option(RISC201_TUI "Build the FTXUI terminal-UI debugger (fetches FTXUI)" ON)

add_library(risc201_config INTERFACE)
target_include_directories(risc201_config INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}
  "${CMAKE_CURRENT_SOURCE_DIR}/control unit")
target_compile_definitions(risc201_config INTERFACE
  RISC201_WORD_BITS=${RISC201_WORD_BITS})

add_library(risc201_cu STATIC
  "control unit/microcode_routines.cpp")
target_link_libraries(risc201_cu PUBLIC risc201_config)
target_compile_options(risc201_cu PRIVATE -Wall -Wextra)

add_executable(risc201-cu-demo "control unit/main.cpp")
target_link_libraries(risc201-cu-demo PRIVATE risc201_cu)
target_compile_options(risc201-cu-demo PRIVATE -Wall -Wextra)

add_library(risc201_core STATIC
  simulator_core/core/memory.cpp
  simulator_core/core/register_file.cpp
  simulator_core/core/decoder.cpp)
target_link_libraries(risc201_core PUBLIC risc201_config)
target_compile_options(risc201_core PRIVATE -Wall -Wextra)

add_executable(risc201-sim simulator_core/main.cpp)
target_link_libraries(risc201-sim PRIVATE risc201_core risc201_cu)
target_compile_options(risc201-sim PRIVATE -Wall -Wextra)

if(RISC201_TUI AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/simulator_core/ui/tui.cpp")
  include(FetchContent)
  FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v5.0.0)
  set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(ftxui)
  target_link_libraries(risc201-sim PRIVATE
    ftxui::screen ftxui::dom ftxui::component)
  target_compile_definitions(risc201-sim PRIVATE RISC201_HAS_TUI=1)
endif()

add_executable(risc201-tests
  tests/test_main.cpp
  tests/test_isa.cpp
  tests/test_memory.cpp
  tests/test_register_file.cpp
  tests/test_decoder.cpp)
target_link_libraries(risc201-tests PRIVATE risc201_core)
target_compile_options(risc201-tests PRIVATE -Wall -Wextra)

enable_testing()
add_test(NAME risc201-tests COMMAND risc201-tests)
