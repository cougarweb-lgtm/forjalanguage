/* migrar_analise.c - o miolo do 'forja migrar' (ver migrar_analise.h).
 * Versao: 0.4.1
 * Changelog: 0.4.1 - G3 G0c: ordenacao total das trocas (OrdenaTroca desempata classe/antigo/novo; ComparaPosicao desempata coluna_fim/linha_fim/classe/antigo/novo).
 *            0.4.0 - S3 P4 (L11b-2, D10): trocas de linha inteira (foreign de libm: sai a declaracao, entra o import "std:math") no grupo "libm"; o teste de linhas iguais nao vale
 *            para elas (a linha some ou entra); os avisos REN_LIBM_AVISO saem no motivo do arquivo; FraseDoGrupo fala da libm.
 *            0.3.0 - IA1-L3d2: --comentarios (MigrarComentarios): as trocas de comentario entram junto das de codigo em Finalizar; estado revisao_comentario.
 *            0.2.0 - IA1-L3d1: modulo de biblioteca (so falta o proc main, E0201) tambem e analisado: std\ e vendor\ sao assim.
 *            0.1.0 - IA1-L3d1: primeira versao. LerFormasAntigas (front-end + semantica com Diag.trocas ligado), grupos import+qualificador, releitura do texto migrado e escolha das
 *            trocas que valem.
 * Verificacao: construir.bat && python tests\ia_first\verificar_migrar.py --forja <forja.exe>
 */
#include <stdlib.h>
#include <string.h>

#include "driver/compilacao.h"
#include "driver/edicoes.h"
#include "driver/migrar_analise.h"
#include "driver/migrar_comentario.h"

enum { GRUPOS_MAX = 64 };

/* O texto a analisar (sem BOM) e onde ele esta (os imports saem da pasta dele). */
typedef struct { const char *caminho, *texto; size_t tam; Arena *arena; } Entrada;

/* O que uma analise achou: compilou?, o 1o erro e as formas antigas do arquivo raiz (sem repeticao, na ordem do texto). */
typedef struct {
    bool compilou;
    const char *codigo;
    MigrarTroca *itens;
    uint32_t n;
    const char *avisos;     /* S3 P4: motivos de foreign de libm que nao migram (NULL = nenhum) */
} Leitura;

/* Um grupo import+qualificador: o apelido antigo (std:arquivo -> arquivo) e o novo (file); as trocas do grupo vivem e morrem juntas. */
typedef struct { const char *antigo, *novo; } Grupo;
typedef struct { Grupo itens[GRUPOS_MAX]; uint32_t n; } Grupos;

/* Por que um conjunto de trocas nao vale: o codigo do erro da releitura (NULL se nao foi um erro do compilador) e uma frase. */
typedef struct { const char *codigo, *frase; } Falha;

/* e: o texto sem BOM; original e tam_original: o arquivo lido por inteiro (BOM incluso), de onde sai o arquivo migrado. t e n: as formas antigas do codigo; ct e nct: as dos comentarios. */
typedef struct { Entrada e; const char *original; size_t tam_original; MigrarTroca *t; uint32_t n; const MigrarTroca *ct; uint32_t nct; } Contexto;

const char *MigrarEstadoTexto(MigrarEstado estado)
{
    static const char *const TEXTOS[] = {"sem_trocas", "ok", "nao_analisado", "revisao", "revisao_comentario"};

    return (unsigned)estado < sizeof TEXTOS / sizeof TEXTOS[0] ? TEXTOS[estado] : "?";
}

static const char *Copiar(Arena *a, const char *s)
{
    return ArenaCopiarTexto(a, s, strlen(s));
}

/* ---------- uma analise ---------- */

/* A Fonte da analise: copia do texto em malloc (os modulos importados sao anexados a ela). */
static bool CarregarFonte(Fonte *f, const Entrada *e)
{
    char *buf = malloc(e->tam + 1);

    if (buf == NULL) return false;
    memcpy(buf, e->texto, e->tam);
    buf[e->tam] = '\0';
    *f = (Fonte){.caminho = e->caminho, .texto = buf, .tam = e->tam};
    FonteIndexarLinhas(f);
    return true;
}

