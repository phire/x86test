#pragma once

#include "x87.h"

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

    virtual void fild(int16_t i) { __asm__ volatile ("filds %0" :: "m"(i)); };
    virtual void fild(int32_t i) { __asm__ volatile ("fildl %0" :: "m"(i)); };
    virtual void fild(int64_t i) { __asm__ volatile ("fildq %0" :: "m"(i)); };

    void fadd()  { fadd(1);  }
    void faddp() { faddp(1); }
    // void fmul()  { fmul(1); }
    // void fmulp() { fmulp(1); }

    virtual tword fstp_t() { tword ret; __asm__ ("fstpt %0" : "=&m"(ret)); return ret; };
    virtual qword fstp_l() { qword ret; __asm__ ("fstpl %0" : "=&m"(ret)); return ret; };
    virtual dword fstp_s() { dword ret; __asm__ ("fstps %0" : "=&m"(ret)); return ret; };

    virtual uint16_t fstcw() { uint16_t cw; __asm__ ("fstcw %0" : "=&m"(cw)); return cw; }
    virtual void fldcw(uint16_t cw) {  __asm__ volatile ("fldcw %0" :: "m"(cw)); }
};