/* parser_decl.c - parser das declaracoes de topo: unit, const, struct, enum, proc, test e foreign.
 * Versao: 0.11.0
 * Changelog: 0.11.0 - Linguagem v1.13 L1a: o import registra o trecho (arquivo_atual) em RegistroImport.arquivo; Decl.modulo segue sendo o pacote.
 *            0.10.1 - M6d P93h: 'ends destroys f' = E0912 de "no maximo uma clausula de posse" (EhClausulaDePosse); 'ends f' segue "ends nao leva parametro".
 *            0.10.0 - M6d P92b (D-M6d-7): clausula 'ends' (sem parametro; 'ends p' = E0912).
 *0.9.0 - M6c P85: clausula de posse do proc foreign (LerClausulaDePosse: creates, creates p, destroys p, takes p; E0912 de sintaxe).
 *0.8.1 - IA1-L2: 'import os' sem aspas vira E0110 (ParserIdiomaImport).
 *            0.8.0 - IA1-L1: 'proc main() -> int' (e outros inteiros) vira E0110 (ParserIdiomaMain).
 *            0.7.0 - Linguagem v1.11 L9: static_assert de topo (AnalisarStaticAssert, D_ASSERT).
 *            0.6.0 - Linguagem v1.8b P2: 'opaque Nome' em foreign (AnalisarOpaco), enum com valor (Membro.valor, Decl.op = 1), variadica foreign = E0904.
 *            0.5.0 - Linguagem v1.8 P2a: AnalisarImport (D_IMPORT), Decl.modulo, 'pub/export import' = E0101, retorno '! apelido.E' (modulo_erro).
 *            0.4.0 - Linguagem v1.7 P1: 'union Nome { A, B }' (AnalisarUniao) e retorno multiplo '-> (q: int, r: int)' (AnalisarRetornoMultiplo).
 *            0.3.0 - Linguagem v1.4 P1: AnalisarForeign/AnalisarLink/AnalisarProcExterno (AST plana), pub/export em todas as decls
 *                    criadas, E0101 para variadico/corpo/header/nome no cabecalho/bloco vazio.
 *            0.2.0 - Linguagem v1.3: AnalisarTipoRef sem o bool; retorno '! E' grava forma TR_NOME; DECLARACOES[TK_QTD].
 *            0.1.2 - Linguagem v1.2: AnalisarLimite detecta estouro de 64 bits (E0420) e grava Campo.pos_min/pos_max.
 *            0.1.1 - Linguagem v1.1: 'pub' e 'export' antes da declaracao (Decl.publico, Decl.exportado).
 *            0.1.0 - Fase 5b: declaracoes do prototipo + enum (membros por virgula e/ou quebra de linha), retorno
 *                    'T ! E' / '! E' e 'test "titulo" { }'. Decl.pos = posicao do nome (do 'test' em D_TEST).
 * Verificacao: construir.bat && forja.exe ast <arq.fj>; python ..\tests\sintaxe\verificar_sintaxe.py
 */
#include "frontend/lexer.h"
#include "frontend/parser_interno.h"

typedef bool (*AnalisaDeclaracao)(Parser *p);

static Decl *NovaDecl(Parser *p, DeclTipo tipo, uint32_t pos)
{
    Decl *d = ArenaAlocar(p->arena, sizeof(Decl));
    d->tipo_no = (uint8_t)tipo;
    d->pos = pos;
    d->nome = -1;
    d->simbolo = -1;
    if (p->programa.ultima == NULL) p->programa.primeira = d; else p->programa.ultima->prox = d;
    p->programa.ultima = d;
    return d;
}

static void PularQuebras(Parser *p)
{
    while (TipoAtual(p) == TK_NL) p->cur++;
}

