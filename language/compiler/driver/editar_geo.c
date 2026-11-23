/* editar_geo.c - tokens, linhas e modulos do fonte para as operacoes inserir_decl, remover_decl e mover_decl do 'forja editar' (ver editar_interno.h).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - Linguagem v1.13 L1b (modulo de pasta): a janela, o caminho e o ponto de troca sao do ARQUIVO (trecho da Fonte) do offset, nao do modulo: EditarTrocarNoModulo vira EditarTrocarNoArquivo (sem o modulo), EditarFimDoConteudo e
 *            EditarPontoSemImports recebem o arquivo; EditarArquivoDoOffset; EditarAcharModuloEArquivo (o "arquivo" do inserir_decl num pacote de 2+ arquivos); EditarAcharModulo acha o arquivo de qualquer trecho.
 *            0.1.0 - IA1-I5: primeira versao. Todo offset aqui e global (o texto de todos os modulos junto, como a Fonte o guarda); a janela de um modulo e [inicio, fim) do trecho dele.
 *            As linhas contam o CR do CRLF como parte do fim de linha. A faixa de uma declaracao leva o comentario '//' colado acima (o mesmo bloco do 'forja consultar api').
 * Verificacao: construir.bat && python tests\editar\verificar_editar_i5.py --forja <forja.exe>
 */
#include <stdio.h>
#include <string.h>

#include "driver/editar_interno.h"

typedef struct { const char *t; uint32_t piso, teto; } Janela;

/* A janela de um arquivo (trecho da Fonte; sem trechos, o texto todo). O modulo de pasta tem um trecho por arquivo: as linhas e o fim de arquivo valem por arquivo. */
static Janela JanelaDoArquivo(const Consulta *q, uint32_t arquivo)
{
    const Fonte *f = &q->c->fonte;
    Janela j = {.t = f->texto, .piso = 0, .teto = (uint32_t)f->tam};

    if (f->ntrechos > 0) {
        j.piso = f->trechos[arquivo].inicio;
        j.teto = f->trechos[arquivo].fim;
    }
    return j;
}

uint8_t EditarModuloDoOffset(const Consulta *q, uint32_t off)
{
    return ConsultaModuloDoOffset(q, off);
}

uint32_t EditarArquivoDoOffset(const Consulta *q, uint32_t off)
{
    return ConsultaTrechoDoOffset(q, off);
}

static Janela JanelaDoOffset(const Consulta *q, uint32_t off)
{
    return JanelaDoArquivo(q, ConsultaTrechoDoOffset(q, off));
}

/* ---------- tokens ---------- */

const Token *EditarToken(const Consulta *q, uint32_t i)
{
    return (const Token *)VecEm(&q->fim.toks, i);
}

uint32_t EditarTokenExato(const Consulta *q, uint32_t off)
{
    uint32_t lo = 0, hi = (uint32_t)q->fim.toks.n;

    while (lo < hi) {
        uint32_t meio = lo + (hi - lo) / 2;

        if (EditarToken(q, meio)->off < off) lo = meio + 1;
        else hi = meio;
    }
    return lo < q->fim.toks.n && EditarToken(q, lo)->off == off ? lo : TOKEN_AUSENTE;
}

uint32_t EditarTokenDoNome(const Consulta *q, const IndUso *u)
{
    uint32_t i = EditarTokenExato(q, u->pos_nome), n = (uint32_t)q->fim.toks.n;

    if (i == TOKEN_AUSENTE) return i;
    while (EditarToken(q, i)->sym != u->alvo->nome && i + 2 < n && EditarToken(q, i + 1)->tipo == TK_DOT && EditarToken(q, i + 2)->tipo == TK_IDENT) i += 2;
    return EditarToken(q, i)->sym == u->alvo->nome ? i : TOKEN_AUSENTE;
}

/* ---------- linhas ---------- */

uint32_t EditarInicioDaLinha(const Consulta *q, uint32_t off)
{
    Janela j = JanelaDoOffset(q, off);

    while (off > j.piso && j.t[off - 1] != '\n') off--;
    return off;
}

