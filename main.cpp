#include <algorithm>
#include <array>
#include <cassert>
#include <stdio.h>
#include <stdint.h>
#include <cstring>
#include <functional>
#include <optional>
#include <random>

#include <fmt/format.h>


#pragma pack(1)
union tword {
    struct  {
        uint64_t significand : 64;
        uint16_t exponent    : 15;
        uint16_t sign        :  1;
    };
    struct {
        uint64_t hex_low;
        uint16_t hex_high;
    };

    // operator long double() {
    //     long double a;
    //     memcpy(&a, this, 10);
    //     return a;
    // }

    std::string to_string() {
        uint64_t fraction = significand & 0x7fff'ffff'ffff'ffff;
        uint64_t interger = significand >> 63;
        return fmt::format("{}_{:04x}_{}_{:016x}", (int)sign, (int)exponent, interger, fraction);
    }

    std::string to_hex() {
        return fmt::format("{:04x}{:016x}", hex_high, hex_low);
    }

    friend bool operator==(tword const& lhs, tword const& rhs)  {
        return (lhs.hex_low == rhs.hex_low) && (lhs.hex_high == rhs.hex_high);
    }
    friend bool operator!=(tword const& lhs, tword const& rhs) {
        return (lhs.hex_low != rhs.hex_low) || (lhs.hex_high != rhs.hex_high);
    }
};
static_assert(sizeof(tword) == 10, "wrong tword size");

struct qword {
    union
    {
        struct  {
            uint64_t significand : 52;
            uint64_t exponent    : 11;
            uint64_t sign        :  1;

        };
        uint64_t hex;
    };

    static constexpr int significand_width = 52;
    static constexpr int exponent_width = 11;

    qword() : hex(0) {}
    qword(double d) {
        std::memcpy(&hex, &d, sizeof(hex));
    }
    operator double() const {
        double d;
        std::memcpy(&d, &hex, sizeof(d));
        return d;
    }

    std::string to_string() {
        return fmt::format("{}_{:03x}_{:013x}", (int)sign, (int)exponent, (uint64_t)significand);
    }

    std::string to_hex() {
        return fmt::format("{:016x}", hex);
    }
};
static_assert(sizeof(qword) == 8, "wrong qword size");

struct dword {
    union {
        struct  {
            uint32_t significand : 23;
            uint32_t exponent    :  8;
            uint32_t sign        :  1;


        };
        uint32_t hex;
    };
    static constexpr int significand_width = 23;
    static constexpr int exponent_width = 8;

    dword() : hex(0) {}
    dword(float f) {
        std::memcpy(&hex, &f, sizeof(hex));
    }
    operator float() const {
        float f;
        std::memcpy(&f, &hex, sizeof(f));
        return f;
    }

    std::string to_string() {
        return fmt::format("{}_{:02x}_{:06x}", (int)sign, (int)exponent, (uint64_t)significand);
    }

    std::string to_hex() {
        return fmt::format("{:08x}", hex);
    }
};
static_assert(sizeof(dword) == 4, "wrong dword size");
#pragma pack()


// Generic interface for x87 fpu implementations
class x87 {
public:
    virtual void faddp(int st) = 0;
    virtual void fadd(int st) = 0;
    virtual void fadd(qword f) = 0;
    virtual void fadd(dword f) = 0;
    // virtual void fmulp(int st) = 0;
    // virtual void fmul(int st) = 0;
    // virtual void fmul(qword f) = 0;
    // virtual void fmul(dword f) = 0;

    void fadd()  { fadd(1);  }
    void faddp() { faddp(1); }
    // void fmul()  { fmul(1); }
    // void fmulp() { fmulp(1); }

    virtual void fld(tword f) = 0;
    virtual void fld(qword f) = 0;
    virtual void fld(dword f) = 0;
    virtual void fld(int st)  = 0;

    virtual tword fstp_t() = 0;
    virtual qword fstp_l() = 0;
    virtual dword fstp_s() = 0;
};

#define ST_ASM(str, val) do { assert(val < 8); switch (val) { \
    case 0: __asm__ volatile(str " %st(0)"); break; \
    case 1: __asm__ volatile(str " %st(1)"); break; \
    case 2: __asm__ volatile(str " %st(2)"); break; \
    case 3: __asm__ volatile(str " %st(3)"); break; \
    case 4: __asm__ volatile(str " %st(4)"); break; \
    case 5: __asm__ volatile(str " %st(5)"); break; \
    case 6: __asm__ volatile(str " %st(6)"); break; \
    case 7: __asm__ volatile(str " %st(7)"); break; \
} } while (false)