static bool AnalisarUnit(Parser *p)
{
    Decl *d = NovaDecl(p, D_UNIDADE_BASE, TokenAtual(p)->off);
    p->cur++;
    if (!LerNome(p, &d->nome, &d->pos, "nome da unidade")) return false;
    if (TipoAtual(p) == TK_COLON) {
        p->cur++;
        if (TipoAtual(p) != TK_IDENT || TokenAtual(p)->sym != SYM_F32) return ErroSintaxe(p, "'f32'");
        p->cur++;
        return EsperarFimDeLinha(p);
    }
    if (!Esperar(p, TK_ASSIGN, "':' ou '='")) return false;
    d->tipo_no = D_UNIDADE_DERIV;
    if (!LerNome(p, &d->nome_a, &d->pos_a, "nome de unidade")) return false;
    if (TipoAtual(p) != TK_STAR && TipoAtual(p) != TK_SLASH) return ErroSintaxe(p, "'*' ou '/'");
    d->op = (uint8_t)(TipoAtual(p) == TK_STAR ? OP_MUL : OP_DIV);
    p->cur++;
    return LerNome(p, &d->nome_b, &d->pos_b, "nome de unidade") && EsperarFimDeLinha(p);
}

static bool AnalisarConst(Parser *p)
{
    Decl *d = NovaDecl(p, D_CONST, TokenAtual(p)->off);
    p->cur++;
    if (!LerNome(p, &d->nome, &d->pos, "nome da constante")) return false;
    if (!Esperar(p, TK_COLON, "':'") || !AnalisarTipoRef(p, &d->tipo)) return false;
    if (!Esperar(p, TK_ASSIGN, "'='")) return false;
    d->init = AnalisarExpr(p);
    return d->init != NULL && EsperarFimDeLinha(p);
}

/* Limite da faixa: inteiro com '-' opcional (digitos lidos do fonte). *pos = offset do '-' ou do numero. A magnitude passa de
 * 64 bits com sinal (9223372036854775807; 9223372036854775808 se negativo) = E0420 aqui mesmo, o unico vindo do parser. */
static bool AnalisarLimite(Parser *p, int64_t *valor, uint32_t *pos)
{
    bool negativo = TipoAtual(p) == TK_MINUS;
    *pos = TokenAtual(p)->off;
    if (negativo) p->cur++;
    if (TipoAtual(p) != TK_INT) return ErroSintaxe(p, "inteiro");
    uint64_t v = 0;
    bool coube = LexerLerInteiroU64(p->fonte->texto + TokenAtual(p)->off, &v); /* v1.10: aceita 0x... e 0b... */
    if (!coube || v > (uint64_t)INT64_MAX + (negativo ? 1u : 0u)) {
        return DiagRelatar(p->diag, E0420, *pos, "limite de faixa fora de 64 bits com sinal (maximo 9223372036854775807)");
    }
    *valor = negativo ? (int64_t)(0u - v) : (int64_t)v;
    p->cur++;
    return true;
}

static bool AnalisarFaixaDoCampo(Parser *p, Campo *f)
{
    f->tem_faixa = true;
    p->cur++;
    if (!AnalisarLimite(p, &f->faixa_min, &f->pos_min)) return false;
    if (TipoAtual(p) != TK_DOTDOTEQ && TipoAtual(p) != TK_DOTDOTLT) return ErroSintaxe(p, "'..=' ou '..<'");
    f->faixa_excl = TipoAtual(p) == TK_DOTDOTLT;
    p->cur++;
    return AnalisarLimite(p, &f->faixa_max, &f->pos_max);
}

static bool AnalisarCampo(Parser *p, Campo *f)
{
    if (!LerNome(p, &f->nome, &f->pos, "nome do campo")) return false;
    if (!Esperar(p, TK_COLON, "':'") || !AnalisarTipoRef(p, &f->tipo)) return false;
    if (TipoAtual(p) == KW_IN && !AnalisarFaixaDoCampo(p, f)) return false;
    if (TipoAtual(p) == TK_ASSIGN) {
        p->cur++;
        f->padrao = AnalisarExpr(p);
        if (f->padrao == NULL) return false;
    }
    return EsperarFimDeLinha(p);
}

