/* lower.c - lowering do programa: uma IrFuncao por proc e por test, mais as declaracoes (structs, enums, constantes).
 * Versao: 0.15.1
 * Changelog: 0.15.1 - Linguagem v1.13 L1a-ir: IrExterno.arquivo (trecho da Fonte de Decl.pos) no lugar de IrExterno.modulo.
 *            0.15.0 - M6d P92e: IrPrograma.fins/nfins (LwExterno anota o simbolo C de todo foreign `ends`).
 *            0.14.1 - M6 P65a: IniciarFuncao leva a tabela de nomes dos locais da Decl para a IrFuncao.
 *            0.14.0 - S4 P1: test de modulo selecionado (Semantica.modulo_testado) entra na IR com nome 0teste_m<k>_<n> e rotulo "[caminho] titulo"; a raiz segue igual.
 *            0.13.0 - L11a-2: EhExternoInterno olha o simbolo C (alias de 'proc sin = "forja_math_sin"' na std:math), nao so o nome da Forja; a chave segue com o prefixo do modulo (sem_chave.c, sem mudanca).
 *            0.12.0 - Linguagem v1.12 M3 P21v-b: LowerPrograma volta a baixar o programa com proc como valor e chamada indireta (a recusa provisoria E0101 do P21v-a sai).
 *            0.11.0 - Linguagem v1.12 M3 P21v-a: LowerPrograma recusa (E0101 provisorio, na posicao do 1o uso) o programa com proc como valor ou chamada indireta (Semantica.usa_proc_valor) ate o P21v-b.
 *            0.10.0 -Linguagem v1.11 L15a: IrPrograma.externos com uma vaga a mais (o servico interno do append na [dynamic, N]T cheia, lower_lista_fixa.c).
 *            0.9.0 - Linguagem v1.11 L11: IrPrograma.funcoes com folga de um proc oculto por enum (LwDepurar).
 *            0.8.0 -Linguagem v1.10 (A6): IrExterno.simbolo (alias de simbolo C do foreign).
 *            0.7.0 - Linguagem v1.8b P5b: IrPrograma.trampolins zerado em LowerPrograma.
 *            0.6.0 -Linguagem v1.8 P6b: IrExterno.interno (foreign da std com nome forja_*).
 *            0.5.0 - Linguagem v1.8 P4: modulos (test de modulo fora do build, IrExterno.modulo, linha local ao arquivo); sai a guarda do P3.
 *            0.4.0 - Linguagem v1.8 P3: guarda PROVISORIA (FalhaInterna "lowering de modulos chega no P4") com mais de 1 modulo.
 *            Linguagem v1.7 P8: sai a guarda provisoria do P7 (?T, union, tupla chegam ao lowering; versao igual).
 *            Linguagem v1.6 P7a: sai a guarda provisoria "lista chega no P7a" (a versao continua 0.3.0).
 *            0.3.0 - Linguagem v1.5 P7: lowering de array e fatia (lower_array.c); saiu a FalhaInterna provisoria de array e de fatia.
 *            0.2.1 - Linguagem v1.5 P2: FalhaInterna "array chega no P7" se a tabela de tipos tem array ou fatia (provisorio).
 *            0.2.0 - Linguagem v1.4 P4: LwExterno e LwLink (IrPrograma.externos/bibliotecas); sai a FalhaInterna do P2.
 *            0.1.3 - Linguagem v1.4 P2: foreign ainda sem IR (FalhaInterna "foreign chega no P4"), provisorio.
 *            0.1.2 - Linguagem v1.3: IrFuncao.linha (so proc exportado).
 *            0.1.1 - Linguagem v1.1: IrFuncao.exportado copiado de Decl.exportado.
 *            0.1.0 - Fase 5d: LowerPrograma, LwFuncaoProc, LwFuncaoTeste. O corpo vem de LwBloco (lower_stmt.c).
 * Verificacao: construir.bat && python ..\tests\rodar_suite.py --compilador forja.exe --corpus ..\tests\dourado
 */
#include "ir/lower_interno.h"

typedef struct { uint32_t funcoes, externos, links; } Contagem;

/* S4 P1: o test da raiz entra sempre; o test de modulo so entra se o forja test o selecionou (Semantica.modulo_testado, so RodarTest liga). */
static bool EhTesteLowerado(const Semantica *sem, const Decl *d)
{
    return d->tipo_no == D_TEST && (d->modulo == 0 || sem->modulo_testado[d->modulo]);
}

