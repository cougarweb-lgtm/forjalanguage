/* assinaturas.c - gerado por classificar_abi.py (0.1.0). Nao editar a mao. */
#include <stdarg.h>

typedef struct S01 { signed char f0; } S01;
typedef struct S02 { short f0; } S02;
typedef struct S03 { signed char f0; signed char f1; signed char f2; } S03;
typedef struct S04 { int f0; } S04;
typedef struct S05 { float f0; } S05;
typedef struct S06 { signed char f0; signed char f1; signed char f2; signed char f3; signed char f4; } S06;
typedef struct S07 { short f0; short f1; short f2; } S07;
typedef struct S08 { int f0; int f1; } S08;
typedef struct S09 { float f0; float f1; } S09;
typedef struct S10 { int f0; float f1; } S10;
typedef struct S11 { float f0; int f1; } S11;
typedef struct S12 { long f0; signed char f1; } S12;
typedef struct S13 { int f0; int f1; float f2; } S13;
typedef struct S14 { float f0; float f1; float f2; } S14;
typedef struct S15 { long f0; double f1; } S15;
typedef struct S16 { double f0; long f1; } S16;
typedef struct S17 { float f0; float f1; float f2; float f3; } S17;
typedef struct S18 { long f0; long f1; } S18;
typedef struct S19 { int f0; int f1; int f2; int f3; int f4; } S19;
typedef struct S20 { long f0; double f1; long f2; } S20;
typedef struct S21 { long f0; long f1; long f2; long f3; } S21;
typedef struct S22 { double f0; int f1; } S22;
typedef struct S23 { signed char f0; double f1; } S23;

