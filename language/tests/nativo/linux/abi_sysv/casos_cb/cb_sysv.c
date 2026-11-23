/* cb_sysv.c - lib de callback SysV (M5b P38-0): cb_call_NN chama o callback com a assinatura NN. Gerado por gerar_abi_sysv_cb 0.1.0. Nao edite a mao. Build: construir_abi_sysv_cb.sh. */
#include "../assinaturas.c"

signed char cb_call_00(signed char (*cb)(signed char))
{
    return cb(-19);
}

short cb_call_01(short (*cb)(short))
{
    return cb(-19);
}

int cb_call_02(int (*cb)(int))
{
    return cb(-19);
}

long cb_call_03(long (*cb)(long))
{
    return cb(-19);
}

float cb_call_04(float (*cb)(float))
{
    return cb(-1.00);
}

double cb_call_05(double (*cb)(double))
{
    return cb(-1.00);
}

void * cb_call_06(void * (*cb)(void *))
{
    static int p0 = -19;
    return cb((void *)&p0);
}

double cb_call_07(double (*cb)(int, double))
{
    return cb(-19, 0.25);
}

int cb_call_08(int (*cb)(double, int))
{
    return cb(-1.00, 18);
}

float cb_call_09(float (*cb)(long, float))
{
    return cb(-19, 0.25);
}

long cb_call_10(long (*cb)(long, long, long, long, long, long))
{
    return cb(-19, 18, -6, -30, 7, -17);
}

double cb_call_11(double (*cb)(double, double, double, double, double, double, double, double))
{
    return cb(-1.00, 0.25, 1.50, -1.50, -0.25, 1.00, 2.25, -0.75);
}

long cb_call_12(long (*cb)(long, long, long, long, long, long, long))
{
    return cb(-19, 18, -6, -30, 7, -17, 20);
}

double cb_call_13(double (*cb)(double, double, double, double, double, double, double, double, double))
{
    return cb(-1.00, 0.25, 1.50, -1.50, -0.25, 1.00, 2.25, -0.75, 0.50);
}

int cb_call_14(int (*cb)(signed char, float, short, double, int, float))
{
    return cb(-19, 0.25, -6, -1.50, 7, 1.00);
}

void * cb_call_15(void * (*cb)(void *, void *))
{
    static int p0 = -19;
    static int p1 = 18;
    return cb((void *)&p0, (void *)&p1);
}

int cb_call_16(int (*cb)(int, int, int))
{
    return cb(-19, 18, -6);
}

S01 cb_call_17(S01 (*cb)(S01))
{
    return cb((S01){ .f0 = -19 });
}

S02 cb_call_18(S02 (*cb)(S02))
{
    return cb((S02){ .f0 = -19 });
}

S03 cb_call_19(S03 (*cb)(S03))
{
    return cb((S03){ .f0 = -19, .f1 = 18, .f2 = -6 });
}

S04 cb_call_20(S04 (*cb)(S04))
{
    return cb((S04){ .f0 = -19 });
}

S05 cb_call_21(S05 (*cb)(S05))
{
    return cb((S05){ .f0 = -1.00 });
}

S06 cb_call_22(S06 (*cb)(S06))
{
    return cb((S06){ .f0 = -19, .f1 = 18, .f2 = -6, .f3 = -30, .f4 = 7 });
}

S07 cb_call_23(S07 (*cb)(S07))
{
    return cb((S07){ .f0 = -19, .f1 = 18, .f2 = -6 });
}

S08 cb_call_24(S08 (*cb)(S08))
{
    return cb((S08){ .f0 = -19, .f1 = 18 });
}

S09 cb_call_25(S09 (*cb)(S09))
{
    return cb((S09){ .f0 = -1.00, .f1 = 0.25 });
}

S10 cb_call_26(S10 (*cb)(S10))
{
    return cb((S10){ .f0 = -19, .f1 = 0.25 });
}

S11 cb_call_27(S11 (*cb)(S11))
{
    return cb((S11){ .f0 = -1.00, .f1 = 18 });
}

S12 cb_call_28(S12 (*cb)(S12))
{
    return cb((S12){ .f0 = -19, .f1 = 18 });
}