static Contagem ContarDeclaracoes(const Semantica *sem)
{
    Contagem n = {0, 0, 0};
    for (const Decl *d = sem->programa->primeira; d != NULL; d = d->prox) {
        if (d->tipo_no == D_PROC && d->externo) n.externos++;
        else if (d->tipo_no == D_PROC || EhTesteLowerado(sem, d)) n.funcoes++; /* test de modulo nao selecionado nao entra no build */
        else if (d->tipo_no == D_LINK) n.links++;
    }
    return n;
}

/* Zera o estado de uma funcao nova e aponta o ctx para o corpo dela. */
static IrFuncao *IniciarFuncao(LowerCtx *c, const Decl *d)
{
    IrFuncao *f = &c->prog->funcoes[c->prog->nfuncoes++];
    c->funcao = f;
    c->decl = d;
    c->atual = &f->corpo;
    c->ntemps = 0;
    c->prox_local = d->nlocais;
    f->nomes_locais = d->nomes_locais; /* NULL se o corpo nao foi analisado: entao nnomes_locais = 0 */
    f->nnomes_locais = d->nomes_locais != NULL ? d->nlocais : 0;
    c->defers.n = 0;
    c->escopos.n = 0;
    return f;
}

static void FecharFuncao(LowerCtx *c)
{
    c->funcao->ntemps = c->ntemps;
    c->funcao->nlocais = c->prox_local;
}

static bool TerminaEmReturn(const Stmt *lista)
{
    const Stmt *ultimo = lista;
    while (ultimo != NULL && ultimo->prox != NULL) ultimo = ultimo->prox;
    return ultimo != NULL && ultimo->tipo_no == ST_RETURN;
}

/* 'proc ... -> ! E' sem return no fim devolve ok: sem isso o C gerado cairia no panic de "fim da proc sem retorno". */
static void FimImplicitoDoResultado(LowerCtx *c, const Decl *d)
{
    if (TipoClasseDe(&c->sem->tipos, d->retorno.tipo) != TC_RESULTADO || TerminaEmReturn(d->corpo)) return;
    IrInstr *ok = LwNovaInstr(c, IR_RES_OK, d->pos_fim);
    ok->destino = LwNovoTemp(c);
    ok->tipo = d->retorno.tipo;
    ok->a = LwValorNenhum();
    IrInstr *ret = LwNovaInstr(c, IR_RETORNA, d->pos_fim);
    ret->a = LwValorTemp(ok->destino, ok->tipo);
}

void LwFuncaoProc(LowerCtx *c, const Decl *d)
{
    IrFuncao *f = IniciarFuncao(c, d);
    f->nome = d->chave; /* IA1-L8: o nome da IR e a chave por modulo (sem prefixo na raiz, no main e no export) */
    f->retorno = d->retorno.tipo;
    f->nparams = d->nparams;
    f->params = LwAlocar(c, d->nparams * sizeof(IrParam));
    for (uint32_t i = 0; i < d->nparams; i++) {
        f->params[i] = (IrParam){d->params[i].nome, d->params[i].tipo.tipo};
    }
    f->e_main = d->modulo == 0 && d->nome == Internar(c->sem->nomes, "main", 4);
    f->exportado = d->exportado;
    if (d->exportado) {
        f->linha = FonteLocalizarNoArquivo(c->sem->fonte, d->pos).linha;
    }
    LwBloco(c, d->corpo, false);
    FimImplicitoDoResultado(c, d);
    FecharFuncao(c);
}

/* S4 P1: rotulo do test de modulo = "[<caminho do import>] <titulo>" (ex.: "[mods/geo] area"), na arena do lowering. */
static int32_t RotuloDoTesteModulo(LowerCtx *c, const Decl *d)
{
    const Interner *nomes = c->sem->nomes;
    const Modulo *m = &c->sem->modulos->itens[d->modulo];
    size_t tam = InternerTam(nomes, m->caminho) + InternerTam(nomes, d->nome) + 4; /* "[", "] ", espaco e o '\0' */
    char *rotulo = LwAlocar(c, tam);
    int n = snprintf(rotulo, tam, "[%s] %s", InternerTexto(nomes, m->caminho), InternerTexto(nomes, d->nome));
    return Internar(c->sem->nomes, rotulo, (uint32_t)n);
}

/* S4 P1: nome interno 0teste_<n> na raiz (inalterado) e 0teste_m<k>_<n> no modulo k (contador proprio). */
static int32_t NomeDoTesteLowerado(LowerCtx *c, const Decl *d)
{
    char nome_interno[32];
    int n;

    if (d->modulo == 0) n = snprintf(nome_interno, sizeof nome_interno, "0teste_%u", c->seq_teste++);
    else n = snprintf(nome_interno, sizeof nome_interno, "0teste_m%u_%u", (unsigned)d->modulo, c->seq_teste_modulo++);
    return Internar(c->sem->nomes, nome_interno, (uint32_t)n);
}

