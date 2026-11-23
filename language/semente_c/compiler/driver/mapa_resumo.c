/* mapa_resumo.c - 'forja mapa --resumo': o mapa em texto hierarquico e curto (ver cmd_mapa.h e mapa_interno.h).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - Linguagem v1.13 L1b: a linha de teste de um pacote de 2+ arquivos leva o nome do arquivo ("a.fj:L3").
 *            0.1.0 - IA1-I3: primeira versao. Geral = cabecalho com o tamanho, entrada, e por modulo uma linha (linhas e imports) mais os tipos e os nomes pub, cada lista cortada em
 *            'limite' nomes ("+K" conta o resto); assinaturas, campos e o comentario de uma linha so com --modulo. Tudo na ordem do programa: mesmos bytes a cada execucao.
 * Verificacao: construir.bat && python tests\consultar\verificar_mapa.py --forja <forja.exe>
 */
#include <string.h>

#include "driver/mapa_interno.h"

enum { LIMITE_GERAL = 6, LIMITE_MODULO = 40, ASSINATURA_MAX = 160, COMENTARIO_MAX = 80, TITULO_MAX = 60, TIPO_DETALHE_MAX = 200 };

/* Uma lista cortada em 'restante' itens; os demais so sao contados. */
typedef struct {
    uint32_t restante, omitidos;
} Corte;

static bool Cabe(Corte *c)
{
    if (c->restante == 0) {
        c->omitidos++;
        return false;
    }
    c->restante--;
    return true;
}

static void AnexarOmitidos(Buf *b, const Corte *c)
{
    if (c->omitidos > 0) BufFormatar(b, " +%u", c->omitidos);
}

static void AnexarNome(const Consulta *q, Buf *b, int32_t nome)
{
    BufAnexarN(b, InternerTexto(&q->c->nomes, nome), InternerTam(&q->c->nomes, nome));
}

static void AnexarCaminho(const Consulta *q, Buf *b, uint8_t modulo)
{
    AnexarNome(q, b, q->c->modulos.itens[modulo].caminho);
}

/* Anexa no maximo 'max' bytes de t[0..n), sem partir um caractere UTF-8, com "..." quando cortou. */
static void AnexarTruncado(Buf *b, const char *t, size_t n, size_t max)
{
    if (n <= max) {
        BufAnexarN(b, t, n);
        return;
    }
    max -= 3;
    while (max > 0 && ((unsigned char)t[max] & 0xC0) == 0x80) max--;
    BufAnexarN(b, t, max);
    BufAnexar(b, "...");
}

static void AnexarLista(Consulta *q, const char *titulo, Buf *lista)
{
    if (lista->tam > 0) BufFormatar(&q->saida, "%s%s%s", titulo, lista->dados, lista->dados[lista->tam - 1] == '\n' ? "" : "\n");
    BufLiberar(lista);
}

static uint32_t LimiteDe(const PedidoConsulta *p, uint32_t padrao)
{
    return p->limite != 0 ? p->limite : padrao;
}

static const char *RotuloDoTipo(const Decl *d)
{
    return d->tipo_no == D_STRUCT ? "struct" : d->tipo_no == D_ENUM ? "enum" : "union";
}

/* ---------- visao geral ---------- */

static void Cabecalho(Consulta *q)
{
    MapaTamanho t;

    MapaMedir(q, &t);
    BufAnexar(&q->saida, "mapa ");
    AnexarCaminho(q, &q->saida, 0);
    BufFormatar(&q->saida, ": %u modulos, %llu linhas, %u pub, %u tipos, %u testes", t.modulos, (unsigned long long)t.linhas, t.publicos, t.tipos, t.testes);
    if (t.externos > 0) BufFormatar(&q->saida, ", %u std", t.externos);
    BufAnexar(&q->saida, " (Nl = linhas; * = pub)\n");
}

static void LinhaDeEntrada(Consulta *q, uint32_t limite)
{
    Buf b = {0};
    Corte corte = {limite, 0};

    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        const char *entrada = ConsultaEntradaDe(q, d);
        if (entrada == NULL || !MapaNoEscopo(q, d->modulo, -1) || !Cabe(&corte)) continue;
        BufAnexar(&b, b.tam > 0 ? ", " : " ");
        if (strcmp(entrada, "export") == 0) BufAnexar(&b, "export ");
        AstIdBaseDaDecl(&b, &q->c->modulos, &q->c->nomes, d);
    }
    AnexarOmitidos(&b, &corte);
    AnexarLista(q, "entrada:", &b);
}

