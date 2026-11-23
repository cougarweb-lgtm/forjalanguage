/* real_curto.c - FormatarRealCurto: o menor texto de real que volta ao mesmo valor, sempre posicional (ver real_curto.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - G3 G0c: primeira versao. Procura o menor P em 1..17 (f32: 1..9) tal que "%.{P-1}e" lido de volta (strtod / strtof) da o mesmo valor e escreve os P digitos
 *                    (sem zeros finais) em forma posicional, a do Display do runtime (runtime\nativo\rt_real_curto.c). O "%.Pe" do C e o P digitos mais proximos do valor, o que o Ryu
 *                    do runtime escolhe entre os de mesmo tamanho; os dois so divergem se o mais proximo de P digitos nao voltar e outro de P digitos voltar (borda de potencia de 2).
 * Verificacao: construir.bat && python -I ..\tests\rodar_bateria.py --so bir (dumps de BIR com real)
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "common/real_curto.h"

#define DIGITOS_MAX_F64 17 /* 17 digitos reconstroem qualquer double */
#define DIGITOS_MAX_F32 9  /* 9 digitos reconstroem qualquer float */
#define TEXTO_CIENTIFICO_MAX 40

/* Digitos significativos (sem zeros finais), expoente decimal do 1o digito e sinal. */
typedef struct {
    char dig[DIGITOS_MAX_F64 + 1];
    int nd;
    int e10;
    bool neg;
} RealDigitos;

/* O texto "%.{p-1}e" do valor lido de volta e igual ao valor (como f32 ou como f64)? */
static bool VoltaIgual(const char *texto, double valor, bool f32)
{
    return f32 ? strtof(texto, NULL) == (float)valor : strtod(texto, NULL) == valor;
}

/* O menor "%.{p-1}e" que volta ao mesmo valor; o de 17 (ou 9) digitos sempre volta. */
static void MenorTextoCientifico(double valor, bool f32, char *texto, size_t cap)
{
    int maximo = f32 ? DIGITOS_MAX_F32 : DIGITOS_MAX_F64;

    for (int p = 1; p <= maximo; p++) {
        snprintf(texto, cap, "%.*e", p - 1, valor);
        if (VoltaIgual(texto, valor, f32))
            return;
    }
}

/* "[-]d.ddde[+-]xx" -> digitos sem zeros finais, expoente e sinal. */
static RealDigitos LerCientifico(const char *texto)
{
    RealDigitos r = {{0}, 0, 0, false};
    const char *p = texto;

    if (*p == '-') {
        r.neg = true;
        p++;
    }
    for (; *p != '\0' && *p != 'e'; p++) {
        if (*p != '.' && r.nd < DIGITOS_MAX_F64)
            r.dig[r.nd++] = *p;
    }
    if (*p == 'e')
        r.e10 = atoi(p + 1);
    while (r.nd > 1 && r.dig[r.nd - 1] == '0')
        r.nd--;
    return r;
}

/* Posicional para qualquer expoente: "12345.678", "0.0001", "1000000". */
static void AnexarPosicional(Buf *saida, const RealDigitos *r)
{
    if (r->neg)
        BufAnexar(saida, "-");
    if (r->e10 >= 0) {
        for (int k = 0; k <= r->e10; k++)
            BufAnexarN(saida, k < r->nd ? &r->dig[k] : "0", 1);
        if (r->nd > r->e10 + 1) {
            BufAnexar(saida, ".");
            BufAnexarN(saida, r->dig + r->e10 + 1, (size_t)(r->nd - r->e10 - 1));
        }
        return;
    }
    BufAnexar(saida, "0.");
    for (int k = 0; k < -r->e10 - 1; k++)
        BufAnexar(saida, "0");
    BufAnexarN(saida, r->dig, (size_t)r->nd);
}

void FormatarRealCurto(Buf *saida, double valor, bool f32)
{
    char texto[TEXTO_CIENTIFICO_MAX];
    RealDigitos r;

    if (isnan(valor)) {
        BufAnexar(saida, "nan");
        return;
    }
    if (isinf(valor)) {
        BufAnexar(saida, valor < 0 ? "-inf" : "inf");
        return;
    }
    MenorTextoCientifico(valor, f32, texto, sizeof texto);
    r = LerCientifico(texto);
    AnexarPosicional(saida, &r);
}
