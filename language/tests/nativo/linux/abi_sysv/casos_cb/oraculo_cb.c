/* oraculo_cb.c - oraculo C da bateria de callback SysV (M5b P38-0): a mesma chamada de cada cbk_NN_<nome>.fj, compilada no alpine. Gerado por gerar_abi_sysv_cb 0.1.0. Nao edite a mao. Uso: oraculo_cb <indice>.
 */
#include <stdio.h>
#include <stdlib.h>
#include "cb_sysv.c"
#include "cb_impl_oraculo.c"

int main(int argc, char **argv)
{
    if (argc < 2) return 3;
    switch (atoi(argv[1])) {
    case 0: {
        signed char r = cb_call_00(cbimpl_00);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 1: {
        short r = cb_call_01(cbimpl_01);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 2: {
        int r = cb_call_02(cbimpl_02);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 3: {
        long r = cb_call_03(cbimpl_03);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 4: {
        float r = cb_call_04(cbimpl_04);
        printf("%lld\n", (long long)((double)(r) * 4.0));
        break;
    }
    case 5: {
        double r = cb_call_05(cbimpl_05);
        printf("%lld\n", (long long)((double)(r) * 4.0));
        break;
    }
    case 6: {
        void * r = cb_call_06(cbimpl_06);
        printf("%lld\n", (long long)*(int *)(r));
        break;
    }
    case 7: {
        double r = cb_call_07(cbimpl_07);
        printf("%lld\n", (long long)((double)(r) * 4.0));
        break;
    }
    case 8: {
        int r = cb_call_08(cbimpl_08);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 9: {
        float r = cb_call_09(cbimpl_09);
        printf("%lld\n", (long long)((double)(r) * 4.0));
        break;
    }
    case 10: {
        long r = cb_call_10(cbimpl_10);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 11: {
        double r = cb_call_11(cbimpl_11);
        printf("%lld\n", (long long)((double)(r) * 4.0));
        break;
    }
    case 12: {
        long r = cb_call_12(cbimpl_12);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 13: {
        double r = cb_call_13(cbimpl_13);
        printf("%lld\n", (long long)((double)(r) * 4.0));
        break;
    }
    case 14: {
        int r = cb_call_14(cbimpl_14);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 15: {
        void * r = cb_call_15(cbimpl_15);
        printf("%lld\n", (long long)*(int *)(r));
        break;
    }
    case 16: {
        int r = cb_call_16(cbimpl_16);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 17: {
        S01 r = cb_call_17(cbimpl_17);
        printf("%lld\n", (long long)(r.f0));
        break;
    }
    case 18: {
        S02 r = cb_call_18(cbimpl_18);
        printf("%lld\n", (long long)(r.f0));
        break;
    }
    case 19: {
        S03 r = cb_call_19(cbimpl_19);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        printf("%lld\n", (long long)(r.f2));
        break;
    }
    case 20: {
        S04 r = cb_call_20(cbimpl_20);
        printf("%lld\n", (long long)(r.f0));
        break;
    }
    case 21: {
        S05 r = cb_call_21(cbimpl_21);
        printf("%lld\n", (long long)((double)(r.f0) * 4.0));
        break;
    }
    case 22: {
        S06 r = cb_call_22(cbimpl_22);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        printf("%lld\n", (long long)(r.f2));
        printf("%lld\n", (long long)(r.f3));
        printf("%lld\n", (long long)(r.f4));
        break;
    }
    case 23: {
        S07 r = cb_call_23(cbimpl_23);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        printf("%lld\n", (long long)(r.f2));
        break;
    }
    case 24: {
        S08 r = cb_call_24(cbimpl_24);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        break;
    }
    case 25: {
        S09 r = cb_call_25(cbimpl_25);
        printf("%lld\n", (long long)((double)(r.f0) * 4.0));
        printf("%lld\n", (long long)((double)(r.f1) * 4.0));
        break;
    }
    case 26: {
        S10 r = cb_call_26(cbimpl_26);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)((double)(r.f1) * 4.0));
        break;
    }
    case 27: {
        S11 r = cb_call_27(cbimpl_27);
        printf("%lld\n", (long long)((double)(r.f0) * 4.0));
        printf("%lld\n", (long long)(r.f1));
        break;
    }
    case 28: {
        S12 r = cb_call_28(cbimpl_28);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        break;
    }
    case 29: {
        S13 r = cb_call_29(cbimpl_29);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        printf("%lld\n", (long long)((double)(r.f2) * 4.0));
        break;
    }
    case 30: {
        S14 r = cb_call_30(cbimpl_30);
        printf("%lld\n", (long long)((double)(r.f0) * 4.0));
        printf("%lld\n", (long long)((double)(r.f1) * 4.0));
        printf("%lld\n", (long long)((double)(r.f2) * 4.0));
        break;
    }
    case 31: {
        S15 r = cb_call_31(cbimpl_31);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)((double)(r.f1) * 4.0));
        break;
    }
    case 32: {
        S16 r = cb_call_32(cbimpl_32);
        printf("%lld\n", (long long)((double)(r.f0) * 4.0));
        printf("%lld\n", (long long)(r.f1));
        break;
    }
    case 33: {
        S17 r = cb_call_33(cbimpl_33);
        printf("%lld\n", (long long)((double)(r.f0) * 4.0));
        printf("%lld\n", (long long)((double)(r.f1) * 4.0));
        printf("%lld\n", (long long)((double)(r.f2) * 4.0));
        printf("%lld\n", (long long)((double)(r.f3) * 4.0));
        break;
    }
    case 34: {
        S18 r = cb_call_34(cbimpl_34);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        break;
    }
    case 35: {
        S19 r = cb_call_35(cbimpl_35);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        printf("%lld\n", (long long)(r.f2));
        printf("%lld\n", (long long)(r.f3));
        printf("%lld\n", (long long)(r.f4));
        break;
    }
    case 36: {
        S20 r = cb_call_36(cbimpl_36);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)((double)(r.f1) * 4.0));
        printf("%lld\n", (long long)(r.f2));
        break;
    }
    case 37: {
        S21 r = cb_call_37(cbimpl_37);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        printf("%lld\n", (long long)(r.f2));
        printf("%lld\n", (long long)(r.f3));
        break;
    }
    case 38: {
        S22 r = cb_call_38(cbimpl_38);
        printf("%lld\n", (long long)((double)(r.f0) * 4.0));
        printf("%lld\n", (long long)(r.f1));
        break;
    }
    case 39: {
        S23 r = cb_call_39(cbimpl_39);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)((double)(r.f1) * 4.0));
        break;
    }
    case 40: {
        long r = cb_call_40(cbimpl_40);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 41: {
        long r = cb_call_41(cbimpl_41);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 42: {
        long r = cb_call_42(cbimpl_42);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 43: {
        long r = cb_call_43(cbimpl_43);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 44: {
        int r = cb_call_44(cbimpl_44);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 45: {
        double r = cb_call_45(cbimpl_45);
        printf("%lld\n", (long long)((double)(r) * 4.0));
        break;
    }
    case 46: {
        int r = cb_call_46(cbimpl_46);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 47: {
        long r = cb_call_47(cbimpl_47);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 48: {
        S08 r = cb_call_48(cbimpl_48);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        break;
    }
    case 49: {
        S09 r = cb_call_49(cbimpl_49);
        printf("%lld\n", (long long)((double)(r.f0) * 4.0));
        printf("%lld\n", (long long)((double)(r.f1) * 4.0));
        break;
    }
    case 50: {
        S10 r = cb_call_50(cbimpl_50);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)((double)(r.f1) * 4.0));
        break;
    }
    case 51: {
        S11 r = cb_call_51(cbimpl_51);
        printf("%lld\n", (long long)((double)(r.f0) * 4.0));
        printf("%lld\n", (long long)(r.f1));
        break;
    }
    case 52: {
        S12 r = cb_call_52(cbimpl_52);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        break;
    }
    case 53: {
        S13 r = cb_call_53(cbimpl_53);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        printf("%lld\n", (long long)((double)(r.f2) * 4.0));
        break;
    }
    case 54: {
        S14 r = cb_call_54(cbimpl_54);
        printf("%lld\n", (long long)((double)(r.f0) * 4.0));
        printf("%lld\n", (long long)((double)(r.f1) * 4.0));
        printf("%lld\n", (long long)((double)(r.f2) * 4.0));
        break;
    }
    case 55: {
        S15 r = cb_call_55(cbimpl_55);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)((double)(r.f1) * 4.0));
        break;
    }
    case 56: {
        S16 r = cb_call_56(cbimpl_56);
        printf("%lld\n", (long long)((double)(r.f0) * 4.0));
        printf("%lld\n", (long long)(r.f1));
        break;
    }
    case 57: {
        S17 r = cb_call_57(cbimpl_57);
        printf("%lld\n", (long long)((double)(r.f0) * 4.0));
        printf("%lld\n", (long long)((double)(r.f1) * 4.0));
        printf("%lld\n", (long long)((double)(r.f2) * 4.0));
        printf("%lld\n", (long long)((double)(r.f3) * 4.0));
        break;
    }
    case 58: {
        S19 r = cb_call_58(cbimpl_58);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        printf("%lld\n", (long long)(r.f2));
        printf("%lld\n", (long long)(r.f3));
        printf("%lld\n", (long long)(r.f4));
        break;
    }
    case 59: {
        S21 r = cb_call_59(cbimpl_59);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        printf("%lld\n", (long long)(r.f2));
        printf("%lld\n", (long long)(r.f3));
        break;
    }
    default:
        return 2;
    }
    return 0;
}