S13 cb_call_29(S13 (*cb)(S13))
{
    return cb((S13){ .f0 = -19, .f1 = 18, .f2 = 1.50 });
}

S14 cb_call_30(S14 (*cb)(S14))
{
    return cb((S14){ .f0 = -1.00, .f1 = 0.25, .f2 = 1.50 });
}

S15 cb_call_31(S15 (*cb)(S15))
{
    return cb((S15){ .f0 = -19, .f1 = 0.25 });
}

S16 cb_call_32(S16 (*cb)(S16))
{
    return cb((S16){ .f0 = -1.00, .f1 = 18 });
}

S17 cb_call_33(S17 (*cb)(S17))
{
    return cb((S17){ .f0 = -1.00, .f1 = 0.25, .f2 = 1.50, .f3 = -1.50 });
}

S18 cb_call_34(S18 (*cb)(S18))
{
    return cb((S18){ .f0 = -19, .f1 = 18 });
}

S19 cb_call_35(S19 (*cb)(S19))
{
    return cb((S19){ .f0 = -19, .f1 = 18, .f2 = -6, .f3 = -30, .f4 = 7 });
}

S20 cb_call_36(S20 (*cb)(S20))
{
    return cb((S20){ .f0 = -19, .f1 = 0.25, .f2 = -6 });
}

S21 cb_call_37(S21 (*cb)(S21))
{
    return cb((S21){ .f0 = -19, .f1 = 18, .f2 = -6, .f3 = -30 });
}

S22 cb_call_38(S22 (*cb)(S22))
{
    return cb((S22){ .f0 = -1.00, .f1 = 18 });
}

S23 cb_call_39(S23 (*cb)(S23))
{
    return cb((S23){ .f0 = -19, .f1 = 0.25 });
}

long cb_call_40(long (*cb)(int, S10, double))
{
    return cb(-19, (S10){ .f0 = 18, .f1 = 1.50 }, -1.50);
}

long cb_call_41(long (*cb)(int, S13, double))
{
    return cb(-19, (S13){ .f0 = 18, .f1 = -6, .f2 = -1.50 }, -0.25);
}

long cb_call_42(long (*cb)(int, S15, double))
{
    return cb(-19, (S15){ .f0 = 18, .f1 = 1.50 }, -1.50);
}

long cb_call_43(long (*cb)(int, S20, double))
{
    return cb(-19, (S20){ .f0 = 18, .f1 = 1.50, .f2 = -30 }, -0.25);
}

int cb_call_44(int (*cb)(int))
{
    return cb(-19);
}

double cb_call_45(double (*cb)(double))
{
    return cb(-1.00);
}

int cb_call_46(int (*cb)(void *))
{
    static int p0 = -19;
    return cb((void *)&p0);
}

long cb_call_47(long (*cb)(long, long))
{
    return cb(-19, 18);
}

S08 cb_call_48(S08 (*cb)(int, int))
{
    return cb(-19, 18);
}

S09 cb_call_49(S09 (*cb)(float, float))
{
    return cb(-1.00, 0.25);
}

S10 cb_call_50(S10 (*cb)(int, float))
{
    return cb(-19, 0.25);
}

S11 cb_call_51(S11 (*cb)(float, int))
{
    return cb(-1.00, 18);
}

S12 cb_call_52(S12 (*cb)(long, signed char))
{
    return cb(-19, 18);
}

S13 cb_call_53(S13 (*cb)(int, int, float))
{
    return cb(-19, 18, 1.50);
}

S14 cb_call_54(S14 (*cb)(float, float, float))
{
    return cb(-1.00, 0.25, 1.50);
}

S15 cb_call_55(S15 (*cb)(long, double))
{
    return cb(-19, 0.25);
}

S16 cb_call_56(S16 (*cb)(double, long))
{
    return cb(-1.00, 18);
}

S17 cb_call_57(S17 (*cb)(float, float, float, float))
{
    return cb(-1.00, 0.25, 1.50, -1.50);
}

S19 cb_call_58(S19 (*cb)(int, int, int, int, int))
{
    return cb(-19, 18, -6, -30, 7);
}

S21 cb_call_59(S21 (*cb)(long, long, long, long))
{
    return cb(-19, 18, -6, -30);
}
