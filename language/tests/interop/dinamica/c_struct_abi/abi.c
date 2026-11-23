/* abi.c - gerado por gerar_abi.py 0.1.0 (semente 1, 40 assinaturas); nao edite a mao.
 * Versao: 0.1.0 (primeira versao, M3 P19i-a). Verificacao: python -I gerar_abi.py e depois o verificar_interop (caso c_struct_abi).
 */
#include <stdint.h>

#define ABI __declspec(dllexport)

typedef struct {
    int8_t f0;
} Ab0;

typedef struct {
    uint8_t f0;
    uint8_t f1;
    uint8_t f2;
} Ab1;

typedef struct {
    int16_t f0;
    int16_t f1;
} Ab2;

typedef struct {
    int32_t f0;
    float f1;
} Ab3;

typedef struct {
    double f0;
} Ab4;

typedef struct {
    int32_t f0;
    int32_t f1;
    int32_t f2;
} Ab5;

typedef struct {
    double f0;
    int64_t f1;
} Ab6;

typedef struct {
    double f0;
    uint8_t f1;
} Ab7;

typedef struct {
    uint8_t f0;
    uint64_t f1;
    uint64_t f2;
} Ab8;

typedef struct {
    int64_t f0;
    uint16_t f1;
    uint8_t f2;
} Ab9;

ABI uint8_t abi_f0(Ab6 p0, Ab0 p1, int32_t p2, uint16_t p3)
{
    double s = 0.0;
    s += (double)p0.f0 * 1;
    s += (double)p0.f1 * 2;
    s += (double)p1.f0 * 3;
    s += (double)p2 * 4;
    s += (double)p3 * 5;
    return (uint8_t)(int64_t)(s);
}

ABI int64_t abi_f1(Ab0 p0, int8_t p1, int64_t p2)
{
    double s = 0.0;
    s += (double)p0.f0 * 1;
    s += (double)p1 * 2;
    s += (double)p2 * 3;
    return (int64_t)(int64_t)(s);
}

ABI float abi_f2(Ab3 p0, uint64_t p1, uint32_t p2, Ab3 p3, int32_t p4, int64_t p5)
{
    double s = 0.0;
    s += (double)p0.f0 * 1;
    s += (double)p0.f1 * 2;
    s += (double)p1 * 3;
    s += (double)p2 * 4;
    s += (double)p3.f0 * 5;
    s += (double)p3.f1 * 6;
    s += (double)p4 * 7;
    s += (double)p5 * 8;
    return (float)s;
}

ABI uint64_t abi_f3(Ab4 p0, Ab5 p1, float p2, int64_t p3, uint16_t p4, Ab9 p5)
{
    double s = 0.0;
    s += (double)p0.f0 * 1;
    s += (double)p1.f0 * 2;
    s += (double)p1.f1 * 3;
    s += (double)p1.f2 * 4;
    s += (double)p2 * 5;
    s += (double)p3 * 6;
    s += (double)p4 * 7;
    s += (double)p5.f0 * 8;
    s += (double)p5.f1 * 9;
    s += (double)p5.f2 * 10;
    return (uint64_t)(int64_t)(s);
}

ABI Ab2 abi_f4(Ab0 p0, Ab6 p1, Ab2 p2, Ab5 p3, Ab8 p4)
{
    double s = 0.0;
    s += (double)p0.f0 * 1;
    s += (double)p1.f0 * 2;
    s += (double)p1.f1 * 3;
    s += (double)p2.f0 * 4;
    s += (double)p2.f1 * 5;
    s += (double)p3.f0 * 6;
    s += (double)p3.f1 * 7;
    s += (double)p3.f2 * 8;
    s += (double)p4.f0 * 9;
    s += (double)p4.f1 * 10;
    s += (double)p4.f2 * 11;
    Ab2 r;
    r.f0 = (int16_t)(int64_t)(s + 0);
    r.f1 = (int16_t)(int64_t)(s + 1);
    return r;
}