__attribute__((noinline)) signed char id_i8(signed char a0) { return (signed char)a0; }
__attribute__((noinline)) short id_i16(short a0) { return (short)a0; }
__attribute__((noinline)) int id_i32(int a0) { return (int)a0; }
__attribute__((noinline)) long id_i64(long a0) { return (long)a0; }
__attribute__((noinline)) float id_f32(float a0) { return (float)a0; }
__attribute__((noinline)) double id_f64(double a0) { return (double)a0; }
__attribute__((noinline)) void * id_ptr(void * a0) { return (void *)a0; }
__attribute__((noinline)) double mista_00(int a0, double a1) { return (double)a1; }
__attribute__((noinline)) int mista_01(double a0, int a1) { return (int)a1; }
__attribute__((noinline)) float mista_02(long a0, float a1) { return (float)a1; }
__attribute__((noinline)) long mista_03(long a0, long a1, long a2, long a3, long a4, long a5) { return (long)a5; }
__attribute__((noinline)) double mista_04(double a0, double a1, double a2, double a3, double a4, double a5, double a6, double a7) { return (double)a7; }
__attribute__((noinline)) long mista_05(long a0, long a1, long a2, long a3, long a4, long a5, long a6) { return (long)a6; }
__attribute__((noinline)) double mista_06(double a0, double a1, double a2, double a3, double a4, double a5, double a6, double a7, double a8) { return (double)a8; }
__attribute__((noinline)) int mista_07(signed char a0, float a1, short a2, double a3, int a4, float a5) { return (int)a5; }
__attribute__((noinline)) void * mista_08(void * a0, void * a1) { return (void *)a1; }
__attribute__((noinline)) int mista_09(int a0, int a1, int a2) { return (int)a2; }
__attribute__((noinline)) S01 id_S01(S01 a0) { return a0; }
__attribute__((noinline)) S02 id_S02(S02 a0) { return a0; }
__attribute__((noinline)) S03 id_S03(S03 a0) { return a0; }
__attribute__((noinline)) S04 id_S04(S04 a0) { return a0; }
__attribute__((noinline)) S05 id_S05(S05 a0) { return a0; }
__attribute__((noinline)) S06 id_S06(S06 a0) { return a0; }
__attribute__((noinline)) S07 id_S07(S07 a0) { return a0; }
__attribute__((noinline)) S08 id_S08(S08 a0) { return a0; }
__attribute__((noinline)) S09 id_S09(S09 a0) { return a0; }
__attribute__((noinline)) S10 id_S10(S10 a0) { return a0; }
__attribute__((noinline)) S11 id_S11(S11 a0) { return a0; }
__attribute__((noinline)) S12 id_S12(S12 a0) { return a0; }
__attribute__((noinline)) S13 id_S13(S13 a0) { return a0; }
__attribute__((noinline)) S14 id_S14(S14 a0) { return a0; }
__attribute__((noinline)) S15 id_S15(S15 a0) { return a0; }
__attribute__((noinline)) S16 id_S16(S16 a0) { return a0; }
__attribute__((noinline)) S17 id_S17(S17 a0) { return a0; }
__attribute__((noinline)) S18 id_S18(S18 a0) { return a0; }
__attribute__((noinline)) S19 id_S19(S19 a0) { return a0; }
__attribute__((noinline)) S20 id_S20(S20 a0) { return a0; }
__attribute__((noinline)) S21 id_S21(S21 a0) { return a0; }
__attribute__((noinline)) S22 id_S22(S22 a0) { return a0; }
__attribute__((noinline)) S23 id_S23(S23 a0) { return a0; }
__attribute__((noinline)) long mix_S10(int a0, S10 a1, double a2) { (void)a1; return (long)a0 + (long)a2 + (long)a1.f0; }
__attribute__((noinline)) long mix_S13(int a0, S13 a1, double a2) { (void)a1; return (long)a0 + (long)a2 + (long)a1.f0; }
__attribute__((noinline)) long mix_S15(int a0, S15 a1, double a2) { (void)a1; return (long)a0 + (long)a2 + (long)a1.f0; }
__attribute__((noinline)) long mix_S20(int a0, S20 a1, double a2) { (void)a1; return (long)a0 + (long)a2 + (long)a1.f0; }
int var_i(int a0, ...) { va_list ap; va_start(ap, a0); va_arg(ap, int); va_end(ap); return (int)a0; }
double var_d(double a0, ...) { va_list ap; va_start(ap, a0); va_arg(ap, int); va_end(ap); return (double)a0; }
int var_p(void * a0, ...) { va_list ap; va_start(ap, a0); va_arg(ap, int); va_end(ap); return (int)a0; }
long var_l(long a0, long a1, ...) { va_list ap; va_start(ap, a1); va_arg(ap, int); va_end(ap); return (long)a0; }
S08 make_S08(int a0, int a1) { S08 r; r.f0 = a0; r.f1 = a1; return r; }
S09 make_S09(float a0, float a1) { S09 r; r.f0 = a0; r.f1 = a1; return r; }
S10 make_S10(int a0, float a1) { S10 r; r.f0 = a0; r.f1 = a1; return r; }
S11 make_S11(float a0, int a1) { S11 r; r.f0 = a0; r.f1 = a1; return r; }
S12 make_S12(long a0, signed char a1) { S12 r; r.f0 = a0; r.f1 = a1; return r; }
S13 make_S13(int a0, int a1, float a2) { S13 r; r.f0 = a0; r.f1 = a1; r.f2 = a2; return r; }
S14 make_S14(float a0, float a1, float a2) { S14 r; r.f0 = a0; r.f1 = a1; r.f2 = a2; return r; }
S15 make_S15(long a0, double a1) { S15 r; r.f0 = a0; r.f1 = a1; return r; }
S16 make_S16(double a0, long a1) { S16 r; r.f0 = a0; r.f1 = a1; return r; }
S17 make_S17(float a0, float a1, float a2, float a3) { S17 r; r.f0 = a0; r.f1 = a1; r.f2 = a2; r.f3 = a3; return r; }
S19 make_S19(int a0, int a1, int a2, int a3, int a4) { S19 r; r.f0 = a0; r.f1 = a1; r.f2 = a2; r.f3 = a3; r.f4 = a4; return r; }
S21 make_S21(long a0, long a1, long a2, long a3) { S21 r; r.f0 = a0; r.f1 = a1; r.f2 = a2; r.f3 = a3; return r; }
