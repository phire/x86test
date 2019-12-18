#include <algorithm>
#include <array>
#include <cassert>
#include <stdio.h>

#include <cstring>

#include <fmt/format.h>

#include "float_types.h"
#include "real_x87.h"
#include "soft_x87.h"
#include "sequence.h"


// We run these tests twice, for 32 and 64bit floats
template<typename T>
void conversion_tests_inner(x87 &fpu_a, x87 &fpu_b) {
    constexpr int bits = sizeof(T) * 8;
    constexpr int exponent_max = (1 << T::exponent_width) - 1;

    auto load_both = [&] (T val) {
        fpu_a.fld(val);
        fpu_b.fld(val);


        tword a = fpu_a.fstp_t();
        tword b = fpu_b.fstp_t();
        if(a != b) {
            fmt::print("{} resulted in {} and {}\n", val.to_string(), a.to_string(), b.to_string());
        }
    };

    // 4 million happy floats
    // Note: zero is not a happy float.
    FilteredSequence<T, 4'000'000> happy_floats([] (T f) {
        return f.exponent != exponent_max && f.exponent != 0;
    });

    fmt::print("loading {}bit \"happy\" floats...\n", bits);
    // Happy path for 32bit/64bit to 80bit floats
    for (auto val : happy_floats) {
        load_both(val);
    }

    TransformedSequence<T, 4'000'000> denormal_floats([] (T f) {
        // apply the implicit interger bit
        uint64_t significand = f.significand | (1ULL << T::significand_width);

        // We want a uniform distribution of encoded exponents
        // So we take the original exponent, modulate it into the correct range
        // and shift the significand by that much
        f.significand = significand >> (f.exponent % T::significand_width);

         // Then zero out the exponent
        f.exponent = 0;
        return f;
    });

    fmt::print("loading {}bit denormal floats...\n", bits);
    // // Denormal floats to 80bit
    for (auto val : denormal_floats) {
        load_both(val);
    }

    // infinities/NANs/zeros to 80bit
    {
        fmt::print("loading {}bit infinities...\n", bits);
        T val;

        // positive infinity
        val.sign = 0;
        val.exponent = (1 << T::exponent_width) - 1;
        val.significand = 0;
        load_both(val);

        // negative infinity
        val.sign = 1;
        load_both(val);

        fmt::print("loading {}bit zeros...\n", bits);

        // negative zero
        val.exponent = 0;
        load_both(val);

        // positive zero
        val.sign = 0;
        load_both(val);

        fmt::print("loading {}bit NaNs...\n", bits);

        TransformedSequence<T, 1'000'000> NaNs([] (T f) {
            f.exponent = exponent_max; // force exponent to zero
            return f;
        });

        for (auto val : NaNs) {
            load_both(val);
        }
    }

    // happy path for 80bit to 32bit/64bit conversions (includes rounding)

    // conversions which require denormalization

    // infinities/NANs/zeros conversions

    // weird unsupported 80bit float encodings
        // peusdo denormals work
        // others generate invalid operand exceptions
}

void conversion_tests(x87 &fpu_a, x87 &fpu_b) {
    UniformSequence<tword, 4'000'000> random_twords; // 4 million 80bit floats

    fmt::print("loading 80bit floats...\n");
    for (tword f : random_twords) {
        fpu_a.fld(f);
        fpu_b.fld(f);

        tword result_a = fpu_a.fstp_t();
        tword result_b = fpu_b.fstp_t();
        if(result_a != result_b) {
            fmt::print("{} resulted in {} and {}\n", f.to_string(), result_a.to_string(), result_b.to_string());
        }
    }

    conversion_tests_inner<dword>(fpu_a, fpu_b);
    conversion_tests_inner<qword>(fpu_a, fpu_b);
}

int main() {
    auto soft = soft_x87();
    auto hard = hard_x87();

    conversion_tests(soft, hard);
}