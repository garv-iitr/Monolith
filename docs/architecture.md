# RISC201 Processor Architecture & Hardware Specification

**Course:** CSC-201 Computer Organization & Architecture  
**Hardware Model:** Method 3 Hybrid $\mu$-Op Cracking Pipelined Processor  
**Control Unit Architecture:** Dual Control Memories (48-bit Horizontal ROM vs. 24-bit Vertical ROM + $\mu$-Decoder)  
**Safety Subsystem:** Hardware Memory Safety Firewall (`StackGuard`)  

---

## 1. System Overview & Teammate Ownership

The RISC201 is a cycle-accurate, software-hardware co-designed processor supporting both integer arithmetic, floating-point execution, memory-safety verification, and dynamic microcode cracking.

| Subsystem | Owner / Lead | Core Hardware Deliverables |
| :--- | :--- | :--- |
| **`control_unit/`** | **Garv (Lead)** | Method 3 Microprogrammed Control Unit, 48b Horizontal & 24b Vertical ROMs, $\mu\text{Sequencer}$, Stage Bundles, 4-Stage & 6-Stage Pipelined Processor |
| **`simulator_core/`** | **Aditya** | Interactive Debugger, State Inspection Hooks, FTXUI Terminal UI |
| **`alu/`** | **Atharva** | 32-bit Kogge-Stone Adder, Radix-4 Booth Multiplier, Non-Restoring Divider, Flags (`Z`, `N`, `C`, `V`), Branch Comparator, FPU |
| **`memory_safety/`** | **Paras** | Hardware Stack Guard (`SP=r14`), Dedicated `MEMCHK` Firewall Stage, ASCII Visualizer |
| **`assembler_disassembler/`** | **Saatvik** | 2-pass Assembler (`risc201asm`), Disassembler (`risc201dis`), Macro Expansion Engine |
| **`isa_spec/`** | **Shared** | Canonical RISC201 ISA definitions, 34 opcodes, instruction formats |

---

## 2. Hardware Datapath Evolution: Sarangi Fig 9.22 vs. RISC201

In classic microprogrammed processors (Sarangi Figure 9.22), a single **shared internal bus** serializes every register transfer, leading to high CPI ($10.18 - 10.38$):
- *Fetch:* $\text{PC} \to \text{Bus} \to \text{MAR} \to \text{Mem} \to \text{MDR} \to \text{Bus} \to \text{IR}$ ($4$ cycles).
- *Decode & Operand Fetch:* $\text{IR} \to \text{RegFile} \to \text{Bus} \to \text{A, B}$ ($2$ cycles).
- *Execute:* $\text{A, B} \to \text{ALU} \to \text{Bus} \to \text{aluResult}$ ($1$ cycle).
- *Memory Access:* $\text{aluResult} \to \text{MAR}; \text{Bus} \to \text{MDR} \to \text{Data Mem}$ ($2$ cycles).
- *Writeback:* $\text{MDR} / \text{aluResult} \to \text{Bus} \to \text{RegFile}$ ($1$ cycle).

The RISC201 replaces the shared bus bottleneck with **synchronous pipeline registers** (`IF/OF`, `OF/EX`, `EX/MA_RW`, `CHK/MA`, `MA/RW`):

```
Classic Microprogrammed (Sarangi 9.22)       RISC201 Pipelined Datapath (Method 3)
┌─────────────────────────────────┐          ┌──────────────┐
│           Shared Bus            │          │  Stage 1: IF │ [PC, Fetch]
└─┬─────▲─────▲──────┬──────┬─────┘          └──────┬───────┘
  │     │     │      │      │                       │ IfOfLatch (pc, raw)
  ▼     │     │      ▼      ▼                       ▼
┌───┐ ┌───┐ ┌───┐  ┌───┐  ┌───┐              ┌──────────────┐
│mar│ │mdr│ │alu│  │ A │  │ B │              │  Stage 2: OF │ [Decode & u-Op Cracking]
└───┘ └───┘ └───┘  └───┘  └───┘              └──────┬───────┘
                                                    │ OfExLatch (rs1, rs2, control, imm)
                                                    ▼
                                             ┌──────────────┐
                                             │  Stage 3: EX │ [ALU, FPU, Forwarding, Branch]
                                             └──────┬───────┘
                                                    │ ExStageLatch (alu_result, store_val)
                                                    ▼
                                             ┌──────────────┐
                                             │ Stage 4: MA  │ [Memory Access & Safety Firewall]
                                             └──────┬───────┘
                                                    │ MaRwLatch (wb_data, dest_reg)
                                                    ▼
                                             ┌──────────────┐
                                             │  Stage 5: RW │ [Register File Writeback]
                                             └──────────────┘
```