ABI int16_t abi_f5(uint32_t p0, Ab0 p1, Ab4 p2, double p3, int64_t p4)
{
    double s = 0.0;
    s += (double)p0 * 1;
    s += (double)p1.f0 * 2;
    s += (double)p2.f0 * 3;
    s += (double)p3 * 4;
    s += (double)p4 * 5;
    return (int16_t)(int64_t)(s);
}

ABI Ab4 abi_f6(Ab0 p0, float p1, float p2, Ab8 p3, Ab9 p4)
{
    double s = 0.0;
    s += (double)p0.f0 * 1;
    s += (double)p1 * 2;
    s += (double)p2 * 3;
    s += (double)p3.f0 * 4;
    s += (double)p3.f1 * 5;
    s += (double)p3.f2 * 6;
    s += (double)p4.f0 * 7;
    s += (double)p4.f1 * 8;
    s += (double)p4.f2 * 9;
    Ab4 r;
    r.f0 = (double)s + 0;
    return r;
}

ABI uint16_t abi_f7(int8_t p0, Ab8 p1, float p2, uint16_t p3, Ab0 p4, Ab5 p5)
{
    double s = 0.0;
    s += (double)p0 * 1;
    s += (double)p1.f0 * 2;
    s += (double)p1.f1 * 3;
    s += (double)p1.f2 * 4;
    s += (double)p2 * 5;
    s += (double)p3 * 6;
    s += (double)p4.f0 * 7;
    s += (double)p5.f0 * 8;
    s += (double)p5.f1 * 9;
    s += (double)p5.f2 * 10;
    return (uint16_t)(int64_t)(s);
}

ABI int16_t abi_f8(Ab5 p0, Ab0 p1, double p2, uint32_t p3, Ab0 p4)
{
    double s = 0.0;
    s += (double)p0.f0 * 1;
    s += (double)p0.f1 * 2;
    s += (double)p0.f2 * 3;
    s += (double)p1.f0 * 4;
    s += (double)p2 * 5;
    s += (double)p3 * 6;
    s += (double)p4.f0 * 7;
    return (int16_t)(int64_t)(s);
}

ABI Ab1 abi_f9(uint8_t p0, int32_t p1, Ab1 p2, Ab0 p3, Ab4 p4)
{
    double s = 0.0;
    s += (double)p0 * 1;
    s += (double)p1 * 2;
    s += (double)p2.f0 * 3;
    s += (double)p2.f1 * 4;
    s += (double)p2.f2 * 5;
    s += (double)p3.f0 * 6;
    s += (double)p4.f0 * 7;
    Ab1 r;
    r.f0 = (uint8_t)(int64_t)(s + 0);
    r.f1 = (uint8_t)(int64_t)(s + 1);
    r.f2 = (uint8_t)(int64_t)(s + 2);
    return r;
}

ABI Ab1 abi_f10(Ab4 p0, Ab2 p1, Ab2 p2, int32_t p3, Ab5 p4)
{
    double s = 0.0;
    s += (double)p0.f0 * 1;
    s += (double)p1.f0 * 2;
    s += (double)p1.f1 * 3;
    s += (double)p2.f0 * 4;
    s += (double)p2.f1 * 5;
    s += (double)p3 * 6;
    s += (double)p4.f0 * 7;
    s += (double)p4.f1 * 8;
    s += (double)p4.f2 * 9;
    Ab1 r;
    r.f0 = (uint8_t)(int64_t)(s + 0);
    r.f1 = (uint8_t)(int64_t)(s + 1);
    r.f2 = (uint8_t)(int64_t)(s + 2);
    return r;
}

ABI Ab3 abi_f11(Ab5 p0)
{
    double s = 0.0;
    s += (double)p0.f0 * 1;
    s += (double)p0.f1 * 2;
    s += (double)p0.f2 * 3;
    Ab3 r;
    r.f0 = (int32_t)(int64_t)(s + 0);
    r.f1 = (float)s + 1;
    return r;
}

ABI Ab6 abi_f12(Ab8 p0, double p1, Ab0 p2)
{
    double s = 0.0;
    s += (double)p0.f0 * 1;
    s += (double)p0.f1 * 2;
    s += (double)p0.f2 * 3;
    s += (double)p1 * 4;
    s += (double)p2.f0 * 5;
    Ab6 r;
    r.f0 = (double)s + 0;
    r.f1 = (int64_t)(int64_t)(s + 1);
    return r;
}