static bool AnalisarStruct(Parser *p)
{
    Decl *d = NovaDecl(p, D_STRUCT, TokenAtual(p)->off);
    size_t base = p->pilha_campos.n;
    p->cur++;
    if (!LerNome(p, &d->nome, &d->pos, "nome da struct") || !EsperarAbreBloco(p)) return false;
    while (TipoAtual(p) != TK_RBRACE) {
        Campo f = {0};
        if (TipoAtual(p) != TK_IDENT) return ErroSintaxe(p, "campo ou '}'");
        if (!AnalisarCampo(p, &f)) return false;
        *(Campo *)VecEmpurrar(&p->pilha_campos) = f;
    }
    d->ncampos = (uint32_t)(p->pilha_campos.n - base);
    d->campos = CopiarLista(p, &p->pilha_campos, base);
    d->pos_fim = TokenAtual(p)->off;
    p->cur++;
    return EsperarFimDeLinha(p);
}

/* Membros separados por virgula e/ou quebra de linha, tudo numa linha ou uma por linha; virgula final aceita. */
static bool AnalisarMembros(Parser *p, Decl *d)
{
    size_t base = p->pilha_membros.n;
    PularQuebras(p);
    while (TipoAtual(p) != TK_RBRACE) {
        Membro m = {0};
        if (!LerNome(p, &m.nome, &m.pos, "nome do membro ou '}'")) return false;
        if (TipoAtual(p) == TK_ASSIGN) {
            p->cur++;
            if (!AnalisarLimite(p, &m.valor, &m.pos_valor)) return false;
            m.tem_valor = true;
            d->op = 1;
        }
        *(Membro *)VecEmpurrar(&p->pilha_membros) = m;
        if (TipoAtual(p) == TK_COMMA) p->cur++;
        else if (TipoAtual(p) != TK_NL && TipoAtual(p) != TK_RBRACE) return ErroSintaxe(p, "',', fim de linha ou '}'");
        PularQuebras(p);
    }
    d->nmembros = (uint32_t)(p->pilha_membros.n - base);
    d->membros = CopiarLista(p, &p->pilha_membros, base);
    return true;
}

/* Enum com valor: o membro sem '= v' vale o anterior + 1 (o 1o vale 0), como no C; sem UB de estouro. */
static void PreencherValoresImplicitos(Decl *d)
{
    int64_t prox = 0;
    for (uint32_t k = 0; k < d->nmembros; k++) {
        Membro *m = &d->membros[k];
        if (!m->tem_valor) {
            m->valor = prox;
            m->pos_valor = m->pos;
        }
        prox = (int64_t)((uint64_t)m->valor + 1u);
    }
}

static bool AnalisarEnum(Parser *p)
{
    Decl *d = NovaDecl(p, D_ENUM, TokenAtual(p)->off);
    p->cur++;
    if (!LerNome(p, &d->nome, &d->pos, "nome do enum") || !Esperar(p, TK_LBRACE, "'{'")) return false;
    if (!AnalisarMembros(p, d)) return false;
    if (d->op == 1) PreencherValoresImplicitos(d);
    d->pos_fim = TokenAtual(p)->off;
    p->cur++;
    return EsperarFimDeLinha(p);
}

/* Variantes de union: tipos separados por virgula e/ou quebra de linha; virgula final aceita. Dois tipos seguidos = E0101 no 2o. */
static bool AnalisarVariantes(Parser *p, Decl *d)
{
    size_t base = p->pilha_campos.n;
    PularQuebras(p);
    while (TipoAtual(p) != TK_RBRACE) {
        Campo v = {0};
        v.nome = -1;
        v.pos = TokenAtual(p)->off;
        if (!AnalisarTipoRef(p, &v.tipo)) return false;
        *(Campo *)VecEmpurrar(&p->pilha_campos) = v;
        if (TipoAtual(p) == TK_COMMA) p->cur++;
        else if (TipoAtual(p) != TK_NL && TipoAtual(p) != TK_RBRACE) return ErroSintaxe(p, "',', fim de linha ou '}'");
        PularQuebras(p);
    }
    d->ncampos = (uint32_t)(p->pilha_campos.n - base);
    d->campos = CopiarLista(p, &p->pilha_campos, base);
    return true;
}

