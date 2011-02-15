//===-- lldb_ARMUtils.h -----------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef lldb_ARMUtils_h_
#define lldb_ARMUtils_h_

#include "InstructionUtils.h"
#include "llvm/Support/MathExtras.h" // for SignExtend64 template function

// Common utilities for the ARM/Thumb Instruction Set Architecture.

namespace lldb_private {

typedef enum
{
    SRType_LSL,
    SRType_LSR,
    SRType_ASR,
    SRType_ROR,
    SRType_RRX
} ARM_ShifterType;

static inline uint32_t DecodeImmShift(const uint32_t type, const uint32_t imm5, ARM_ShifterType &shift_t)
{
    switch (type) {
    default:
        assert(0 && "Invalid shift type");
    case 0:
        shift_t = SRType_LSL;
        return imm5;
    case 1:
        shift_t = SRType_LSR;
        return (imm5 == 0 ? 32 : imm5);
    case 2:
        shift_t = SRType_ASR;
        return (imm5 == 0 ? 32 : imm5);
    case 3:
        if (imm5 == 0)
        {
            shift_t = SRType_RRX;
            return 1;
        }
        else
        {
            shift_t = SRType_ROR;
            return imm5;
        }
    }
}

static inline uint32_t DecodeImmShift(const ARM_ShifterType shift_t, const uint32_t imm5)
{
    ARM_ShifterType dont_care;
    return DecodeImmShift(shift_t, imm5, dont_care);
}

static inline ARM_ShifterType DecodeRegShift(const uint32_t type)
{
    switch (type) {
    default:
        assert(0 && "Invalid shift type");
    case 0:
        return SRType_LSL;
    case 1:
        return SRType_LSR;
    case 2:
        return SRType_ASR;
    case 3:
        return SRType_ROR;
    }
}

static inline uint32_t LSL_C(const uint32_t value, const uint32_t amount, uint32_t &carry_out)
{
    assert(amount > 0 && amount < 32);
    carry_out = Bit32(value, 32 - amount);
    return value << amount;
}

static inline uint32_t LSL(const uint32_t value, const uint32_t amount)
{
    assert(amount >= 0 && amount < 32);
    if (amount == 0)
        return value;
    uint32_t dont_care;
    return LSL_C(value, amount, dont_care);
}

static inline uint32_t LSR_C(const uint32_t value, const uint32_t amount, uint32_t &carry_out)
{
    assert(amount > 0 && amount <= 32);
    carry_out = Bit32(value, amount - 1);
    return value >> amount;
}

static inline uint32_t LSR(const uint32_t value, const uint32_t amount)
{
    assert(amount >= 0 && amount <= 32);
    if (amount == 0)
        return value;
    uint32_t dont_care;
    return LSR_C(value, amount, dont_care);
}

static inline uint32_t ASR_C(const uint32_t value, const uint32_t amount, uint32_t &carry_out)
{
    assert(amount > 0 && amount <= 32);
    carry_out = Bit32(value, amount - 1);
    int64_t extended = llvm::SignExtend64<32>(value);
    return UnsignedBits(extended, amount + 31, amount);
}

static inline uint32_t ASR(const uint32_t value, const uint32_t amount)
{
    assert(amount >= 0 && amount <= 32);
    if (amount == 0)
        return value;
    uint32_t dont_care;
    return ASR_C(value, amount, dont_care);
}

static inline uint32_t ROR_C(const uint32_t value, const uint32_t amount, uint32_t &carry_out)
{
    assert(amount > 0 && amount < 32);
    uint32_t result = Rotr32(value, amount);
    carry_out = Bit32(value, 31);
    return result;
}

static inline uint32_t ROR(const uint32_t value, const uint32_t amount)
{
    assert(amount >= 0 && amount < 32);
    if (amount == 0)
        return value;
    uint32_t dont_care;
    return ROR_C(value, amount, dont_care);
}

static inline uint32_t RRX_C(const uint32_t value, const uint32_t carry_in, uint32_t &carry_out)
{
    carry_out = Bit32(value, 0);
    return Bit32(carry_in, 0) << 31 | Bits32(value, 31, 1);
}

static inline uint32_t RRX(const uint32_t value, const uint32_t carry_in)
{
    uint32_t dont_care;
    return RRX_C(value, carry_in, dont_care);
}

static inline uint32_t Shift_C(const uint32_t value, ARM_ShifterType type, const uint32_t amount,
                               const uint32_t carry_in, uint32_t &carry_out)
{
    assert(type != SRType_RRX || amount == 1);
    if (amount == 0)
    {
        carry_out = carry_in;
        return value;
    }
    uint32_t result;
    switch (type) {
    case SRType_LSL:
        result = LSL_C(value, amount, carry_out);
        break;
    case SRType_LSR:
        result = LSR_C(value, amount, carry_out);
        break;
    case SRType_ASR:
        result = ASR_C(value, amount, carry_out);
        break;
    case SRType_ROR:
        result = ROR_C(value, amount, carry_out);
        break;
    case SRType_RRX:
        result = RRX_C(value, amount, carry_out);
        break;
    }
    return result;
}

static inline uint32_t Shift(const uint32_t value, ARM_ShifterType type, const uint32_t amount,
                             const uint32_t carry_in)
{
    // Don't care about carry out in this case.
    uint32_t dont_care;
    return Shift_C(value, type, amount, carry_in, dont_care);
}

static inline uint32_t bits(const uint32_t val, const uint32_t msbit, const uint32_t lsbit)
{
    return Bits32(val, msbit, lsbit);
}

static inline uint32_t bit(const uint32_t val, const uint32_t msbit)
{
    return bits(val, msbit, msbit);
}

static uint32_t ror(uint32_t val, uint32_t N, uint32_t shift)
{
    uint32_t m = shift % N;
    return (val >> m) | (val << (N - m));
}

// (imm32, carry_out) = ARMExpandImm_C(imm12, carry_in)
static inline uint32_t ARMExpandImm_C(uint32_t val, uint32_t carry_in, uint32_t &carry_out)
{
    uint32_t imm32;                      // the expanded result
    uint32_t imm = bits(val, 7, 0);      // immediate value
    uint32_t amt = 2 * bits(val, 11, 8); // rotate amount
    if (amt == 0)
    {
        imm32 = imm;
        carry_out = carry_in;
    }
    else
    {
        imm32 = ror(imm, 32, amt);
        carry_out = Bit32(imm32, 31);
    }
    return imm32;
}

static inline uint32_t ARMExpandImm(uint32_t val)
{
    // 'carry_in' argument to following function call does not affect the imm32 result.
    uint32_t carry_in = 0;
    uint32_t carry_out;
    return ARMExpandImm_C(val, carry_in, carry_out);
}

// (imm32, carry_out) = ThumbExpandImm_C(imm12, carry_in)
static inline uint32_t ThumbExpandImm_C(uint32_t val, uint32_t carry_in, uint32_t &carry_out)
{
    uint32_t imm32; // the expaned result
    const uint32_t i = bit(val, 26);
    const uint32_t imm3 = bits(val, 14, 12);
    const uint32_t abcdefgh = bits(val, 7, 0);
    const uint32_t imm12 = i << 11 | imm3 << 8 | abcdefgh;

    if (bits(imm12, 11, 10) == 0)
    {
        switch (bits(imm12, 8, 9)) {
        case 0:
            imm32 = abcdefgh;
            break;

        case 1:
            imm32 = abcdefgh << 16 | abcdefgh;
            break;

        case 2:
            imm32 = abcdefgh << 24 | abcdefgh << 8;
            break;

        case 3:
            imm32 = abcdefgh  << 24 | abcdefgh << 16 | abcdefgh << 8 | abcdefgh; 
            break;
        }
        carry_out = carry_in;
    }
    else
    {
        const uint32_t unrotated_value = 0x80 | bits(imm12, 6, 0);
        imm32 = ror(unrotated_value, 32, bits(imm12, 11, 7));
        carry_out = Bit32(imm32, 31);
    }
    return imm32;
}

static inline uint32_t ThumbExpandImm(uint32_t val)
{
    // 'carry_in' argument to following function call does not affect the imm32 result.
    uint32_t carry_in = 0;
    uint32_t carry_out;
    return ThumbExpandImm_C(val, carry_in, carry_out);
}

// imm32 = ZeroExtend(i:imm3:imm8, 32)
static inline uint32_t ThumbImm12(uint32_t val)
{
  const uint32_t i = bit(val, 26);
  const uint32_t imm3 = bits(val, 14, 12);
  const uint32_t imm8 = bits(val, 7, 0);
  const uint32_t imm12 = i << 11 | imm3 << 8 | imm8;
  return imm12;
}

// imm32 = ZeroExtend(imm7:'00', 32)
static inline uint32_t ThumbImmScaled(uint32_t val)
{
  const uint32_t imm7 = bits(val, 6, 0);
  return imm7 * 4;
}

// This function performs the check for the register numbers 13 and 15 that are
// not permitted for many Thumb register specifiers.
static inline bool BadReg(uint32_t n) { return n == 13 || n == 15; }

}   // namespace lldb_private

#endif  // lldb_ARMUtils_h_
