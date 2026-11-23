/* sem_posse.c - posse de memoria: origem do local no delete, em = escopo (defer sintetico) e posse estatica SEM fluxo (E0316/E0317).
 * Versao: 0.8.0
 * Changelog: 0.8.0 - M6c P87: rastreado de handle de lib C (ALOC_HANDLE; regras e textos em sem_posse_handle.c): ponto de posse, reatribuicao, saida e fim de bloco o desviam; PONTO_EMPRESTIMO vale como argumento para heap/escopo.
 *            0.7.0 - B01h: ValorVeioDeChamadaHeap passa por try (texto "veio de uma chamada"); reatribuicao grava em LocalRastreado.origem a origem do valor novo (heap ou PARAM), e nao so o estado VIVO.
 *            0.6.0 - B01g: o resultado de proc do usuario com 'alloc = heap' e rastreado como o de new/make (LocalRastreado.de_chamada); os textos do E0317 de heap dizem que o valor veio de uma chamada.
 *            0.5.0 - B01b: local com origem ALOC_PARAM (memoria do alocador do chamador, 'alloc = alloc') e rastreado como o de heap, com textos proprios; delete de texto/objeto dele = E0310, de colecao vale.
 *            0.4.0 - Linguagem v1.9 P7: ErroDeOrigem usa SemColecaoDona (Arena libera soa dinamica com delete, como a lista).
 *            0.3.0 - Linguagem v1.8b P6: ErroDeTextoSemDono (ORIGEM_C: texto de proc foreign e emprestado, E0310 no delete).
 *            0.2.0 - Linguagem v1.8 P7: rastreados (let/var com heap/escopo), pontos de posse, saida, fim de bloco, reatribuicao e SemDeferDoEscopo.
 *            0.1.0 - Linguagem v1.8 P6a: SemConferirDelete (3.12 (e) sem a parte de rastreados, que chega no P7).
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_interno.h"

/* Rastreado de posse: heap, ou memoria do alocador do chamador (B01b), que tem de ser entregue como a de heap. */
static inline bool OrigemRastreadaComoHeap(uint8_t origem)
{
    return origem == ALOC_HEAP || origem == ALOC_PARAM;
}

/* Complemento do texto do E0317 de heap: o valor nasceu de uma chamada de proc do usuario, nao de new/make/format. */
static const char *OrigemDaChamada(const LocalRastreado *r)
{
    return r->de_chamada ? " (veio de uma chamada com alloc = heap)" : "";
}

/* O valor do let/var/atribuicao e o resultado de proc do usuario (REF_PROC) com alocador heap? */
static bool ValorVeioDeChamadaHeap(const Expr *valor)
{
    if (valor->tipo_no == EX_EMBRULHA || valor->tipo_no == EX_TRY) valor = valor->a;
    return valor->tipo_no == EX_CHAMADA && valor->ref == REF_PROC && valor->alocacao == ALOC_HEAP;
}

/* Texto sem dono Forja: literal (v1.8) ou devolvido por proc foreign (v1.8b, emprestado da biblioteca C). */
static bool ErroDeTextoSemDono(Semantica *s, const Expr *alvo, uint8_t origem)
{
    const char *nome = SemNomeTexto(s, alvo->nome);
    if (origem == ORIGEM_C) {
        TipoClasse classe = TipoClasseDe(&s->tipos, alvo->tipo);
        bool ponteiro = classe == TC_PONTEIRO || classe == TC_ANULAVEL;
        return SemErro(s, E0310, alvo->pos, "'%s' veio de um proc foreign: %s e emprestado da biblioteca C e delete nao o libera; "
                       "se a biblioteca pede para liberar, chame a funcao dela", nome, ponteiro ? "o objeto" : "o texto");
    }
    return SemErro(s, E0310, alvo->pos, "'%s' guarda um texto literal: delete so libera o que veio de format(..., alloc = heap)", nome);
}

