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
  "${CMAKE_CURRENT_SOURCE_DIR}/memory"
  "${CMAKE_CURRENT_SOURCE_DIR}/assembler_disassembler")
target_compile_definitions(risc201_config INTERFACE
  RISC201_WORD_BITS=${RISC201_WORD_BITS}
  $<$<PLATFORM_ID:Windows>:NOMINMAX;WIN32_LEAN_AND_MEAN>)

function(risc201_set_warnings target)
  target_compile_options(${target} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W3;/utf-8>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra>)
endfunction()

add_library(risc201_cu STATIC
  "control_unit/control_unit.cpp"
  "control_unit/pipeline_processor.cpp"
  "memory/memory_unit.cpp")
target_link_libraries(risc201_cu PUBLIC risc201_config risc201_safety risc201_alu)
risc201_set_warnings(risc201_cu)

add_library(risc201_alu STATIC
  alu/ALU.cpp
  fpu/fpu.cpp)
target_link_libraries(risc201_alu PUBLIC risc201_config)
risc201_set_warnings(risc201_alu)

add_library(risc201_safety STATIC
  memory_safety/stack_guard.cpp
  memory_safety/visualizer.cpp)
target_link_libraries(risc201_safety PUBLIC risc201_config)
risc201_set_warnings(risc201_safety)

add_executable(risc201asm assembler_disassembler/asm.cpp)
target_link_libraries(risc201asm PRIVATE risc201_config)
risc201_set_warnings(risc201asm)

add_executable(risc201dis assembler_disassembler/disasm.cpp)
target_link_libraries(risc201dis PRIVATE risc201_config)
risc201_set_warnings(risc201dis)

add_executable(risc201-sim simulator_core/main.cpp)
add_dependencies(risc201-sim risc201asm)
target_link_libraries(risc201-sim PRIVATE risc201_cu risc201_alu)
risc201_set_warnings(risc201-sim)

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
  tests/test_pipeline.cpp
  tests/test_edge_cases.cpp)
add_dependencies(risc201-tests risc201asm)
target_link_libraries(risc201-tests PRIVATE risc201_cu risc201_alu risc201_safety)
target_compile_definitions(risc201-tests PRIVATE
  RISC201_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
risc201_set_warnings(risc201-tests)

add_executable(stack-guard-test tests/test_stack_guard.cpp)
target_link_libraries(stack-guard-test PRIVATE risc201_safety)
risc201_set_warnings(stack-guard-test)

enable_testing()
add_test(NAME risc201-tests COMMAND risc201-tests)
add_test(NAME stack-guard-test COMMAND stack-guard-test)

add_executable(memory-unit-test
  tests/test_memory_unit.cpp)
target_link_libraries(memory-unit-test PRIVATE risc201_cu risc201_safety)
risc201_set_warnings(memory-unit-test)
add_test(NAME memory-unit-test COMMAND memory-unit-test)

add_executable(stack-visualizer-test
  tests/test_visualizer.cpp)
target_link_libraries(stack-visualizer-test PRIVATE risc201_safety)
risc201_set_warnings(stack-visualizer-test)
add_test(NAME stack-visualizer-test COMMAND stack-visualizer-test)

# Assembler & Disassembler Toolchain Integration Tests
macro(add_toolchain_test test_name asm_file)
  add_test(NAME toolchain-asm-${test_name} COMMAND risc201asm "${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/${asm_file}" "${CMAKE_CURRENT_BINARY_DIR}/${test_name}.hex" --hex)
  add_test(NAME toolchain-dis-${test_name} COMMAND risc201dis "${CMAKE_CURRENT_BINARY_DIR}/${test_name}.hex")
  set_tests_properties(toolchain-dis-${test_name} PROPERTIES DEPENDS toolchain-asm-${test_name})
endmacro()

add_toolchain_test(fib fib.asm)
add_toolchain_test(fac fac.asm)
add_toolchain_test(matmul matmul.asm)
add_toolchain_test(sort sort.asm)
add_toolchain_test(all-34 all_34_opcodes.asm)
add_toolchain_test(alu-ops alu_ops.asm)
add_toolchain_test(control-flow control_flow.asm)
add_toolchain_test(fpu-ops fpu_ops.asm)
add_toolchain_test(memory-ops memory_ops.asm)
