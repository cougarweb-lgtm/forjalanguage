/* libconst.c - dados constantes de membro ELF (M5b P37d): .rodata (tabela, string, ponteiros), .rodata.cst4/.cst8/.cst16 (reais e a mascara do negativo),
 * jump table do switch, .data e .bss. Compilada no WSL (alpine-build) por construir.sh, com e sem -fPIC.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M5b P37d: primeira versao (fixture do censo de secoes ELF; prova que o ligador aceita .rodata e .data.rel.ro).
 */
static const int cst_tabela[8] = {0, 1, 4, 9, 16, 25, 36, 49};
static const char *const cst_nomes[4] = {"zero", "um", "dois", "tres"};
static int cst_contador = 5;
static int cst_zerado[16];

int cst_tabela_de(int i)
{
    return cst_tabela[i & 7];
}

double cst_real(double x)
{
    return x * 2.5 + 0.125;
}

double cst_float(float x)
{
    return (double)(x * 1.5f);
}

double cst_neg(double x)
{
    return -x;
}

int cst_switch(int k)
{
    switch (k) {
    case 0: return k + 10;
    case 1: return k * 3;
    case 2: return k - 4;
    case 3: return k ^ 21;
    case 4: return k * 5 + 1;
    case 5: return k + 100;
    case 6: return k * 9;
    case 7: return 2 - k;
    default: return -1;
    }
}

int cst_nome_len(int i)
{
    const char *p = cst_nomes[i & 3];
    int n = 0;
    while (p[n])
        n++;
    return n;
}

int cst_texto_len(void)
{
    const char *t = "constante de texto da rodata";
    int n = 0;
    while (t[n])
        n++;
    return n;
}

int cst_global(int d)
{
    cst_contador += d;
    return cst_contador;
}

int cst_bss(int i, int v)
{
    cst_zerado[i & 15] = v;
    return cst_zerado[i & 15];
}
