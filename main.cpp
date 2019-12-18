#include <algorithm>
#include <array>
#include <cassert>
#include <stdio.h>

#include <cstring>
#include <functional>
#include <optional>
#include <random>

#include <fmt/format.h>

#include "float_types.h"
#include "real_x87.h"
#include "soft_x87.h"

// inline long double fadd(long double a, long double b) {
//     long double ret;
//     __asm__ volatile ("fldt %0" :: "m"(a));
//     __asm__ volatile ("fldt %0" :: "m"(b));
//     __asm__ volatile ("faddp");
//     __asm__ volatile ("fstpt %0" : "=m"(ret));
//     return ret;
// }

// Iterates over a sequence defined by a generator
template<class T>
struct sequenceIterator {
    using iterator_category = std::input_iterator_tag;
    using value_type = T;
    using difference_type = void;
    using pointer = T*;
    using reference = T&;

    sequenceIterator() {}
    sequenceIterator(std::function<std::optional<T>()> gen) : generator(gen), current(gen()) { }
    T operator*() const { return *current; }
    T* operator->() const { return current.operator->(); }
    sequenceIterator& operator++() {
        current = generator();
        return *this;
    }
    friend bool operator==(sequenceIterator const& lhs, sequenceIterator const& rhs) {
        return lhs.current == rhs.current;
    }
    friend bool operator!=(sequenceIterator const& lhs, sequenceIterator const& rhs) {
        return !(lhs==rhs);
    }

    struct postinc_return {
        postinc_return(T value) : value(value) {}
        T operator*() { return value; }
    private:
        T value;
    };
    postinc_return operator++(int) {
        postinc_return ret(*this);
        ++this;
        return ret;
    }

protected:
    std::optional<T> current;
    std::function<std::optional<T>()> generator;
};


template<class T, size_t length>
struct Sequence {
    sequenceIterator<T> begin() {
        auto inner_generator = generator;
        size_t count = 0;

        auto boundedGenerator = [inner_generator,count] () mutable -> std::optional<T> {
            if (count++ < length) {
                return inner_generator();
            }
            return std::nullopt;
        };

        return sequenceIterator<T>(boundedGenerator);
    }

    sequenceIterator<T> end() {
        return sequenceIterator<T>();
    }

protected:
    std::function<T()> generator;
};

template<class T, size_t length, uint64_t seed=0>
struct UniformSequence : public Sequence<T, length> {
    UniformSequence() {
        std::mt19937_64 rng;
        rng.seed(seed);

        this->generator = [rng] () mutable -> T {
            T result;
            ssize_t remaining_bytes = sizeof(result);
            char *data = reinterpret_cast<char*>(&result);

            while (remaining_bytes > 0) {
                uint64_t num = rng();
                std::memcpy(data, &num, std::min<ssize_t>(remaining_bytes, sizeof(num)));
                remaining_bytes -= sizeof(num);
                data += sizeof(num);
            }
            return result;
        };
    }
};

template<class T, size_t length, uint64_t seed=0>
struct FilteredSequence : public UniformSequence<T, length, seed> {
    FilteredSequence(std::function<bool(T)> filter) : UniformSequence<T, length, seed>() {
        auto original_generator = this->generator;

        this->generator = [original_generator, filter] () -> T {
            while (true) {
                T value = original_generator();
                if (filter(value))
                    return value;
            }
        };
    }
};

template<class T, size_t length, uint64_t seed=0>
struct TransformedSequence : public UniformSequence<T, length, seed> {
    TransformedSequence(std::function<T(T)> transformation) : UniformSequence<T, length, seed>() {
        auto original_generator = this->generator;

        this->generator = [original_generator, transformation] () -> T {
            return transformation(original_generator());
        };
    }
};

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
    auto fpu = soft_x87();
    auto soft = soft_x87();
    auto hard = hard_x87();

    fpu.fld(qword(1.0));
    fpu.fld(qword(1.2));
    fpu.faddp();
    qword result = fpu.fstp_l();

    // qword d(1.0);
    // soft.fld(d);
    // hard.fld(d);
    // auto s = soft.fstp_t();
    // auto h = hard.fstp_t();

    hard.fld(qword(1.0));
    hard.fld(qword(1.2));
    hard.faddp();
    qword hard_result = hard.fstp_l();

    conversion_tests(soft, hard);

   //fmt::print("{} {} {}\n", hard_result.to_string(), result.to_string(), (double)result);
}