# ============================================================
# Makefile for RISC201 assembler and disassembler
# Requires: g++ with C++17 support
# ============================================================

CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# Common object files (linked into both executables)
COMMON_SRCS = isa.cpp formats.cpp macros.cpp
COMMON_OBJS = $(COMMON_SRCS:.cpp=.o)

ASM_SRCS  = assembler.cpp main_asm.cpp
ASM_OBJS  = $(ASM_SRCS:.cpp=.o)

DIS_SRCS  = disassembler.cpp main_dis.cpp
DIS_OBJS  = $(DIS_SRCS:.cpp=.o)

.PHONY: all clean

all: risc201asm risc201dis

risc201asm: $(COMMON_OBJS) $(ASM_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

risc201dis: $(COMMON_OBJS) $(DIS_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Pattern rule for .o files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Header dependencies
isa.o:          isa.cpp isa.hpp
formats.o:      formats.cpp formats.hpp isa.hpp
macros.o:       macros.cpp macros.hpp isa.hpp
assembler.o:    assembler.cpp assembler.hpp formats.hpp macros.hpp isa.hpp
disassembler.o: disassembler.cpp disassembler.hpp formats.hpp isa.hpp
main_asm.o:     main_asm.cpp assembler.hpp isa.hpp
main_dis.o:     main_dis.cpp disassembler.hpp isa.hpp

clean:
	rm -f *.o risc201asm risc201dis
