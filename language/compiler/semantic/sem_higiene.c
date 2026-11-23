/* sem_higiene.c - higiene do codigo (v1.11 L10a): var nunca alterada (E0707), let/var nunca lido (E0708), valor devolvido que se perde (E0709)
 * e import cujo apelido nunca aparece (E1005).
 * Versao: 0.4.0
 * Changelog: 0.4.0 - Linguagem v1.13 L1a: import_usado por arquivo (trecho da Fonte), nao por modulo: E1005 por arquivo do pacote.
 *            0.3.0 - IA1-b04: o achado leva a sugestao aplicavel (SemHigieneSugerir/Editar): E0707 var -> let (certa), E0708 nome -> _nome (certa se o nome nunca foi alterado), E0709 '_ = ' antes da chamada (certa); Relatar copia a sugestao para o Diag.
 *            0.2.0 - Linguagem v1.11 L10a-bis: nome com '_' na frente isenta o E0708; E0707 por declaracao ('var a, b = f()', LocalInfo.grupo); 'try f()' solto com payload e E0709.
 *            0.1.0 - Linguagem v1.11 L10a: primeira versao.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 *
 * REGRA: a higiene so vale em declaracao sem outro erro, e na pratica so em programa sem nenhum outro erro (decisao do L10a: os err_* antigos
 * tem um proc com erro e outro com um let sem uso, e tem de manter as linhas deles). O achado do proc (o de menor posicao) fica PENDENTE
 * enquanto o proc e conferido, vai para Semantica.higienes quando o proc fecha sem erro (e sem tocar veneno) e so e relatado depois da fuga
 * (E0706), se a lista de erros estiver vazia. O import sem uso tambem. As flags ficam no LocalInfo (usos): ExNome marca
 * LOCAL_LIDO; SemLvalorMutavelDe e '&' de nome marcam LOCAL_ALTERADO; a atribuicao simples nao conta como leitura da raiz.
 */
#include "semantic/sem_interno.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* O achado mais cedo do proc (menor posicao no fonte) e o que fica; aceito, ele comeca sem sugestao (quem aceitou acrescenta as dele). */
bool SemHigienePropor(Semantica *s, DiagCodigo codigo, uint32_t pos, const char *fmt, ...)
{
    va_list ap;
    if (s->higiene.tem && s->higiene.pos <= pos) return false;
    s->higiene.tem = true;
    s->higiene.codigo = codigo;
    s->higiene.pos = pos;
    s->higiene.sug.n = 0;
    va_start(ap, fmt);
    vsnprintf(s->higiene.causa, sizeof s->higiene.causa, fmt, ap);
    va_end(ap);
    return true;
}

void SemHigieneSugerir(Semantica *s, DiagConfianca confianca, const char *fmt, ...)
{
    char texto[DIAG_DESCRICAO_MAX];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(texto, sizeof texto, fmt, ap);
    va_end(ap);
    DiagSugerir(&s->higiene.sug, confianca, "%s", texto);
}

void SemHigieneEditar(Semantica *s, uint32_t ini, uint32_t fim, const char *novo)
{
    DiagEditar(&s->higiene.sug, ini, fim, novo);
}

static bool EhCaractereDeNome(char c)
{
    return isalnum((unsigned char)c) != 0 || c == '_';
}

static bool EhEspacoDeLinha(char c)
{
    return c == ' ' || c == '\t';
}

/* Elo da cadeia 'x.c[i][..]p^': o que vem antes dele esta em x->a. */
static bool EhEloDaCadeia(const Expr *x)
{
    if (x->tipo_no == EX_UNARIO) return x->op == OP_DEREF;
    return x->tipo_no == EX_CAMPO || x->tipo_no == EX_INDICE || x->tipo_no == EX_FATIA;
}

/* Raiz sintatica da cadeia como local do proc em analise; NULL se a raiz nao e um nome local. */
static LocalInfo *RaizLocal(Semantica *s, const Expr *x)
{
    while (EhEloDaCadeia(x)) x = x->a;
    if (x->tipo_no != EX_NOME || x->modulo != 0) return NULL;
    return EscopoBuscarMut(&s->escopo, x->nome);
}