/* union Nome { A, B, C } (v1.7). */
static bool AnalisarUniao(Parser *p)
{
    Decl *d = NovaDecl(p, D_UNIAO, TokenAtual(p)->off);
    p->cur++;
    if (!LerNome(p, &d->nome, &d->pos, "nome da union") || !Esperar(p, TK_LBRACE, "'{'")) return false;
    if (!AnalisarVariantes(p, d)) return false;
    d->pos_fim = TokenAtual(p)->off;
    p->cur++;
    return EsperarFimDeLinha(p);
}

static bool AnalisarParam(Parser *p, Param *par)
{
    if (!LerNome(p, &par->nome, &par->pos, "nome do parametro")) return false;
    if (!Esperar(p, TK_COLON, "':'") || !AnalisarTipoRef(p, &par->tipo)) return false;
    if (TipoAtual(p) == TK_ASSIGN) {
        p->cur++;
        par->padrao = AnalisarExpr(p);
        if (par->padrao == NULL) return false;
    }
    return true;
}

static bool AnalisarParams(Parser *p, Decl *d)
{
    size_t base = p->pilha_params.n;
    while (TipoAtual(p) != TK_RPAREN) {
        Param par = {0};
        if (d->externo && TipoAtual(p) == TK_DOTDOT) {
            return DiagRelatar(p->diag, E0904, TokenAtual(p)->off,
                               "proc foreign variadico ('...') nao atravessa a fronteira C: no Windows x64 o C le os reais variadicos em outro registrador; declare uma funcao C de parametros fixos (forja ligar recusa com G0008)");
        }
        if (!AnalisarParam(p, &par)) return false;
        *(Param *)VecEmpurrar(&p->pilha_params) = par;
        if (TipoAtual(p) != TK_COMMA) break;
        p->cur++;
    }
    d->nparams = (uint32_t)(p->pilha_params.n - base);
    d->params = CopiarLista(p, &p->pilha_params, base);
    return Esperar(p, TK_RPAREN, "')'");
}

/* '(' nome: T, nome: T ')' [! E] (v1.7): elementos em Decl.retornos; a quantidade (2 a 8) e conferida na semantica (E0314). */
static bool AnalisarRetornoMultiplo(Parser *p, Decl *d)
{
    TipoRef *r = &d->retorno;
    size_t base = p->pilha_params.n;
    r->forma = TR_TUPLA;
    r->alvo = NULL;
    r->nome = r->erro = -1;
    r->pos = TokenAtual(p)->off;
    p->cur++;
    for (;;) {
        Param el = {0};
        if (!LerNome(p, &el.nome, &el.pos, "nome do elemento do retorno")) return false;
        if (!Esperar(p, TK_COLON, "':'") || !AnalisarTipoRef(p, &el.tipo)) return false;
        *(Param *)VecEmpurrar(&p->pilha_params) = el;
        if (TipoAtual(p) != TK_COMMA) break;
        p->cur++;
    }
    d->nretornos = (uint32_t)(p->pilha_params.n - base);
    d->retornos = CopiarLista(p, &p->pilha_params, base);
    if (!Esperar(p, TK_RPAREN, "')'")) return false;
    if (TipoAtual(p) != TK_BANG) return true;
    r->pos_tamanho = TokenAtual(p)->off;
    p->cur++;
    uint32_t pos_erro;
    return LerNome(p, &r->erro, &pos_erro, "nome do tipo de erro");
}