uint32_t EditarFimDaLinha(const Consulta *q, uint32_t off)
{
    Janela j = JanelaDoOffset(q, off);

    while (off < j.teto && j.t[off] != '\n') off++;
    if (off < j.teto && off > j.piso && j.t[off - 1] == '\r') off--;
    return off;
}

uint32_t EditarProximaLinha(const Consulta *q, uint32_t off)
{
    Janela j = JanelaDoOffset(q, off);

    while (off < j.teto && j.t[off] != '\n') off++;
    return off < j.teto ? off + 1 : j.teto;
}

/* A linha que comeca em 'ini' so tem espacos e tabs? */
static bool LinhaEmBranco(const Consulta *q, uint32_t ini)
{
    const char *t = q->c->fonte.texto;
    uint32_t fim = EditarFimDaLinha(q, ini);

    for (uint32_t k = ini; k < fim; k++) {
        if (t[k] != ' ' && t[k] != '\t') return false;
    }
    return true;
}

/* A linha que comeca em 'ini' e de comentario ('//' depois do recuo)? */
static bool LinhaDeComentario(const Consulta *q, uint32_t ini)
{
    const char *t = q->c->fonte.texto;
    uint32_t fim = EditarFimDaLinha(q, ini);

    while (ini < fim && (t[ini] == ' ' || t[ini] == '\t')) ini++;
    return fim - ini >= 2 && t[ini] == '/' && t[ini + 1] == '/';
}

bool EditarLinhaTemTexto(const Consulta *q, uint32_t off)
{
    return !LinhaEmBranco(q, EditarInicioDaLinha(q, off));
}

bool EditarLinhaSeguinte(const Consulta *q, uint32_t off, uint32_t *ini, bool *em_branco)
{
    uint32_t prox = EditarProximaLinha(q, off);

    if (prox >= JanelaDoOffset(q, off).teto) return false;
    *ini = prox;
    *em_branco = LinhaEmBranco(q, prox);
    return true;
}

uint32_t EditarFimDoConteudo(const Consulta *q, uint32_t arquivo)
{
    Janela j = JanelaDoArquivo(q, arquivo);
    uint32_t achado = j.piso;

    for (uint32_t ini = j.piso; ini < j.teto; ini = EditarProximaLinha(q, ini)) {
        if (!LinhaEmBranco(q, ini)) achado = EditarFimDaLinha(q, ini);
    }
    return achado;
}

bool EditarPontoSemImports(const Consulta *q, uint32_t arquivo, uint32_t *onde, bool *apos_cabecalho)
{
    Janela j = JanelaDoArquivo(q, arquivo);
    uint32_t ini = j.piso, ultima = j.piso;

    while (ini < j.teto && LinhaDeComentario(q, ini)) {
        ultima = ini;
        ini = EditarProximaLinha(q, ini);
    }
    *onde = j.piso;
    *apos_cabecalho = false;
    if (ini == j.piso || ini >= j.teto || !LinhaEmBranco(q, ini)) return j.teto > j.piso && ini < j.teto;
    *onde = EditarFimDaLinha(q, ultima);
    *apos_cabecalho = true;
    return EditarProximaLinha(q, ini) < j.teto;
}

/* ---------- modulos ---------- */

static const char *CaminhoDoArquivo(const Consulta *q, uint32_t arquivo)
{
    const Fonte *f = &q->c->fonte;

    return f->ntrechos > 0 ? f->trechos[arquivo].caminho : f->caminho;
}

bool EditarTrocarNoArquivo(Plano *p, uint32_t ini, uint32_t fim, const char *novo)
{
    uint32_t piso = ConsultaInicioDoArquivo(p->q, ini);

    return PlanoTrocarLocais(p, CaminhoDoArquivo(p->q, ConsultaTrechoDoOffset(p->q, ini)), ini - piso, fim - piso, novo);
}

bool EditarModuloEditavel(Plano *p, uint8_t modulo)
{
    const Modulo *m = &p->q->c->modulos.itens[modulo];

    if (m->origem == MOD_RAIZ || m->origem == MOD_ARQUIVO) return true;
    return PlanoErro(p, "o modulo \"%.*s\" e da biblioteca padrao, de vendor ou da engine: nao se edita", (int)InternerTam(&p->q->c->nomes, m->caminho),
                     InternerTexto(&p->q->c->nomes, m->caminho));
}

