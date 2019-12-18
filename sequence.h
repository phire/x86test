#pragma once

#include <functional>
#include <optional>
#include <random>

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
