/*
 * rt_arquivo.c - std:arquivo no runtime nativo: os simbolos forja_arquivo_* que std\arquivo.fj declara em 'foreign' sem link (M5 P26c).
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - M5 P26c: forja_arquivo_abrir_ler, _criar, _tamanho, _ler, _gravar e _fechar sobre sys_arquivo_* (rt_arquivo_win.c). Mora na unidade do rt_win_ryu.o (rt_ryu.c), pelo
 *           mesmo motivo do rt_proc.c: o rt_win.o dos exes sem o modulo nao muda um byte (D-M5-11). Sem estado e sem texto: nenhum literal novo na .rdata.
 *
 * Todo tamanho e int64_t (nunca int32): o arquivo pode passar de 2 GiB. A leitura e a gravacao vao em lotes de no maximo 1 GiB por chamada ao sistema (o ReadFile e o WriteFile do
 * Windows contam em 32 bits). Quem le (std\arquivo.fj) pede blocos de 64 KiB; quem grava entrega o buffer inteiro. O buffer e do programa Forja (a lista [dynamic]u8 em heap):
 * a posse, o delete e o R0009 sao dele, nao daqui. Incluido por rt_ryu.c depois de rt_arquivo_win.c; nao compila sozinho.
 */
#include <stddef.h>
#include <stdint.h>
#include "rt_sys.h"
#include "forja_rt_nativo.h"

#define RT_ARQUIVO_LOTE_MAX 0x40000000LL /* 1 GiB */

uint64_t forja_arquivo_abrir_ler(const char *caminho, int32_t *erro)
{
    return sys_arquivo_abrir_ler(caminho, erro);
}

uint64_t forja_arquivo_criar(const char *caminho, int32_t *erro)
{
    return sys_arquivo_criar(caminho, erro);
}

int64_t forja_arquivo_tamanho(uint64_t arquivo)
{
    return sys_arquivo_tamanho(arquivo);
}

/* Le ate quantos bytes (um lote so, no maximo 1 GiB): devolve quantos leu, 0 no fim do arquivo ou -1 em falha. */
int64_t forja_arquivo_ler(uint64_t arquivo, uint8_t *destino, int64_t quantos)
{
    if (quantos <= 0)
        return 0;
    return sys_arquivo_ler(arquivo, destino, quantos > RT_ARQUIVO_LOTE_MAX ? RT_ARQUIVO_LOTE_MAX : quantos);
}

/* Grava os quantos bytes, em lotes: devolve quantos gravou (= quantos) ou -1 na primeira falha. */
int64_t forja_arquivo_gravar(uint64_t arquivo, const uint8_t *origem, int64_t quantos)
{
    int64_t feito = 0;

    while (feito < quantos) {
        int64_t lote = quantos - feito > RT_ARQUIVO_LOTE_MAX ? RT_ARQUIVO_LOTE_MAX : quantos - feito;

        if (sys_arquivo_gravar(arquivo, origem + feito, lote) != lote)
            return -1;
        feito += lote;
    }
    return feito;
}

int32_t forja_arquivo_fechar(uint64_t arquivo)
{
    return sys_arquivo_fechar(arquivo);
}