/* Atribuicao, '&', fatia, for &item, append, delete: o local da raiz mudou. */
void SemHigieneAlterar(Semantica *s, const Expr *alvo)
{
    LocalInfo *local = RaizLocal(s, alvo);
    if (local != NULL) local->usos |= LOCAL_ALTERADO;
}

/* Antes de conferir o alvo de 'x = v': a raiz ja tinha sido lida? (sem local = true: nada a desfazer depois). */
bool SemHigieneRaizLida(Semantica *s, const Expr *alvo)
{
    const LocalInfo *local = RaizLocal(s, alvo);
    return local == NULL || (local->usos & LOCAL_LIDO) != 0;
}

/* Depois de conferir o alvo de 'x = v' (v ainda nao): escrever na propria variavel, sem passar por ponteiro ou fatia, nao le a raiz. */
void SemHigieneEscrita(Semantica *s, const Expr *alvo, bool lida_antes)
{
    const Expr *raiz = SemRaizDaCadeia(s, alvo);
    if (lida_antes || raiz == NULL || raiz->tipo_no != EX_NOME || raiz->modulo != 0) return;
    LocalInfo *local = EscopoBuscarMut(&s->escopo, raiz->nome);
    if (local != NULL) local->usos &= (uint8_t)~LOCAL_LIDO;
}

/* v1.11 L10a-bis: algum nome da declaracao 'var a, b = f()' foi alterado (declaracao de um nome so: o proprio). */
static bool DeclaracaoAlterada(const Semantica *s, const LocalInfo *l, uint32_t altura)
{
    if ((l->usos & LOCAL_ALTERADO) != 0) return true;
    if (l->grupo == 0) return false;
    for (uint32_t i = altura; i < EscopoAltura(&s->escopo); i++) {
        const LocalInfo *outro = VecEm(&s->escopo.pilha, i);
        if (outro->grupo == l->grupo && (outro->usos & LOCAL_ALTERADO) != 0) return true;
    }
    return false;
}

/* v1.11 L10a-bis: '_r', '_unused' (sublinhado e mais algum caractere) avisam que o nao-uso e de proposito: isentam o E0708. */
static bool NomeIsentoDeUso(const char *nome)
{
    return nome[0] == '_' && nome[1] != '\0';
}

/* 'var' escrito logo antes do nome da declaracao (so espaco ou tab no meio): ini = onde o 'var' comeca. false = nao esta ali (2o nome de 'var a, b = f()'). */
static bool VarAntesDoNome(const char *texto, uint32_t pos, uint32_t *ini)
{
    uint32_t fim = pos;

    while (fim > 0 && EhEspacoDeLinha(texto[fim - 1])) fim--;
    if (fim < 3 || memcmp(texto + fim - 3, "var", 3) != 0) return false;
    if (fim > 3 && EhCaractereDeNome(texto[fim - 4])) return false;
    *ini = fim - 3;
    return true;
}

/* E0707: trocar o 'var' por 'let' e a unica correcao e nao muda o sentido (certa). Sem o 'var' junto do nome, so a dica. */
static void SugerirVarParaLet(Semantica *s, const LocalInfo *l, const char *nome)
{
    uint32_t ini;

    if (!VarAntesDoNome(s->fonte->texto, l->pos, &ini)) {
        SemHigieneSugerir(s, DIAG_SO_DICA, "troque 'var' por 'let' na declaracao que traz '%s'", nome);
        return;
    }
    SemHigieneSugerir(s, DIAG_CERTA, "troque 'var' por 'let' na declaracao de '%s'", nome);
    SemHigieneEditar(s, ini, ini + 3, "let");
}

/* E0708: o '_' na frente do nome so e certo se nada mais cita o nome (nunca foi alterado: as atribuicoes ficariam com o nome velho). A outra saida (usar o valor
 * ou remover a declaracao) nao tem edicao: depende do que o programa quer. */