/* O codigo do 1o erro (a lista da semantica ou o Diag unico do lexico e da sintaxe); NULL = falha sem diagnostico. */
static const char *CodigoDoErro(const Compilacao *c, Arena *arena)
{
    if (c->sem.lista.n > 0) return Copiar(arena, DiagCodigoTexto(c->sem.lista.itens[0].codigo));
    return c->diag.tem ? Copiar(arena, DiagCodigoTexto(c->diag.codigo)) : NULL;
}

static int ComparaTroca(const void *a, const void *b)
{
    const RenTroca *x = a, *y = b;

    if (x->off != y->off) return x->off < y->off ? -1 : 1;
    return x->tam < y->tam ? -1 : x->tam > y->tam;
}

/* Ordem total: a chave de ComparaTroca e, nos empates, classe, antigo e novo (repetido continua sendo so a chave; fica o 1o dessa ordem). */
static int OrdenaTroca(const void *a, const void *b)
{
    const RenTroca *x = a, *y = b;
    int c = ComparaTroca(a, b);

    if (c != 0) return c;
    if (x->classe != y->classe) return x->classe < y->classe ? -1 : 1;
    c = strcmp(x->antigo, y->antigo);
    return c != 0 ? c : strcmp(x->novo, y->novo);
}

/* Fim do texto do arquivo raiz na Fonte (os modulos importados vem depois dele). */
static uint32_t FimDaRaiz(const Fonte *f)
{
    return f->ntrechos > 0 ? f->trechos[0].fim : (uint32_t)f->tam;
}

/* Troca de linha inteira (S3 P4): tam > 0 leva a linha toda, com o LF, ate o inicio da seguinte; tam = 0 e a insercao no inicio do arquivo. */
static MigrarTroca ParaMigrarTroca(const Fonte *f, const RenTroca *t, Arena *arena)
{
    MigrarTroca m = {.classe = t->classe, .antigo = Copiar(arena, t->antigo), .novo = Copiar(arena, t->novo), .grupo = -1};

    FonteLocalizar(f, t->off, &m.linha, &m.coluna);
    m.coluna_fim = m.coluna + t->tam;
    if (t->classe == REN_LIBM_LINHA && t->tam > 0) {
        m.antigo = ArenaCopiarTexto(arena, f->texto + t->off, t->tam);
        m.linha_fim = m.linha + 1;
        m.coluna = 1;
        m.coluna_fim = 1;
    }
    return m;
}

/* Um aviso de foreign de libm que nao migra, na ordem do texto: "'nome' nao migra para std:math: motivo". */
static void SomarAviso(Buf *avisos, const RenTroca *t)
{
    BufFormatar(avisos, "%s'%s' nao migra para std:math: %s", avisos->tam > 0 ? "; " : "", t->antigo, t->novo);
}

/* As formas antigas escritas no arquivo raiz, ordenadas e sem repeticao (a analise revisita expressoes). */
static void ColetarTrocas(const Compilacao *c, RenTrocas *reg, Arena *arena, Leitura *l)
{
    uint32_t fim = FimDaRaiz(&c->fonte);
    RenTroca *brutas = reg->itens.dados;
    MigrarTroca *itens = ArenaAlocar(arena, (reg->itens.n + 1) * sizeof *itens);
    Buf avisos = {0};

    if (reg->itens.n > 0) qsort(brutas, reg->itens.n, sizeof *brutas, OrdenaTroca);
    for (size_t k = 0; k < reg->itens.n; k++) {
        bool repetida = k > 0 && ComparaTroca(&brutas[k], &brutas[k - 1]) == 0;

        if (brutas[k].off >= fim || repetida) continue;
        if (brutas[k].classe == REN_LIBM_AVISO) SomarAviso(&avisos, &brutas[k]);
        else itens[l->n++] = ParaMigrarTroca(&c->fonte, &brutas[k], arena);
    }
    l->itens = itens;
    l->avisos = avisos.tam > 0 ? Copiar(arena, avisos.dados) : NULL;
    BufLiberar(&avisos);
}

