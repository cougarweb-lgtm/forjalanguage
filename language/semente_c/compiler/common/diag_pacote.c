/* diag_pacote.c - o erro como pacote pronto (contrato IA1, secao 1.1): sugestoes aplicaveis por maquina, trecho com marcador, correcao, confianca,
 * relacionados, spec e id. O diag.c chama DiagPacoteTrecho em todo diagnostico e DiagPacoteFim (com sugestao, Diag.sug.n > 0) ou DiagPacoteSemSugestao (sem).
 * Versao: 0.4.0
 * Changelog: 0.4.0 - Portao Z z5: DiagPacoteSemSugestao (correcao [], confianca "so_dica", relacionados, spec, id) e o trecho passam a sair em todo diagnostico (M-D11).
 *            0.3.0 - IA1-b14: "relacionados" deixa de ser sempre []: DiagRelacionar (lista por ocorrencia, no maximo DIAG_REL_MAX) e DiagPacoteRelacionados ({papel, arquivo, linha, coluna, texto, nota}; o texto e a linha do offset, ate DIAG_REL_TEXTO_MAX bytes).
 *            0.2.0 - IA1-b09: o spec (e a decisao spec x spec_ref) sai para o diag_spec.c e passa a valer para todo diagnostico; DiagPacoteIniciarLinha some e DiagPacoteFim perde o parametro 'forma'.
 *            0.1.0 - IA1-b04: primeira versao. Formato (ordem fixa, depois de forma_certa e dos extras): "correcao": [{arquivo, linha, coluna, coluna_fim, antigo, novo}],
 *            "confianca", "sugestao" (a descricao), "alternativas" (so se ha mais de uma sugestao), "relacionados": [] (o b14 preenche), "spec" na 1a vez do codigo
 *            na execucao e "spec_ref" nas seguintes (ate o b09 o spec e a forma_certa do catalogo), "id": "<codigo>@<arquivo>:<linha>:<coluna>". "trecho" (lista de
 *            linhas com numero e o marcador ^~~~) entra logo depois de coluna_fim. Coluna em BYTES (a mesma do diagnostico).
 * Verificacao: construir.bat && teste_common.exe && python ..\tests\ia_first\verificar_sugestoes.py
 */
#include "common/diag_pacote.h"

#include "common/diag_spec.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ---------- lista de sugestoes ---------- */

bool DiagSugerir(DiagSugestoes *s, DiagConfianca confianca, const char *fmt, ...)
{
    va_list ap;
    if (s->n >= DIAG_SUGESTOES_MAX) return false;
    DiagSugestao *nova = &s->itens[s->n++];
    nova->confianca = (uint8_t)confianca;
    nova->nedicoes = 0;
    va_start(ap, fmt);
    vsnprintf(nova->descricao, sizeof nova->descricao, fmt, ap);
    va_end(ap);
    return true;
}

bool DiagEditar(DiagSugestoes *s, uint32_t ini, uint32_t fim, const char *novo)
{
    if (s->n == 0 || fim < ini || fim - ini > DIAG_ANTIGO_MAX || strlen(novo) >= DIAG_NOVO_MAX) return false;
    DiagSugestao *atual = &s->itens[s->n - 1];
    if (atual->nedicoes >= DIAG_EDICOES_MAX) return false;
    DiagEdicao *e = &atual->edicoes[atual->nedicoes++];
    e->ini = ini;
    e->fim = fim;
    snprintf(e->novo, sizeof e->novo, "%s", novo);
    return true;
}

bool DiagRelacionar(Diag *d, DiagRelPapel papel, uint32_t off, const char *fmt, ...)
{
    va_list ap;
    DiagRelacionados *r = &d->rel;

    if (r->n >= DIAG_REL_MAX) return false;
    for (uint8_t k = 0; k < r->n; k++) {
        if (r->itens[k].papel == papel && r->itens[k].off == off) return false;
    }
    DiagRelacionado *novo = &r->itens[r->n++];
    novo->papel = (uint8_t)papel;
    novo->off = off;
    va_start(ap, fmt);
    vsnprintf(novo->nota, sizeof novo->nota, fmt, ap);
    va_end(ap);
    return true;
}

/* ---------- trecho ---------- */

enum { CONTEXTO_LINHAS = 2, JANELA_MAX = 2 * CONTEXTO_LINHAS + 1, LINHA_MAX = 200, MARCADOR_MAX = 60 };