ABI float abi_f13(Ab2 p0, Ab8 p1)
{
    double s = 0.0;
    s += (double)p0.f0 * 1;
    s += (double)p0.f1 * 2;
    s += (double)p1.f0 * 3;
    s += (double)p1.f1 * 4;
    s += (double)p1.f2 * 5;
    return (float)s;
}

ABI int64_t abi_f14(float p0, Ab8 p1)
{
    double s = 0.0;
    s += (double)p0 * 1;
    s += (double)p1.f0 * 2;
    s += (double)p1.f1 * 3;
    s += (double)p1.f2 * 4;
    return (int64_t)(int64_t)(s);
}

ABI Ab4 abi_f15(uint32_t p0, int64_t p1, Ab4 p2, Ab3 p3, int32_t p4, Ab1 p5)
{
    double s = 0.0;
    s += (double)p0 * 1;
    s += (double)p1 * 2;
    s += (double)p2.f0 * 3;
    s += (double)p3.f0 * 4;
    s += (double)p3.f1 * 5;
    s += (double)p4 * 6;
    s += (double)p5.f0 * 7;
    s += (double)p5.f1 * 8;
    s += (double)p5.f2 * 9;
    Ab4 r;
    r.f0 = (double)s + 0;
    return r;
}

ABI int64_t abi_f16(Ab9 p0, Ab0 p1, int8_t p2, uint16_t p3, double p4, Ab9 p5)
{
    double s = 0.0;
    s += (double)p0.f0 * 1;
    s += (double)p0.f1 * 2;
    s += (double)p0.f2 * 3;
    s += (double)p1.f0 * 4;
    s += (double)p2 * 5;
    s += (double)p3 * 6;
    s += (double)p4 * 7;
    s += (double)p5.f0 * 8;
    s += (double)p5.f1 * 9;
    s += (double)p5.f2 * 10;
    return (int64_t)(int64_t)(s);
}

ABI uint64_t abi_f17(Ab3 p0, int64_t p1)
{
    double s = 0.0;
    s += (double)p0.f0 * 1;
    s += (double)p0.f1 * 2;
    s += (double)p1 * 3;
    return (uint64_t)(int64_t)(s);
}

ABI Ab7 abi_f18(int64_t p0)
{
    double s = 0.0;
    s += (double)p0 * 1;
    Ab7 r;
    r.f0 = (double)s + 0;
    r.f1 = (uint8_t)(int64_t)(s + 1);
    return r;
}

ABI int8_t abi_f19(Ab6 p0)
{
    double s = 0.0;
    s += (double)p0.f0 * 1;
    s += (double)p0.f1 * 2;
    return (int8_t)(int64_t)(s);
}

ABI uint32_t abi_f20(Ab5 p0, double p1)
{
    double s = 0.0;
    s += (double)p0.f0 * 1;
    s += (double)p0.f1 * 2;
    s += (double)p0.f2 * 3;
    s += (double)p1 * 4;
    return (uint32_t)(int64_t)(s);
}

ABI Ab0 abi_f21(Ab1 p0, float p1, Ab8 p2, Ab8 p3)
{
    double s = 0.0;
    s += (double)p0.f0 * 1;
    s += (double)p0.f1 * 2;
    s += (double)p0.f2 * 3;
    s += (double)p1 * 4;
    s += (double)p2.f0 * 5;
    s += (double)p2.f1 * 6;
    s += (double)p2.f2 * 7;
    s += (double)p3.f0 * 8;
    s += (double)p3.f1 * 9;
    s += (double)p3.f2 * 10;
    Ab0 r;
    r.f0 = (int8_t)(int64_t)(s + 0);
    return r;
}

ABI uint16_t abi_f22(Ab2 p0)
{
    double s = 0.0;
    s += (double)p0.f0 * 1;
    s += (double)p0.f1 * 2;
    return (uint16_t)(int64_t)(s);
}