/* Recusa o delete que nao combina com a origem do local: texto literal, objeto de Arena, de Pool sem 'em' ou de outro alocador. */
static bool ErroDeOrigem(Semantica *s, const Expr *alvo, uint8_t origem, bool com_pool)
{
    const char *nome = SemNomeTexto(s, alvo->nome);
    bool lista = SemColecaoDona(s, alvo->tipo);
    if (origem == ALOC_ESCOPO) return SemErro(s, E0316, alvo->pos, "'%s' e de escopo: o delete e automatico no fim do bloco", nome);
    if (origem >= ORIGEM_LITERAL) return ErroDeTextoSemDono(s, alvo, origem);
    if (origem == ALOC_PARAM && !lista) {
        return SemErro(s, E0310, alvo->pos, "'%s' veio do alocador do chamador (alloc): o delete so libera heap e a memoria pode ser de Arena; devolva-a por return, guarde-a num campo ou entregue-a a outra proc", nome);
    }
    if (origem == ALOC_ARENA && !lista) {
        return SemErro(s, E0310, alvo->pos, "objeto de Arena nao se libera um a um: arena_reset ou arena_destroy liberam tudo");
    }
    if (origem == ALOC_POOL && !com_pool) return SemErro(s, E0310, alvo->pos, "objeto de Pool volta com delete(%s, alloc = pool)", nome);
    if (origem != ALOC_POOL && com_pool) {
        return SemErro(s, E0310, alvo->pos, "delete(p, alloc = pool) pede ^T vindo de new(T, alloc = pool)");
    }
    return true;
}

/* Rastreado que o nome liga (local do proc em analise); NULL se 'e' nao e nome de local rastreado. */
LocalRastreado *SemAcharRastreado(Semantica *s, const Expr *e)
{
    if (e->tipo_no != EX_NOME || e->ref != REF_LOCAL || e->modulo != 0) return NULL;
    for (size_t k = s->rastreados.n; k > 0; k--) {
        LocalRastreado *r = VecEm(&s->rastreados, k - 1);
        if (r->local_id == e->ref_id) return r;
    }
    return NULL;
}

/* delete(local ...): confere a origem guardada no let/var e marca o rastreado de heap como LIBERADO (D20: vale em qualquer estado). */
bool SemConferirDelete(Semantica *s, const Expr *e)
{
    const Expr *alvo = e->args[0].valor;
    const LocalInfo *local;
    LocalRastreado *r;
    if (alvo->tipo_no != EX_NOME || alvo->modulo != 0) return true;
    local = EscopoBuscar(&s->escopo, alvo->nome);
    if (local == NULL || local->origem == ALOC_NENHUM) return true;
    r = s->rastreados.n == 0 ? NULL : SemAcharRastreado(s, alvo);
    if (!ErroDeOrigem(s, alvo, local->origem, e->nargs > 1)) {
        if (r != NULL && r->origem == ALOC_PARAM) r->estado = POSSE_LIBERADO; /* o E0310 ja disse o que fazer: sem E0317 em cascata */
        return false;
    }
    if (r == NULL) return true;
    if (OrigemRastreadaComoHeap(r->origem)) r->estado = POSSE_LIBERADO;
    return true;
}

/* O nome rastreado R chega a um ponto de posse: copia, guarda, saida ou argumento (3.12 (b)). */
static bool PosseDoNome(Semantica *s, const Expr *e, LocalRastreado *r, PontoDePosse ponto)
{
    if (r->origem == ALOC_HANDLE) return SemPosseDoHandle(s, e, r, ponto);
    if (ponto >= PONTO_EMPRESTIMO) ponto = PONTO_ARGUMENTO; /* heap e escopo: o argumento de qualquer proc segue a regra de sempre */
    const char *nome = SemNomeTexto(s, r->nome);
    if (r->origem == ALOC_ESCOPO) {
        if (ponto == PONTO_ARGUMENTO) return true; /* o chamado so empresta (risco R4) */
        return SemErro(s, E0316, e->pos, "'%s' e de escopo: nao sai do bloco (return, campo, elemento, append ou copia); use alloc = heap para entregar", nome);
    }
    if (ponto == PONTO_COPIA && r->origem == ALOC_PARAM) {
        return SemErro(s, E0317, e->pos, "'%s' e dono de memoria do alocador do chamador (alloc): copiar cria dois donos; entregue (proc, return, campo) ou use &%s", nome, nome);
    }
    if (ponto == PONTO_COPIA) {
        return SemErro(s, E0317, e->pos, "'%s' e dono de memoria de heap%s: copiar cria dois donos; entregue (proc, return, campo) ou use &%s", nome, OrigemDaChamada(r), nome);
    }
    r->estado = POSSE_ENTREGUE;
    return true;
}