/* [piso, teto) = os bytes do arquivo (trecho de modulo) que contem o diagnostico: o contexto nao atravessa para outro modulo. */
typedef struct { uint32_t piso, teto; } FaixaDoArquivo;
typedef struct { uint32_t inicio[JANELA_MAX]; uint32_t n, alvo; } Janela; /* inicio de cada linha mostrada; alvo = indice da linha do erro */
typedef struct { const Fonte *f; FaixaDoArquivo faixa; Janela janela; uint32_t linha_alvo; int largura; FILE *saida; } Trecho;

static FaixaDoArquivo FaixaDe(const Fonte *f, uint32_t off)
{
    FaixaDoArquivo fx = {0, (uint32_t)f->tam};
    uint32_t k = 0;
    if (f->ntrechos <= 1) return fx;
    while (k + 1 < f->ntrechos && f->trechos[k + 1].inicio <= off) k++;
    fx.piso = f->trechos[k].inicio;
    if (k + 1 < f->ntrechos) fx.teto = f->trechos[k + 1].inicio;
    return fx;
}

static uint32_t InicioDaLinha(const char *texto, uint32_t piso, uint32_t off)
{
    while (off > piso && texto[off - 1] != '\n') off--;
    return off;
}

/* Posicao do LF que fecha a linha (ou teto). */
static uint32_t FimDaLinha(const char *texto, uint32_t teto, uint32_t inicio)
{
    while (inicio < teto && texto[inicio] != '\n') inicio++;
    return inicio;
}

static bool EhContinuacaoUtf8(char c)
{
    return ((unsigned char)c & 0xC0) == 0x80;
}

/* A linha do erro, as CONTEXTO_LINHAS de antes (se existem no mesmo arquivo) e as de depois (idem). */
static void MontarJanela(Janela *j, const Fonte *f, FaixaDoArquivo fx, uint32_t off)
{
    uint32_t atual = InicioDaLinha(f->texto, fx.piso, off);
    uint32_t antes[CONTEXTO_LINHAS];
    uint32_t nant = 0;
    uint32_t s = atual;

    while (nant < CONTEXTO_LINHAS && s > fx.piso) {
        s = InicioDaLinha(f->texto, fx.piso, s - 1);
        antes[nant++] = s;
    }
    j->n = 0;
    while (nant > 0) j->inicio[j->n++] = antes[--nant];
    j->alvo = j->n;
    j->inicio[j->n++] = atual;
    s = atual;
    for (uint32_t k = 0; k < CONTEXTO_LINHAS; k++) {
        uint32_t fim = FimDaLinha(f->texto, fx.teto, s);
        if (fim + 1 >= fx.teto) break; /* sem linha seguinte (o LF final nao abre uma linha vazia) */
        s = fim + 1;
        j->inicio[j->n++] = s;
    }
}

static uint32_t Digitos(uint32_t v)
{
    uint32_t n = 1;
    while (v >= 10) {
        v /= 10;
        n++;
    }
    return n;
}

/* Bytes da linha que vao para o trecho: sem o CR, no maximo LINHA_MAX e sem cortar um caractere UTF-8 ao meio. */
static uint32_t TamanhoDaLinha(const Trecho *t, uint32_t inicio)
{
    const char *texto = t->f->texto;
    uint32_t tam = FimDaLinha(texto, t->faixa.teto, inicio) - inicio;
    if (tam > 0 && texto[inicio + tam - 1] == '\r') tam--;
    if (tam > LINHA_MAX) {
        tam = LINHA_MAX;
        while (tam > 0 && EhContinuacaoUtf8(texto[inicio + tam])) tam--;
    }
    return tam;
}

static void LinhaDoTrecho(const Trecho *t, uint32_t k)
{
    char buf[LINHA_MAX + 16];
    uint32_t inicio = t->janela.inicio[k];
    uint32_t numero = t->linha_alvo - t->janela.alvo + k;

    snprintf(buf, sizeof buf, "%*u | %.*s", t->largura, numero, (int)TamanhoDaLinha(t, inicio), t->f->texto + inicio);
    ImprimirTextoJson(t->saida, buf);
}

/* A linha "   | <espacos>^~~~": o recuo copia o tab e conta caracteres (nao bytes); a faixa e o token (no maximo MARCADOR_MAX). Coluna fora de LINHA_MAX: sem marcador. */
static void LinhaDoMarcador(const Trecho *t, uint32_t off, uint32_t tam_token)
{
    char buf[LINHA_MAX + MARCADOR_MAX + 32];
    const char *texto = t->f->texto;
    uint32_t inicio = t->janela.inicio[t->janela.alvo];
    uint32_t fim_linha = FimDaLinha(texto, t->faixa.teto, inicio);
    uint32_t fim_token = off + tam_token < fim_linha ? off + tam_token : fim_linha;
    uint32_t marca = 0;
    size_t n;

    if (off - inicio > LINHA_MAX) return;
    n = (size_t)snprintf(buf, sizeof buf, "%*s | ", t->largura, "");
    for (uint32_t i = inicio; i < off; i++) {
        if (!EhContinuacaoUtf8(texto[i])) buf[n++] = texto[i] == '\t' ? '\t' : ' ';
    }
    buf[n++] = '^';
    for (uint32_t i = off; i < fim_token && marca < MARCADOR_MAX; i++) {
        if (!EhContinuacaoUtf8(texto[i])) marca++;
    }
    for (uint32_t k = 1; k < marca; k++) buf[n++] = '~';
    buf[n] = '\0';
    fputs(", ", t->saida);
    ImprimirTextoJson(t->saida, buf);
}

