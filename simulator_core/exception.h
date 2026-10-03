#ifndef EXCEPTION_H
#define EXCEPTION_H
#include <cstdint>

enum class ExceptionCode { DIV_BY_ZERO, BAD_MEM_ACCESS, STACK_OVERFLOW, STACK_UNDERFLOW };

struct RuntimeException {
    ExceptionCode code;
    std::uint32_t address;
};
#endif