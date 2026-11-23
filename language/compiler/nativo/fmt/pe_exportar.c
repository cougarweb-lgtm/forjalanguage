/* pe_exportar.c - export directory do PE32+ de uma DLL Forja (M3 P22a): a secao ".edata" com a tabela de exportacao do ld (nomes em ordem ASCII, ordinais base 1).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 P22a: primeira versao. IMAGE_EXPORT_DIRECTORY (40 B, TimeDateStamp 0: build reprodutivel) + EAT (RVA de cada funcao) + ENPT (RVA de cada nome)
 *                    + EOT (indice na EAT de cada nome; como EAT e ENPT andam juntas, o indice k vale k) + os nomes e o nome do arquivo da DLL. Ordinal do k-esimo nome = k + 1.
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes) e python -I ..\tests\nativo\pe\verificar_pe.py
 */
#include <stdlib.h>
#include <string.h>

#include "nativo/fmt/pe.h"

#define EXP_TAM_DIRETORIO 40u
#define EXP_TAM_RVA 4u
#define EXP_TAM_ORDINAL 2u
#define EXP_ORDINAL_BASE 1u
#define EXP_BITS_BYTE 8u

/* Onde cada parte cai (RVA): o diretorio abre a secao, depois EAT, ENPT, EOT e, por ultimo, os textos. */
typedef struct {
    uint32_t eat, enpt, eot, textos;
} ExpPlano;

static void ExpAnexarLe(Buf *b, uint64_t v, uint32_t n)
{
    for (uint32_t k = 0; k < n; k++) {
        char byte = (char)(v >> (k * EXP_BITS_BYTE));
        BufAnexarN(b, &byte, 1);
    }
}

static int ExpComparaNome(const void *a, const void *b)
{
    return strcmp(((const PeExporta *)a)->nome, ((const PeExporta *)b)->nome);
}

static bool ExpTemRepetido(const PeExporta *ordenados, uint32_t n)
{
    for (uint32_t k = 1; k < n; k++) {
        if (strcmp(ordenados[k - 1].nome, ordenados[k].nome) == 0)
            return true;
    }
    return false;
}

static ExpPlano ExpPlanejar(uint32_t base, uint32_t n)
{
    ExpPlano p;

    p.eat = base + EXP_TAM_DIRETORIO;
    p.enpt = p.eat + n * EXP_TAM_RVA;
    p.eot = p.enpt + n * EXP_TAM_RVA;
    p.textos = p.eot + n * EXP_TAM_ORDINAL;
    return p;
}

/* IMAGE_EXPORT_DIRECTORY: Characteristics, TimeDateStamp, versoes, Name, Base, NumberOfFunctions, NumberOfNames, AddressOfFunctions/Names/NameOrdinals. */
static void ExpDiretorio(Buf *b, const ExpPlano *p, uint32_t rva_nome_dll, uint32_t n)
{
    ExpAnexarLe(b, 0, 4);
    ExpAnexarLe(b, 0, 4);
    ExpAnexarLe(b, 0, 4);
    ExpAnexarLe(b, rva_nome_dll, 4);
    ExpAnexarLe(b, EXP_ORDINAL_BASE, 4);
    ExpAnexarLe(b, n, 4);
    ExpAnexarLe(b, n, 4);
    ExpAnexarLe(b, p->eat, 4);
    ExpAnexarLe(b, p->enpt, 4);
    ExpAnexarLe(b, p->eot, 4);
}

/* Entrada de ExpMontar: onde a secao cai (base), o nome do arquivo da DLL e os n simbolos JA em ordem ASCII. */
typedef struct {
    uint32_t base;
    const char *nome_dll;
    const PeExporta *ordenados;
    uint32_t n;
} ExpPedido;

/* Os RVAs dos nomes, na ordem dos simbolos, sao os dos textos que vem logo depois da EOT. */
static void ExpMontar(Buf *b, const ExpPedido *e)
{
    ExpPlano p = ExpPlanejar(e->base, e->n);
    uint32_t rva_texto = p.textos;

    for (uint32_t k = 0; k < e->n; k++)
        rva_texto += (uint32_t)strlen(e->ordenados[k].nome) + 1;
    ExpDiretorio(b, &p, rva_texto, e->n);
    for (uint32_t k = 0; k < e->n; k++)
        ExpAnexarLe(b, e->ordenados[k].rva, EXP_TAM_RVA);
    rva_texto = p.textos;
    for (uint32_t k = 0; k < e->n; k++) {
        ExpAnexarLe(b, rva_texto, EXP_TAM_RVA);
        rva_texto += (uint32_t)strlen(e->ordenados[k].nome) + 1;
    }
    for (uint32_t k = 0; k < e->n; k++)
        ExpAnexarLe(b, k, EXP_TAM_ORDINAL);
    for (uint32_t k = 0; k < e->n; k++)
        BufAnexarN(b, e->ordenados[k].nome, strlen(e->ordenados[k].nome) + 1);
    BufAnexarN(b, e->nome_dll, strlen(e->nome_dll) + 1);
}

bool PeReservarExportacoes(Imagem *im, const char *nome_dll, const PeExporta *simbolos, uint32_t n)
{
    PeExporta *ordenados;
    Buf edata = {0};
    Diag erro = {0};
    uint32_t base = ImagemProximoRva(im);
    bool ok;

    if (n == 0)
        return false;
    ordenados = malloc(n * sizeof *ordenados);
    if (ordenados == NULL)
        FalhaInterna("pe: memoria esgotada montando a .edata");
    memcpy(ordenados, simbolos, n * sizeof *ordenados);
    qsort(ordenados, n, sizeof *ordenados, ExpComparaNome);
    ok = !ExpTemRepetido(ordenados, n);
    if (ok) {
        ExpMontar(&edata, &(ExpPedido){base, nome_dll, ordenados, n});
        PeSecaoPedido secao = {".edata", PE_SECAO_LEITURA, &edata, (uint32_t)edata.tam};
        if (ImagemSecao(im, &secao, &erro) != base)
            FalhaInterna("pe: .edata recusado ou fora do RVA previsto: %s", erro.causa);
        im->dll = true;
        for (uint32_t k = 0; k < n; k++)
            *(PeExporta *)VecEmpurrar(&im->exportacoes) = ordenados[k];
    }
    BufLiberar(&edata);
    free(ordenados);
    return ok;
}
