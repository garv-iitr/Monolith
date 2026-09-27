#pragma once

#include <cstdint>

enum class ExceptionCode {
  NONE = 0,
  DIV_BY_ZERO,
  BAD_MEM_ACCESS,
  MISALIGNED_ACCESS,
  STACK_OVERFLOW,
  STACK_UNDERFLOW,
  ILLEGAL_INSTRUCTION,
};

inline const char* to_string(ExceptionCode code) {
  switch (code) {
    case ExceptionCode::NONE:                return "none";
    case ExceptionCode::DIV_BY_ZERO:         return "divide by zero";
    case ExceptionCode::BAD_MEM_ACCESS:      return "bad memory access";
    case ExceptionCode::MISALIGNED_ACCESS:   return "misaligned access";
    case ExceptionCode::STACK_OVERFLOW:      return "stack overflow";
    case ExceptionCode::STACK_UNDERFLOW:     return "stack underflow";
    case ExceptionCode::ILLEGAL_INSTRUCTION: return "illegal instruction";
  }
  return "unknown";
}

struct RuntimeException {
  ExceptionCode code = ExceptionCode::NONE;
  std::uint64_t address = 0;
};