/* Modulo de biblioteca (sem proc main): a semantica so reclama de E0201, que e a ultima conferencia, depois de todos os corpos; as formas antigas valem. */
static bool SoFaltaMain(const Compilacao *c)
{
    return c->sem.lista.n == 1 && c->sem.lista.itens[0].codigo == E0201;
}

/* Front-end e semantica sobre o texto como o check faz (modulos resolvidos), com Diag.trocas ligado: nada e impresso nem gravado. */
static void LerFormasAntigas(const Entrada *e, Leitura *l)
{
    Compilacao *c = calloc(1, sizeof *c);
    Opcoes o = {.comando = "check", .entrada = e->caminho};
    RenTrocas reg;

    memset(l, 0, sizeof *l);
    if (c == NULL) FalhaInterna("memoria esgotada alocando a compilacao");
    if (!CarregarFonte(&c->fonte, e)) {
        free(c);
        return;
    }
    IniciarCompilacao(c);
    c->opcoes = &o;
    c->sem.lista.teto = OpcoesMaxErros(&o);
    RenTrocasIniciar(&reg);
    c->diag.trocas = &reg;
    l->compilou = AnalisarSintaxe(c, true) && (AnalisarSemantica(c) || SoFaltaMain(c));
    if (l->compilou) ColetarTrocas(c, &reg, e->arena, l);
    else l->codigo = CodigoDoErro(c, e->arena);
    RenTrocasLiberar(&reg);
    LiberarCompilacao(c);
    free(c);
}

/* ---------- grupos import + qualificador ---------- */

/* O que vem depois do ultimo '/' ou ':' (o apelido padrao do caminho do import). */
static const char *UltimoSegmento(const char *texto)
{
    const char *p = texto + strlen(texto);

    while (p > texto && p[-1] != '/' && p[-1] != ':') p--;
    return p;
}

/* A troca de um import (REN_MODULO) cujo ultimo segmento muda, ou de um uso do qualificador antigo, faz parte de um grupo: false = nao faz. */
static bool ChaveDoGrupo(const MigrarTroca *t, Grupo *g)
{
    if (t->classe == REN_LIBM || t->classe == REN_LIBM_LINHA) { /* S3 P4: a libm vai junto: chamadas, declaracoes e o import valem ou caem juntos */
        *g = (Grupo){"libm", "libm"};
        return true;
    }
    if (t->classe == REN_QUALIFICADOR) {
        *g = (Grupo){t->antigo, t->novo};
        return true;
    }
    if (t->classe != REN_MODULO) return false;
    *g = (Grupo){UltimoSegmento(t->antigo), UltimoSegmento(t->novo)};
    return strcmp(g->antigo, g->novo) != 0;
}

static uint32_t AcharGrupo(const Grupos *gs, const char *antigo)
{
    uint32_t j = 0;

    while (j < gs->n && strcmp(gs->itens[j].antigo, antigo) != 0) j++;
    return j;
}

static void AtribuirGrupos(MigrarTroca *t, uint32_t n, Grupos *gs)
{
    gs->n = 0;
    for (uint32_t k = 0; k < n; k++) {
        Grupo g;
        uint32_t j;

        if (!ChaveDoGrupo(&t[k], &g)) continue;
        j = AcharGrupo(gs, g.antigo);
        if (j == gs->n && gs->n == GRUPOS_MAX) continue;
        if (j == gs->n) gs->itens[gs->n++] = g;
        t[k].grupo = (int32_t)j;
    }
}

/* ---------- releitura do texto migrado ---------- */

static Edicao EdicaoDaTroca(const MigrarTroca *t)
{
    return (Edicao){.linha = t->linha, .coluna = t->coluna, .coluna_fim = t->coluna_fim, .antigo = t->antigo, .novo = t->novo, .linha_fim = t->linha_fim};
}

