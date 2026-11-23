/* oraculo_sysv.c - oraculo C da bateria ABI SysV (M5b P37c): a mesma chamada de cada sysv_NN_<nome>.fj, compilada no alpine com assinaturas.c.
 * Gerado por gerar_abi_sysv.py 0.1.0. Nao edite a mao. Uso: oraculo_sysv <indice>.
 */
#include <stdio.h>
#include <stdlib.h>
#include "../assinaturas.c"

int main(int argc, char **argv)
{
    if (argc < 2) return 3;
    switch (atoi(argv[1])) {
    case 0: {
        signed char r = id_i8(-19);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 1: {
        short r = id_i16(-19);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 2: {
        int r = id_i32(-19);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 3: {
        long r = id_i64(-19);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 4: {
        float r = id_f32(-1.00);
        printf("%lld\n", (long long)((double)(r) * 4.0));
        break;
    }
    case 5: {
        double r = id_f64(-1.00);
        printf("%lld\n", (long long)((double)(r) * 4.0));
        break;
    }
    case 6: {
        int p0 = -19;
        void * r = id_ptr((void *)&p0);
        printf("%lld\n", (long long)*(int *)(r));
        break;
    }
    case 7: {
        double r = mista_00(-19, 0.25);
        printf("%lld\n", (long long)((double)(r) * 4.0));
        break;
    }
    case 8: {
        int r = mista_01(-1.00, 18);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 9: {
        float r = mista_02(-19, 0.25);
        printf("%lld\n", (long long)((double)(r) * 4.0));
        break;
    }
    case 10: {
        long r = mista_03(-19, 18, -6, -30, 7, -17);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 11: {
        double r = mista_04(-1.00, 0.25, 1.50, -1.50, -0.25, 1.00, 2.25, -0.75);
        printf("%lld\n", (long long)((double)(r) * 4.0));
        break;
    }
    case 12: {
        long r = mista_05(-19, 18, -6, -30, 7, -17, 20);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 13: {
        double r = mista_06(-1.00, 0.25, 1.50, -1.50, -0.25, 1.00, 2.25, -0.75, 0.50);
        printf("%lld\n", (long long)((double)(r) * 4.0));
        break;
    }
    case 14: {
        int r = mista_07(-19, 0.25, -6, -1.50, 7, 1.00);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 15: {
        int p0 = -19;
        int p1 = 18;
        void * r = mista_08((void *)&p0, (void *)&p1);
        printf("%lld\n", (long long)*(int *)(r));
        break;
    }
    case 16: {
        int r = mista_09(-19, 18, -6);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 17: {
        S01 r = id_S01((S01){ .f0 = -19 });
        printf("%lld\n", (long long)(r.f0));
        break;
    }
    case 18: {
        S02 r = id_S02((S02){ .f0 = -19 });
        printf("%lld\n", (long long)(r.f0));
        break;
    }
    case 19: {
        S03 r = id_S03((S03){ .f0 = -19, .f1 = 18, .f2 = -6 });
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        printf("%lld\n", (long long)(r.f2));
        break;
    }
    case 20: {
        S04 r = id_S04((S04){ .f0 = -19 });
        printf("%lld\n", (long long)(r.f0));
        break;
    }
    case 21: {
        S05 r = id_S05((S05){ .f0 = -1.00 });
        printf("%lld\n", (long long)((double)(r.f0) * 4.0));
        break;
    }
    case 22: {
        S06 r = id_S06((S06){ .f0 = -19, .f1 = 18, .f2 = -6, .f3 = -30, .f4 = 7 });
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        printf("%lld\n", (long long)(r.f2));
        printf("%lld\n", (long long)(r.f3));
        printf("%lld\n", (long long)(r.f4));
        break;
    }
    case 23: {
        S07 r = id_S07((S07){ .f0 = -19, .f1 = 18, .f2 = -6 });
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        printf("%lld\n", (long long)(r.f2));
        break;
    }
    case 24: {
        S08 r = id_S08((S08){ .f0 = -19, .f1 = 18 });
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        break;
    }
    case 25: {
        S09 r = id_S09((S09){ .f0 = -1.00, .f1 = 0.25 });
        printf("%lld\n", (long long)((double)(r.f0) * 4.0));
        printf("%lld\n", (long long)((double)(r.f1) * 4.0));
        break;
    }
    case 26: {
        S10 r = id_S10((S10){ .f0 = -19, .f1 = 0.25 });
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)((double)(r.f1) * 4.0));
        break;
    }
    case 27: {
        S11 r = id_S11((S11){ .f0 = -1.00, .f1 = 18 });
        printf("%lld\n", (long long)((double)(r.f0) * 4.0));
        printf("%lld\n", (long long)(r.f1));
        break;
    }
    case 28: {
        S12 r = id_S12((S12){ .f0 = -19, .f1 = 18 });
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        break;
    }
    case 29: {
        S13 r = id_S13((S13){ .f0 = -19, .f1 = 18, .f2 = 1.50 });
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        printf("%lld\n", (long long)((double)(r.f2) * 4.0));
        break;
    }
    case 30: {
        S14 r = id_S14((S14){ .f0 = -1.00, .f1 = 0.25, .f2 = 1.50 });
        printf("%lld\n", (long long)((double)(r.f0) * 4.0));
        printf("%lld\n", (long long)((double)(r.f1) * 4.0));
        printf("%lld\n", (long long)((double)(r.f2) * 4.0));
        break;
    }
    case 31: {
        S15 r = id_S15((S15){ .f0 = -19, .f1 = 0.25 });
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)((double)(r.f1) * 4.0));
        break;
    }
    case 32: {
        S16 r = id_S16((S16){ .f0 = -1.00, .f1 = 18 });
        printf("%lld\n", (long long)((double)(r.f0) * 4.0));
        printf("%lld\n", (long long)(r.f1));
        break;
    }
    case 33: {
        S17 r = id_S17((S17){ .f0 = -1.00, .f1 = 0.25, .f2 = 1.50, .f3 = -1.50 });
        printf("%lld\n", (long long)((double)(r.f0) * 4.0));
        printf("%lld\n", (long long)((double)(r.f1) * 4.0));
        printf("%lld\n", (long long)((double)(r.f2) * 4.0));
        printf("%lld\n", (long long)((double)(r.f3) * 4.0));
        break;
    }
    case 34: {
        S18 r = id_S18((S18){ .f0 = -19, .f1 = 18 });
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        break;
    }
    case 35: {
        S19 r = id_S19((S19){ .f0 = -19, .f1 = 18, .f2 = -6, .f3 = -30, .f4 = 7 });
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        printf("%lld\n", (long long)(r.f2));
        printf("%lld\n", (long long)(r.f3));
        printf("%lld\n", (long long)(r.f4));
        break;
    }
    case 36: {
        S20 r = id_S20((S20){ .f0 = -19, .f1 = 0.25, .f2 = -6 });
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)((double)(r.f1) * 4.0));
        printf("%lld\n", (long long)(r.f2));
        break;
    }
    case 37: {
        S21 r = id_S21((S21){ .f0 = -19, .f1 = 18, .f2 = -6, .f3 = -30 });
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        printf("%lld\n", (long long)(r.f2));
        printf("%lld\n", (long long)(r.f3));
        break;
    }
    case 38: {
        S22 r = id_S22((S22){ .f0 = -1.00, .f1 = 18 });
        printf("%lld\n", (long long)((double)(r.f0) * 4.0));
        printf("%lld\n", (long long)(r.f1));
        break;
    }
    case 39: {
        S23 r = id_S23((S23){ .f0 = -19, .f1 = 0.25 });
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)((double)(r.f1) * 4.0));
        break;
    }
    case 40: {
        long r = mix_S10(-19, (S10){ .f0 = 18, .f1 = 1.50 }, -1.50);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 41: {
        long r = mix_S13(-19, (S13){ .f0 = 18, .f1 = -6, .f2 = -1.50 }, -0.25);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 42: {
        long r = mix_S15(-19, (S15){ .f0 = 18, .f1 = 1.50 }, -1.50);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 43: {
        long r = mix_S20(-19, (S20){ .f0 = 18, .f1 = 1.50, .f2 = -30 }, -0.25);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 44: {
        int r = var_i(-19);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 45: {
        double r = var_d(-1.00);
        printf("%lld\n", (long long)((double)(r) * 4.0));
        break;
    }
    case 46: {
        int r = var_p((void *)0);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 47: {
        long r = var_l(-19, 18);
        printf("%lld\n", (long long)(r));
        break;
    }
    case 48: {
        S08 r = make_S08(-19, 18);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        break;
    }
    case 49: {
        S09 r = make_S09(-1.00, 0.25);
        printf("%lld\n", (long long)((double)(r.f0) * 4.0));
        printf("%lld\n", (long long)((double)(r.f1) * 4.0));
        break;
    }
    case 50: {
        S10 r = make_S10(-19, 0.25);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)((double)(r.f1) * 4.0));
        break;
    }
    case 51: {
        S11 r = make_S11(-1.00, 18);
        printf("%lld\n", (long long)((double)(r.f0) * 4.0));
        printf("%lld\n", (long long)(r.f1));
        break;
    }
    case 52: {
        S12 r = make_S12(-19, 18);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        break;
    }
    case 53: {
        S13 r = make_S13(-19, 18, 1.50);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        printf("%lld\n", (long long)((double)(r.f2) * 4.0));
        break;
    }
    case 54: {
        S14 r = make_S14(-1.00, 0.25, 1.50);
        printf("%lld\n", (long long)((double)(r.f0) * 4.0));
        printf("%lld\n", (long long)((double)(r.f1) * 4.0));
        printf("%lld\n", (long long)((double)(r.f2) * 4.0));
        break;
    }
    case 55: {
        S15 r = make_S15(-19, 0.25);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)((double)(r.f1) * 4.0));
        break;
    }
    case 56: {
        S16 r = make_S16(-1.00, 18);
        printf("%lld\n", (long long)((double)(r.f0) * 4.0));
        printf("%lld\n", (long long)(r.f1));
        break;
    }
    case 57: {
        S17 r = make_S17(-1.00, 0.25, 1.50, -1.50);
        printf("%lld\n", (long long)((double)(r.f0) * 4.0));
        printf("%lld\n", (long long)((double)(r.f1) * 4.0));
        printf("%lld\n", (long long)((double)(r.f2) * 4.0));
        printf("%lld\n", (long long)((double)(r.f3) * 4.0));
        break;
    }
    case 58: {
        S19 r = make_S19(-19, 18, -6, -30, 7);
        printf("%lld\n", (long long)(r.f0));
        printf("%lld\n", (long long)(r.f1));
        printf("%lld\n", (long long)(r.f2));
        printf("%lld\n", (long long)(r.f3));
        printf("%lld\n", (long long)(r.f4));
        break;
    }
    case 59: {
        S21 r = make_S21(-19, 18, -6, -30);
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
