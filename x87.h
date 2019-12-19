#pragma once

#include "float_types.h"

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

    template<typename T>
    T fstp() {
        if constexpr (std::is_same<tword, T>::value)
            return fstp_t();
        if constexpr (std::is_same<qword, T>::value)
            return fstp_l();
        if constexpr (std::is_same<dword, T>::value)
            return fstp_s();
    }
};