#pragma once

#include "x87.h"

class soft_x87 : public x87 {
private:
    std::array<tword, 8> stack;
    int top = 0;

    tword& ST(int i) { return stack[(top + i) & 7]; }
    tword POP() { tword val = stack[top]; top = (top + 1) & 7; return val; }
    void PUSH() { top = (top - 1) & 7; }

    template<class T>
    tword expand(T f); // Expands 32bit/64 bit floats to 80bit

    template<class T>
    T compress(tword f); // Compresses 80bit floats to 32bit/64bit

    tword convert(int64_t i); // Converts signed ints to 80bit float

    void add(tword& a, tword &b, bool subtract = false);

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

    virtual void fild(int16_t i) { PUSH(); ST(0) = convert(int64_t(i)); };
    virtual void fild(int32_t i) { PUSH(); ST(0) = convert(int64_t(i)); };
    virtual void fild(int64_t i) { PUSH(); ST(0) = convert(int64_t(i)); };

    void fadd()  { fadd(1);  }
    void faddp() { faddp(1); }

    virtual tword fstp_t() { return POP(); };
    virtual qword fstp_l() { return compress<qword>(POP()); };
    virtual dword fstp_s() { return compress<dword>(POP()); };
};