/* Depois do '!': o enum de erro, com 'apelido.' opcional (v1.8: TipoRef.modulo_erro). */
static bool LerErroDoRetorno(Parser *p, TipoRef *r)
{
    uint32_t pos_erro;
    if (p->napelidos != 0) r->modulo_erro = LerApelido(p, &pos_erro);
    return LerNome(p, &r->erro, &pos_erro, "nome do tipo de erro");
}

/* Depois de '->': "T", "T ! E", "! E" (so erro: retorno.nome = -1) ou "(nome: T, ...) [! E]". */
static bool AnalisarRetorno(Parser *p, Decl *d)
{
    TipoRef *r = &d->retorno;
    d->tem_retorno = true;
    if (TipoAtual(p) == TK_LPAREN) return AnalisarRetornoMultiplo(p, d);
    if (TipoAtual(p) == TK_BANG) {
        r->forma = TR_NOME;
        r->alvo = NULL;
        r->nome = r->erro = -1;
        r->pos = TokenAtual(p)->off;
    } else {
        if (!AnalisarTipoRef(p, r)) return false;
        if (TipoAtual(p) != TK_BANG) return true;
    }
    p->cur++;
    return LerErroDoRetorno(p, r);
}

static bool AnalisarProc(Parser *p)
{
    Decl *d = NovaDecl(p, D_PROC, TokenAtual(p)->off);
    p->cur++;
    if (!LerNome(p, &d->nome, &d->pos, "nome do proc")) return false;
    if (!Esperar(p, TK_LPAREN, "'('") || !AnalisarParams(p, d)) return false;
    if (TipoAtual(p) == TK_ARROW) {
        if (ParserIdiomaMain(p, d)) return false;
        p->cur++;
        if (!AnalisarRetorno(p, d)) return false;
    }
    return AnalisarCorpoComChaves(p, &d->corpo, &d->pos_fim) && EsperarFimDeLinha(p);
}

static bool AnalisarTest(Parser *p)
{
    Decl *d = NovaDecl(p, D_TEST, TokenAtual(p)->off);
    p->cur++;
    if (TipoAtual(p) != TK_TEXTO) return ErroSintaxe(p, "titulo do teste entre aspas");
    d->nome = TokenAtual(p)->sym;
    d->pos_titulo = TokenAtual(p)->off;
    p->cur++;
    return AnalisarCorpoComChaves(p, &d->corpo, &d->pos_fim) && EsperarFimDeLinha(p);
}

/* 'link "biblioteca"' dentro de foreign: nome = texto cru da string, pos = offset da string. */
static bool AnalisarLink(Parser *p)
{
    Decl *d = NovaDecl(p, D_LINK, TokenAtual(p)->off);
    p->cur++;
    if (TipoAtual(p) != TK_TEXTO) return ErroSintaxe(p, "nome da biblioteca entre aspas");
    d->nome = TokenAtual(p)->sym;
    d->pos = d->pos_fim = TokenAtual(p)->off;
    p->cur++;
    return EsperarFimDeLinha(p);
}

/* v1.10 (A6): '= "simbolo_c"' depois do nome do proc foreign: o nome da Forja difere do simbolo C (remove, get, has, insert...). */
static bool LerAliasDoSimbolo(Parser *p, Decl *d)
{
    if (TipoAtual(p) != TK_ASSIGN) return true;
    p->cur++;
    if (TipoAtual(p) != TK_TEXTO) return ErroSintaxe(p, "o simbolo C entre aspas: proc c_remove = \"remove\"(caminho: cstr) -> i32");
    d->simbolo = TokenAtual(p)->sym;
    d->pos_simbolo = TokenAtual(p)->off;
    p->cur++;
    return true;
}

/* 'opaque', 'header' e as palavras da clausula de posse so sao reconhecidos pelo texto (o resto do bloco nao paga a comparacao). */
static bool EhPalavraDeForeign(const Parser *p, const char *palavra, uint32_t tam)
{
    const Token *t = TokenAtual(p);
    return t->tipo == TK_IDENT && InternerTam(p->nomes, t->sym) == tam && memcmp(InternerTexto(p->nomes, t->sym), palavra, tam) == 0;
}