/* As edicoes das trocas de codigo marcadas em 'usar'; com 'comentarios', as dos comentarios tambem (a releitura de ConjuntoValido nao as leva: so o codigo conta la). */
static Edicao *EdicoesDe(const Contexto *c, const bool *usar, uint32_t *ne, bool comentarios)
{
    Edicao *ed = ArenaAlocar(c->e.arena, ((size_t)c->n + c->nct + 1) * sizeof *ed);

    *ne = 0;
    for (uint32_t k = 0; k < c->n; k++)
        if (usar[k]) ed[(*ne)++] = EdicaoDaTroca(&c->t[k]);
    for (uint32_t k = 0; comentarios && k < c->nct; k++) ed[(*ne)++] = EdicaoDaTroca(&c->ct[k]);
    return ed;
}

/* O texto com as trocas marcadas compila e so usa ainda as formas antigas das trocas que ficaram de fora (a contagem bate)? Se nao, 'falha' diz por que. */
static bool ConjuntoValido(const Contexto *c, const bool *usar, Falha *falha)
{
    uint32_t ne;
    Edicao *ed = EdicoesDe(c, usar, &ne, false);
    Buf migrado = {0};
    Entrada e = c->e;
    Leitura l;

    *falha = (Falha){NULL, "as edicoes nao se aplicam ao texto"};
    if (EdicoesAplicar(c->e.texto, c->e.tam, ed, ne, &migrado, NULL) != EDICAO_OK) return false;
    e.texto = migrado.dados;
    e.tam = migrado.tam;
    LerFormasAntigas(&e, &l);
    BufLiberar(&migrado);
    if (!l.compilou) {
        *falha = (Falha){l.codigo, "o arquivo migrado nao compila"};
        return false;
    }
    if (l.n != c->n - ne) *falha = (Falha){NULL, "o arquivo migrado nao tem exatamente as formas antigas que ficaram de fora"};
    return l.n == c->n - ne;
}

static void MarcarTodas(bool *usar, uint32_t n, bool valor)
{
    for (uint32_t k = 0; k < n; k++) usar[k] = valor;
}

static void MarcarGrupo(const Contexto *c, bool *usar, int32_t grupo, bool valor)
{
    for (uint32_t k = 0; k < c->n; k++)
        if (c->t[k].grupo == grupo) usar[k] = valor;
}

/* Uma frase por grupo que ficou de fora, separadas por "; ". */
static void FraseDoGrupo(Buf *motivo, const Grupo *g, const Falha *falha)
{
    if (motivo->tam > 0) BufAnexar(motivo, "; ");
    if (strcmp(g->antigo, "libm") == 0) {
        BufFormatar(motivo, "a migracao da libm fica como esta: %s%s%s", falha->frase, falha->codigo != NULL ? " " : "", falha->codigo != NULL ? falha->codigo : "");
        return;
    }
    BufFormatar(motivo, "o import e os usos de '%s.' ficam como estao: com o qualificador '%s' %s%s%s", g->antigo, g->novo, falha->frase,
                falha->codigo != NULL ? " " : "", falha->codigo != NULL ? falha->codigo : "");
}

/* Todas juntas nao valem, mas as que nao sao de grupo valem: devolve cada grupo, um por vez, enquanto o conjunto continua valendo. */
static void AceitarGrupos(const Contexto *c, const Grupos *gs, bool *usar, MigrarResultado *r)
{
    Buf motivo = {0};

    for (uint32_t j = 0; j < gs->n; j++) {
        Falha falha;

        MarcarGrupo(c, usar, (int32_t)j, true);
        if (ConjuntoValido(c, usar, &falha)) continue;
        MarcarGrupo(c, usar, (int32_t)j, false);
        FraseDoGrupo(&motivo, &gs->itens[j], &falha);
        if (r->diagnostico == NULL) r->diagnostico = falha.codigo;
    }
    r->motivo = motivo.dados != NULL ? Copiar(c->e.arena, motivo.dados) : "";
    BufLiberar(&motivo);
}