/* Os tipos de uma categoria ("struct A* B"); o rotulo da categoria so abre se algum tipo cabe no corte. */
typedef struct {
    uint8_t modulo, categoria; /* categoria = DeclTipo */
} CategoriaDeTipos;

static void TiposDaCategoria(const Consulta *q, CategoriaDeTipos quais, Corte *corte, Buf *b)
{
    bool aberto = false;

    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        if (d->modulo != quais.modulo || d->tipo_no != quais.categoria || !Cabe(corte)) continue;
        if (!aberto) BufFormatar(b, "%s %s", b->tam > 0 ? ";" : "", RotuloDoTipo(d));
        aberto = true;
        BufAnexar(b, " ");
        AnexarNome(q, b, d->nome);
        if (d->publico) BufAnexar(b, "*");
    }
}

static void TiposDoModulo(const Consulta *q, uint8_t modulo, uint32_t limite, Buf *b)
{
    Corte corte = {limite, 0};

    TiposDaCategoria(q, (CategoriaDeTipos){modulo, D_STRUCT}, &corte, b);
    TiposDaCategoria(q, (CategoriaDeTipos){modulo, D_ENUM}, &corte, b);
    TiposDaCategoria(q, (CategoriaDeTipos){modulo, D_UNIAO}, &corte, b);
    AnexarOmitidos(b, &corte);
}

/* Nomes pub que nao sao tipo: proc e foreign com "()", o resto (const, unit) puro. */
static void PubsDoModulo(const Consulta *q, uint8_t modulo, uint32_t limite, Buf *b)
{
    Corte corte = {limite, 0};

    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        if (d->modulo != modulo || !ConsultaEhApi(d) || MapaEhTipo(d) || !Cabe(&corte)) continue;
        BufAnexar(b, " ");
        AnexarNome(q, b, d->nome);
        if (d->tipo_no == D_PROC) BufAnexar(b, "()");
    }
    AnexarOmitidos(b, &corte);
}

static void LinhaDoModulo(Consulta *q, uint8_t modulo)
{
    BufAnexar(&q->saida, "\n");
    AnexarCaminho(q, &q->saida, modulo);
    BufFormatar(&q->saida, " %llul", (unsigned long long)ConsultaLinhasDoModulo(q, modulo));
    if (modulo == 0) BufAnexar(&q->saida, " raiz");
    bool primeiro = true;
    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        if (d->tipo_no != D_IMPORT || d->modulo != modulo) continue;
        BufAnexar(&q->saida, primeiro ? " -> " : " ");
        primeiro = false;
        AnexarCaminho(q, &q->saida, (uint8_t)d->indice);
    }
    BufAnexar(&q->saida, "\n");
}

static void BlocoDoModulo(Consulta *q, uint8_t modulo, uint32_t limite)
{
    Buf tipos = {0}, pubs = {0};

    LinhaDoModulo(q, modulo);
    TiposDoModulo(q, modulo, limite, &tipos);
    PubsDoModulo(q, modulo, limite, &pubs);
    AnexarLista(q, "  tipos:", &tipos);
    AnexarLista(q, "  pub:", &pubs);
}

/* "std: mem os" - o std embutido que o projeto importa. */
static void LinhaDoStd(Consulta *q)
{
    Buf b = {0};

    for (uint32_t k = 1; k < q->c->modulos.n; k++) {
        if (q->c->modulos.itens[k].origem != MOD_STD) continue;
        int32_t nome = q->c->modulos.itens[k].caminho;
        BufAnexar(&b, " ");
        BufAnexarN(&b, InternerTexto(&q->c->nomes, nome) + 4, InternerTam(&q->c->nomes, nome) - 4); /* sem o "std:" */
    }
    AnexarLista(q, "\nstd:", &b);
}