ABI Ab3 abi_f23(double p0, int32_t p1, Ab5 p2)
{
    double s = 0.0;
    s += (double)p0 * 1;
    s += (double)p1 * 2;
    s += (double)p2.f0 * 3;
    s += (double)p2.f1 * 4;
    s += (double)p2.f2 * 5;
    Ab3 r;
    r.f0 = (int32_t)(int64_t)(s + 0);
    r.f1 = (float)s + 1;
    return r;
}

ABI uint32_t abi_f24(uint64_t p0, Ab8 p1, uint32_t p2, Ab1 p3, Ab2 p4)
{
    double s = 0.0;
    s += (double)p0 * 1;
    s += (double)p1.f0 * 2;
    s += (double)p1.f1 * 3;
    s += (double)p1.f2 * 4;
    s += (double)p2 * 5;
    s += (double)p3.f0 * 6;
    s += (double)p3.f1 * 7;
    s += (double)p3.f2 * 8;
    s += (double)p4.f0 * 9;
    s += (double)p4.f1 * 10;
    return (uint32_t)(int64_t)(s);
}

ABI Ab8 abi_f25(int64_t p0)
{
    double s = 0.0;
    s += (double)p0 * 1;
    Ab8 r;
    r.f0 = (uint8_t)(int64_t)(s + 0);
    r.f1 = (uint64_t)(int64_t)(s + 1);
    r.f2 = (uint64_t)(int64_t)(s + 2);
    return r;
}

ABI uint8_t abi_f26(int32_t p0, Ab4 p1)
{
    double s = 0.0;
    s += (double)p0 * 1;
    s += (double)p1.f0 * 2;
    return (uint8_t)(int64_t)(s);
}

ABI int8_t abi_f27(uint8_t p0, int32_t p1, Ab0 p2, Ab1 p3)
{
    double s = 0.0;
    s += (double)p0 * 1;
    s += (double)p1 * 2;
    s += (double)p2.f0 * 3;
    s += (double)p3.f0 * 4;
    s += (double)p3.f1 * 5;
    s += (double)p3.f2 * 6;
    return (int8_t)(int64_t)(s);
}

ABI Ab3 abi_f28(Ab9 p0, Ab1 p1)
{
    double s = 0.0;
    s += (double)p0.f0 * 1;
    s += (double)p0.f1 * 2;
    s += (double)p0.f2 * 3;
    s += (double)p1.f0 * 4;
    s += (double)p1.f1 * 5;
    s += (double)p1.f2 * 6;
    Ab3 r;
    r.f0 = (int32_t)(int64_t)(s + 0);
    r.f1 = (float)s + 1;
    return r;
}

ABI float abi_f29(uint8_t p0, Ab6 p1)
{
    double s = 0.0;
    s += (double)p0 * 1;
    s += (double)p1.f0 * 2;
    s += (double)p1.f1 * 3;
    return (float)s;
}

ABI Ab4 abi_f30(uint64_t p0, Ab3 p1, int8_t p2)
{
    double s = 0.0;
    s += (double)p0 * 1;
    s += (double)p1.f0 * 2;
    s += (double)p1.f1 * 3;
    s += (double)p2 * 4;
    Ab4 r;
    r.f0 = (double)s + 0;
    return r;
}

ABI uint64_t abi_f31(uint64_t p0, Ab6 p1, Ab5 p2, uint64_t p3, Ab3 p4, float p5)
{
    double s = 0.0;
    s += (double)p0 * 1;
    s += (double)p1.f0 * 2;
    s += (double)p1.f1 * 3;
    s += (double)p2.f0 * 4;
    s += (double)p2.f1 * 5;
    s += (double)p2.f2 * 6;
    s += (double)p3 * 7;
    s += (double)p4.f0 * 8;
    s += (double)p4.f1 * 9;
    s += (double)p5 * 10;
    return (uint64_t)(int64_t)(s);
}

