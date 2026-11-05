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
  "${CMAKE_CURRENT_SOURCE_DIR}/control_unit"
  "${CMAKE_CURRENT_SOURCE_DIR}/legacy")
target_compile_definitions(risc201_config INTERFACE
  RISC201_WORD_BITS=${RISC201_WORD_BITS})

add_library(risc201_cu STATIC
  "control_unit/control_unit.cpp"
  "control_unit/pipeline_processor.cpp"
  "legacy/common/internal_registers.cpp"
  "legacy/datapath/datapath.cpp"
  "legacy/datapath/memory_unit.cpp"
  "legacy/microcode/microcode_program.cpp"
  "legacy/microcode/h_assembler.cpp"
  "legacy/microcode/v_assembler.cpp"
  "legacy/vertical/v_decoder.cpp")
target_link_libraries(risc201_cu PUBLIC risc201_config risc201_safety risc201_alu)
target_compile_options(risc201_cu PRIVATE -Wall -Wextra)

add_library(risc201_alu STATIC
  alu/ALU.cpp
  fpu/fpu.cpp)
target_link_libraries(risc201_alu PUBLIC risc201_config)
target_compile_options(risc201_alu PRIVATE -Wall -Wextra)

add_library(risc201_safety STATIC
  memory_safety/stack_guard.cpp
  memory_safety/visualizer.cpp)
target_link_libraries(risc201_safety PUBLIC risc201_config)
target_compile_options(risc201_safety PRIVATE -Wall -Wextra)

add_executable(risc201asm assembler_disassembler/asm.cpp)
target_link_libraries(risc201asm PRIVATE risc201_config)
target_compile_options(risc201asm PRIVATE -Wall -Wextra)

add_executable(risc201dis assembler_disassembler/disasm.cpp)
target_link_libraries(risc201dis PRIVATE risc201_config)
target_compile_options(risc201dis PRIVATE -Wall -Wextra)

add_library(risc201_asm STATIC
  legacy/assembler/isa.cpp
  legacy/assembler/formats.cpp
  legacy/assembler/macros.cpp
  legacy/assembler/assembler.cpp
  legacy/assembler/disassembler.cpp)
target_link_libraries(risc201_asm PUBLIC risc201_config)
target_include_directories(risc201_asm PUBLIC legacy/assembler)
target_compile_options(risc201_asm PRIVATE -Wall -Wextra)

add_executable(risc201-cu-demo "legacy/main.cpp")
target_link_libraries(risc201-cu-demo PRIVATE risc201_cu)
target_compile_options(risc201-cu-demo PRIVATE -Wall -Wextra)

add_executable(risc201-sim simulator_core/main.cpp)
target_link_libraries(risc201-sim PRIVATE risc201_cu risc201_alu risc201_asm)
target_compile_options(risc201-sim PRIVATE -Wall -Wextra)

if(RISC201_TUI)
  include(FetchContent)
  FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v5.0.0)
  set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(ftxui)
  target_sources(risc201-sim PRIVATE simulator_core/ui/tui.cpp)
  target_link_libraries(risc201-sim PRIVATE
    ftxui::screen ftxui::dom ftxui::component)
  target_compile_definitions(risc201-sim PRIVATE RISC201_HAS_TUI=1)
endif()

add_executable(risc201-tests
  tests/test_main.cpp
  tests/test_cpu.cpp
  tests/test_debugger.cpp
  tests/test_cu_programs.cpp
  tests/test_programs.cpp
  tests/test_assembler.cpp
  tests/test_pipeline.cpp)
target_link_libraries(risc201-tests PRIVATE risc201_cu risc201_alu risc201_asm)
target_compile_options(risc201-tests PRIVATE -Wall -Wextra)

add_executable(stack-guard-test tests/test_stack_guard.cpp)
target_link_libraries(stack-guard-test PRIVATE risc201_safety)
target_compile_options(stack-guard-test PRIVATE -Wall -Wextra)

add_executable(stack-visualizer-demo tests/demo_visualizer.cpp)
target_link_libraries(stack-visualizer-demo PRIVATE risc201_safety)
target_compile_options(stack-visualizer-demo PRIVATE -Wall -Wextra)

enable_testing()
add_test(NAME risc201-tests COMMAND risc201-tests)
add_test(NAME stack-guard-test COMMAND stack-guard-test)

add_executable(memory-unit-test
  tests/test_memory_unit.cpp)
target_link_libraries(memory-unit-test PRIVATE risc201_cu risc201_safety)
target_compile_options(memory-unit-test PRIVATE -Wall -Wextra)
add_test(NAME memory-unit-test COMMAND memory-unit-test)

add_executable(stack-visualizer-test
  tests/test_visualizer.cpp)
target_link_libraries(stack-visualizer-test PRIVATE risc201_safety)
target_compile_options(stack-visualizer-test PRIVATE -Wall -Wextra)
add_test(NAME stack-visualizer-test COMMAND stack-visualizer-test)
