/* cb_impl_oraculo.c - callbacks em C da bateria de callback (M5b P38-0): cbimpl_NN imprime os argumentos e devolve o plano, como cbk_NN .fj. Gerado por gerar_abi_sysv_cb 0.1.0. Nao edite a mao. */

static signed char cbimpl_00(signed char a0)
{
    printf("%lld\n", (long long)(a0));
    return 18;
}

static short cbimpl_01(short a0)
{
    printf("%lld\n", (long long)(a0));
    return 18;
}

static int cbimpl_02(int a0)
{
    printf("%lld\n", (long long)(a0));
    return 18;
}

static long cbimpl_03(long a0)
{
    printf("%lld\n", (long long)(a0));
    return 18;
}

static float cbimpl_04(float a0)
{
    printf("%lld\n", (long long)((double)(a0) * 4.0));
    return 0.25;
}

static double cbimpl_05(double a0)
{
    printf("%lld\n", (long long)((double)(a0) * 4.0));
    return 0.25;
}

static void * cbimpl_06(void * a0)
{
    printf("%lld\n", (long long)*(int *)(a0));
    return a0;
}

static double cbimpl_07(int a0, double a1)
{
    printf("%lld\n", (long long)(a0));
    printf("%lld\n", (long long)((double)(a1) * 4.0));
    return 1.50;
}

static int cbimpl_08(double a0, int a1)
{
    printf("%lld\n", (long long)((double)(a0) * 4.0));
    printf("%lld\n", (long long)(a1));
    return -6;
}

static float cbimpl_09(long a0, float a1)
{
    printf("%lld\n", (long long)(a0));
    printf("%lld\n", (long long)((double)(a1) * 4.0));
    return 1.50;
}

static long cbimpl_10(long a0, long a1, long a2, long a3, long a4, long a5)
{
    printf("%lld\n", (long long)(a0));
    printf("%lld\n", (long long)(a1));
    printf("%lld\n", (long long)(a2));
    printf("%lld\n", (long long)(a3));
    printf("%lld\n", (long long)(a4));
    printf("%lld\n", (long long)(a5));
    return 20;
}

static double cbimpl_11(double a0, double a1, double a2, double a3, double a4, double a5, double a6, double a7)
{
    printf("%lld\n", (long long)((double)(a0) * 4.0));
    printf("%lld\n", (long long)((double)(a1) * 4.0));
    printf("%lld\n", (long long)((double)(a2) * 4.0));
    printf("%lld\n", (long long)((double)(a3) * 4.0));
    printf("%lld\n", (long long)((double)(a4) * 4.0));
    printf("%lld\n", (long long)((double)(a5) * 4.0));
    printf("%lld\n", (long long)((double)(a6) * 4.0));
    printf("%lld\n", (long long)((double)(a7) * 4.0));
    return 0.50;
}

static long cbimpl_12(long a0, long a1, long a2, long a3, long a4, long a5, long a6)
{
    printf("%lld\n", (long long)(a0));
    printf("%lld\n", (long long)(a1));
    printf("%lld\n", (long long)(a2));
    printf("%lld\n", (long long)(a3));
    printf("%lld\n", (long long)(a4));
    printf("%lld\n", (long long)(a5));
    printf("%lld\n", (long long)(a6));
    return -4;
}

static double cbimpl_13(double a0, double a1, double a2, double a3, double a4, double a5, double a6, double a7, double a8)
{
    printf("%lld\n", (long long)((double)(a0) * 4.0));
    printf("%lld\n", (long long)((double)(a1) * 4.0));
    printf("%lld\n", (long long)((double)(a2) * 4.0));
    printf("%lld\n", (long long)((double)(a3) * 4.0));
    printf("%lld\n", (long long)((double)(a4) * 4.0));
    printf("%lld\n", (long long)((double)(a5) * 4.0));
    printf("%lld\n", (long long)((double)(a6) * 4.0));
    printf("%lld\n", (long long)((double)(a7) * 4.0));
    printf("%lld\n", (long long)((double)(a8) * 4.0));
    return 1.75;
}

static int cbimpl_14(signed char a0, float a1, short a2, double a3, int a4, float a5)
{
    printf("%lld\n", (long long)(a0));
    printf("%lld\n", (long long)((double)(a1) * 4.0));
    printf("%lld\n", (long long)(a2));
    printf("%lld\n", (long long)((double)(a3) * 4.0));
    printf("%lld\n", (long long)(a4));
    printf("%lld\n", (long long)((double)(a5) * 4.0));
    return 20;
}