/* P93h: o token atual e uma palavra de clausula de posse (creates, destroys, takes, ends). */
static bool EhClausulaDePosse(const Parser *p)
{
    return EhPalavraDeForeign(p, "creates", 7) || EhPalavraDeForeign(p, "destroys", 8) || EhPalavraDeForeign(p, "takes", 5) || EhPalavraDeForeign(p, "ends", 4);
}

/* M6c P85: 'creates' | 'creates p' | 'destroys p' | 'takes p' depois da assinatura (D-M6c-3). Valida so o que a sintaxe ve (nome existe,
 * palavra conhecida, uma clausula so); o tipo do handle e conferido na semantica (sem_posse_c.c). */
static bool LerClausulaDePosse(Parser *p, Decl *d)
{
    if (TipoAtual(p) != TK_IDENT) return true;
    uint32_t pos_palavra = TokenAtual(p)->off;
    if (EhPalavraDeForeign(p, "creates", 7)) d->posse_c = POSSE_C_CRIA_RETORNO;
    else if (EhPalavraDeForeign(p, "destroys", 8)) d->posse_c = POSSE_C_DESTROI;
    else if (EhPalavraDeForeign(p, "takes", 5)) d->posse_c = POSSE_C_ENTREGA;
    else if (EhPalavraDeForeign(p, "ends", 4)) d->posse_c = POSSE_C_ENCERRA;
    else return DiagRelatar(p->diag, E0912, pos_palavra, "depois da assinatura do proc foreign so cabe creates, creates p, destroys p, takes p ou ends");
    d->posse_param = -1;
    d->pos_posse = pos_palavra;
    p->cur++;
    if (d->posse_c == POSSE_C_ENCERRA) { /* M6d P92b: 'ends' nao leva parametro; 'ends destroys f' e 2 clausulas (P93h), 'ends f' e o parametro */
        if (TipoAtual(p) != TK_IDENT) return true;
        if (EhClausulaDePosse(p)) return DiagRelatar(p->diag, E0912, TokenAtual(p)->off, "no maximo uma clausula de posse por proc foreign");
        return DiagRelatar(p->diag, E0912, TokenAtual(p)->off, "ends nao leva parametro: o proc encerra a lib inteira");
    }
    if (TipoAtual(p) == TK_IDENT) {
        d->pos_posse = TokenAtual(p)->off;
        for (uint32_t i = 0; i < d->nparams && d->posse_param < 0; i++) {
            if (d->params[i].nome == TokenAtual(p)->sym) d->posse_param = (int16_t)i;
        }
        if (d->posse_param < 0) return DiagRelatar(p->diag, E0912, d->pos_posse, "a clausula cita um parametro que o proc nao tem");
        if (d->posse_c == POSSE_C_CRIA_RETORNO) d->posse_c = POSSE_C_CRIA_SAIDA;
        p->cur++;
    } else if (d->posse_c != POSSE_C_CRIA_RETORNO) {
        return DiagRelatar(p->diag, E0912, pos_palavra, "destroys e takes pedem o nome do parametro: destroys f");
    } else if (!d->tem_retorno) {
        return DiagRelatar(p->diag, E0912, pos_palavra, "creates sem nome pede um retorno de handle; para parametro de saida escreva creates p");
    }
    if (TipoAtual(p) == TK_IDENT) return DiagRelatar(p->diag, E0912, TokenAtual(p)->off, "no maximo uma clausula de posse por proc foreign");
    return true;
}