---

## 3. Supported Pipeline Configurations

### A. 4-Stage Architecture (`[IF] -> [OF] -> [EX] -> [MA/RW]`)
Optimized for maximum IPC and minimal latency:
```text
[ IF ] ────────► [ OF ] ────────► [ EX ] ────────► [ MA/RW ]
Fetch            Decode &         Execute &         Merged Memory Access &
                 u-Op Cracking    Forwarding        Register Writeback
```
- **Front-End:** Decoupled Instruction Fetch (`IF`) and Decode (`OF`).
- **Back-End:** Merged Memory Access and Register Writeback (`MA/RW`) completing in the same clock cycle.
- **Branch Penalty:** 2 cycles (branch resolved in `EX`, flushes `IF` and `OF`).
- **Load-Use Hazard Penalty:** 1 cycle stall bubble.
- **Safety Verification:** Inline firewall checking executed concurrently with `MA/RW`.

### B. 6-Stage Architecture (`[IF] -> [OF] -> [EX] -> [MEMCHK] -> [MA] -> [RW]`)
Optimized for high-frequency clock scaling and physical hardware safety firewall isolation:
```text
[ IF ] ────► [ OF ] ────► [ EX ] ────► [ MEMCHK ] ────► [ MA ] ────► [ RW ]
Fetch        Decode &      Execute &      Hardware Safety     Memory         Register
             Cracking      Forwarding     Firewall Stage      Access         Writeback
```
- **Firewall Isolation:** Stage 4 (`MEMCHK`) physically isolates memory address generation from bus strobes. Out-of-bounds stack operations and misalignments trigger `RuntimeException` before memory read/write strobes are asserted.
- **Branch Penalty:** 2 cycles.
- **Load-Use Hazard Penalty:** 2 cycle stall bubbles.

---

## 4. Method 3 Hybrid $\mu$-Op Cracking Engine