static void SugerirNomeComSublinhado(Semantica *s, const LocalInfo *l, const char *nome)
{
    bool alterado = (l->usos & LOCAL_ALTERADO) != 0;

    if (nome[0] != '_' && !alterado) {
        SemHigieneSugerir(s, DIAG_CERTA, "troque o nome '%s' por '_%s' (o sublinhado avisa que nao ler e de proposito)", nome, nome);
        SemHigieneEditar(s, l->pos, l->pos, "_");
    }
    SemHigieneSugerir(s, DIAG_SO_DICA, "use o valor de '%s' ou remova a declaracao%s", nome, alterado ? " e as atribuicoes a ela" : "");
}

static void AchadoDoLocal(Semantica *s, const LocalInfo *l, uint32_t altura)
{
    const char *nome = SemNomeTexto(s, l->nome);
    bool lido = (l->usos & LOCAL_LIDO) != 0;
    if (!lido && !s->em_teste && !NomeIsentoDeUso(nome)) {
        if (SemHigienePropor(s, E0708, l->pos, "'%s' nunca e lido: remova o %s, use o valor, ou troque o nome por '_%s' (para descartar um retorno: _ = f() ou let _r = f())",
                             nome, l->natureza == LOC_VAR ? "var" : "let", nome))
            SugerirNomeComSublinhado(s, l, nome);
    } else if (l->natureza == LOC_VAR && !DeclaracaoAlterada(s, l, altura)) {
        if (SemHigienePropor(s, E0707, l->pos, "'%s' nunca e alterada: troque var por let", nome)) SugerirVarParaLet(s, l, nome);
    }
}

/* Fim de bloco: os let/var do bloco (parametro, for e ligacao de switch nao entram) com as flags que faltam. */
void SemHigieneFechar(Semantica *s, uint32_t altura)
{
    for (uint32_t i = altura; i < EscopoAltura(&s->escopo); i++) {
        const LocalInfo *l = VecEm(&s->escopo.pilha, i);
        if (l->natureza == LOC_LET || l->natureza == LOC_VAR) AchadoDoLocal(s, l, altura);
    }
}

/* Onde o comando comeca: a posicao do comando recuada sobre o nome qualificado ('mod.f(...)') e sobre o 'try' que o precede. */
static uint32_t InicioDoComando(const char *texto, uint32_t pos)
{
    uint32_t ini = pos;

    while (ini > 0 && (EhCaractereDeNome(texto[ini - 1]) || texto[ini - 1] == '.')) ini--;
    uint32_t antes = ini;
    while (antes > 0 && EhEspacoDeLinha(texto[antes - 1])) antes--;
    if (antes < ini && antes >= 3 && memcmp(texto + antes - 3, "try", 3) == 0 && (antes == 3 || !EhCaractereDeNome(texto[antes - 4]))) return antes - 3;
    return ini;
}

/* Chamada de proc (da Forja ou foreign) como comando: o valor devolvido nao some em silencio.
 * Sugestao certa: '_ = ' na frente do comando (descartar de proposito e o que o programa ja fazia, agora dito). */
void SemHigieneRetornoDescartado(Semantica *s, const Expr *comando)
{
    bool com_try = comando->tipo_no == EX_TRY; /* v1.11 L10a-bis: 'try f()' solto tambem perde o payload de 'T ! E' */
    const Expr *chamada = com_try ? comando->a : comando;
    if (chamada->tipo_no != EX_CHAMADA || chamada->ref != REF_PROC || comando->tipo == TIPO_VAZIO) return;
    const char *nome = SemNomeTexto(s, chamada->nome);
    const char *tentar = com_try ? "try " : "";
    if (!SemHigienePropor(s, E0709, comando->pos, "o valor devolvido por '%s' foi descartado: guarde (let r = %s%s(...)), use, ou descarte de proposito com _ = %s%s(...)",
                          nome, tentar, nome, tentar, nome))
        return;
    uint32_t ini = InicioDoComando(s->fonte->texto, comando->pos);
    SemHigieneSugerir(s, DIAG_CERTA, "descarte de proposito: escreva '_ = ' antes de '%s%s(...)'", tentar, nome);
    SemHigieneEditar(s, ini, ini, "_ = ");
}