/* 'proc nome[ = "simbolo"](params) [-> T] [creates|destroys p|takes p]' dentro de foreign: sem corpo (esta na biblioteca C). */
static bool AnalisarProcExterno(Parser *p)
{
    Decl *d = NovaDecl(p, D_PROC, TokenAtual(p)->off);
    d->externo = true;
    p->cur++;
    if (!LerNome(p, &d->nome, &d->pos, "nome do proc") || !LerAliasDoSimbolo(p, d)) return false;
    if (!Esperar(p, TK_LPAREN, "'('") || !AnalisarParams(p, d)) return false;
    if (TipoAtual(p) == TK_ARROW) {
        p->cur++;
        if (!AnalisarRetorno(p, d)) return false;
    }
    if (!LerClausulaDePosse(p, d)) return false;
    if (TipoAtual(p) == TK_LBRACE) {
        return DiagRelatar(p->diag, E0101, TokenAtual(p)->off, "proc de foreign nao tem corpo: o corpo esta na biblioteca C");
    }
    d->pos_fim = TokenAtual(p)->off;
    return EsperarFimDeLinha(p);
}

/* 'opaque Nome' dentro de foreign (v1.8b): struct sem campos, so vale atras de ponteiro (conferido na semantica). */
static bool AnalisarOpaco(Parser *p)
{
    Decl *d = NovaDecl(p, D_STRUCT, TokenAtual(p)->off);
    d->externo = true;
    p->cur++;
    if (!LerNome(p, &d->nome, &d->pos, "nome do tipo opaco")) return false;
    if (TipoAtual(p) == TK_LBRACE) {
        return DiagRelatar(p->diag, E0101, TokenAtual(p)->off,
                           "tipo opaco nao tem campos: o layout fica na biblioteca C; declare 'opaque Nome' e use ^Nome");
    }
    d->pos_fim = TokenAtual(p)->off;
    return EsperarFimDeLinha(p);
}

static bool AnalisarItemDeForeign(Parser *p)
{
    if (TipoAtual(p) == TK_IDENT && TokenAtual(p)->sym == SYM_LINK) return AnalisarLink(p);
    if (TipoAtual(p) == KW_PROC) return AnalisarProcExterno(p);
    if (EhPalavraDeForeign(p, "opaque", 6)) return AnalisarOpaco(p);
    if (EhPalavraDeForeign(p, "header", 6)) {
        return DiagRelatar(p->diag, E0101, TokenAtual(p)->off,
                           "header nao entra no .fj: gere o modulo com 'forja ligar <header.h> --link <lib>' e importe-o (ou declare os procs a mao)");
    }
    return ErroSintaxe(p, "'link \"biblioteca\"', 'opaque Nome', 'proc' ou '}'");
}

/* foreign { link "lib" / proc nome(...) -> T }: um item por linha; a AST fica plana (cada item vira uma Decl de topo). */
static bool AnalisarForeign(Parser *p)
{
    p->cur++;
    if (TipoAtual(p) == TK_TEXTO) {
        return DiagRelatar(p->diag, E0101, TokenAtual(p)->off,
                           "foreign nao leva nome: cada biblioteca e uma linha 'link \"nome\"' dentro do bloco");
    }
    if (!EsperarAbreBloco(p)) return false;
    uint32_t itens = 0;
    while (TipoAtual(p) != TK_RBRACE) {
        if (!AnalisarItemDeForeign(p)) return false;
        itens++;
    }
    if (itens == 0) {
        return DiagRelatar(p->diag, E0101, TokenAtual(p)->off,
                           "bloco foreign vazio: declare 'link \"biblioteca\"' ou 'proc nome(...)'");
    }
    p->cur++;
    return EsperarFimDeLinha(p);
}