ABI uint16_t abi_f32(Ab2 p0, int32_t p1, Ab5 p2, Ab4 p3, Ab7 p4, Ab9 p5)
{
    double s = 0.0;
    s += (double)p0.f0 * 1;
    s += (double)p0.f1 * 2;
    s += (double)p1 * 3;
    s += (double)p2.f0 * 4;
    s += (double)p2.f1 * 5;
    s += (double)p2.f2 * 6;
    s += (double)p3.f0 * 7;
    s += (double)p4.f0 * 8;
    s += (double)p4.f1 * 9;
    s += (double)p5.f0 * 10;
    s += (double)p5.f1 * 11;
    s += (double)p5.f2 * 12;
    return (uint16_t)(int64_t)(s);
}

ABI Ab1 abi_f33(int8_t p0, Ab5 p1, double p2, int32_t p3)
{
    double s = 0.0;
    s += (double)p0 * 1;
    s += (double)p1.f0 * 2;
    s += (double)p1.f1 * 3;
    s += (double)p1.f2 * 4;
    s += (double)p2 * 5;
    s += (double)p3 * 6;
    Ab1 r;
    r.f0 = (uint8_t)(int64_t)(s + 0);
    r.f1 = (uint8_t)(int64_t)(s + 1);
    r.f2 = (uint8_t)(int64_t)(s + 2);
    return r;
}

ABI int8_t abi_f34(double p0, Ab3 p1, Ab3 p2, Ab4 p3, uint8_t p4)
{
    double s = 0.0;
    s += (double)p0 * 1;
    s += (double)p1.f0 * 2;
    s += (double)p1.f1 * 3;
    s += (double)p2.f0 * 4;
    s += (double)p2.f1 * 5;
    s += (double)p3.f0 * 6;
    s += (double)p4 * 7;
    return (int8_t)(int64_t)(s);
}

ABI Ab8 abi_f35(Ab5 p0, Ab2 p1, Ab5 p2, Ab2 p3, Ab2 p4, uint32_t p5)
{
    double s = 0.0;
    s += (double)p0.f0 * 1;
    s += (double)p0.f1 * 2;
    s += (double)p0.f2 * 3;
    s += (double)p1.f0 * 4;
    s += (double)p1.f1 * 5;
    s += (double)p2.f0 * 6;
    s += (double)p2.f1 * 7;
    s += (double)p2.f2 * 8;
    s += (double)p3.f0 * 9;
    s += (double)p3.f1 * 10;
    s += (double)p4.f0 * 11;
    s += (double)p4.f1 * 12;
    s += (double)p5 * 13;
    Ab8 r;
    r.f0 = (uint8_t)(int64_t)(s + 0);
    r.f1 = (uint64_t)(int64_t)(s + 1);
    r.f2 = (uint64_t)(int64_t)(s + 2);
    return r;
}

ABI Ab6 abi_f36(Ab3 p0, Ab0 p1, double p2, float p3, uint16_t p4)
{
    double s = 0.0;
    s += (double)p0.f0 * 1;
    s += (double)p0.f1 * 2;
    s += (double)p1.f0 * 3;
    s += (double)p2 * 4;
    s += (double)p3 * 5;
    s += (double)p4 * 6;
    Ab6 r;
    r.f0 = (double)s + 0;
    r.f1 = (int64_t)(int64_t)(s + 1);
    return r;
}

ABI uint64_t abi_f37(Ab3 p0, Ab1 p1, uint64_t p2, float p3, Ab7 p4)
{
    double s = 0.0;
    s += (double)p0.f0 * 1;
    s += (double)p0.f1 * 2;
    s += (double)p1.f0 * 3;
    s += (double)p1.f1 * 4;
    s += (double)p1.f2 * 5;
    s += (double)p2 * 6;
    s += (double)p3 * 7;
    s += (double)p4.f0 * 8;
    s += (double)p4.f1 * 9;
    return (uint64_t)(int64_t)(s);
}

ABI Ab7 abi_f38(Ab5 p0)
{
    double s = 0.0;
    s += (double)p0.f0 * 1;
    s += (double)p0.f1 * 2;
    s += (double)p0.f2 * 3;
    Ab7 r;
    r.f0 = (double)s + 0;
    r.f1 = (uint8_t)(int64_t)(s + 1);
    return r;
}

ABI int8_t abi_f39(int64_t p0)
{
    double s = 0.0;
    s += (double)p0 * 1;
    return (int8_t)(int64_t)(s);
}
