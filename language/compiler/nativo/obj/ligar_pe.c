/* ligar_pe.c - o que so o ligador do PE faz depois de aplicar as relocacoes: ordem dos DIR64 (.reloc) e do .pdata.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M5 P27a: extraido do ligar.c 0.8.3 (LigarOrdenarDir64, LigarLeRva e LigarPdataOrdenado) para o ligar.c ficar abaixo de 800 linhas; mesmo comportamento e mesma mensagem.
 * Verificacao: construir.bat && teste_nativo.exe obj <pasta> e python -I ..\tests\nativo\obj\verificar_obj.py
 */
#include <stdlib.h>

#include "nativo/obj/ligar_pe.h"

#define PE_TAM_ENTRADA_PDATA 12u
#define PE_TAM_RVA 4u
#define PE_BITS_BYTE 8u

static int ComparaRva(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a;
    uint32_t y = *(const uint32_t *)b;
    return (x > y) - (x < y);
}

/* O .reloc sai por pagina: a lista de DIR64 tem de vir em ordem crescente. */
static void OrdenarDir64(Imagem *im)
{
    if (im->dir64.n > 1)
        qsort(im->dir64.dados, im->dir64.n, sizeof(uint32_t), ComparaRva);
}

static uint32_t LeRva(const uint8_t *p)
{
    uint32_t v = 0;
    for (uint32_t k = 0; k < PE_TAM_RVA; k++)
        v |= (uint32_t)p[k] << (k * PE_BITS_BYTE);
    return v;
}

/* Entradas RUNTIME_FUNCTION (BeginAddress, EndAddress, UnwindInfo): o EXCEPTION precisa de BeginAddress crescente. */
static bool PdataOrdenado(const Imagem *im, int idx_pdata, Diag *erro)
{
    if (idx_pdata < 0)
        return true;
    const PeSecao *ps = &im->secoes[idx_pdata];
    uint32_t qtd = (uint32_t)(ps->bytes.tam / PE_TAM_ENTRADA_PDATA);
    for (uint32_t i = 1; i < qtd; i++) {
        const uint8_t *ant = (const uint8_t *)ps->bytes.dados + (i - 1) * PE_TAM_ENTRADA_PDATA;
        const uint8_t *atu = ant + PE_TAM_ENTRADA_PDATA;
        if (LeRva(ant) > LeRva(atu))
            return DiagRelatar(erro, X0905, 0, "autoconferencia do PE: .pdata fora de ordem na entrada %u", i);
    }
    return true;
}

bool LigarPeFinalizar(Imagem *im, int idx_pdata, Diag *erro)
{
    OrdenarDir64(im);
    return PdataOrdenado(im, idx_pdata, erro);
}