static void * cbimpl_15(void * a0, void * a1)
{
    printf("%lld\n", (long long)*(int *)(a0));
    printf("%lld\n", (long long)*(int *)(a1));
    return a1;
}

static int cbimpl_16(int a0, int a1, int a2)
{
    printf("%lld\n", (long long)(a0));
    printf("%lld\n", (long long)(a1));
    printf("%lld\n", (long long)(a2));
    return -30;
}

static S01 cbimpl_17(S01 a0)
{
    printf("%lld\n", (long long)(a0.f0));
    return (S01){ .f0 = 18 };
}

static S02 cbimpl_18(S02 a0)
{
    printf("%lld\n", (long long)(a0.f0));
    return (S02){ .f0 = 18 };
}

static S03 cbimpl_19(S03 a0)
{
    printf("%lld\n", (long long)(a0.f0));
    printf("%lld\n", (long long)(a0.f1));
    printf("%lld\n", (long long)(a0.f2));
    return (S03){ .f0 = -30, .f1 = 7, .f2 = -17 };
}

static S04 cbimpl_20(S04 a0)
{
    printf("%lld\n", (long long)(a0.f0));
    return (S04){ .f0 = 18 };
}

static S05 cbimpl_21(S05 a0)
{
    printf("%lld\n", (long long)((double)(a0.f0) * 4.0));
    return (S05){ .f0 = 0.25 };
}

static S06 cbimpl_22(S06 a0)
{
    printf("%lld\n", (long long)(a0.f0));
    printf("%lld\n", (long long)(a0.f1));
    printf("%lld\n", (long long)(a0.f2));
    printf("%lld\n", (long long)(a0.f3));
    printf("%lld\n", (long long)(a0.f4));
    return (S06){ .f0 = -17, .f1 = 20, .f2 = -4, .f3 = -28, .f4 = 9 };
}

static S07 cbimpl_23(S07 a0)
{
    printf("%lld\n", (long long)(a0.f0));
    printf("%lld\n", (long long)(a0.f1));
    printf("%lld\n", (long long)(a0.f2));
    return (S07){ .f0 = -30, .f1 = 7, .f2 = -17 };
}

static S08 cbimpl_24(S08 a0)
{
    printf("%lld\n", (long long)(a0.f0));
    printf("%lld\n", (long long)(a0.f1));
    return (S08){ .f0 = -6, .f1 = -30 };
}

static S09 cbimpl_25(S09 a0)
{
    printf("%lld\n", (long long)((double)(a0.f0) * 4.0));
    printf("%lld\n", (long long)((double)(a0.f1) * 4.0));
    return (S09){ .f0 = 1.50, .f1 = -1.50 };
}

static S10 cbimpl_26(S10 a0)
{
    printf("%lld\n", (long long)(a0.f0));
    printf("%lld\n", (long long)((double)(a0.f1) * 4.0));
    return (S10){ .f0 = -6, .f1 = -1.50 };
}

static S11 cbimpl_27(S11 a0)
{
    printf("%lld\n", (long long)((double)(a0.f0) * 4.0));
    printf("%lld\n", (long long)(a0.f1));
    return (S11){ .f0 = 1.50, .f1 = -30 };
}

static S12 cbimpl_28(S12 a0)
{
    printf("%lld\n", (long long)(a0.f0));
    printf("%lld\n", (long long)(a0.f1));
    return (S12){ .f0 = -6, .f1 = -30 };
}

static S13 cbimpl_29(S13 a0)
{
    printf("%lld\n", (long long)(a0.f0));
    printf("%lld\n", (long long)(a0.f1));
    printf("%lld\n", (long long)((double)(a0.f2) * 4.0));
    return (S13){ .f0 = -30, .f1 = 7, .f2 = 1.00 };
}

static S14 cbimpl_30(S14 a0)
{
    printf("%lld\n", (long long)((double)(a0.f0) * 4.0));
    printf("%lld\n", (long long)((double)(a0.f1) * 4.0));
    printf("%lld\n", (long long)((double)(a0.f2) * 4.0));
    return (S14){ .f0 = -1.50, .f1 = -0.25, .f2 = 1.00 };
}

