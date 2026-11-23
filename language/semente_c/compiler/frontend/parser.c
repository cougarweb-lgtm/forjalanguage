/* parser.c - nucleo do parser: erro de sintaxe, consumo de tokens, listas em construcao e ponto de entrada.
 * Versao: 0.10.0
 * Changelog: 0.10.0 - Linguagem v1.13 L1a: os apelidos de import (ModulosApelido, ModulosApelidoNovo) sao consultados pelo trecho (arquivo_atual), nao pelo modulo.
 *            0.9.0 - IA1-L3d1: CanonizarMembro e LerApelido registram a forma antiga e o qualificador antigo em Diag.trocas (RenomeadoNotar, NotarQualificadorAntigo) para o forja migrar.
 *            0.8.0 - IA1-L3c2: CanonizarMembro so proc e tipo (o membro de enum sai da lista: SemAcharMembro).
 *            0.7.0 - IA1-L3c: LerApelido troca o nome antigo de proc/tipo do modulo pelo novo (CanonizarMembro, tabela de renomeados; flag recusar -> E0110).
 *            0.6.0 - IA1-L1: ErroSintaxe consulta ParserIdioma antes do E0101 (forma de outra linguagem = E0110).
 *            0.5.0 - IA1-b15: varios erros de sintaxe por execucao (Parser.erros): ParserRecuperarComando (fim da linha/bloco) e RecuperarDeclaracao (proxima declaracao de topo).
 *            0.4.0 - IA1-b07: o E0101 sai com a forma da ocorrencia (parser_forma.c): ErroSintaxe chama ParserFormaEsperado e ParserAnalisar chama ParserFormaDaRegra no erro.
 *            0.3.0 - Linguagem v1.8 P2a: 'import' deixa de ser "nao suportado"; LerApelido; ParserAnalisar reentrante (pilhas iniciadas uma vez).
 *            0.2.0 - Linguagem v1.4 P1: 'foreign' deixa de ser "nao suportado".
 *            0.1.1 - Linguagem v1.1: 'pub' e 'export' deixam de ser "nao suportados" (o parser os aceita antes da declaracao).
 *            0.1.0 - Fase 5b: Cur/Expect/ParseIdent do prototipo viram TokenAtual/Esperar/LerNome sobre Parser;
 *                    listas (args, campos, params, membros, casos) usam pilhas Vec em vez de malloc/realloc soltos.
 * Verificacao: construir.bat && forja.exe check <arq.fj>; python ..\tests\sintaxe\verificar_sintaxe.py
 */
#include "frontend/parser_interno.h"

#include "common/renomeados.h"

#include <string.h>

static bool EhNaoSuportada(TokenTipo tipo)
{
    return tipo == KW_UNION || tipo == KW_NONE;
}

bool ErroSintaxe(Parser *p, const char *esperado)
{
    const Token *t = TokenAtual(p);
    const char *texto = p->fonte->texto + t->off;
    if (ParserIdioma(p, esperado)) return false; /* IA1-L1: forma de outra linguagem = E0110 */
    if (EhNaoSuportada((TokenTipo)t->tipo))
        return DiagRelatar(p->diag, E0101, t->off, "construcao ainda nao suportada na Fase 5: '%.*s'", (int)t->len, texto);
    if (t->tipo == TK_NL) DiagRelatar(p->diag, E0101, t->off, "fim de linha inesperado; esperado %s", esperado);
    else if (t->tipo == TK_EOF) DiagRelatar(p->diag, E0101, t->off, "fim de arquivo inesperado; esperado %s", esperado);
    else DiagRelatar(p->diag, E0101, t->off, "token inesperado '%.*s'; esperado %s", (int)t->len, texto, esperado);
    ParserFormaEsperado(p, esperado);
    return false;
}

bool Esperar(Parser *p, TokenTipo tipo, const char *esperado)
{
    if (TipoAtual(p) != tipo) return ErroSintaxe(p, esperado);
    p->cur++;
    return true;
}

bool EsperarFimDeLinha(Parser *p)
{
    if (TipoAtual(p) == TK_EOF) return true;
    return Esperar(p, TK_NL, "fim de linha");
}

bool LerNome(Parser *p, int32_t *nome, uint32_t *pos, const char *esperado)
{
    if (TipoAtual(p) != TK_IDENT) return ErroSintaxe(p, esperado);
    *nome = TokenAtual(p)->sym;
    *pos = TokenAtual(p)->off;
    p->cur++;
    return true;
}

