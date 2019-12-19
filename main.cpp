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
        return f.exponent != T::exponent_max && f.exponent != 0;
    });

    fmt::print("loading {}bit \"happy\" floats...\n", T::bits);
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

    fmt::print("loading {}bit denormal floats...\n", T::bits);
    // // Denormal floats to 80bit
    for (auto val : denormal_floats) {
        load_both(val);
    }



    // infinities/NaNs/zeros to 80bit
    {
        fmt::print("loading {}bit infinities...\n", T::bits);
        T val;

        // positive infinity
        val.sign = 0;
        val.exponent = (1 << T::exponent_width) - 1;
        val.significand = 0;
        load_both(val);

        // negative infinity
        val.sign = 1;
        load_both(val);

        fmt::print("loading {}bit zeros...\n", T::bits);

        // negative zero
        val.exponent = 0;
        load_both(val);

        // positive zero
        val.sign = 0;
        load_both(val);

        fmt::print("loading {}bit NaNs...\n", T::bits);
        TransformedSequence<T, 1'000'000> NaNs([] (T f) {
            f.exponent = T::exponent_max; // force exponent to zero
            return f;
        });

        for (auto val : NaNs) {
            load_both(val);
        }
    }

    auto store_both = [&] (tword val) {
        fpu_a.fld(val);
        fpu_b.fld(val);

        T a = fpu_a.fstp<T>();
        T b = fpu_b.fstp<T>();
        if(a != b) {
            fmt::print("{} resulted in {} and {}\n", val.to_string(), a.to_string(), b.to_string());
        }
    };

    fmt::print("storing \"happy\" floats to {}bit...\n", T::bits);
    {
        FilteredSequence<tword, 10'000'000> happy_long_floats([] (tword f) {
            bool is_denormal = (f.significand & tword::interger_bit_mask) == 0; // already denormal
            bool fits_in_T   = ((tword::exponent_bias - f.exponent) + T::exponent_bias) < 0; // will become denormal
            bool is_real     = f.exponent != tword::exponent_max; // not infinity or nand
            return is_real && !is_denormal; // FIXME: && fits_in_T;
        });

        // happy path for 80bit to 32bit/64bit conversions (includes rounding)
        for (auto val : happy_long_floats) {
        store_both(val);
        }
    }


    // TODO: proper rounding test


    store_both(tword(1, 0x3f69, 0xcc53702c050d3513));
    store_both(tword(0, 0x3bff, 0x8e65bd8630709000));
    store_both(tword(0, 0x3f80, 0xffffff1fd1ad2bdd));
    store_both(tword(0, 0x3f80, 0xffffff8000000000));
    store_both(tword(0, 0x3f80, 0xfffffe8000000000));
    store_both(tword(0, 0x3c00, 0x801ceee9d3ec8800));
    store_both(tword(0, 0x3c00, 0x801ceee9d3ec8801));
    store_both(tword(0, 0x3c00, 0x801ceee9d3ec8c00));

    fmt::print("storing floats requiring denormalization to {}bit...\n", T::bits);
    {
        TransformedSequence<tword, 10'000'000> denormalable_floats([] (tword f) {
            constexpr int min_exponent = -T::exponent_bias - T::significand_width;
            constexpr int max_exponent = -T::exponent_bias;
            constexpr int denormal_exponent_range = (max_exponent - min_exponent) + 1;

            // Take the tword exponent and force it into T's denormal exponent range with modulus
            f.exponent = (tword::exponent_bias + min_exponent) + (f.exponent % denormal_exponent_range);

            // Force to be an interger
            f.significand |= tword::interger_bit_mask;

            return f;
        });

        // conversions which require denormalization
        for (auto val : denormalable_floats) {
            store_both(val);
        }
    }



    // infinities/NaNs/zeros conversions
    {
        fmt::print("storing zeros to {}bit...\n", T::bits);
        store_both(tword(0, 0, 0));
        store_both(tword(1, 0, 0));

        fmt::print("storing infinities to {}bit...\n", T::bits);
        store_both(tword(0, 0x7fff, 0x8000'0000'0000'0000));
        store_both(tword(1, 0x7fff, 0x8000'0000'0000'0000));

        fmt::print("storing NaNs to {}bit...\n", T::bits);
        TransformedSequence<tword, 1'000'000> NaNs([] (tword f) {
            f.exponent = tword::exponent_max; // force exponent to zero
            f.significand |= tword::interger_bit_mask;
            return f;
        });

        for (auto val : NaNs) {
            store_both(val);
        }

        // large numbers

        // small numbers
    }

    // weird unsupported 80bit float encodings
        // peusdo denormals work
        // others generate invalid operand exceptions
}

void conversion_tests(x87 &fpu_a, x87 &fpu_b) {
    UniformSequence<tword, 4'000'000> random_twords; // 4 million 80bit floats

    // Quick test to make sure loading and storing of 80bit floats works.
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