static void LinhaDeTestes(Consulta *q, const uint8_t *ordem, int n)
{
    Buf b = {0};

    for (int i = 0; i < n; i++) {
        uint32_t total = 0;
        for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
            total += d->modulo == ordem[i] && ConsultaEhTesteDoProjeto(q, d);
        }
        if (total == 0) continue;
        BufAnexar(&b, b.tam > 0 ? ", " : " ");
        AnexarCaminho(q, &b, ordem[i]);
        BufFormatar(&b, " %u", total);
    }
    AnexarLista(q, "testes:", &b);
}

/* Cada biblioteca de 'link' uma vez. */
static void LinhaDeLinks(Consulta *q)
{
    Buf b = {0};

    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        bool repetido = false;
        if (d->tipo_no != D_LINK) continue;
        for (const Decl *a = q->c->sem.programa->primeira; a != d && !repetido; a = a->prox) repetido = a->tipo_no == D_LINK && a->nome == d->nome;
        if (repetido) continue;
        BufAnexar(&b, " ");
        AnexarNome(q, &b, d->nome);
    }
    AnexarLista(q, "links:", &b);
}

static int ResumoGeral(Consulta *q, const PedidoConsulta *p)
{
    uint8_t ordem[MODULOS_MAX];
    int n = ConsultaOrdemDosModulos(q, ordem);
    uint32_t limite = LimiteDe(p, LIMITE_GERAL);

    Cabecalho(q);
    LinhaDeEntrada(q, limite);
    BufAnexar(&q->saida, "modulos:");
    for (int i = 0; i < n; i++) {
        if (MapaNoEscopo(q, ordem[i], -1)) BlocoDoModulo(q, ordem[i], limite);
    }
    LinhaDoStd(q);
    LinhaDeTestes(q, ordem, n);
    LinhaDeLinks(q);
    return CONSULTAR_OK;
}

/* ---------- um modulo ---------- */

/* Um lado do grafo de imports de 'modulo': o que ele importa, ou quem o importa. */
typedef struct {
    uint8_t modulo;
    bool importadores;
} LadoDoImport;

static void ImportsDoModulo(const Consulta *q, LadoDoImport lado, uint32_t limite, Buf *b)
{
    Corte corte = {limite, 0};

    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        uint8_t dono = lado.importadores ? (uint8_t)d->indice : d->modulo;
        if (d->tipo_no != D_IMPORT || dono != lado.modulo || !Cabe(&corte)) continue;
        BufAnexar(b, " ");
        AnexarCaminho(q, b, lado.importadores ? d->modulo : (uint8_t)d->indice);
    }
    AnexarOmitidos(b, &corte);
}

/* "  struct Corpo* {massa: f32, cor: Cor}" / "  enum Cor* {vermelho, azul}" / "  union Forma* {Corpo, i32}". */
static void DetalheDoTipo(const Consulta *q, const Decl *d, uint32_t limite, Buf *b)
{
    Corte corte = {limite, 0};
    bool membros = d->tipo_no == D_ENUM;
    uint32_t n = membros ? d->nmembros : d->ncampos;

    BufFormatar(b, "  %s ", RotuloDoTipo(d));
    AnexarNome(q, b, d->nome);
    BufAnexar(b, d->publico ? "* {" : " {");
    for (uint32_t i = 0; i < n; i++) {
        if (!Cabe(&corte)) continue;
        BufAnexar(b, i > 0 ? ", " : "");
        if (membros) AnexarNome(q, b, d->membros[i].nome);
        else if (d->tipo_no == D_STRUCT) BufFormatar(b, "%.*s: ", (int)InternerTam(&q->c->nomes, d->campos[i].nome), InternerTexto(&q->c->nomes, d->campos[i].nome));
        if (!membros) TipoAnexarTexto(&q->c->sem.tipos, d->campos[i].tipo.tipo, b);
    }
    AnexarOmitidos(b, &corte);
    BufAnexar(b, "}\n");
}

static void TiposDetalhados(Consulta *q, uint8_t modulo, uint32_t limite)
{
    Buf b = {0};
    Corte corte = {limite, 0};

    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        Buf um = {0};
        if (d->modulo != modulo || !MapaEhTipo(d) || !Cabe(&corte)) continue;
        DetalheDoTipo(q, d, limite, &um);
        AnexarTruncado(&b, um.dados, um.tam - 1, TIPO_DETALHE_MAX);
        BufAnexar(&b, "\n");
        BufLiberar(&um);
    }
    if (corte.omitidos > 0) BufFormatar(&b, "  +%u tipos\n", corte.omitidos);
    AnexarLista(q, "tipos:\n", &b);
}