void DiagPacoteTrecho(const Diag *d, const Fonte *f, uint32_t tam_token, FILE *saida)
{
    uint32_t off = d->off < f->tam ? d->off : (uint32_t)f->tam;
    Trecho t = {.f = f, .faixa = FaixaDe(f, off), .saida = saida};

    t.linha_alvo = FonteLocalizarNoArquivo(f, off).linha;
    MontarJanela(&t.janela, f, t.faixa, off);
    t.largura = (int)Digitos(t.linha_alvo + t.janela.n - 1 - t.janela.alvo);
    fputs(", \"trecho\": [", saida);
    for (uint32_t k = 0; k < t.janela.n; k++) {
        if (k > 0) fputs(", ", saida);
        LinhaDoTrecho(&t, k);
        if (k == t.janela.alvo) LinhaDoMarcador(&t, off, tam_token);
    }
    fputc(']', saida);
}

/* ---------- correcao, confianca, alternativas ---------- */

static const char *NomeDaConfianca(uint8_t confianca)
{
    static const char *const NOMES[] = {"certa", "provavel", "so_dica"};
    return confianca <= DIAG_SO_DICA ? NOMES[confianca] : NOMES[DIAG_SO_DICA];
}

/* Caminho com '/' no lugar de '\' (o mesmo que o campo "arquivo" imprime). */
static void CaminhoNormal(char *destino, size_t tam, const char *caminho)
{
    snprintf(destino, tam, "%s", caminho);
    for (char *p = destino; *p != '\0'; p++) {
        if (*p == '\\') *p = '/';
    }
}

static void ImprimirEdicao(const Fonte *f, const DiagEdicao *e, FILE *saida)
{
    FontePosicao ini = FonteLocalizarNoArquivo(f, e->ini);
    FontePosicao fim = FonteLocalizarNoArquivo(f, e->fim);
    char caminho[1024];
    char antigo[DIAG_ANTIGO_MAX + 1];
    size_t tam = e->fim <= f->tam ? e->fim - e->ini : 0;

    memcpy(antigo, f->texto + e->ini, tam);
    antigo[tam] = '\0';
    CaminhoNormal(caminho, sizeof caminho, ini.caminho);
    fputs("{\"arquivo\": ", saida);
    ImprimirTextoJson(saida, caminho);
    fprintf(saida, ", \"linha\": %u, \"coluna\": %u, \"coluna_fim\": %u, \"antigo\": ", ini.linha, ini.coluna, fim.coluna);
    ImprimirTextoJson(saida, antigo);
    fputs(", \"novo\": ", saida);
    ImprimirTextoJson(saida, e->novo);
    fputc('}', saida);
}

static void ImprimirEdicoes(const Fonte *f, const DiagSugestao *s, FILE *saida)
{
    fputc('[', saida);
    for (uint32_t k = 0; k < s->nedicoes; k++) {
        if (k > 0) fputs(", ", saida);
        ImprimirEdicao(f, &s->edicoes[k], saida);
    }
    fputc(']', saida);
}

/* As sugestoes depois da principal: [{confianca, sugestao, correcao}]. */
static void ImprimirAlternativas(const Diag *d, const Fonte *f, FILE *saida)
{
    fputs(", \"alternativas\": [", saida);
    for (uint32_t k = 1; k < d->sug.n; k++) {
        const DiagSugestao *s = &d->sug.itens[k];
        if (k > 1) fputs(", ", saida);
        fprintf(saida, "{\"confianca\": \"%s\", \"sugestao\": ", NomeDaConfianca(s->confianca));
        ImprimirTextoJson(saida, s->descricao);
        fputs(", \"correcao\": ", saida);
        ImprimirEdicoes(f, s, saida);
        fputc('}', saida);
    }
    fputc(']', saida);
}

/* ---------- id ---------- */