/* 'texto' sem o ".fj" do fim, com barras normais, em 'saida'. */
static void SemExtensao(const char *texto, char *saida, size_t tam)
{
    size_t n;

    snprintf(saida, tam, "%s", texto);
    for (char *c = saida; *c != '\0'; c++) {
        if (*c == '\\') *c = '/';
    }
    n = strlen(saida);
    if (n > 3 && _stricmp(saida + n - 3, ".fj") == 0) saida[n - 3] = '\0';
}

/* O arquivo (trecho da Fonte) cujo caminho relativo a pasta da raiz, sem o ".fj", e 'normal'; -1 se nenhum. */
static int ArquivoPeloCaminho(Plano *p, const char *normal)
{
    char rel[EDITAR_CAMINHO_MAX], rel_sem[EDITAR_CAMINHO_MAX];
    uint32_t nt = p->q->c->fonte.ntrechos > 0 ? p->q->c->fonte.ntrechos : 1;

    for (uint32_t t = 0; t < nt; t++) {
        EditarCaminhoRelativo(p, CaminhoDoArquivo(p->q, t), rel, sizeof rel);
        SemExtensao(rel, rel_sem, sizeof rel_sem);
        if (_stricmp(rel_sem, normal) == 0) return (int)t;
    }
    return -1;
}

int EditarAcharModulo(Plano *p, const char *texto)
{
    char normal[EDITAR_CAMINHO_MAX];
    int m = ConsultaAcharModulo(p->q, texto), arquivo;

    if (m >= 0) return m;
    SemExtensao(texto, normal, sizeof normal);
    if (ConsultaAcharModulo(p->q, normal) >= 0) return ConsultaAcharModulo(p->q, normal);
    arquivo = ArquivoPeloCaminho(p, normal);
    if (arquivo >= 0) return ModulosPacoteDoArquivo(&p->q->c->modulos, (uint32_t)arquivo);
    PlanoErro(p, "'%s' nao e modulo do projeto (use o caminho do import, ex.: geo/vetor, ou o arquivo .fj relativo a pasta da raiz)", texto);
    return -1;
}

int EditarAcharModuloEArquivo(Plano *p, const char *texto, uint32_t *arquivo)
{
    char normal[EDITAR_CAMINHO_MAX], rel[EDITAR_CAMINHO_MAX];
    int m = EditarAcharModulo(p, texto), nomeado;
    uint32_t primeiro, n;

    if (m < 0) return -1;
    SemExtensao(texto, normal, sizeof normal);
    nomeado = ArquivoPeloCaminho(p, normal);
    if (nomeado >= 0 && ModulosPacoteDoArquivo(&p->q->c->modulos, (uint32_t)nomeado) == m) {
        *arquivo = (uint32_t)nomeado;
        return m;
    }
    n = ModulosTrechosDoPacote(&p->q->c->modulos, p->q->c->fonte.ntrechos, (uint8_t)m, &primeiro);
    if (n > 1) {
        EditarCaminhoRelativo(p, CaminhoDoArquivo(p->q, primeiro), rel, sizeof rel);
        PlanoErro(p, "o modulo '%s' e uma pasta com %u arquivos: diga em qual entra com \"arquivo\" (o .fj relativo a pasta da raiz, ex.: \"%s\")", texto, n, rel);
        return -1;
    }
    *arquivo = primeiro;
    return m;
}

/* ---------- a faixa de uma declaracao ---------- */

/* O que vem na linha de 'fim' (o fim exclusivo da declaracao) ate o fim da linha e so espaco ou comentario? */
static bool RestoDaLinhaLivre(const Consulta *q, uint32_t fim, uint32_t fim_linha)
{
    const char *t = q->c->fonte.texto;

    while (fim < fim_linha && (t[fim] == ' ' || t[fim] == '\t')) fim++;
    return fim == fim_linha || (fim_linha - fim >= 2 && t[fim] == '/' && t[fim + 1] == '/');
}

