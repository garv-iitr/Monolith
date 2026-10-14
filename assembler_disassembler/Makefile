# ============================================================
# Makefile for RISC201 assembler and disassembler
# Requires: g++ with C++17 support
# ============================================================

CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# Common object files for disassembler
COMMON_SRCS = isa.cpp formats.cpp macros.cpp
COMMON_OBJS = $(COMMON_SRCS:.cpp=.o)

DIS_SRCS  = disassembler.cpp main_dis.cpp
DIS_OBJS  = $(DIS_SRCS:.cpp=.o)

.PHONY: all clean

all: asm risc201asm dis risc201dis

# Single-file standalone assembler (replaces multi-file assembler)
asm: asm.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

risc201asm: asm
	cp asm risc201asm
	cp asm risc201-asm

# Single-file standalone disassembler (replaces multi-file disassembler)
dis: dis.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

risc201dis: dis
	cp dis risc201dis
	cp dis risc201-disasm

clean:
	rm -f *.o risc201asm risc201dis risc201-asm risc201-disasm asm dis