static S15 cbimpl_31(S15 a0)
{
    printf("%lld\n", (long long)(a0.f0));
    printf("%lld\n", (long long)((double)(a0.f1) * 4.0));
    return (S15){ .f0 = -6, .f1 = -1.50 };
}

static S16 cbimpl_32(S16 a0)
{
    printf("%lld\n", (long long)((double)(a0.f0) * 4.0));
    printf("%lld\n", (long long)(a0.f1));
    return (S16){ .f0 = 1.50, .f1 = -30 };
}

static S17 cbimpl_33(S17 a0)
{
    printf("%lld\n", (long long)((double)(a0.f0) * 4.0));
    printf("%lld\n", (long long)((double)(a0.f1) * 4.0));
    printf("%lld\n", (long long)((double)(a0.f2) * 4.0));
    printf("%lld\n", (long long)((double)(a0.f3) * 4.0));
    return (S17){ .f0 = -0.25, .f1 = 1.00, .f2 = 2.25, .f3 = -0.75 };
}

static S18 cbimpl_34(S18 a0)
{
    printf("%lld\n", (long long)(a0.f0));
    printf("%lld\n", (long long)(a0.f1));
    return (S18){ .f0 = -6, .f1 = -30 };
}

static S19 cbimpl_35(S19 a0)
{
    printf("%lld\n", (long long)(a0.f0));
    printf("%lld\n", (long long)(a0.f1));
    printf("%lld\n", (long long)(a0.f2));
    printf("%lld\n", (long long)(a0.f3));
    printf("%lld\n", (long long)(a0.f4));
    return (S19){ .f0 = -17, .f1 = 20, .f2 = -4, .f3 = -28, .f4 = 9 };
}

static S20 cbimpl_36(S20 a0)
{
    printf("%lld\n", (long long)(a0.f0));
    printf("%lld\n", (long long)((double)(a0.f1) * 4.0));
    printf("%lld\n", (long long)(a0.f2));
    return (S20){ .f0 = -30, .f1 = -0.25, .f2 = -17 };
}

static S21 cbimpl_37(S21 a0)
{
    printf("%lld\n", (long long)(a0.f0));
    printf("%lld\n", (long long)(a0.f1));
    printf("%lld\n", (long long)(a0.f2));
    printf("%lld\n", (long long)(a0.f3));
    return (S21){ .f0 = 7, .f1 = -17, .f2 = 20, .f3 = -4 };
}

static S22 cbimpl_38(S22 a0)
{
    printf("%lld\n", (long long)((double)(a0.f0) * 4.0));
    printf("%lld\n", (long long)(a0.f1));
    return (S22){ .f0 = 1.50, .f1 = -30 };
}

static S23 cbimpl_39(S23 a0)
{
    printf("%lld\n", (long long)(a0.f0));
    printf("%lld\n", (long long)((double)(a0.f1) * 4.0));
    return (S23){ .f0 = -6, .f1 = -1.50 };
}

static long cbimpl_40(int a0, S10 a1, double a2)
{
    printf("%lld\n", (long long)(a0));
    printf("%lld\n", (long long)(a1.f0));
    printf("%lld\n", (long long)((double)(a1.f1) * 4.0));
    printf("%lld\n", (long long)((double)(a2) * 4.0));
    return 7;
}

static long cbimpl_41(int a0, S13 a1, double a2)
{
    printf("%lld\n", (long long)(a0));
    printf("%lld\n", (long long)(a1.f0));
    printf("%lld\n", (long long)(a1.f1));
    printf("%lld\n", (long long)((double)(a1.f2) * 4.0));
    printf("%lld\n", (long long)((double)(a2) * 4.0));
    return -17;
}

static long cbimpl_42(int a0, S15 a1, double a2)
{
    printf("%lld\n", (long long)(a0));
    printf("%lld\n", (long long)(a1.f0));
    printf("%lld\n", (long long)((double)(a1.f1) * 4.0));
    printf("%lld\n", (long long)((double)(a2) * 4.0));
    return 7;
}

static long cbimpl_43(int a0, S20 a1, double a2)
{
    printf("%lld\n", (long long)(a0));
    printf("%lld\n", (long long)(a1.f0));
    printf("%lld\n", (long long)((double)(a1.f1) * 4.0));
    printf("%lld\n", (long long)(a1.f2));
    printf("%lld\n", (long long)((double)(a2) * 4.0));
    return -17;
}

