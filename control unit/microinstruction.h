#ifndef MICROINSTRUCTION_H
#define MICROINSTRUCTION_H

#include "control_signals.h"

// ============================================================
// Horizontal microinstruction: every control signal gets its
// own dedicated field, no encoding/sharing between fields.
// This IS a row of Control Memory.
// ============================================================

struct Microinstruction {
    ControlSignals signals;
};

#endif // MICROINSTRUCTION_H
