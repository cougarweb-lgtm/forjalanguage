/* nativo_interface.c - a struct estatica que forja_dll_info devolve: magic, formato, hash FNV-1a 64 do texto canonico da interface, endereco e tamanho do texto.
 * Versao: 0.1.1
 * Changelog: 0.1.1 - M8 P2: AbrirRdata vira NativoAbrirRdata (publica; a tabela FPF1 do --perfil, nativo_perfil.c, a reaproveita); mesmo codigo.
 *            0.1.0 - M8 H1 (D-M8-2): primeira versao. O texto sai de EmitirInterfaceDll (a mesma IR do .h); a BirFuncao forja_dll_info (bir_construir_fronteira.c)
 *                    devolve o endereco desta struct e entra no export directory como qualquer export.
 * Verificacao: python -I ..\tests\rodar_bateria.py --so interop (caso dinamica\dll_info)
 */
#include <string.h>

#include "backend/interface_dll.h"
#include "common/hash.h"
#include "nativo/nativo_interface.h"

#define DLL_INFO_ALINHAMENTO 8u
#define DLL_INFO_BITS_BYTE 8u

static void AnexarLe(Buf *b, uint64_t v, uint32_t n)
{
    for (uint32_t k = 0; k < n; k++) {
        char byte = (char)(v >> (k * DLL_INFO_BITS_BYTE));
        BufAnexarN(b, &byte, 1);
    }
}

/* O indice do .rdata do programa, aberto aqui se o programa nao tem texto nenhum; o alinhamento sobe a 8 (o campo texto e um ponteiro de 8 bytes). */
uint32_t NativoAbrirRdata(Objeto *o)
{
    uint32_t rdata = UINT32_MAX;
    ObjSecao *s;

    for (uint32_t k = 0; k < o->secoes.n && rdata == UINT32_MAX; k++) {
        if (strcmp(((ObjSecao *)VecEm(&o->secoes, k))->nome, ".rdata") == 0)
            rdata = k;
    }
    if (rdata == UINT32_MAX)
        rdata = ObjSecaoAbrir(o, ".rdata", OS_RDATA, DLL_INFO_ALINHAMENTO);
    s = VecEm(&o->secoes, rdata);
    if (s->alinhamento < DLL_INFO_ALINHAMENTO)
        s->alinhamento = DLL_INFO_ALINHAMENTO;
    return rdata;
}

bool NativoInterfaceDll(Objeto *programa, const NativoPedido *pedido)
{
    Buf texto = {0};
    uint32_t rdata;
    ObjSecao *s;
    uint32_t off;
    uint32_t simbolo;

    if (!pedido->dll)
        return true;
    rdata = NativoAbrirRdata(programa);
    s = VecEm(&programa->secoes, rdata);
    EmitirInterfaceDll(pedido->ir, pedido->tipos, pedido->nomes, &texto);
    while (s->bytes.tam % DLL_INFO_ALINHAMENTO != 0)
        BufAnexarN(&s->bytes, "", 1);
    off = (uint32_t)s->bytes.tam;
    AnexarLe(&s->bytes, DLL_INFO_MAGIC, 4);
    AnexarLe(&s->bytes, DLL_INFO_FORMATO, 4);
    AnexarLe(&s->bytes, Hash64(texto.dados, texto.tam, HASH64_SEMENTE), 8);
    AnexarLe(&s->bytes, 0, 8); /* endereco do texto: o ADDR64 abaixo, que o ligador escreve (o adendo fica na relocacao, nunca nos bytes) */
    AnexarLe(&s->bytes, texto.tam, 8);
    BufAnexarN(&s->bytes, texto.dados, texto.tam);
    BufAnexarN(&s->bytes, "", 1);
    simbolo = ObjSimboloDefinir(programa, DLL_INFO_SIMBOLO_DADOS, (int32_t)rdata, off);
    ObjRelocar(programa, (ObjAlvoReloc){rdata, off + DLL_INFO_OFF_TEXTO}, (ObjCorrecao){OR_ADDR64, simbolo, DLL_INFO_TAM});
    BufLiberar(&texto);
    return true;
}
