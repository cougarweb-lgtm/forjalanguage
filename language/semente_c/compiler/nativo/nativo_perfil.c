/* nativo_perfil.c - o runtime do perfil e a tabela FPF1 de nomes do --perfil (M8 P2, D-M8-5; ver nativo_perfil.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M8 P2: primeira versao (NativoPerfilLer, NativoPerfilTabela).
 * Verificacao: construir_rapido.py && forja build --perfil fib.fj -o fib.exe && fib.exe (o fib.exe.perfil.txt diz fib com 21891 chamadas para fib(20))
 */
#include <stdlib.h>
#include <string.h>

#include "common/base.h"
#include "nativo/nativo_interface.h"
#include "nativo/nativo_perfil.h"
#include "nativo/obj/coff.h"
#include "nativo/optimizer/perfil.h"
#include "nativo/rt_embutido.h"

#define PERFIL_MAGIC 0x31465046u /* "FPF1" em little-endian */
#define PERFIL_BYTES_U32 4u
#define PERFIL_BITS_BYTE 8u
#define PERFIL_ALINHAMENTO_TABELA 8u

bool NativoPerfilLer(const NativoPedido *pedido, bool alvo_elf, Objeto *saida, Diag *erro)
{
    size_t n = 0;
    const uint8_t *bytes;

    if (!pedido->perfil)
        return true;
    bytes = alvo_elf ? RtNativoLinuxPerfil(&n) : RtNativoWinPerfil(&n);
    return CoffLer(bytes, n, saida, erro);
}

static void AnexarU32(Buf *b, uint32_t v)
{
    for (uint32_t k = 0; k < PERFIL_BYTES_U32; k++) {
        char byte = (char)(v >> (k * PERFIL_BITS_BYTE));

        BufAnexarN(b, &byte, 1);
    }
}

/* Os textos "modulo.proc" das procs de usuario, um atras do outro (NUL no fim de cada); offsets[k] = onde o k-esimo comeca no bloco de textos. Devolve a contagem. */
static uint32_t ColherNomes(const BirPrograma *bir, Buf *textos, uint32_t *offsets)
{
    uint32_t n = 0;
    char nome[PERFIL_NOME_MAX];

    for (uint32_t i = 0; i < bir->nfuncoes; i++) {
        if (!OtimPerfilProc(bir, &bir->funcoes[i]))
            continue;
        OtimPerfilNome(bir, &bir->funcoes[i], nome, sizeof nome);
        offsets[n++] = (uint32_t)textos->tam;
        BufAnexarN(textos, nome, strlen(nome) + 1);
    }
    return n;
}

bool NativoPerfilTabela(Objeto *programa, const BirPrograma *bir, const NativoPedido *pedido)
{
    Buf textos = {0};
    uint32_t *offsets;
    uint32_t n;
    uint32_t rdata;
    uint32_t off;
    ObjSecao *s;

    if (!pedido->perfil)
        return true;
    offsets = malloc(((size_t)bir->nfuncoes + 1u) * sizeof *offsets);
    if (offsets == NULL)
        FalhaInterna("perfil: memoria esgotada");
    n = ColherNomes(bir, &textos, offsets);
    rdata = NativoAbrirRdata(programa);
    s = VecEm(&programa->secoes, rdata);
    while (s->bytes.tam % PERFIL_ALINHAMENTO_TABELA != 0)
        BufAnexarN(&s->bytes, "", 1);
    off = (uint32_t)s->bytes.tam;
    AnexarU32(&s->bytes, PERFIL_MAGIC);
    AnexarU32(&s->bytes, n);
    for (uint32_t k = 0; k < n; k++)
        AnexarU32(&s->bytes, (uint32_t)(2u * PERFIL_BYTES_U32 + n * PERFIL_BYTES_U32) + offsets[k]);
    BufAnexarN(&s->bytes, textos.dados, textos.tam);
    ObjSimboloDefinir(programa, PERFIL_TABELA_SIMBOLO, (int32_t)rdata, off);
    BufLiberar(&textos);
    free(offsets);
    return true;
}