bool SemConferirPosseLento(Semantica *s, const Expr *e, PontoDePosse ponto)
{
    LocalRastreado *r;
    if (e->tipo_no == EX_EMBRULHA) e = e->a;
    if (e->tipo_no == EX_UNARIO && e->op == OP_ENDERECO) {
        r = SemAcharRastreado(s, e->a);
        if (r != NULL && OrigemRastreadaComoHeap(r->origem)) r->estado = POSSE_ENTREGUE;
        return true;
    }
    r = SemAcharRastreado(s, e);
    return r == NULL || PosseDoNome(s, e, r, ponto);
}

/* Atribuicao sem op a um rastreado: escopo nao se reatribui; heap VIVO vazaria; heap solto volta a VIVO se recebe outro heap. */
bool SemConferirReatribuicaoLento(Semantica *s, const Stmt *st)
{
    LocalRastreado *r = SemAcharRastreado(s, st->alvo);
    if (r == NULL) return true;
    if (r->origem == ALOC_HANDLE) return SemReatribuicaoDoHandle(s, st, r);
    const char *nome = SemNomeTexto(s, r->nome);
    if (r->origem == ALOC_ESCOPO) return SemErro(s, E0316, st->alvo->pos, "'%s' e de escopo: nao se reatribui", nome);
    if (r->estado == POSSE_VIVO && r->origem == ALOC_PARAM) {
        return SemErro(s, E0317, st->alvo->pos, "'%s' ainda segura memoria do alocador do chamador (alloc) sem entrega: devolva-a ou entregue-a antes de reatribuir", nome);
    }
    if (r->estado == POSSE_VIVO) {
        return SemErro(s, E0317, st->alvo->pos, "'%s' ainda segura memoria de heap%s: delete(%s) antes de reatribuir", nome, OrigemDaChamada(r), nome);
    }
    uint8_t origem_nova = SemOrigemDoValor(st->expr);
    if (OrigemRastreadaComoHeap(origem_nova)) {
        r->estado = POSSE_VIVO;
        r->origem = origem_nova; /* B01h: o E0317 e os textos seguem o valor novo (PARAM ou heap) */
        r->de_chamada = ValorVeioDeChamadaHeap(st->expr);
    }
    return true;
}

/* Proc do usuario (ou foreign): cada argumento rastreado de heap e entregue; o de escopo so empresta. Handle: so o parametro
 * destroys/takes do proc entrega; os demais emprestam (M6c P87). */
bool SemPosseDosArgumentosLento(Semantica *s, const Expr *chamada, const Decl *proc)
{
    PontoDePosse emprestimo = proc->externo ? PONTO_EMPRESTIMO : PONTO_EMPRESTIMO_FORJA;
    for (uint32_t k = 0; k < chamada->nargs; k++) {
        PontoDePosse ponto = SemEntregaDoArgumento(proc, chamada->args[k].indice) ? PONTO_ARGUMENTO : emprestimo;
        if (!SemConferirPosseLento(s, chamada->args[k].valor, ponto)) return false;
    }
    return true;
}

/* return, break ou continue: heap ainda VIVO (a partir da marca) vazaria nesta saida. */
bool SemConferirSaidaLenta(Semantica *s, uint32_t marca, const Stmt *st)
{
    for (size_t k = marca; k < s->rastreados.n; k++) {
        const LocalRastreado *r = VecEm(&s->rastreados, k);
        if (r->estado != POSSE_VIVO) continue;
        if (r->origem == ALOC_HANDLE) return SemErroDoHandleVivo(s, r, st->pos, true);
        if (!OrigemRastreadaComoHeap(r->origem)) continue;
        const char *nome = SemNomeTexto(s, r->nome);
        if (r->origem == ALOC_PARAM) {
            return SemErro(s, E0317, st->pos, "'%s' guarda memoria do alocador do chamador (alloc) e esta saida (return, break ou continue) a deixa sem entregar: devolva-a por return, guarde-a num campo ou entregue-a a outra proc", nome);
        }
        return SemErro(s, E0317, st->pos, "'%s' segura memoria de heap%s e esta saida (return, break ou continue) a deixa viva: use defer delete(%s) logo depois do let", nome, OrigemDaChamada(r), nome);
    }
    return true;
}