// This is a pass-though to the real x87 fpu.
// We take advantage of the fact that gcc/llvm won't emit any x87 code, unless we use the long double type.
// But this does mean there are a few restrictions. This class is not thread safe.
class hard_x87 : public x87 {
public:
    virtual void faddp(int st) { ST_ASM("faddp", st); };
    virtual void fadd(int st)  { ST_ASM("fadd", st); };;
    virtual void fadd(qword f) { __asm__ volatile ("faddl %0" :: "m"(f)); };
    virtual void fadd(dword f) { __asm__ volatile ("fadds %0" :: "m"(f)); };
    // virtual void fmulp(int st) { ST_ASM("fmulp", st); };
    // virtual void fmul(int st)  { ST_ASM("fmul", st); };
    // virtual void fmul(qword f) { __asm__ volatile ("fmull %0" :: "m"(f)); };
    // virtual void fmul(dword f) { __asm__ volatile ("fmuls %0" :: "m"(f)); };

    virtual void fld(tword f)  { __asm__ volatile ("fldt %0" :: "m"(f)); };
    virtual void fld(qword f)  { __asm__ volatile ("fldl %0" :: "m"(f)); };
    virtual void fld(dword f)  { __asm__ volatile ("flds %0" :: "m"(f)); };
    virtual void fld(int st)   { ST_ASM("fld", st); };

    void fadd()  { fadd(1);  }
    void faddp() { faddp(1); }
    // void fmul()  { fmul(1); }
    // void fmulp() { fmulp(1); }

    virtual tword fstp_t() { tword ret; __asm__ ("fstpt %0" : "=&m"(ret)); return ret; };
    virtual qword fstp_l() { qword ret; __asm__ ("fstpl %0" : "=&m"(ret)); return ret; };
    virtual dword fstp_s() { dword ret; __asm__ ("fstps %0" : "=&m"(ret)); return ret; };
};

class soft_x87 : public x87 {
private:
    std::array<tword, 8> stack;
    int top = 0;

    tword& ST(int i) { return stack[(top + i) & 7]; }
    tword POP() { tword val = stack[top]; top = (top + 1) & 7; return val; }
    void PUSH() { top = (top - 1) & 7; }

    template<class T>
    tword expand(T f) {
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
    T compress(tword f) {
        static_assert(std::is_same<T, qword>::value || std::is_same<T, dword>::value, "Unsupported type");

        T compressed;
        compressed.sign = f.sign;

        int shift = 63 - compressed.significand_width;
        compressed.significand = (f.significand & 0x7FFFFFFFFFFFFFFF) >> shift;

        // Handle pre-existing infinity
        int exponent_max = (1 << compressed.exponent_width) - 1;
        if (f.exponent == 0x7fff && f.significand == 0) {
            compressed.significand = 0;
            compressed.exponent = exponent_max;
            return compressed;
        }

        // Handle NAN
        if (f.exponent == 0x7fff) {
            compressed.exponent = exponent_max;
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
        if (exponent < 0) {
            if (exponent < -compressed.significand_width) {
                // Too big for denormal, underflow to zero
                compressed.significand = 0;
                compressed.exponent = 0;
                return compressed;
            }

            // denormalize
            uint64_t significand = compressed.significand | (1ULL << compressed.significand_width);
            while (exponent > 0)
            {
                exponent++;
                significand >>= 1;
            }
            compressed.significand = significand;
        }

        compressed.exponent = exponent;

        return compressed;
    }

    void add(tword& a, tword &b, bool subtract = false) {
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

public:
    virtual void fadd(int st) { add(ST(0), ST(st)); }
    virtual void faddp(int st) { tword &a = ST(st); tword b = POP(); add(a, b); }
    virtual void fadd(tword b) { add(ST(0), b); }
    virtual void fadd(qword f) { tword b = expand(f); add(ST(0), b); }
    virtual void fadd(dword f) { tword b = expand(f); add(ST(0), b); }

    virtual void fld(tword f)  { PUSH(); ST(0) = f; };
    virtual void fld(qword f)  { PUSH(); ST(0) = expand(f); };
    virtual void fld(dword f)  { PUSH(); ST(0) = expand(f);  };
    virtual void fld(int st)   { PUSH(); ST(0) = ST(st); };

    void fadd()  { fadd(1);  }
    void faddp() { faddp(1); }

    virtual tword fstp_t() { return POP(); };
    virtual qword fstp_l() { return compress<qword>(POP()); };
    virtual dword fstp_s() { return compress<dword>(POP()); };
};

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