# Assignment: Processor Design 

## Problem Statement 

1. Design 

    1. a 4-stage and 6-stage RISC201 ISA. Compare the behavior of both processors.  
    2. a corresponding microprogrammed control unit (you are free to choose a horizontal or vertical model).  
    3. A corresponding ALU with efficient arithmetic and logical operations.  

2. Implement a complete software environment and simulation framework. The software suite must encompass  
    1. A  two-pass  assembler  that  translates  RISC201  assembly  programs  into  binary  or hexadecimal representations. 
    2. a disassembler that parses binary or hexadecimal word strings and reconstructs assembly instructions. 
    3. an  interactive  command-line  interface  to  simulate  register  file,  instruction  execution pipeline,  
    4. a step-by-step CPU execution simulator with debugger capabilities, and an interactive CLI debugger. 
    5. stack  preprocessor  macro  expansion  to  incorporate  preprocessor  support  for  full-descending stack operations. 
    6. stack boundary safety verification, and a runtime exception framework to incorporate runtime memory protection against stack corruption. 
    7. An active stack memory visualizer to implement an ASCII stack memory frame renderer. 

3. Add the microprogrammed control logic directly to your CPU simulator to execute RISC201 using  Control  Memory  microcode rather  than  hardwired  logic.  More  appreciation  for implementing both horizontal and vertical microprogrammed control units and comparing the behaviour. Make sure to extend the CLI simulator interface to display the microPC's step-by-step state and raise exceptions where necessary. 

4. Add an optimized RISC201 ALU class into your simulator code to drive all arithmetic, shift, and 
logical operations. 

5. Evaluate the processor, control unit, and ALU for different possible test cases by considering 4-
stage and 6-stage ISAs, different assembly programs, horizontal vs. vertical microprogrammed 
controllers (optional), and different ALU algorithms.