static void ImprimirId(const Diag *d, const Fonte *f, const char *codigo, FILE *saida)
{
    FontePosicao pos = FonteLocalizarNoArquivo(f, d->off);
    char caminho[1024];
    char id[1100];

    CaminhoNormal(caminho, sizeof caminho, pos.caminho);
    snprintf(id, sizeof id, "%s@%s:%u:%u", codigo, caminho, pos.linha, pos.coluna);
    fputs(", \"id\": ", saida);
    ImprimirTextoJson(saida, id);
}

/* ---------- relacionados ---------- */

static const char *NomeDoPapel(uint8_t papel)
{
    static const char *const NOMES[] = {"declaracao", "tipo", "assinatura", "parecido", "uso_anterior"};
    return papel <= DIAG_REL_USO_ANTERIOR ? NOMES[papel] : NOMES[DIAG_REL_DECLARACAO];
}

/* A linha do offset sem a indentacao, o CR e o '{' que abre o corpo; mais que DIAG_REL_TEXTO_MAX bytes vira o corte (sem partir UTF-8) mais "...". */
static void TextoDoRelacionado(const Fonte *f, uint32_t off, char *saida)
{
    FaixaDoArquivo fx = FaixaDe(f, off);
    uint32_t ini = InicioDaLinha(f->texto, fx.piso, off);
    uint32_t fim = FimDaLinha(f->texto, fx.teto, ini);
    uint32_t corte = DIAG_REL_TEXTO_MAX - 3;

    while (ini < fim && (f->texto[ini] == ' ' || f->texto[ini] == '\t')) ini++;
    while (fim > ini && strchr(" \t\r", f->texto[fim - 1]) != NULL) fim--;
    if (fim > ini && f->texto[fim - 1] == '{') {
        fim--;
        while (fim > ini && f->texto[fim - 1] == ' ') fim--;
    }
    if (fim - ini <= DIAG_REL_TEXTO_MAX) {
        memcpy(saida, f->texto + ini, fim - ini);
        saida[fim - ini] = '\0';
        return;
    }
    while (corte > 0 && EhContinuacaoUtf8(f->texto[ini + corte])) corte--;
    memcpy(saida, f->texto + ini, corte);
    memcpy(saida + corte, "...", 4); /* cabe: saida tem DIAG_REL_TEXTO_MAX + 1 bytes e corte <= DIAG_REL_TEXTO_MAX - 3 */
}

static void ImprimirRelacionado(const Fonte *f, const DiagRelacionado *r, FILE *saida)
{
    uint32_t off = r->off < f->tam ? r->off : (uint32_t)f->tam;
    FontePosicao pos = FonteLocalizarNoArquivo(f, off);
    char caminho[1024];
    char texto[DIAG_REL_TEXTO_MAX + 1];

    CaminhoNormal(caminho, sizeof caminho, pos.caminho);
    TextoDoRelacionado(f, off, texto);
    fprintf(saida, "{\"papel\": \"%s\", \"arquivo\": ", NomeDoPapel(r->papel));
    ImprimirTextoJson(saida, caminho);
    fprintf(saida, ", \"linha\": %u, \"coluna\": %u, \"texto\": ", pos.linha, pos.coluna);
    ImprimirTextoJson(saida, texto);
    fputs(", \"nota\": ", saida);
    ImprimirTextoJson(saida, r->nota);
    fputc('}', saida);
}

void DiagPacoteRelacionados(const Diag *d, const Fonte *f, FILE *saida)
{
    fputs(", \"relacionados\": [", saida);
    for (uint8_t k = 0; k < d->rel.n; k++) {
        if (k > 0) fputs(", ", saida);
        ImprimirRelacionado(f, &d->rel.itens[k], saida);
    }
    fputc(']', saida);
}

void DiagPacoteFim(const Diag *d, const Fonte *f, const char *codigo, FILE *saida)
{
    const DiagSugestao *principal = &d->sug.itens[0];

    fputs(", \"correcao\": ", saida);
    ImprimirEdicoes(f, principal, saida);
    fprintf(saida, ", \"confianca\": \"%s\", \"sugestao\": ", NomeDaConfianca(principal->confianca));
    ImprimirTextoJson(saida, principal->descricao);
    if (d->sug.n > 1) ImprimirAlternativas(d, f, saida);
    DiagPacoteRelacionados(d, f, saida);
    DiagSpecImprimir(d->codigo, saida);
    ImprimirId(d, f, codigo, saida);
}

void DiagPacoteSemSugestao(const Diag *d, const Fonte *f, const char *codigo, FILE *saida)
{
    fputs(", \"correcao\": [], \"confianca\": \"so_dica\"", saida);
    DiagPacoteRelacionados(d, f, saida);
    DiagSpecImprimir(d->codigo, saida);
    ImprimirId(d, f, codigo, saida);
}
