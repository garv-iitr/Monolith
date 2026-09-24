cmake_minimum_required(VERSION 3.16)
project(risc201 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Parameters (see isa_spec/isa.h once it lands)
# ---------------------------------------------------------------------------
set(RISC201_WORD_BITS 64 CACHE STRING "RISC201 data word width in bits (32 or 64)")
set_property(CACHE RISC201_WORD_BITS PROPERTY STRINGS 32 64)

option(RISC201_TUI "Build the FTXUI terminal-UI debugger (fetches FTXUI)" ON)

# ---------------------------------------------------------------------------
# Source lists. These grow one implementation step at a time.
# ---------------------------------------------------------------------------
set(RISC201_CORE_SOURCES
  # simulator_core/core/memory.cpp          (Step 2)
  # simulator_core/core/register_file.cpp   (Step 2)
  # simulator_core/core/decoder.cpp         (Step 3)
  # simulator_core/core/pipeline.cpp        (Step 7)
  # simulator_core/core/loader.cpp          (Step 8)
)

set(RISC201_UI_SOURCES
  # simulator_core/ui/headless.cpp          (Step 8)
  # simulator_core/ui/tui.cpp               (Step 9)
)

set(RISC201_TEST_SOURCES
  # tests/test_decoder.cpp                  (Step 3)
)

# ---------------------------------------------------------------------------
# risc201-sim : the simulator binary
# ---------------------------------------------------------------------------
add_executable(risc201-sim
  simulator_core/main.cpp
  ${RISC201_CORE_SOURCES}
  ${RISC201_UI_SOURCES}
)
target_include_directories(risc201-sim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(risc201-sim PRIVATE RISC201_WORD_BITS=${RISC201_WORD_BITS})
target_compile_options(risc201-sim PRIVATE -Wall -Wextra)

# FTXUI is only fetched once the TUI source exists, so scaffolding needs no
# network. Step 9 adds simulator_core/ui/tui.cpp and this block activates.
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

# ---------------------------------------------------------------------------
# risc201-tests : lightweight assert-based unit tests
# ---------------------------------------------------------------------------
add_executable(risc201-tests
  tests/test_main.cpp
  ${RISC201_CORE_SOURCES}
  ${RISC201_TEST_SOURCES}
)
target_include_directories(risc201-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(risc201-tests PRIVATE RISC201_WORD_BITS=${RISC201_WORD_BITS})
target_compile_options(risc201-tests PRIVATE -Wall -Wextra)

enable_testing()
add_test(NAME risc201-tests COMMAND risc201-tests)
