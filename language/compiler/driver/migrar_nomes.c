/* migrar_nomes.c - as regras por nome do 'forja migrar --comentarios' (ver migrar_nomes.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-L3d2: primeira versao.
 * Verificacao: construir.bat && python tests\ia_first\verificar_migrar.py --forja <forja.exe>
 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "driver/migrar_nomes.h"

enum { NOME_MAX = 96, CAMINHO_MAX = 192, MODULO_MAX = 128, FRAMES_MAX = 32, IMPORTADOS_MAX = 8 };

/* Uma chamada aberta: o nome simples do chamado (make, transmute ...) ou o modulo do chamado qualificado de vendor (db.open); os dois NULL = chamada sem regra. */
typedef struct { const char *chamado, *modulo; } Frame;

typedef struct {
    const NomesEntrada *e;
    Arena *arena;
    Vec *trocas, *duvidas;
    Frame pilha[FRAMES_MAX];
    int topo;                                   /* parenteses abertos (pode passar de FRAMES_MAX: os de fora nao guardam nada) */
    const char *importados[IMPORTADOS_MAX];     /* apelido padrao dos imports antigos do proprio texto (ultimo segmento do caminho) */
    int nimportados;
} Varredura;

/* O que uma sequencia q.m[.x] tem de antigo: token = 0 (q), 1 (m) ou 2 (x), a classe e as duas grafias (strings da tabela). */
typedef struct { int token; RenClasse classe; const char *antigo, *novo; } Renome;
typedef struct { Renome itens[3]; int n; bool duvida; } Renomes;

typedef enum { Q_NENHUM, Q_STD, Q_VENDOR } QualTipo;

/* ---------- a tabela ---------- */