Method 3 unifies RISC speed with CISC flexibility:
1. **RISC Fast-Path (Single-Cycle Ops):**
   - Arithmetic (`ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `AND`, `OR`, `XOR`, `LSL`, `LSR`, `ASR`), data movement (`MOV`, `NOT`), memory (`LD`, `ST`), and branches stream through `OF` with $\text{CPI} = 1.0$.
2. **CISC Micro-Sequencer Path (Complex Ops):**
   - Complex instructions (`CALL`, `RET`, `BNDCHK`) trigger the `MicroSequencer`.
   - The Control Unit asserts `freeze_if_of = true` to freeze `PC` and `IF/OF` latches.
   - Atomic $\mu$-ops are sequentially injected into `OF/EX` until `is_last_uop = true`:
     - **`CALL` ($\mu$-op 0):** `SP = SP - 4` (with stack bounds checking).
     - **`CALL` ($\mu$-op 1):** `Mem[SP] = PC + 4; PC = Target`.
     - **`RET` ($\mu$-op 0):** `RA = Mem[SP]` (stack pop into return register).
     - **`RET` ($\mu$-op 1):** `SP = SP + 4; PC = RA`.

---

## 5. First-Principles Control Memory (ROM) Encodings

```
                              Instruction [31:0]
                                      │
                                      ▼
                        ┌───────────────────────────┐
                        │ Microcode Control Store   │
                        │ (128 words, uPC dispatch) │
                        └─────────────┬─────────────┘
                                      │
                 ┌────────────────────┴────────────────────┐
                 │                                         │
        [HORIZONTAL MODE]                         [VERTICAL MODE]
      48-Bit Direct Control                     24-Bit Compact Word
                 │                                         │
                 │                                         ▼
                 │                              ┌─────────────────────┐
                 │                              │ Comb. Micro-Decoder │
                 │                              │ (op_class, sub_op)  │
                 │                              └──────────┬──────────┘
                 │                                         │
                 └────────────────────┬────────────────────┘
                                      ▼
                        ┌───────────────────────────┐
                        │ Pipeline Control Bundle   │
                        │ (Ex, Chk, Ma, Wb bundles) │
                        └───────────────────────────┘
```


### A. Horizontal Control Word (48 Bits, $T_{\text{dec}} = 0$)
Directly drives datapath multiplexers with zero combinational gate delay:

| Field | Bits | Function | Encoding Values |
| :--- | :---: | :--- | :--- |
| `alu_op` | [3:0] | Integer ALU Operation | `NOP=0`, `ADD=1`, `SUB=2`, `MUL=3`, `DIV=4`, `MOD=5`, `AND=6`, `OR=7`, `NOT=8`, `XOR=9`, `LSL=10`, `LSR=11`, `ASR=12`, `CMP=13`, `PASS_A=14`, `PASS_B=15` |
| `op_a_src` | [5:4] | Operand A Multiplexer | `0=RS1`, `1=IMM`, `2=PC`, `3=ZERO` |
| `op_b_src` | [7:6] | Operand B Multiplexer | `0=RS2`, `1=IMM`, `2=ZERO` |
| `set_flags`| [8] | Update Condition Codes | `0=Hold`, `1=Update Z, N, C, V` |
| `branch_cond`| [11:9]| Branch Condition | `EQ=0`, `NE=1`, `GT=2`, `LT=3` |
| `is_branch`| [12] | Branch Strobe | `0=Sequential`, `1=Evaluate Branch` |
| `is_call` | [13] | Subroutine Call Push | `1=Assert stack push and link` |
| `is_ret` | [14] | Subroutine Return Pop | `1=Assert stack pop to PC` |
| `is_fpu` | [15] | FPU Domain Route | `0=Integer ALU`, `1=FPU Execution` |
| `fpu_op` | [18:16]| Floating-Point Operation | `FADD=1`, `FSUB=2`, `FMUL=3`, `FDIV=4`, `FCMP=5`, `FMOV=6` |
| `chk_enable`| [19] | Memory Firewall Enable | `0=Bypass`, `1=Verify Memory Safety` |
| `chk_mode` | [21:20]| Safety Check Mode | `0=STACK_BOUNDS`, `1=MEMORY_ADDR`, `2=BNDCHK` |
| `mem_read` | [22] | Data Memory Read Strobe | `1=Assert Read Bus Transaction` |
| `mem_write`| [23] | Data Memory Write Strobe| `1=Assert Write Bus Transaction` |
| `ma_is_fpu`| [24] | Memory FPU Strobe | `0=GPR`, `1=FPR` |
| `reg_write_en`| [25] | Register Write Enable | `1=Commit to Register File` |
| `reg_write_src`| [28:26]| Writeback Multiplexer | `0=ALU_OUT`, `1=MEM_DATA`, `2=PC_PLUS4`, `3=FPU_OUT`, `4=PASS_B` |
| `wb_is_fpu`| [29] | Writeback Destination FPR| `0=GPR`, `1=FPR` |
| `freeze_if_of`| [30] | Pipeline Stall Strobe | `1=Freeze IF/OF Latches` |
| `is_multi_uop`| [31] | Multi-uOp Sequence Flag| `1=Microcode Routine In-Flight` |
| `is_last_uop` | [32] | Retirement Indicator | `1=Final u-Op of Macro-Instruction` |
| `halt` | [33] | Processor Halt Strobe | `1=Stop Clock Execution` |
| `next_micro_pc`| [47:34]| Micro-Sequencer Address | 14-bit Control Store Address |
| **Total** | **48 bits** | **Packed into 6 bytes** | **$T_{\text{dec}} = 0\text{ ns}$ delay** |

### B. Vertical Control Word (24 Bits, $-50.0\%$ Area)
Field-encoded compact format expanded via a combinational $\mu$-decoder:

| Field | Bits | Description |
| :--- | :---: | :--- |
| `op_class` | [2:0] | Operation class (`0=ALU`, `1=FPU`, `2=MEM`, `3=BRANCH`, `4=SYSTEM`, `5=FLOW`) |
| `sub_op` | [6:3] | Sub-operation code within selected class |
| `operand_mode` | [9:7] | Joint encoding of operand source multiplexers |
| `mem_action` | [11:10]| Memory transaction strobe (`0=None`, `1=Read`, `2=Write`, `3=CheckOnly`) |
| `wb_target` | [14:12]| Register writeback target (`0=None`, `1=ALU`, `2=MEM`, `3=PC+4`, `4=FPU`) |
| `is_fpu_domain`| [15] | Register domain (`0=Integer GPR`, `1=Floating-Point FPR`) |
| `set_flags` | [16] | Condition code register update strobe |
| `freeze_if_of` | [17] | Pipeline freeze strobe |
| `is_multi_uop` | [18] | Multi-uop sequence active indicator |
| `is_last_uop` | [19] | Final uop indicator (retires instruction) |
| `halt` | [20] | Processor execution halt strobe |
| `next_uop_step`| [23:21]| Micro-step index ($0-7$) |
| **Total** | **24 bits** | **Packed into 3 bytes (50.0% ROM Silicon Area Reduction)** |

---

## 6. Verified Hardware Evaluation Matrix

Performance verified under full cycle-accurate MinGW64 simulation across the reference benchmark programs:

| Benchmark Program | Pipeline Datapath | Control Memory Encoding | Execution Cycles | Retired Instructions | Average CPI | Speedup vs Baseline | Hazards (Stalls + Flushes) |
| :--- | :--- | :--- | :---: | :---: | :---: | :---: | :---: |
| **Recursive Fibonacci ($N=10$)** | 4-Stage | Horizontal (48-bit) | **3,597** | 1,825 | **1.97** | **5.26x** | 1,415 |
| | 4-Stage | Vertical (24-bit) | **3,597** | 1,825 | **1.97** | **5.26x** | 1,415 |
| | 6-Stage | Horizontal (48-bit) | **3,952** | 1,825 | **2.17** | **4.79x** | 1,768 |
| | 6-Stage | Vertical (24-bit) | **3,952** | 1,825 | **2.17** | **4.79x** | 1,768 |
| | *Baseline Single-Bus* | *Microprogrammed* | *18,937* | *1,825* | *10.38* | *1.00x* | *N/A* |
| **Matrix Multiply + FPU ($2\times2$)** | 4-Stage | Horizontal (48-bit) | **315** | 229 | **1.38** | **7.40x** | 83 |
| | 4-Stage | Vertical (24-bit) | **315** | 229 | **1.38** | **7.40x** | 83 |
| | 6-Stage | Horizontal (48-bit) | **326** | 229 | **1.42** | **7.15x** | 92 |
| | 6-Stage | Vertical (24-bit) | **326** | 229 | **1.42** | **7.15x** | 92 |
| | *Baseline Single-Bus* | *Microprogrammed* | *2,331* | *229* | *10.18* | *1.00x* | *N/A* |
| **Bubble Sort + Checksum** | 4-Stage | Horizontal (48-bit) | **818** | 606 | **1.35** | **7.23x** | 209 |
| | 4-Stage | Vertical (24-bit) | **818** | 606 | **1.35** | **7.23x** | 209 |
| | 6-Stage | Horizontal (48-bit) | **869** | 606 | **1.43** | **6.81x** | 258 |
| | 6-Stage | Vertical (24-bit) | **869** | 606 | **1.43** | **6.81x** | 258 |
| | *Baseline Single-Bus* | *Microprogrammed* | *5,918* | *606* | *9.77* | *1.00x* | *N/A* |

---

## 7. Build & Test Commands (GCC / MinGW)

```powershell
# Configure build with GCC (MinGW Makefiles)
cmake -B build -G "MinGW Makefiles"

# Compile all targets with GCC
cmake --build build

# Execute full test suite
.\build\risc201-tests.exe

# Execute all CTest harnesses
ctest --test-dir build --output-on-failure
```
