#pragma once

#include <stdint.h>
#include <string>
#include <fmt/format.h>

template<size_t sig_size, size_t exp_size, bool has_int_bit = false, bool has_sign_bit = true>
struct soft_float {
    static constexpr size_t bits = sig_size + exp_size + size_t(has_sign_bit);
    static constexpr size_t bytes = bits / 8;

    static_assert(bits == bytes * 8, "total bits should be multiple of 8");

    static constexpr int significand_width = sig_size;
    static constexpr uint64_t significand_max = (1ull << sig_size) - 1;
    static constexpr uint64_t interger_bit_mask = has_int_bit ? (1ull << (significand_width - 1)) : 0;


    static constexpr int exponent_width = exp_size;
    static constexpr int exponent_max = (1 << exp_size) - 1;
    static constexpr int exponent_bias = (1 << (exp_size - 1)) - 1;

    #pragma pack(1)
    struct {
        uint64_t significand : sig_size;
        uint16_t exponent    : exp_size;
        uint16_t sign        : 1;
    };
    #pragma pack()

    soft_float() { memset(mem(), 0, bytes);  }
    soft_float(unsigned sign, unsigned exp, uint64_t sig) : sign(sign), exponent(exp), significand(sig) {}

    soft_float(double d) { // rough conversion from double. Don't trust it to be accurate
        soft_float<52, 11> temp;
        memcpy(temp.mem(), &d, sizeof(temp));

        int shift = temp.significand_width - significand_width;

        significand = temp.significand >> shift;
        if (has_int_bit)
            significand = (significand >> 1) | interger_bit_mask;

        exponent = (temp.exponent - temp.exponent_bias) + exponent_bias;
        if (temp.exponent == 0x7ff) {
            exponent = exponent_max;
        }

        sign = temp.sign;
     }

    std::string to_string() {
        int sig_pad = (significand_width + 3) / 4;
        int exp_pad = (exponent_width + 3) / 4;

        std::string str = fmt::format("{0}_{1:0{2}x}", (int)sign, (int)exponent, exp_pad);
        if (has_int_bit) {
            uint64_t fraction = significand & ~interger_bit_mask;
            uint64_t interger = significand >> (significand_width - 1);

            return fmt::format("{0}_{1}_{2:0{3}x}", str, interger, fraction, sig_pad);
        } else {
            return fmt::format("{0}_{1:0{2}x}", str, (uint64_t)significand, sig_pad);
        }
    }

    friend bool operator==(soft_float const& lhs, soft_float const& rhs)  {
        return memcmp(lhs.mem(), rhs.mem(), bytes) == 0;
    }

    friend bool operator!=(soft_float const& lhs, soft_float const& rhs)  {
        return memcmp(lhs.mem(), rhs.mem(), bytes) != 0;
    }

protected:
    const void* mem() const {
        return reinterpret_cast<const void*>(this);
    }

    void* mem() {
        return reinterpret_cast<void*>(this);
    }
};

using tword = soft_float<64, 15, true>;
using qword = soft_float<52, 11>;
using dword = soft_float<23, 8>;

static_assert(sizeof(tword) == 10, "tword wrong size");
static_assert(sizeof(qword) == 8,  "qword wrong size");
static_assert(sizeof(dword) == 4,  "dword wrong size");