/* "  proc f(a: i32) -> i32  // 1a linha do comentario" */
static void LinhaDeApi(Consulta *q, const Decl *d, Buf *b)
{
    Buf assinatura = {0}, comentario = {0};
    size_t fim = 0;

    ConsultaAssinaturaDaApi(q, d, &assinatura);
    ConsultaComentarioAcima(q, d, &comentario);
    BufAnexar(b, "  ");
    AnexarTruncado(b, assinatura.dados, assinatura.tam, ASSINATURA_MAX);
    while (fim < comentario.tam && comentario.dados[fim] != '\n') fim++;
    if (fim > 0) {
        BufAnexar(b, "  // ");
        AnexarTruncado(b, comentario.dados, fim, COMENTARIO_MAX);
    }
    BufAnexar(b, "\n");
    BufLiberar(&assinatura);
    BufLiberar(&comentario);
}

static void ApiDetalhada(Consulta *q, uint8_t modulo, uint32_t limite)
{
    Buf b = {0};
    Corte corte = {limite, 0};

    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        if (d->modulo != modulo || !ConsultaEhApi(d) || MapaEhTipo(d) || !Cabe(&corte)) continue;
        LinhaDeApi(q, d, &b);
    }
    if (corte.omitidos > 0) BufFormatar(&b, "  +%u nomes\n", corte.omitidos);
    AnexarLista(q, "api:\n", &b);
}

/* O nome do arquivo (sem a pasta) onde 'd' esta, com ':' no fim; vazio num modulo de um arquivo so (o modulo de pasta precisa dizer de qual arquivo e a linha). */
static void AnexarArquivoDoPacote(const Consulta *q, Buf *b, const Decl *d)
{
    const char *caminho = FonteLocalizarNoArquivo(&q->c->fonte, d->pos).caminho, *nome = caminho;

    if (ConsultaQtdArquivos(q, d->modulo) < 2) return;
    for (const char *p = caminho; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') nome = p + 1;
    }
    BufFormatar(b, "%s:", nome);
}

static void TestesDetalhados(Consulta *q, uint8_t modulo, uint32_t limite)
{
    Buf b = {0};
    Corte corte = {limite, 0};

    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        if (d->modulo != modulo || !ConsultaEhTesteDoProjeto(q, d) || !Cabe(&corte)) continue;
        BufAnexar(&b, "  ");
        AnexarArquivoDoPacote(q, &b, d);
        BufFormatar(&b, "L%u ", FonteLocalizarNoArquivo(&q->c->fonte, d->pos).linha);
        AnexarTruncado(&b, InternerTexto(&q->c->nomes, d->nome), InternerTam(&q->c->nomes, d->nome), TITULO_MAX);
        BufAnexar(&b, "\n");
    }
    if (corte.omitidos > 0) BufFormatar(&b, "  +%u testes\n", corte.omitidos);
    AnexarLista(q, "testes:\n", &b);
}

static int ResumoDoModulo(Consulta *q, const PedidoConsulta *p, uint8_t modulo)
{
    uint32_t limite = LimiteDe(p, LIMITE_MODULO);
    Buf importa = {0}, importado = {0};

    BufAnexar(&q->saida, "modulo ");
    AnexarCaminho(q, &q->saida, modulo);
    BufFormatar(&q->saida, " (%s, %llul)\n", ConsultaOrigemDoModulo(q, modulo), (unsigned long long)ConsultaLinhasDoModulo(q, modulo));
    ImportsDoModulo(q, (LadoDoImport){modulo, false}, limite, &importa);
    ImportsDoModulo(q, (LadoDoImport){modulo, true}, limite, &importado);
    AnexarLista(q, "importa:", &importa);
    AnexarLista(q, "importado por:", &importado);
    TiposDetalhados(q, modulo, limite);
    ApiDetalhada(q, modulo, limite);
    TestesDetalhados(q, modulo, limite);
    return CONSULTAR_OK;
}

int MapaResumo(Consulta *q, const PedidoConsulta *p, int so)
{
    return so >= 0 ? ResumoDoModulo(q, p, (uint8_t)so) : ResumoGeral(q, p);
}