/* Escolhe as trocas que valem em 'usar'. Todas valem: estado ok. Senao: sem os grupos; se ainda assim nao vale, nada e trocado. */
static void Decidir(const Contexto *c, const Grupos *gs, bool *usar, MigrarResultado *r)
{
    Falha falha;

    MarcarTodas(usar, c->n, true);
    if (ConjuntoValido(c, usar, &falha)) return;
    r->estado = MIGRAR_REVISAO;
    r->diagnostico = falha.codigo;
    for (uint32_t k = 0; k < c->n; k++) usar[k] = c->t[k].grupo < 0;
    if (gs->n > 0 && ConjuntoValido(c, usar, &falha)) {
        AceitarGrupos(c, gs, usar, r);
        return;
    }
    MarcarTodas(usar, c->n, false);
    r->motivo = Copiar(c->e.arena, falha.frase);
    if (falha.codigo != NULL) r->diagnostico = falha.codigo;
}

/* ---------- resultado ---------- */

/* Uma linha do texto: onde comeca, quantos bytes tem (com o LF, se tem) e se termina em LF. */
typedef struct { const char *p; size_t tam; bool lf; } Linha;

static Linha ProximaLinha(const char *p, const char *fim)
{
    const char *lf = memchr(p, '\n', (size_t)(fim - p));

    return (Linha){p, lf != NULL ? (size_t)(lf - p) + 1 : (size_t)(fim - p), lf != NULL};
}

/* Sem troca: igual byte a byte. Com troca: so muda por dentro, o fim da linha (LF ou fim do arquivo) fica onde estava. */
static bool LinhaConfere(const Linha *antes, const Linha *depois, bool tem_troca)
{
    if (tem_troca) return antes->lf == depois->lf;
    return antes->tam == depois->tam && memcmp(antes->p, depois->p, antes->tam) == 0;
}

/* Mesmo numero de linhas e toda linha sem troca igual byte a byte (a linha da troca so muda por dentro). */
static bool LinhasIguais(const Buf *antes, const Buf *depois, const MigrarTroca *t, uint32_t nt)
{
    const char *a = antes->dados, *b = depois->dados, *fa = a + antes->tam, *fb = b + depois->tam;
    uint32_t linha = 1, k = 0;

    while (a < fa && b < fb) {
        Linha la = ProximaLinha(a, fa), lb = ProximaLinha(b, fb);

        while (k < nt && t[k].linha < linha) k++;
        if (!LinhaConfere(&la, &lb, k < nt && t[k].linha == linha)) return false;
        a += la.tam;
        b += lb.tam;
        linha++;
    }
    return a == fa && b == fb;
}

static void SemTrocasQueValem(MigrarResultado *r, const char *motivo)
{
    r->estado = MIGRAR_REVISAO;
    r->motivo = motivo;
    r->ntrocas = 0;
    BufLiberar(&r->novo);
    r->novo = (Buf){0};
}

static int ComparaPosicao(const void *a, const void *b)
{
    const MigrarTroca *x = a, *y = b;

    if (x->linha != y->linha) return x->linha < y->linha ? -1 : 1;
    if (x->coluna != y->coluna) return x->coluna < y->coluna ? -1 : 1;
    if (x->coluna_fim != y->coluna_fim) return x->coluna_fim < y->coluna_fim ? -1 : 1;
    if (x->linha_fim != y->linha_fim) return x->linha_fim < y->linha_fim ? -1 : 1;
    if (x->classe != y->classe) return x->classe < y->classe ? -1 : 1;
    if (strcmp(x->antigo, y->antigo) != 0) return strcmp(x->antigo, y->antigo) < 0 ? -1 : 1;
    return strcmp(x->novo, y->novo); /* desempate total pelo conteudo: o array guarda valores, sem indice de insercao */
}

/* As trocas de codigo marcadas em 'usar' e todas as de comentario, ordenadas por (linha, coluna). */
static MigrarTroca *TrocasQueValem(const Contexto *c, const bool *usar, uint32_t *n)
{
    MigrarTroca *validas = ArenaAlocar(c->e.arena, ((size_t)c->n + c->nct + 1) * sizeof *validas);

    *n = 0;
    for (uint32_t k = 0; k < c->n; k++)
        if (usar[k]) validas[(*n)++] = c->t[k];
    for (uint32_t k = 0; k < c->nct; k++) validas[(*n)++] = c->ct[k];
    if (c->nct > 0 && *n > 1) qsort(validas, *n, sizeof *validas, ComparaPosicao);
    return validas;
}