/* Fim do conteudo da ultima linha com texto antes de 'ini'; o comeco do modulo se nao ha. */
static uint32_t FimDoConteudoAntes(const Consulta *q, uint32_t ini, uint32_t piso)
{
    while (ini > piso) {
        uint32_t anterior = EditarInicioDaLinha(q, ini - 1);

        if (!LinhaEmBranco(q, anterior)) return EditarFimDaLinha(q, anterior);
        ini = anterior;
    }
    return piso;
}

/* Existe linha com texto a partir de 'ini' ate o fim do modulo? */
static bool HaTextoDepois(const Consulta *q, uint32_t ini, uint32_t teto)
{
    for (; ini < teto; ini = EditarProximaLinha(q, ini)) {
        if (!LinhaEmBranco(q, ini)) return true;
    }
    return false;
}

/* Antes de 'ini' so ha comentario e linha em branco (nenhum import nem declaracao)? */
static bool SoCabecalhoAntes(const Consulta *q, uint32_t piso, uint32_t ini)
{
    for (uint32_t l = piso; l < ini; l = EditarProximaLinha(q, l)) {
        if (!LinhaEmBranco(q, l) && !LinhaDeComentario(q, l)) return false;
    }
    return true;
}

/* A faixa que sai do arquivo, de modo que sobre uma linha em branco entre os vizinhos, nunca duas nem nenhuma onde havia uma. A faixa vai do fim do conteudo da linha de cima ate o fim da
 * ultima linha da declaracao (assim o diff mostra so as linhas que saem); no topo do arquivo, ate o comeco da linha de baixo. Sem texto depois, sai tambem toda linha em branco de antes. */
static void FaixaDeRemocao(const Consulta *q, uint32_t piso, uint32_t teto, FaixaDecl *f)
{
    uint32_t prox = EditarProximaLinha(q, f->fim_texto), primeira = f->ini_texto;
    bool branco_depois = prox < teto && LinhaEmBranco(q, prox);
    bool branco_antes = f->ini_texto > piso && LinhaEmBranco(q, EditarInicioDaLinha(q, f->ini_texto - 1));

    f->fim_remocao = f->fim_texto;
    if (!HaTextoDepois(q, prox, teto)) {
        f->ini_remocao = FimDoConteudoAntes(q, f->ini_texto, piso);
        return;
    }
    if (branco_antes && branco_depois && !SoCabecalhoAntes(q, piso, f->ini_texto)) primeira = EditarInicioDaLinha(q, f->ini_texto - 1);
    if (branco_antes && branco_depois && primeira == f->ini_texto) {
        /* primeira declaracao do arquivo: um import novo entra no fim do cabecalho, entao a faixa nao pode comecar la; sai a linha em branco de depois */
        f->ini_remocao = f->ini_texto;
        f->fim_remocao = EditarProximaLinha(q, prox);
        return;
    }
    if (primeira > piso) {
        f->ini_remocao = EditarFimDaLinha(q, EditarInicioDaLinha(q, primeira - 1));
        return;
    }
    f->ini_remocao = piso;
    f->fim_remocao = branco_depois ? EditarProximaLinha(q, prox) : prox;
}

bool EditarFaixaDaDecl(Plano *p, const Decl *d, FaixaDecl *f)
{
    const Consulta *q = p->q;
    uint32_t inicio = ConsultaInicioDecl(q, d), fim = AstFimDecl(&q->fim, d);
    Janela j = JanelaDoOffset(q, d->pos);
    const char *t = j.t;

    if (EditarInicioDaLinha(q, inicio) != inicio) {
        for (uint32_t k = EditarInicioDaLinha(q, inicio); k < inicio; k++) {
            if (t[k] != ' ' && t[k] != '\t') return PlanoErro(p, "a declaracao nao comeca no inicio da linha dela: nao se separa do que vem antes");
        }
    }
    f->fim_texto = EditarFimDaLinha(q, fim - 1);
    if (!RestoDaLinhaLivre(q, fim, f->fim_texto)) return PlanoErro(p, "ha mais codigo na linha em que a declaracao termina: nao se separa");
    f->ini_texto = ConsultaInicioComentario(q, d);
    FaixaDeRemocao(q, j.piso, j.teto, f);
    return true;
}
