/* terceiros.h - monta o 4o objeto do ligador a partir dos membros de bibliotecas estaticas que a selecao (estatica.h) puxou: a uniao COFF, o fecho dos fracos e dos comuns, os
 * slots __imp_ dos simbolos que o programa pede a uma estatica e os thunks das funcoes de DLL que o codigo de terceiros chama sem __imp_ (P23e do M3b).
 * Versao: 0.4.0
 * Changelog: 0.4.0 - M6d P92c: TerceirosPedido.libmem e TerceirosCriarCelula (era TrCriarCelula, static).
 *            0.3.0 - M5b P36: TerceirosPedido.formato (membros ELF no alvo Linux).
 *            0.2.0 - M3b P23f: TerceirosRelato.tem_ctors.
 *            0.1.1 - M3b P23e: TerceirosPedido.outros.
 *             0.1.0 - M3b P23e: TerceirosMontar.
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes) e python -I ..\tests\nativo\verificar_nativo.py --pasta m3/estatica
 */
#ifndef FORJA_NATIVO_OBJ_TERCEIROS_H
#define FORJA_NATIVO_OBJ_TERCEIROS_H

#include "common/diag.h"
#include "common/vec.h"
#include "nativo/obj/libmem.h"
#include "nativo/obj/ligar.h"
#include "nativo/obj/objeto.h"

typedef struct {
    const Vec *membros;               /* EstaticaMembro, na ordem em que foram puxados */
    const char *const *celulas;       /* nomes C (sem __imp_) que o programa pede a uma estatica: ganham o slot __imp_<nome> com o endereco da definicao */
    uint32_t qtd_celulas;
    const ImportTabela *tabelas;      /* as DLLs de import: um simbolo indefinido (sem __imp_) que esta numa tabela ganha um thunk "jmp [__imp_<nome>]" */
    uint32_t qtd_tabelas;
    const Objeto *const *outros;      /* programa, runtime e Ryu da ligacao: o que eles definem (memcpy do runtime) nao ganha thunk de DLL */
    uint32_t qtd_outros;
    ImagemFormato formato;            /* IMAGEM_ELF: os membros sao ELF e entram na uniao (M5b P36); IMAGEM_PE: membro ELF e X0105 */
    const TerceirosLibmem *libmem;    /* M6d P92c: so em dev (nao DLL) e com algum arquivo contado (qtd_slots > 0) a familia malloc das libs vai para os involucros; NULL = como antes, byte a byte */
} TerceirosPedido;

/* O que o .drectve dos membros trouxe e nao virou ligacao: ignoradas diretivas (primeira_ignorada e a 1a delas, no Objeto) e exportados (Vec de const char *, iniciado por quem chama,
 * nomes na arena do Objeto; so a DLL os usa). */
typedef struct {
    uint32_t ignoradas;
    const char *primeira_ignorada;
    Vec *exportados;
    bool tem_ctors;                   /* algum membro tem construtor (.CRT$XC*, .ctors): o objeto leva forja_ctors_ini/forja_ctors_fim e a entrada tem de ser a forja_entrada_ctor do legado */
} TerceirosRelato;

/* saida vem de ObjIniciar e fica com todos os membros (sempre libere com ObjLiberar). false com X0903/X0106 em erro (um membro torto, relocacao desconhecida, NODUPLICATES). */
bool TerceirosMontar(const TerceirosPedido *pedido, Objeto *saida, TerceirosRelato *relato, Diag *erro);

/* Cria o slot __imp_<nome> (8 bytes + ADDR64 para <nome>) numa secao .rdata$imp nova. Quem chama garante que __imp_<nome> ainda nao esta definido. */
void TerceirosCriarCelula(Objeto *o, const char *nome);

#endif