/* S3 P4: alguma troca de 'validas' tira ou poe uma linha inteira (o teste de linhas iguais nao se aplica). */
static bool TemLinhaInteira(const MigrarTroca *validas, uint32_t n)
{
    for (uint32_t k = 0; k < n; k++) {
        if (validas[k].linha_fim > 0) return true;
    }
    return false;
}

/* Monta as trocas que valem e o arquivo migrado (BOM e quebras de linha do original); confere que as linhas ficaram iguais. */
static void Finalizar(const Contexto *c, const bool *usar, MigrarResultado *r)
{
    uint32_t ne;
    Edicao *ed = EdicoesDe(c, usar, &ne, true);
    MigrarTroca *validas = TrocasQueValem(c, usar, &r->ntrocas);
    Buf antes = {0};

    r->trocas = validas;
    if (r->ntrocas == 0) return;
    if (!EdicoesMontarArquivo(c->original, c->tam_original, ed, ne, &r->novo)) {
        SemTrocasQueValem(r, "as edicoes nao se aplicam ao arquivo");
        return;
    }
    antes.dados = (char *)c->original;
    antes.tam = c->tam_original;
    if (TemLinhaInteira(validas, r->ntrocas)) return; /* S3 P4: a linha inteira sai ou entra; a releitura de ConjuntoValido ja conferiu o arquivo */
    r->linhas_iguais = LinhasIguais(&antes, &r->novo, validas, r->ntrocas);
    if (!r->linhas_iguais) SemTrocasQueValem(r, "a troca mudaria as linhas do arquivo");
}

/* --comentarios: as trocas dos comentarios entram no Contexto; a lista de revisao vai direto para o resultado. */
static void LerComentarios(Contexto *c, MigrarResultado *r)
{
    ComentariosSaida saida;

    MigrarComentarios(c->e.caminho, c->e.texto, c->e.tam, c->e.arena, &saida);
    c->ct = saida.trocas;
    c->nct = saida.ntrocas;
    r->revisoes = saida.revisoes;
    r->nrevisoes = saida.nrevisoes;
}

/* Com troca e sem nenhuma que ficou de fora, ok; sem troca nenhuma e com comentario para revisar, revisao_comentario. */
static void FecharEstado(MigrarResultado *r)
{
    if (r->estado == MIGRAR_SEM_TROCAS && r->ntrocas > 0) r->estado = MIGRAR_OK;
    if (r->estado == MIGRAR_SEM_TROCAS && r->nrevisoes > 0) r->estado = MIGRAR_REVISAO_COMENTARIO;
}

void MigrarArquivo(const MigrarEntrada *entrada, Arena *arena, MigrarResultado *r)
{
    size_t bom = EdicoesTemBom(entrada->original, entrada->tam) ? EDICOES_BOM_TAM : 0;
    Contexto c = {.e = {entrada->caminho, entrada->original + bom, entrada->tam - bom, arena}, .original = entrada->original, .tam_original = entrada->tam};
    Grupos grupos = {.n = 0};
    Leitura l;
    bool *usar;

    memset(r, 0, sizeof *r);
    r->linhas_iguais = true;
    LerFormasAntigas(&c.e, &l);
    if (!l.compilou) {
        r->estado = MIGRAR_NAO_ANALISADO;
        r->diagnostico = l.codigo;
        return;
    }
    r->motivo = l.avisos; /* S3 P4: o aviso de libm que nao migra; uma decisao de grupo depois o troca */
    c.t = l.itens;
    c.n = l.n;
    if (entrada->comentarios) LerComentarios(&c, r);
    if (c.n == 0 && c.nct == 0) {
        FecharEstado(r);
        return;
    }
    usar = ArenaAlocar(arena, (size_t)c.n + 1);
    if (c.n > 0) {
        AtribuirGrupos(c.t, c.n, &grupos);
        r->estado = MIGRAR_OK;
        Decidir(&c, &grupos, usar, r);
    }
    Finalizar(&c, usar, r);
    FecharEstado(r);
}