/* 'import "caminho" [as apelido]' (v1.8): so no topo do arquivo. A forma do caminho e o apelido unico saem de ModulosRegistrar (E1001). */
static bool AnalisarImport(Parser *p)
{
    uint32_t pos_import = TokenAtual(p)->off;
    if (p->modulos == NULL) return DiagRelatar(p->diag, E0101, pos_import, "construcao ainda nao suportada: 'import'");
    if (p->viu_declaracao) {
        return DiagRelatar(p->diag, E0101, pos_import, "import so vale no topo do arquivo, antes de qualquer declaracao");
    }
    p->cur++;
    if (TipoAtual(p) != TK_TEXTO) {
        if (ParserIdiomaImport(p)) return false;
        return DiagRelatar(p->diag, E0101, TokenAtual(p)->off, "import pede o caminho entre aspas: import \"std:mem\"");
    }
    Decl *d = NovaDecl(p, D_IMPORT, pos_import);
    RegistroImport r = {.texto = TokenAtual(p)->sym, .apelido = -1, .arquivo = p->arquivo_atual, .pos_texto = TokenAtual(p)->off};
    d->nome = r.texto;
    d->pos_a = d->pos_b = r.pos_apelido = r.pos_texto;
    p->cur++;
    if (TipoAtual(p) == KW_AS) {
        p->cur++;
        if (TipoAtual(p) != TK_IDENT) return DiagRelatar(p->diag, E0101, TokenAtual(p)->off, "depois de 'as' vem o apelido");
        r.apelido = TokenAtual(p)->sym;
        d->pos_b = r.pos_apelido = TokenAtual(p)->off;
        p->cur++;
    }
    uint8_t indice;
    if (!EsperarFimDeLinha(p) || !ModulosRegistrar(p->modulos, &r, p->diag, &indice)) return false;
    d->indice = indice;
    d->nome_a = r.apelido >= 0 ? r.apelido : p->modulos->itens[indice].apelido;
    p->napelidos++;
    return true;
}

/* static_assert(cond, msg = "...") no topo (v1.11 L9): a chamada inteira fica em Decl.init; a semantica a confere como o comando de proc. */
static bool AnalisarStaticAssert(Parser *p)
{
    Decl *d = NovaDecl(p, D_ASSERT, TokenAtual(p)->off);
    d->init = AnalisarExpr(p);
    if (d->init == NULL) return false;
    d->pos_fim = TokenAtual(p)->off;
    return EsperarFimDeLinha(p);
}

static const AnalisaDeclaracao DECLARACOES[TK_QTD] = {
    [KW_IMPORT] = AnalisarImport, [KW_UNIT] = AnalisarUnit, [KW_CONST] = AnalisarConst, [KW_STRUCT] = AnalisarStruct,
    [KW_ENUM] = AnalisarEnum, [KW_UNION] = AnalisarUniao, [KW_PROC] = AnalisarProc, [KW_TEST] = AnalisarTest, [KW_FOREIGN] = AnalisarForeign,
};

/* [pub] [export] antes da declaracao: o parser so grava os dois marcadores em TODAS as decls criadas (foreign cria varias);
 * quem os confere e a semantica (E0903, E0906). */
bool AnalisarDeclaracao(Parser *p)
{
    bool publico = TipoAtual(p) == KW_PUB;
    if (publico) p->cur++;
    bool exportado = TipoAtual(p) == KW_EXPORT;
    if (exportado) p->cur++;
    AnalisaDeclaracao analisa = DECLARACOES[TipoAtual(p)];
    if (analisa == NULL && TipoAtual(p) == TK_IDENT && TokenAtual(p)->sym == SYM_STATIC_ASSERT) analisa = AnalisarStaticAssert;
    if (analisa == NULL) return ErroSintaxe(p, "declaracao (unit, const, struct, enum, proc, test ou foreign)");
    if (TipoAtual(p) != KW_IMPORT) p->viu_declaracao = true;
    else if (publico || exportado) return DiagRelatar(p->diag, E0101, TokenAtual(p)->off, "import nao leva pub nem export");
    Decl *antes = p->programa.ultima;
    if (!analisa(p)) return false;
    for (Decl *d = antes == NULL ? p->programa.primeira : antes->prox; d != NULL; d = d->prox) {
        d->publico = publico;
        d->exportado = exportado;
        d->modulo = p->modulo_atual;
    }
    return true;
}