bool EsperarAbreBloco(Parser *p)
{
    return Esperar(p, TK_LBRACE, "'{'") && Esperar(p, TK_NL, "fim de linha depois de '{'");
}

void *CopiarLista(Parser *p, Vec *pilha, size_t base)
{
    size_t n = pilha->n - base;
    pilha->n = base;
    if (n == 0) return NULL;
    void *destino = ArenaAlocar(p->arena, n * pilha->tam);
    memcpy(destino, VecEm(pilha, base), n * pilha->tam);
    return destino;
}

/* Guarda o erro pendente na lista (com a forma da regra fixa); false = lista cheia. */
static bool GuardarErroPendente(Parser *p)
{
    ParserFormaDaRegra(p); /* so age no E0101 que o ErroSintaxe nao cobriu (regra fixa) */
    return DiagListaGuardar(p->erros, p->diag, p->modulo_atual);
}

/* IA1-L3c: o nome antigo de proc/tipo/membro do modulo (tabela de renomeados: banco.abrir) vira o novo (open) no proprio token, que o semantico enxerga como se tivesse sido escrito
 * assim; a posicao e o tamanho do token seguem os do texto escrito. So vale para o nome qualificado pelo apelido do modulo da entrada. Com 'recusar' (L3f) guarda o E0110 na lista e
 * o parse segue com o nome novo (o erro faz o ParserAnalisar devolver false). O vetor de tokens e do lexer (heap), entao a escrita e valida. */
static void CanonizarMembro(Parser *p, uint8_t modulo)
{
    static const RenClasse CLASSES[] = {REN_PROC, REN_TIPO}; /* L3c2: o membro de enum nao vem logo depois do apelido; a semantica o resolve (SemAcharMembro) */
    Token *t = (Token *)&p->toks[p->cur];
    const char *caminho = InternerTexto(p->nomes, p->modulos->itens[modulo].caminho), *nome = InternerTexto(p->nomes, t->sym);

    for (size_t k = 0; k < sizeof CLASSES / sizeof CLASSES[0]; k++) {
        bool era_antigo = false;
        const Renomeado *ren = RenomeadoAchar(CLASSES[k], caminho, nome, &era_antigo);

        if (ren == NULL || !era_antigo) continue;
        RenomeadoNotarTabela(p->diag, t->off, t->len, ren);
        if (RenomeadoRecusa(ren) && p->erros != NULL) {
            RenomeadoRecusar(p->diag, t->off, t->len, ren);
            GuardarErroPendente(p);
        }
        t->sym = Internar(p->nomes, ren->novo, (uint32_t)strlen(ren->novo));
        return;
    }
}

/* L3d1: o token atual e o apelido padrao de um import de forma antiga, e o apelido da forma nova e outro (arquivo -> file): registra a troca do qualificador para o forja migrar. */
static void NotarQualificadorAntigo(Parser *p)
{
    const Token *t = TokenAtual(p);
    int32_t novo;

    if (p->diag->trocas == NULL) return;
    novo = ModulosApelidoNovo(p->modulos, p->arquivo_atual, t->sym);
    if (novo < 0) return;
    RenomeadoNotar(p->diag, &(RenTroca){.off = t->off, .tam = t->len, .classe = REN_QUALIFICADOR, .antigo = InternerTexto(p->nomes, t->sym), .novo = InternerTexto(p->nomes, novo)});
}

/* v1.8: 'apelido .' antes de um IDENT, quando 'apelido' e um import deste arquivo: consome os dois tokens, devolve o modulo e o
 * offset do apelido. 0 = nao e qualificado (nada consumido). So chamar com p->napelidos != 0. */
uint8_t LerApelido(Parser *p, uint32_t *pos)
{
    if (TipoAtual(p) != TK_IDENT || TipoSeguinte(p) != TK_DOT || p->toks[p->cur + 2].tipo != TK_IDENT) return 0;
    uint8_t modulo = ModulosApelido(p->modulos, p->arquivo_atual, TokenAtual(p)->sym);
    if (modulo == 0) return 0;
    NotarQualificadorAntigo(p);
    *pos = TokenAtual(p)->off;
    p->cur += 2;
    CanonizarMembro(p, modulo);
    return modulo;
}

static bool AbreBloco(TokenTipo tipo)
{
    return tipo == KW_IF || tipo == KW_WHILE || tipo == KW_FOR || tipo == KW_SWITCH;
}

enum { SEM_FIM = UINT32_MAX };

/* Indice do TK_NL que fecha o comando que comecou em 'ini' (a linha do erro e, se ela abre '{', o bloco inteiro); SEM_FIM = nao da (fim de arquivo ou '}' a mais).
 * *abriu_bloco diz se o comando abriu algum '{'. */