/* E0709 do '_ = valor' (st_pos = o '_'). Chamada que nao devolve valor: tirar o '_ = ' deixa o comando valido e igual (certa). Sem chamada, o que fazer depende do programa (so dica). */
void SemHigieneSugerirDescarteInutil(Semantica *s, uint32_t st_pos, bool valor_e_chamada)
{
    const char *texto = s->fonte->texto;
    uint32_t fim = st_pos + 1;

    if (!valor_e_chamada) {
        SemHigieneSugerir(s, DIAG_SO_DICA, "'_ =' so descarta o retorno de uma chamada: use o valor, ou apague o comando");
        return;
    }
    while (EhEspacoDeLinha(texto[fim])) fim++;
    if (texto[fim] == '=') fim++;
    while (EhEspacoDeLinha(texto[fim])) fim++;
    SemHigieneSugerir(s, DIAG_CERTA, "a chamada nao devolve valor: tire o '_ = '");
    SemHigieneEditar(s, st_pos, fim, "");
}

/* Corpo novo: nenhum achado pendente. */
void SemHigieneIniciarCorpo(Semantica *s)
{
    s->higiene.tem = false;
}

/* Proc fechado sem erro: o achado pendente espera a fuga (corpo que tocou o veneno nao tem higiene). */
void SemHigieneGuardar(Semantica *s, Decl *d)
{
    if (!s->higiene.tem) return;
    s->higiene.tem = false;
    if (s->tocou_veneno) return;
    s->higiene.decl = d;
    *(SemHigiene *)VecEmpurrar(&s->higienes) = s->higiene;
}

/* Depois da fuga, com o programa SEM nenhum outro erro (o fixture err_* mantem as linhas dele): cada achado guardado vira o erro do proc (um por proc). */
bool SemHigieneRelatar(Semantica *s)
{
    bool ok = true;
    if (s->lista.n > 0) return true;
    for (size_t i = 0; i < s->higienes.n && !s->lista.cortado; i++) {
        const SemHigiene *h = VecEm(&s->higienes, i);
        ok = false;
        h->decl->com_erro = true;
        SemErro(s, h->codigo, h->pos, "%s", h->causa);
        s->diag->sug = h->sug;
        if (!SemGuardarErro(s, h->decl->modulo)) break;
    }
    return ok;
}

/* Nome qualificado 'apelido.nome' de outro modulo, escrito no arquivo em analise. */
void SemHigieneImportUsado(Semantica *s, uint8_t modulo)
{
    if (s->import_usado == NULL || modulo == 0) return;
    s->import_usado[(size_t)s->arquivo_atual * s->qtd_modulos + modulo] = 1;
}

/* v1.13: cada arquivo do pacote importa o que usa; o uso do apelido num arquivo nao paga o import do outro. */
static bool ImportSemUso(const Semantica *s, const Decl *d)
{
    if (d->tipo_no != D_IMPORT) return false;
    return s->import_usado[(size_t)FonteTrechoDoOffset(s->fonte, d->pos) * s->qtd_modulos + (size_t)d->indice] == 0;
}

/* Programa sem nenhum outro erro: import cujo apelido nunca aparece. */
bool SemHigieneImports(Semantica *s)
{
    bool ok = true;
    if (s->import_usado == NULL) return true;
    for (const Decl *d = s->programa->primeira; d != NULL && !s->lista.cortado; d = d->prox) {
        if (!ImportSemUso(s, d)) continue;
        ok = false;
        SemErro(s, E1005, d->pos, "o import \"%s\" nunca e usado: nenhum '%s.nome' no arquivo; remova a linha do import", SemNomeTexto(s, d->nome),
                SemNomeTexto(s, d->nome_a));
        if (!SemGuardarErro(s, d->modulo)) break;
    }
    return ok;
}
