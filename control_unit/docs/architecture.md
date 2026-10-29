# RISC201 Microprogrammed Control Unit

**Owner:** Garv (Module Lead)  
**Scope:** Control Memory ROM, Micro-Sequencer, Micro-Op Cracking Engine, Horizontal & Vertical Decoders.

---

## 1. Design Overview

The RISC201 Control Unit implements **Method 3: Hybrid $\mu$-Op Cracking** in the **OF (Decode)** stage of the processor pipeline. It unifies single-cycle RISC execution with multi-cycle microcode execution without stalling the pipeline on simple instructions:

- **Fast-Path (Single-cycle ops):** Instructions like `ADD`, `SUB`, `AND`, `LD`, `ST`, `BNE` perform a 1-cycle direct ROM crack and immediately emit stage control signals down the pipeline.
- **Micro-Sequencer Path (Complex ops):** Instructions like `CALL`, `RET`, `SWAP`, `BNDCHK` trigger the $\mu\text{Sequencer}$. The Control Unit asserts `freeze_if_of = true` (stalling IF and OF) and steps through internal microcode routines, emitting a sequence of $\mu$-ops into the EX stage until completed.

---

## 2. Horizontal vs. Vertical Dual-Mode Architecture

The unit supports two user-selectable Control Memory (ROM) encodings:

```
                       ┌───────────────────────────────┐
                       │   Instruction Word (from IF)  │
                       └──────────────┬────────────────┘
                                      │ Opcode
                                      ▼
                       ┌───────────────────────────────┐
                       │  Control Memory ROM (Entry)   │
                       └──────┬─────────────────┬──────┘
                              │                 │
         [HORIZONTAL MODE]    │                 │    [VERTICAL MODE]
         77-bit Wide Word     │                 │    45-bit Encoded Word
                              ▼                 ▼
                       ┌─────────────┐   ┌─────────────┐
                       │ Direct Wire │   │ µ-Decoder   │
                       │ Pass-through│   │ Circuitry   │
                       └──────┬──────┘   └──────┬──────┘
                              │                 │
                              └────────┬────────┘
                                       ▼
                       ┌───────────────────────────────┐
                       │ Pipeline Control Signals      │
                       │ (ExControl, ChkControl, ...)  │
                       └───────────────────────────────┘
```

| Parameter | Horizontal Control Unit | Vertical Control Unit |
| :--- | :--- | :--- |
| **Word Width** | **77 bits** | **45 bits** |
| **Storage Footprint** | Baseline ($1.0\times$) | **$41.6\%$ reduction** ($0.584\times$) |
| **Decoding Latency** | $T_{dec} = 0\text{ ns}$ (Direct wires) | $T_{dec} > 0\text{ ns}$ (Combinational gate delay) |
| **Target Application** | Maximum clock frequency | Minimum silicon area |

---

## 3. Stage-Decoupled Control Signal Bundles

Rather than a monolithic bus, signals are bundled by pipeline stage and propagated across inter-stage pipeline latches:

```cpp
struct ExControl {
    ALUOp       alu_op          = ALUOp::NONE;
    OperandASrc operand_a_src   = OperandASrc::RS1;
    OperandBSrc operand_b_src   = OperandBSrc::RS2;
    bool        set_flags       = false;
    BranchCond  branch_cond     = BranchCond::NONE;
};

struct ChkControl {
    bool        memchk_enable   = false;  // Active in 6-stage pipeline
    ChkMode     chk_mode        = ChkMode::STACK_BOUNDS;
};

struct MaControl {
    bool        mem_read        = false;
    bool        mem_write       = false;
    MemWidth    width           = MemWidth::WORD;
};

struct WbControl {
    bool        reg_write_en    = false;
    RegWriteSrc reg_write_src   = RegWriteSrc::ALU_OUT;
    uint8_t     dest_reg        = 0;
};
```

---

## 4. Micro-Sequencer & Cracking Protocol

```
State Machine Cycle:
1. Decode Opcode at `IF/OF` latch.
2. If single-cycle:
   - Emit signals -> `OF/EX` latch.
   - Advance `PC`, fetch next instruction.
3. If multi-cycle (`CALL`, `RET`, `SWAP`, `BNDCHK`):
   - Assert `freeze_if_of = true`.
   - microPC <- EntryPoint[Opcode].
   - Loop:
       - Emit current µ-op signals -> `OF/EX` latch.
       - microPC <- NextMicroAddr(microPC).
       - If end of routine: de-assert `freeze_if_of`, advance `PC`.
```

### Multi-Op Routine Cycles:
- **`CALL` (2 µ-ops):**
  1. `ALU: SP = SP - 4`, `MEMCHK: verify(SP)`
  2. `MEM: [SP] = PC_return`, `PC = target`
- **`RET` (2 µ-ops):**
  1. `MEM: PC_return = [SP]`, `MEMCHK: verify(SP)`
  2. `ALU: SP = SP + 4`, `PC = PC_return`
- **`SWAP rA, rB` (3 µ-ops):**
  1. `TEMP = rA`
  2. `rA = rB`
  3. `rB = TEMP`
- **`BNDCHK rA, rLow, rHigh` (2 µ-ops):**
  1. `CMP rA, rLow` -> fault if `<`
  2. `CMP rA, rHigh` -> fault if `>`

---

## 5. Module Interface (`control_unit.h`)

```cpp
namespace risc201::cu {

class ControlUnit {
public:
    virtual ~ControlUnit() = default;
    
    // Step decode logic for the current cycle
    virtual void decode_cycle(Word instruction, const Flags& flags) = 0;
    
    // Status flags queried by the CPU core
    virtual bool is_cracking() const = 0;
    virtual bool freeze_pipeline() const = 0;
    
    // Stage control signals emitted into the OF/EX latch
    virtual const ExControl&  ex_signals()  const = 0;
    virtual const ChkControl& chk_signals() const = 0;
    virtual const MaControl&  ma_signals()  const = 0;
    virtual const WbControl&  wb_signals()  const = 0;
    
    virtual void reset() = 0;
};

} // namespace risc201::cu
```