static bool EhPalavra(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

/* O escopo (caminho do modulo) de vendor que a tabela conhece com esse caminho; NULL se nao ha. */
static const char *ModuloDaTabela(const char *modulo)
{
    for (size_t k = 0; k < RenomeadosContagem(); k++) {
        const Renomeado *r = RenomeadoPorIndice(k);

        if (r->classe == REN_PROC && strcmp(r->escopo, modulo) == 0) return r->escopo;
    }
    return NULL;
}

/* std\file.fj: o que o modulo exporta (pub). Eram ingleses antes da renomeacao, entao a tabela nao tem linha para eles. */
static bool MembroDeStdFile(const char *nome)
{
    static const char *const MEMBROS[] = {"read_all", "write_all", "FileError"};

    for (size_t k = 0; k < sizeof MEMBROS / sizeof MEMBROS[0]; k++)
        if (strcmp(MEMBROS[k], nome) == 0) return true;
    return false;
}

/* q e qualificador de std:file (arquivo, file), de um pacote de vendor da tabela (banco ou db, json ...) ou de nada. *modulo: o caminho do modulo de vendor (so em Q_VENDOR). */
static QualTipo TipoDoQualificador(const char *q, const char **modulo)
{
    const Renomeado *antigo = RenomeadoBuscar(REN_QUALIFICADOR, q);
    const char *novo = antigo != NULL ? antigo->novo : q;
    char caminho[MODULO_MAX];

    *modulo = NULL;
    if (strcmp(novo, "file") == 0) return Q_STD;
    snprintf(caminho, sizeof caminho, "vendor:%s/%s", novo, novo);
    *modulo = ModuloDaTabela(caminho);
    return *modulo != NULL ? Q_VENDOR : Q_NENHUM;
}

/* O proc ou o tipo do modulo com esse nome (antigo ou novo); *era diz se o que esta escrito e o antigo. */
static const Renomeado *EntradaDoMembro(const char *modulo, const char *nome, bool *era)
{
    const Renomeado *r = RenomeadoAchar(REN_PROC, modulo, nome, era);

    return r != NULL ? r : RenomeadoAchar(REN_TIPO, modulo, nome, era);
}

static void Somar(Renomes *out, int token, RenClasse classe, const char *antigo, const char *novo)
{
    out->itens[out->n++] = (Renome){token, classe, antigo, novo};
}

/* O membro x do enum 'tipo_novo' do modulo (db.DbError.nao_abriu). */
static void AvaliarMembroDeEnum(const char *modulo, const char *tipo_novo, const char *x, int token, Renomes *out)
{
    const char *const palavras[] = {modulo, tipo_novo, NULL};
    bool era = false;
    const Renomeado *r = RenomeadoAcharEm(REN_MEMBRO, palavras, x, &era);

    if (r != NULL && era) Somar(out, token, REN_MEMBRO, r->antigo, r->novo);
}

/* ErroBanco.nao_abriu sem qualificador: o tipo so vale troca se o que vem depois do ponto e membro dele (senao pode ser outra coisa com o mesmo nome). */
static void AvaliarTipoDireto(const char *nome, const char *m, Renomes *out)
{
    const Renomeado *t = RenomeadoBuscar(REN_TIPO, nome);
    bool antigo = t != NULL, era = false;
    const Renomeado *membro;
    const char *palavras[3];

    if (t == NULL) t = RenomeadoBuscarNovo(REN_TIPO, nome);
    if (t == NULL) return;
    palavras[0] = t->escopo;
    palavras[1] = t->novo;
    palavras[2] = NULL;
    membro = RenomeadoAcharEm(REN_MEMBRO, palavras, m, &era);
    if (membro == NULL) return;
    if (antigo) Somar(out, 0, REN_TIPO, t->antigo, t->novo);
    if (era) Somar(out, 1, REN_MEMBRO, membro->antigo, membro->novo);
}

/* q.m[.x]: as formas antigas da sequencia. importou = o proprio texto importa o modulo antigo de q. */
static void AvaliarQualificado(const char *q, const char *m, const char *x, bool importou, Renomes *out)
{
    const char *modulo;
    QualTipo tipo = TipoDoQualificador(q, &modulo);
    const Renomeado *qual = RenomeadoBuscar(REN_QUALIFICADOR, q), *membro = NULL;
    bool era = false;

    out->n = 0;
    out->duvida = false;
    if (tipo == Q_NENHUM) {
        AvaliarTipoDireto(q, m, out);
        return;
    }
    if (tipo == Q_VENDOR) membro = EntradaDoMembro(modulo, m, &era);
    if (qual != NULL) {
        if (membro != NULL || (tipo == Q_STD && MembroDeStdFile(m)) || importou) Somar(out, 0, REN_QUALIFICADOR, qual->antigo, qual->novo);
        else out->duvida = true;
    }
    if (membro == NULL) return;
    if (era) Somar(out, 1, membro->classe, membro->antigo, membro->novo);
    if (membro->classe == REN_TIPO && x != NULL) AvaliarMembroDeEnum(modulo, membro->novo, x, 2, out);
}

/* ---------- varredura por tokens ---------- */

static const Token *TokenEm(const Varredura *v, size_t k)
{
    return k < v->e->ntokens ? &v->e->tokens[k] : NULL;
}

static bool TipoDe(const Varredura *v, size_t k, TokenTipo tipo)
{
    const Token *t = TokenEm(v, k);

    return t != NULL && t->tipo == tipo;
}

/* O texto do identificador k (do fonte, nao do Interner: o parser reescreve o sym dos nomes antigos); NULL se nao e identificador ou e longo demais. */
static const char *TextoDe(const Varredura *v, size_t k, char *destino)
{
    const Token *t = TokenEm(v, k);

    if (t == NULL || t->tipo != TK_IDENT || t->len >= NOME_MAX) return NULL;
    memcpy(destino, v->e->texto + t->off, t->len);
    destino[t->len] = '\0';
    return destino;
}

static void Adicionar(Varredura *v, const Token *t, uint32_t atraso, uint32_t tam, const Renome *r)
{
    NomeTroca *n;

    if (t->off < v->e->ini || t->off >= v->e->fim) return;
    n = VecEmpurrar(v->trocas);
    *n = (NomeTroca){t->off + atraso, tam, r->classe, r->antigo, r->novo};
}

static void Lembrar(Varredura *v, const char *apelido)
{
    if (v->nimportados < IMPORTADOS_MAX) v->importados[v->nimportados++] = apelido;
}

static bool Importou(const Varredura *v, const char *q)
{
    for (int k = 0; k < v->nimportados; k++)
        if (strcmp(v->importados[k], q) == 0) return true;
    return false;
}

/* O que vem depois do ultimo '/' ou ':' (o apelido padrao do caminho do import). */
static const char *UltimoSegmento(const char *texto)
{
    const char *p = texto + strlen(texto);

    while (p > texto && p[-1] != '/' && p[-1] != ':') p--;
    return p;
}

/* O caminho entre aspas do token de texto k (cru, como esta no fonte); n = seu tamanho. */
static size_t CaminhoDoImport(const Varredura *v, const Token *t, char *destino)
{
    const char *p = v->e->texto + t->off + 1;
    size_t n = 0;

    while (p[n] != '"' && p[n] != '\0' && p[n] != '\n' && n < CAMINHO_MAX - 1) n++;
    memcpy(destino, p, n);
    destino[n] = '\0';
    return n;
}

/* import "caminho": o caminho inteiro do modulo antigo, ou so a pasta do pacote de vendor. */
static void ImportAntigo(Varredura *v, size_t k)
{
    const Token *t = &v->e->tokens[k];
    char caminho[CAMINHO_MAX], pasta[NOME_MAX];
    const Renomeado *r;
    size_t n = CaminhoDoImport(v, t, caminho), tam_pasta;
    bool era = false;

    r = RenomeadoBuscar(REN_MODULO, caminho);
    if (r != NULL) {
        Adicionar(v, t, 1, (uint32_t)n, &(Renome){0, REN_MODULO, r->antigo, r->novo});
        Lembrar(v, UltimoSegmento(r->antigo));
        return;
    }
    if (strncmp(caminho, "vendor:", 7) != 0) return;
    tam_pasta = strcspn(caminho + 7, "/");
    if (tam_pasta >= sizeof pasta) return;
    memcpy(pasta, caminho + 7, tam_pasta);
    pasta[tam_pasta] = '\0';
    r = RenomeadoAchar(REN_PACOTE, "import", pasta, &era);
    if (r != NULL && era) Adicionar(v, t, 1 + 7, (uint32_t)tam_pasta, &(Renome){0, REN_PACOTE, r->antigo, r->novo});
}

/* Os imports primeiro: o apelido antigo que eles criam vale para o resto do texto, venha o uso antes ou depois. */
static void LerImports(Varredura *v)
{
    for (size_t k = 0; k + 1 < v->e->ntokens; k++)
        if (v->e->tokens[k].tipo == KW_IMPORT && v->e->tokens[k + 1].tipo == TK_TEXTO) ImportAntigo(v, k + 1);
}

/* O chamado em tokens[k] ('nome(' ou 'q.nome('): make e cia por nome simples; db.open e cia pelo modulo do qualificador. */
static Frame FrameDaChamada(const Varredura *v, size_t k)
{
    char nome[NOME_MAX], q[NOME_MAX];
    const char *modulo;

    if (TextoDe(v, k, nome) == NULL) return (Frame){NULL, NULL};
    if (k >= 2 && TipoDe(v, k - 1, TK_DOT) && TextoDe(v, k - 2, q) != NULL) {
        if (TipoDoQualificador(q, &modulo) != Q_VENDOR) return (Frame){NULL, NULL};
        return (Frame){NULL, modulo};
    }
    return (Frame){ArenaCopiarTexto(v->arena, nome, strlen(nome)), NULL};
}

static void EmpilharChamada(Varredura *v, size_t k)
{
    Frame f = k > 0 ? FrameDaChamada(v, k - 1) : (Frame){NULL, NULL};

    if (v->topo < FRAMES_MAX) v->pilha[v->topo] = f;
    v->topo++;
}

static const Frame *FrameAtual(const Varredura *v)
{
    return v->topo > 0 && v->topo <= FRAMES_MAX ? &v->pilha[v->topo - 1] : NULL;
}

/* O valor 'escopo' de alloc = escopo (depois do rotulo em ou alloc): vira scope. */
static void ValorDoAlocador(Varredura *v, size_t k)
{
    char valor[NOME_MAX];
    const Renomeado *r;
    bool fim = TipoDe(v, k + 3, TK_COMMA) || TipoDe(v, k + 3, TK_RPAREN) || TipoDe(v, k + 3, TK_NL) || TipoDe(v, k + 3, TK_EOF);

    if (!TipoDe(v, k + 1, TK_ASSIGN) || TextoDe(v, k + 2, valor) == NULL || !fim) return;
    r = RenomeadoBuscar(REN_VALOR_ALOCADOR, valor);
    if (r != NULL) Adicionar(v, TokenEm(v, k + 2), 0, TokenEm(v, k + 2)->len, &(Renome){0, r->classe, r->antigo, r->novo});
}

/* Rotulo 'nome =' direto numa chamada: da tabela de rotulos do chamado simples ou, em chamada qualificada de vendor, dos rotulos do modulo. */
static void Rotulo(Varredura *v, size_t k)
{
    char nome[NOME_MAX];
    const Frame *f = FrameAtual(v);
    const Renomeado *r = NULL;
    bool era = false;

    if (f == NULL || TextoDe(v, k, nome) == NULL) return;
    if (f->modulo != NULL) r = RenomeadoAchar(REN_PARAM, f->modulo, nome, &era);
    else if (f->chamado != NULL) r = RenomeadoAchar(REN_ROTULO, f->chamado, nome, &era);
    if (r == NULL) return;
    if (era) Adicionar(v, TokenEm(v, k), 0, TokenEm(v, k)->len, &(Renome){0, r->classe, r->antigo, r->novo});
    if (r->classe == REN_ROTULO && strcmp(r->antigo, "em") == 0) ValorDoAlocador(v, k);
}

static void RegistrarDuvida(Varredura *v, size_t k)
{
    const Token *q = TokenEm(v, k), *m = TokenEm(v, k + 2);

    if (q->off < v->e->ini || q->off >= v->e->fim) return;
    *(NomeTroca *)VecEmpurrar(v->duvidas) = (NomeTroca){q->off, m->off + m->len - q->off, REN_QUALIFICADOR, NULL, NULL};
}

/* q.m[.x] com q em tokens[k]. */
static void Qualificado(Varredura *v, size_t k)
{
    char q[NOME_MAX], m[NOME_MAX], x[NOME_MAX];
    bool tem_x = TipoDe(v, k + 3, TK_DOT);
    Renomes ren;

    if (TextoDe(v, k, q) == NULL || TextoDe(v, k + 2, m) == NULL) return;
    AvaliarQualificado(q, m, tem_x && TextoDe(v, k + 4, x) != NULL ? x : NULL, Importou(v, q), &ren);
    for (int i = 0; i < ren.n; i++) {
        const Token *t = TokenEm(v, k + 2 * (size_t)ren.itens[i].token);

        Adicionar(v, t, 0, t->len, &ren.itens[i]);
    }
    if (ren.duvida) RegistrarDuvida(v, k);
}

static bool AbreArgumento(const Varredura *v, size_t k)
{
    return k > 0 && (TipoDe(v, k - 1, TK_LPAREN) || TipoDe(v, k - 1, TK_COMMA) || TipoDe(v, k - 1, TK_NL));
}

static void VerIdentificador(Varredura *v, size_t k)
{
    if (TipoDe(v, k + 1, TK_ASSIGN) && AbreArgumento(v, k)) Rotulo(v, k);
    else if (TipoDe(v, k + 1, TK_DOT) && TipoDe(v, k + 2, TK_IDENT) && !(k > 0 && TipoDe(v, k - 1, TK_DOT))) Qualificado(v, k);
}

void NomesDoCodigo(const NomesEntrada *e, Arena *arena, Vec *trocas, Vec *duvidas)
{
    Varredura v = {.e = e, .arena = arena, .trocas = trocas, .duvidas = duvidas};

    LerImports(&v);
    for (size_t k = 0; k < e->ntokens; k++) {
        TokenTipo tipo = (TokenTipo)e->tokens[k].tipo;

        if (tipo == TK_LPAREN) EmpilharChamada(&v, k);
        else if (tipo == TK_RPAREN && v.topo > 0) v.topo--;
        else if (tipo == TK_IDENT) VerIdentificador(&v, k);
    }
}

/* ---------- varredura solta ---------- */

static void LembrarForma(Arena *arena, Vec *formas, const char *texto, size_t n)
{
    char *copia = ArenaCopiarTexto(arena, texto, n);

    for (size_t k = 0; k < formas->n; k++)
        if (strcmp(*(const char **)VecEm(formas, k), copia) == 0) return;
    *(const char **)VecEmpurrar(formas) = copia;
}

/* Os caminhos antigos de modulo e as pastas antigas de pacote escritos em qualquer lugar do texto. */
static void ModulosSoltos(const char *texto, Arena *arena, Vec *formas)
{
    char pacote[CAMINHO_MAX];

    for (size_t k = 0; k < RenomeadosContagem(); k++) {
        const Renomeado *r = RenomeadoPorIndice(k);
        const char *achou = NULL;

        if (r->classe == REN_MODULO) achou = strstr(texto, r->antigo);
        if (r->classe == REN_PACOTE) {
            snprintf(pacote, sizeof pacote, "vendor:%s/", r->antigo);
            achou = strstr(texto, pacote);
        }
        if (achou != NULL) LembrarForma(arena, formas, achou, strcspn(achou, "\" \t\r\n"));
    }
}

/* O inicio da palavra depois de 'p' (que aponta para o '.'), ou NULL se nao ha. */
static const char *PalavraDepois(const char *p, size_t *n)
{
    const char *ini = p + 1;

    *n = 0;
    while (EhPalavra(ini[*n])) (*n)++;
    return *n > 0 ? ini : NULL;
}

/* palavra.m[.x]: o texto q.m dela se tem forma antiga. */
static void QualificadoSolto(const char *texto, size_t fim_q, const char *q, Arena *arena, Vec *formas)
{
    size_t nm, nx = 0;
    const char *m = PalavraDepois(texto + fim_q, &nm), *x = NULL;
    char mm[NOME_MAX], xx[NOME_MAX];
    Renomes ren;

    if (texto[fim_q] != '.' || m == NULL || nm >= NOME_MAX) return;
    memcpy(mm, m, nm);
    mm[nm] = '\0';
    if (m[nm] == '.') x = PalavraDepois(m + nm, &nx);
    if (x != NULL && nx < NOME_MAX) {
        memcpy(xx, x, nx);
        xx[nx] = '\0';
    }
    AvaliarQualificado(q, mm, x != NULL && nx < NOME_MAX ? xx : NULL, false, &ren);
    if (ren.n > 0) LembrarForma(arena, formas, texto + fim_q - strlen(q), strlen(q) + 1 + nm);
}

/* Palavra solta: so rotulo e valor (em, para, escopo), nome de rotulo de vendor seguido de '=', ou o inicio de q.m. */
static void PalavraSolta(const char *texto, size_t ini, size_t fim, Arena *arena, Vec *formas)
{
    char palavra[NOME_MAX];
    size_t depois = fim;

    if (fim - ini >= NOME_MAX) return;
    memcpy(palavra, texto + ini, fim - ini);
    palavra[fim - ini] = '\0';
    if (RenomeadoBuscar(REN_ROTULO, palavra) != NULL || RenomeadoBuscar(REN_VALOR_ALOCADOR, palavra) != NULL) {
        LembrarForma(arena, formas, palavra, fim - ini);
        return;
    }
    while (texto[depois] == ' ' || texto[depois] == '\t') depois++;
    if (texto[depois] == '=' && texto[depois + 1] != '=' && RenomeadoBuscar(REN_PARAM, palavra) != NULL) {
        LembrarForma(arena, formas, palavra, fim - ini);
        return;
    }
    QualificadoSolto(texto, fim, palavra, arena, formas);
}

void NomesSoltos(const char *texto, size_t tam, Arena *arena, Vec *formas)
{
    size_t i = 0;

    ModulosSoltos(texto, arena, formas);
    while (i < tam) {
        size_t ini = i;

        while (i < tam && EhPalavra(texto[i])) i++;
        if (i == ini) {
            i++;
            continue;
        }
        if (ini == 0 || texto[ini - 1] != '.') PalavraSolta(texto, ini, i, arena, formas);
    }
}
