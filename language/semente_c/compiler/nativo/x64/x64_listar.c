/* x64_listar.c - registros do --debug-code colhidos durante a geracao: uma ListaFuncao por funcao e uma ListaInstr por instrucao emitida.
 * Versao: 0.3.0
 * Changelog: 0.3.0 - M2 P11a: X64ListarFuncao guarda os slots de memoria da funcao (ListaFuncao.slots_agregado).
 *            0.2.0 - backend x64 P8b: deixa de escrever texto; guarda registros (alvo.h) que nativo_debug.c formata em texto e em JSON. Os bytes
 *                    finais vem da imagem ligada (o salto e o call ja resolvidos), nao do que o codificador emitiu.
 *            0.1.0 - backend x64 P8: X64ListarFuncao, X64ListarInstr e X64ListarQuadro (so rodam com c->lista != NULL).
 * Verificacao: construir.bat && forja build --backend x64 --debug-code asm,quadros <arq.fj> -o <saida.exe>
 */
#include <stdlib.h>
#include <string.h>

#include "nativo/x64/x64_interno.h"

/* Texto (com NUL) no pool da listagem; devolve o offset. */
static uint32_t GuardarTexto(AlvoListagem *lista, const char *texto)
{
    uint32_t offset = (uint32_t)lista->textos.tam;

    BufAnexarN(&lista->textos, texto, strlen(texto) + 1);
    return offset;
}

static ListaFuncao *FuncaoAtual(const X64Ctx *c)
{
    return VecEm(&c->lista->funcoes, c->lista->funcoes.n - 1);
}

void X64ListarFuncao(X64Ctx *c)
{
    ListaFuncao *f = VecEmpurrar(&c->lista->funcoes);

    f->nome = GuardarTexto(c->lista, c->p->simbolos[c->f->simbolo].nome);
    f->inicio = c->inicio;
    f->pos = c->f->pos;
    f->nslots_agregado = c->f->nslots;
    f->slots_agregado = NULL;
    if (c->f->nslots == 0)
        return;
    f->slots_agregado = malloc(c->f->nslots * sizeof *f->slots_agregado);
    if (f->slots_agregado == NULL)
        FalhaInterna("x64: memoria esgotada na listagem dos slots");
    for (uint32_t k = 0; k < c->f->nslots; k++)
        f->slots_agregado[k] = (ListaSlot){.tam = c->f->slots[k].tam, .alinhamento = c->f->slots[k].alinhamento, .offset = c->slot_off[k]};
}

/* Nome do simbolo para o qual a instrucao aponta (call, lea [rip+d]); 0 se nao aponta. */
static uint32_t SimboloDaInstr(const X64Ctx *c, const MInstr *mi)
{
    for (uint8_t k = 0; k < mi->nops; k++) {
        const MOperando *o = &mi->op[k];
        bool aponta = o->tipo == MO_SIMBOLO || (o->tipo == MO_MEM && o->base == X64_RIP);

        if (aponta)
            return GuardarTexto(c->lista, ((const ObjSimbolo *)VecEm(&c->o->simbolos, o->simbolo))->nome) + 1;
    }
    return 0;
}

/* Bloco da BIR e posicao dentro dele; sem instrucao em selecao (prologo e epilogo) nao ha. */
static void LocalNaBir(const X64Ctx *c, ListaInstr *registro)
{
    registro->bloco = LISTA_SEM_INDICE;
    registro->indice = LISTA_SEM_INDICE;
    registro->pos = BIR_SEM_POS;
    if (c->instr == NULL)
        return;
    registro->bloco = c->ordem[c->pos_ordem];
    registro->indice = (uint32_t)(c->instr - c->f->blocos[registro->bloco].instrs);
    registro->pos = c->instr->pos;
}

void X64ListarInstr(X64Ctx *c, const MInstr *mi, uint32_t tam)
{
    ListaInstr *registro = VecEmpurrar(&c->lista->instrs);
    Buf texto = {0};

    X64Formatar(mi, &texto);
    registro->funcao = (uint32_t)(c->lista->funcoes.n - 1);
    registro->off = X64Offset(c) - tam;
    registro->tam = tam;
    registro->origem = c->origem;
    registro->texto = GuardarTexto(c->lista, texto.dados != NULL ? texto.dados : "?");
    registro->simbolo = SimboloDaInstr(c, mi);
    LocalNaBir(c, registro);
    BufLiberar(&texto);
}

void X64ListarQuadro(const X64Ctx *c, uint32_t fim, const uint8_t *unwind, uint32_t tam_unwind)
{
    ListaFuncao *f = FuncaoAtual(c);

    f->fim = fim;
    f->quadro = c->quadro.tam;
    f->slots = c->quadro.slots;
    f->maxargs = c->quadro.maxargs;
    f->sondagem = c->quadro.sondar ? 1 : 0;
    f->push = c->prologo.push;
    f->fpreg = c->prologo.fpreg;
    f->alocacao = c->prologo.alocacao;
    f->tam_unwind = (uint8_t)tam_unwind;
    memcpy(f->unwind, unwind, tam_unwind);
}
