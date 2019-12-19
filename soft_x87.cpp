#include "soft_x87.h"


void soft_x87::add(tword& a, tword &b, bool subtract) {
    subtract = subtract ^ (a.sign ^ b.sign);

    int diff = a.exponent - b.exponent;

    uint64_t bigger_sig = a.significand;
    uint64_t smaller_sig = b.significand;
    int exponent = a.exponent;
    uint64_t sign = a.sign;

    // Swap if b is bigger than a;
    if (diff < 0) {
        diff = -diff;
        bigger_sig = b.significand;
        smaller_sig = a.significand;
        exponent = b.exponent;
        sign = b.sign;
    }

    uint64_t smaller_sig_shifted = smaller_sig >> diff;

    uint64_t significand;
    if (subtract) {
        significand = bigger_sig - smaller_sig_shifted;
        if (significand == 0) {
            exponent = 0;
        } else {
            int msb = __builtin_clz(significand);
            int shift = std::min(msb, exponent); // Clamp to zero
            significand <<= shift;
            exponent -= shift;
        }
        // TODO: sign.
    } else {
        bool overflow = __builtin_add_overflow(bigger_sig, smaller_sig_shifted, &significand);
        if (overflow) {
            significand = (significand >> 1) | 0x8000000000000000;
            exponent++;
        }
    }

    tword &result = ST(0);
    result.significand = significand;
    result.exponent = exponent;
    result.sign = sign;
}

template<class T>
tword soft_x87::expand(T f) {
    static_assert(std::is_same<T, qword>::value || std::is_same<T, dword>::value, "Unsupported type");

    tword expanded;
    expanded.sign = f.sign;

    int shift = 63 - f.significand_width;
    uint64_t significand = f.significand;
    int exponent = f.exponent;

    int exponent_max = (1 << f.exponent_width) - 1;
    if (exponent == exponent_max) {
        expanded.exponent = 0x7fff;
        if (significand == 0)
            expanded.significand = 0x8000'0000'0000'0000;
        else
            expanded.significand = (significand << shift) | 0xc000'0000'0000'0000;
        //expanded.significand &= ~0x4000000000000000; // Suppress signaling NaNs.
        return expanded;
    }

    if (exponent == 0) {
        if (significand == 0) {
            expanded.exponent = 0;
            expanded.significand = 0x0000000000000000;
            return expanded;
        }

        // convert denormal
        // TODO: Does x87 actually convert denormals?
        uint64_t mask = 1ULL << (f.significand_width);
        while ((significand & mask) == 0) {
            // Shift significand until interger bit is set
            significand <<= 1;
            exponent -= 1;
        }
        exponent += 1;
    }

    int zero_offset = (1 << (f.exponent_width -1)) - 1;
    exponent = exponent - zero_offset;

    int expanded_zero_offset = (1 << (15 -1)) - 1;
    expanded.exponent = exponent + expanded_zero_offset;


    expanded.significand = 0x8000000000000000 | (significand << shift);

    return expanded;
}

template<class T>
T soft_x87::compress(tword f) {
    static_assert(std::is_same<T, qword>::value || std::is_same<T, dword>::value, "Unsupported type");

    T compressed;
    compressed.sign = f.sign;

    uint64_t significand = f.significand & ~tword::interger_bit_mask;
    int shift = 63 - T::significand_width;


    // Handle pre-existing infinity
    int exponent_max = (1 << compressed.exponent_width) - 1;
    if (f.exponent == tword::exponent_max && f.significand == 0) {
        compressed.significand = 0;
        compressed.exponent = exponent_max;
        return compressed;
    }

    // Handle NAN
    if (f.exponent == tword::exponent_max) {
        compressed.exponent = exponent_max;
        if (significand) {
            significand |= 0x4000'0000'0000'0000; // Force to be quiet
            compressed.significand = significand >> shift;
        }
        return compressed;
    }

    int compressed_zero_offset = (1 << (compressed.exponent_width -1)) - 1;
    int zero_offset = (1 << (15 -1)) - 1;

    int exponent = (f.exponent - zero_offset) + compressed_zero_offset;

    // Overflow to infinity
    if (exponent >= exponent_max) {
        compressed.significand = 0;
        compressed.exponent = exponent_max;
        return compressed;
    }

    // Underflow
    if (exponent <= 0) {
        if (exponent < -T::significand_width) {
            // Too big for denormal, underflow to zero
            compressed.significand = 0;
            compressed.exponent = 0;
            return compressed;
        }

        // denormalize
        significand |= tword::interger_bit_mask; // restore upper bit
        // uint64_t rounding = significand & 1;
        // significand >>= 1;
        // while (exponent < 0)
        // {
        //     exponent++;
        //     rounding = significand & 1;
        //     significand >>= 1;
        // }

        int denormal_shift = (-exponent) + 1;
        shift += denormal_shift;
        exponent = 0;

    }

    uint64_t rounding;
    uint64_t rounding_point_five;
    if (shift == 64) {
        rounding = significand;
        rounding_point_five = 1ull << (shift - 1);
        significand = 0;
    } else {
        uint64_t mask = (1ull << shift) - 1;
        rounding = significand & mask;
        rounding_point_five = 1ull << (shift - 1);
        significand = significand >> shift;
    }

    bool odd = (significand & 1) == 1;
    if (rounding > rounding_point_five || (rounding == rounding_point_five && odd)) {
        significand += 1;

        if (significand > T::significand_max) {
            exponent += 1;
            significand = 0;
        }
    }

    compressed.significand = significand;
    compressed.exponent = exponent;

    return compressed;
}

template tword soft_x87::expand(dword);
template tword soft_x87::expand(qword);

template dword soft_x87::compress(tword);
template qword soft_x87::compress(tword);