/* Fim do bloco: os rastreados declarados nele saem de cena; heap ainda VIVO vazou. */
bool SemConferirFimDoBloco(Semantica *s, uint32_t marca)
{
    for (size_t k = marca; k < s->rastreados.n; k++) {
        const LocalRastreado *r = VecEm(&s->rastreados, k);
        if (r->estado != POSSE_VIVO) continue;
        if (r->origem == ALOC_HANDLE) return SemErroDoHandleVivo(s, r, r->pos, false);
        if (!OrigemRastreadaComoHeap(r->origem)) continue;
        const char *nome = SemNomeTexto(s, r->nome);
        if (r->origem == ALOC_PARAM) {
            return SemErro(s, E0317, r->pos, "'%s' recebe memoria do alocador do chamador (alloc) e nao e entregue ate o fim do bloco: devolva-a por return, guarde-a num campo ou entregue-a a outra proc", nome);
        }
        return SemErro(s, E0317, r->pos, "'%s' recebe memoria de heap%s e nao e liberado nem entregue ate o fim do bloco: delete(%s), defer delete(%s) ou alloc = scope", nome, OrigemDaChamada(r), nome, nome);
    }
    s->rastreados.n = marca;
    return true;
}

/* let/var com origem heap ou escopo (ja declarado: st->local_id vale) entra na lista de rastreados, VIVO. */
bool SemRastrearLocal(Semantica *s, const Stmt *st, uint8_t origem)
{
    LocalRastreado *r = VecEmpurrar(&s->rastreados);
    r->local_id = st->local_id;
    r->nome = st->nome;
    r->pos = SemPosDepoisDaPalavra(s, st->pos);
    r->origem = origem;
    r->tipo = st->tipo_local;
    r->estado = POSSE_VIVO;
    r->de_chamada = ValorVeioDeChamadaHeap(st->expr);
    return true;
}

/* 'em = escopo' (D18): grava em st->corpo o 'defer delete(nome)' sintetico; o lowering o expande no fim do bloco. Nunca passa pela semantica. */
void SemDeferDoEscopo(Semantica *s, Stmt *st)
{
    Expr *nome = ArenaAlocar(s->arena, sizeof *nome);
    Expr *chamada = ArenaAlocar(s->arena, sizeof *chamada);
    Stmt *comando = ArenaAlocar(s->arena, sizeof *comando);
    Stmt *adiado = ArenaAlocar(s->arena, sizeof *adiado);
    nome->tipo_no = EX_NOME;
    nome->nome = st->nome;
    nome->pos = nome->pos_op = st->pos;
    nome->ref = REF_LOCAL;
    nome->ref_id = st->local_id;
    nome->tipo = st->tipo_local;
    chamada->tipo_no = EX_CHAMADA;
    chamada->nome = s->nome_delete;
    chamada->pos = chamada->pos_op = st->pos;
    chamada->ref = REF_EMBUTIDO;
    chamada->ref_id = EMB_DELETE;
    chamada->tipo = TIPO_VAZIO;
    chamada->alocacao = ALOC_HEAP;
    chamada->nargs = 1;
    chamada->args = ArenaAlocar(s->arena, sizeof(Arg));
    chamada->args[0] = (Arg){.nome = -1, .pos_nome = st->pos, .valor = nome, .indice = 0};
    comando->tipo_no = ST_CHAMADA;
    comando->pos = st->pos;
    comando->expr = chamada;
    adiado->tipo_no = ST_DEFER;
    adiado->pos = st->pos;
    adiado->corpo = comando;
    st->corpo = adiado;
}