static int cbimpl_44(int a0)
{
    printf("%lld\n", (long long)(a0));
    return 18;
}

static double cbimpl_45(double a0)
{
    printf("%lld\n", (long long)((double)(a0) * 4.0));
    return 0.25;
}

static int cbimpl_46(void * a0)
{
    printf("%lld\n", (long long)*(int *)(a0));
    return 18;
}

static long cbimpl_47(long a0, long a1)
{
    printf("%lld\n", (long long)(a0));
    printf("%lld\n", (long long)(a1));
    return -6;
}

static S08 cbimpl_48(int a0, int a1)
{
    printf("%lld\n", (long long)(a0));
    printf("%lld\n", (long long)(a1));
    return (S08){ .f0 = -6, .f1 = -30 };
}

static S09 cbimpl_49(float a0, float a1)
{
    printf("%lld\n", (long long)((double)(a0) * 4.0));
    printf("%lld\n", (long long)((double)(a1) * 4.0));
    return (S09){ .f0 = 1.50, .f1 = -1.50 };
}

static S10 cbimpl_50(int a0, float a1)
{
    printf("%lld\n", (long long)(a0));
    printf("%lld\n", (long long)((double)(a1) * 4.0));
    return (S10){ .f0 = -6, .f1 = -1.50 };
}

static S11 cbimpl_51(float a0, int a1)
{
    printf("%lld\n", (long long)((double)(a0) * 4.0));
    printf("%lld\n", (long long)(a1));
    return (S11){ .f0 = 1.50, .f1 = -30 };
}

static S12 cbimpl_52(long a0, signed char a1)
{
    printf("%lld\n", (long long)(a0));
    printf("%lld\n", (long long)(a1));
    return (S12){ .f0 = -6, .f1 = -30 };
}

static S13 cbimpl_53(int a0, int a1, float a2)
{
    printf("%lld\n", (long long)(a0));
    printf("%lld\n", (long long)(a1));
    printf("%lld\n", (long long)((double)(a2) * 4.0));
    return (S13){ .f0 = -30, .f1 = 7, .f2 = 1.00 };
}

static S14 cbimpl_54(float a0, float a1, float a2)
{
    printf("%lld\n", (long long)((double)(a0) * 4.0));
    printf("%lld\n", (long long)((double)(a1) * 4.0));
    printf("%lld\n", (long long)((double)(a2) * 4.0));
    return (S14){ .f0 = -1.50, .f1 = -0.25, .f2 = 1.00 };
}

static S15 cbimpl_55(long a0, double a1)
{
    printf("%lld\n", (long long)(a0));
    printf("%lld\n", (long long)((double)(a1) * 4.0));
    return (S15){ .f0 = -6, .f1 = -1.50 };
}

static S16 cbimpl_56(double a0, long a1)
{
    printf("%lld\n", (long long)((double)(a0) * 4.0));
    printf("%lld\n", (long long)(a1));
    return (S16){ .f0 = 1.50, .f1 = -30 };
}

static S17 cbimpl_57(float a0, float a1, float a2, float a3)
{
    printf("%lld\n", (long long)((double)(a0) * 4.0));
    printf("%lld\n", (long long)((double)(a1) * 4.0));
    printf("%lld\n", (long long)((double)(a2) * 4.0));
    printf("%lld\n", (long long)((double)(a3) * 4.0));
    return (S17){ .f0 = -0.25, .f1 = 1.00, .f2 = 2.25, .f3 = -0.75 };
}

static S19 cbimpl_58(int a0, int a1, int a2, int a3, int a4)
{
    printf("%lld\n", (long long)(a0));
    printf("%lld\n", (long long)(a1));
    printf("%lld\n", (long long)(a2));
    printf("%lld\n", (long long)(a3));
    printf("%lld\n", (long long)(a4));
    return (S19){ .f0 = -17, .f1 = 20, .f2 = -4, .f3 = -28, .f4 = 9 };
}

static S21 cbimpl_59(long a0, long a1, long a2, long a3)
{
    printf("%lld\n", (long long)(a0));
    printf("%lld\n", (long long)(a1));
    printf("%lld\n", (long long)(a2));
    printf("%lld\n", (long long)(a3));
    return (S21){ .f0 = 7, .f1 = -17, .f2 = 20, .f3 = -4 };
}