void LwFuncaoTeste(LowerCtx *c, const Decl *d)
{
    IrFuncao *f = IniciarFuncao(c, d);
    f->nome = NomeDoTesteLowerado(c, d);
    f->retorno = TIPO_VAZIO;
    f->e_teste = true;
    f->titulo = d->modulo == 0 ? d->nome : RotuloDoTesteModulo(c, d);
    LwBloco(c, d->corpo, false);
    FecharFuncao(c);
}

/* Foreign interno (D13): declarado na std embutida e com simbolo C forja_* (o alias de 'proc nome = "forja_..."' conta, L11a-2); o simbolo C ja existe no runtime. */
static bool EhExternoInterno(const LowerCtx *c, const Decl *d)
{
    const Modulos *m = c->sem->modulos;
    if (m == NULL || c->sem->qtd_modulos <= 1 || m->itens[d->modulo].origem != MOD_STD) return false;
    return strncmp(InternerTexto(c->sem->nomes, d->simbolo >= 0 ? d->simbolo : d->nome), "forja_", 6) == 0;
}

/* Proc de 'foreign': so a assinatura; o corpo esta na biblioteca C. */
static void LwExterno(LowerCtx *c, const Decl *d)
{
    IrExterno *e = &c->prog->externos[c->prog->nexternos++];

    e->nome = d->chave; /* IA1-L8: o involucro fj_/fjx_ leva a chave; o simbolo C (rotulo) nunca */
    e->simbolo = d->simbolo >= 0 ? d->simbolo : d->nome; /* v1.10 (A6): alias da Forja para o simbolo C */
    e->retorno = d->retorno.tipo;
    e->nparams = d->nparams;
    e->params = LwAlocar(c, d->nparams * sizeof(IrParam));
    for (uint32_t i = 0; i < d->nparams; i++) {
        e->params[i] = (IrParam){d->params[i].nome, d->params[i].tipo.tipo};
    }
    e->arquivo = FonteTrechoDoOffset(c->sem->fonte, d->pos);
    e->interno = EhExternoInterno(c, d);
    e->linha = FonteLocalizarNoArquivo(c->sem->fonte, d->pos).linha;
    if (d->posse_c == POSSE_C_ENCERRA) /* M6d P92e: o simbolo C de todo `ends` declarado, usado ou nao */
        c->prog->fins[c->prog->nfins++] = e->simbolo;
}

/* Biblioteca repetida em varios 'link' vale uma vez (a 1a aparicao fixa a ordem do -l). */
static void LwLink(LowerCtx *c, const Decl *d)
{
    IrPrograma *p = c->prog;
    for (uint32_t i = 0; i < p->nbibliotecas; i++) {
        if (p->bibliotecas[i] == d->nome) return;
    }
    p->bibliotecas[p->nbibliotecas++] = d->nome;
}

static void LiberarContexto(LowerCtx *c)
{
    VecLiberar(&c->defers);
    VecLiberar(&c->escopos);
}

bool LowerPrograma(Semantica *sem, IrPrograma *saida)
{
    LowerCtx c = {.sem = sem, .arena = sem->arena, .prog = saida};
    VecIniciar(&c.defers, sizeof(Stmt *));
    VecIniciar(&c.escopos, sizeof(LwEscopo));
    Contagem n = ContarDeclaracoes(sem);
    saida->funcoes = LwAlocar(&c, (n.funcoes + sem->qtd_enum) * sizeof(IrFuncao)); /* + 1 proc oculto por enum ({x:?}, lower_depurar.c) */
    saida->externos = LwAlocar(&c, (n.externos + 1) * sizeof(IrExterno)); /* + 1: o servico do append na [dynamic, N]T cheia */
    saida->bibliotecas = LwAlocar(&c, n.links * sizeof(int32_t));
    saida->trampolins = NULL;
    saida->ntrampolins = 0;
    saida->fins = LwAlocar(&c, (n.externos + 1) * sizeof(int32_t)); /* M6d P92e: no maximo um fim por foreign */
    saida->nfins = 0;
    LwDeclaracoes(&c);
    for (const Decl *d = sem->programa->primeira; d != NULL && !sem->diag->tem; d = d->prox) {
        if (d->tipo_no == D_PROC && d->externo) LwExterno(&c, d);
        else if (d->tipo_no == D_PROC) LwFuncaoProc(&c, d);
        else if (EhTesteLowerado(sem, d)) LwFuncaoTeste(&c, d);
        else if (d->tipo_no == D_LINK) LwLink(&c, d);
    }
    LiberarContexto(&c);
    return !sem->diag->tem;
}