static uint32_t FimDoComando(const Parser *p, uint32_t ini, bool *abriu_bloco)
{
    int32_t profundidade = 0;

    *abriu_bloco = false;
    for (uint32_t k = ini;; k++) {
        TokenTipo tipo = (TokenTipo)p->toks[k].tipo;
        if (tipo == TK_EOF) return SEM_FIM;
        if (tipo == TK_LBRACE) {
            profundidade++;
            *abriu_bloco = true;
        }
        if (tipo == TK_RBRACE) profundidade--;
        if (profundidade < 0) return SEM_FIM;
        if (tipo == TK_NL && k >= p->cur && profundidade == 0) return k;
    }
}

bool ParserRecuperarComando(Parser *p, uint32_t ini)
{
    bool abriu_bloco;

    if (p->erros == NULL || !p->diag->tem) return false;
    for (uint32_t k = ini; k < p->cur; k++) {
        if (p->toks[k].tipo == TK_NL) return false; /* o erro esta numa linha de baixo do comando: o resto dele e imprevisivel */
    }
    uint32_t fim = FimDoComando(p, ini, &abriu_bloco);
    if (fim == SEM_FIM) return false;
    if (!abriu_bloco && AbreBloco((TokenTipo)p->toks[ini].tipo)) return false; /* cabecalho sem '{': as linhas de baixo eram o corpo e viram cascata */
    if (!GuardarErroPendente(p)) return false;
    p->cur = fim + 1;
    p->sem_literal_struct = false;
    return true;
}

/* Declaracao de topo comeca no 1o caractere da linha com uma destas palavras (corpo recuado nunca confunde). */
static bool ComecaDeclaracaoNoTopo(const Parser *p)
{
    const Token *t = TokenAtual(p);
    TokenTipo tipo = (TokenTipo)t->tipo;
    bool palavra = tipo == KW_PUB || tipo == KW_EXPORT || tipo == KW_IMPORT || tipo == KW_UNIT || tipo == KW_CONST || tipo == KW_STRUCT ||
                   tipo == KW_ENUM || tipo == KW_UNION || tipo == KW_PROC || tipo == KW_TEST || tipo == KW_FOREIGN ||
                   (tipo == TK_IDENT && t->sym == SYM_STATIC_ASSERT);
    return palavra && (t->off == 0 || p->fonte->texto[t->off - 1] == '\n');
}

/* Erro que nao deu para re-sincronizar no comando: guarda e pula ate a proxima declaracao de topo. false = sem lista ou lista cheia. */
static bool RecuperarDeclaracao(Parser *p, uint32_t ini)
{
    if (p->erros == NULL) return false;
    if (p->diag->tem && !GuardarErroPendente(p)) return false;
    if (p->erros->cortado) return false;
    if (p->cur <= ini && TipoAtual(p) != TK_EOF) p->cur++; /* sempre anda */
    while (TipoAtual(p) != TK_EOF && !ComecaDeclaracaoNoTopo(p)) p->cur++;
    p->sem_literal_struct = false;
    return true;
}

bool ParserAnalisar(Parser *p)
{
    bool ok = true;
    uint32_t ini;

    p->toks = p->tokens->dados;
    p->cur = 0;
    p->sem_literal_struct = false;
    p->viu_declaracao = false;
    p->napelidos = 0;
    if (p->pilha_args.tam == 0) {
        VecIniciar(&p->pilha_args, sizeof(Arg));
        VecIniciar(&p->pilha_campos, sizeof(Campo));
        VecIniciar(&p->pilha_params, sizeof(Param));
        VecIniciar(&p->pilha_membros, sizeof(Membro));
        VecIniciar(&p->pilha_casos, sizeof(Caso));
    }
    while (TipoAtual(p) != TK_EOF) {
        ini = p->cur;
        if (AnalisarDeclaracao(p)) continue;
        ok = false;
        if (RecuperarDeclaracao(p, ini)) continue;
        ParserFormaDaRegra(p); /* so age no E0101 que o ErroSintaxe nao cobriu (regra fixa) */
        return false;
    }
    return ok && (p->erros == NULL || p->erros->n == 0);
}

void ParserLiberar(Parser *p)
{
    VecLiberar(&p->pilha_args);
    VecLiberar(&p->pilha_campos);
    VecLiberar(&p->pilha_params);
    VecLiberar(&p->pilha_membros);
    VecLiberar(&p->pilha_casos);
}
