/* consultar_api.c - 'forja consultar api | testes | entrada': a API publica de um modulo (assinatura e o comentario logo acima), os test e os pontos de entrada (ver consultar_interno.h).
 * Versao: 0.3.0
 * Changelog: 0.3.0 - Linguagem v1.13 L1b: o comentario acima da declaracao para no inicio do ARQUIVO dela (modulo de pasta); "arquivos" na resposta de api e "arquivo" em cada item so num pacote de 2+ arquivos.
 *            0.2.0 - IA1-I5: ConsultaInicioComentario (o offset que ConsultaComentarioAcima usa por dentro, agora publico para o 'forja editar').
 *            0.1.0 - IA1-I3: primeira versao. API = declaracoes 'pub' que 'simbolos' lista (proc, foreign, struct, enum, union, const, unit); o comentario e o bloco de linhas '//'
 *            coladas acima da declaracao (sem linha em branco no meio), sem o '//'. testes = os test de todo modulo que nao e std; entrada = main e os 'pub export proc'.
 * Verificacao: construir.bat && python tests\consultar\verificar_mapa.py --forja <forja.exe>
 */
#include <string.h>

#include "driver/consultar_interno.h"

bool ConsultaEhApi(const Decl *d)
{
    return d->publico && ConsultaEhSimbolo(d);
}

/* ---------- comentario acima da declaracao ---------- */

static uint32_t InicioDaLinha(const char *texto, uint32_t piso, uint32_t off)
{
    while (off > piso && texto[off - 1] != '\n') off--;
    return off;
}

/* A linha [ini, fim) e de comentario ('//' depois do recuo)? */
static bool LinhaDeComentario(const char *texto, uint32_t ini, uint32_t fim)
{
    while (ini < fim && (texto[ini] == ' ' || texto[ini] == '\t')) ini++;
    return fim - ini >= 2 && texto[ini] == '/' && texto[ini + 1] == '/';
}

/* Inicio da 1a linha do bloco de comentario que termina na linha de 'linha_decl'; igual a linha_decl se nao ha comentario. */
static uint32_t InicioDoBloco(const char *texto, uint32_t piso, uint32_t linha_decl)
{
    uint32_t linha = linha_decl;

    while (linha > piso) {
        uint32_t fim = linha - 1; /* o LF da linha de cima */
        uint32_t ini = InicioDaLinha(texto, piso, fim);
        if (!LinhaDeComentario(texto, ini, fim)) break;
        linha = ini;
    }
    return linha;
}

uint32_t ConsultaInicioComentario(const Consulta *q, const Decl *d)
{
    const char *texto = q->c->fonte.texto;
    uint32_t piso = ConsultaInicioDoArquivo(q, d->pos); /* o comentario e do arquivo da declaracao (modulo de pasta: nao sobe para o arquivo de cima) */

    return InicioDoBloco(texto, piso, InicioDaLinha(texto, piso, ConsultaInicioDecl(q, d)));
}

/* Anexa o texto da linha de comentario [ini, fim) sem o recuo, sem o '//' (e o espaco que o segue) e sem o CR/espacos do fim. */
static void AnexarLinhaDeComentario(Buf *saida, const char *texto, uint32_t ini, uint32_t fim)
{
    while (texto[ini] != '/') ini++;
    ini += 2;
    if (ini < fim && texto[ini] == ' ') ini++;
    while (fim > ini && (texto[fim - 1] == '\r' || texto[fim - 1] == ' ' || texto[fim - 1] == '\t')) fim--;
    BufAnexarN(saida, texto + ini, fim - ini);
}

void ConsultaComentarioAcima(const Consulta *q, const Decl *d, Buf *saida)
{
    const char *texto = q->c->fonte.texto;
    uint32_t piso = ConsultaInicioDoArquivo(q, d->pos);
    uint32_t linha_decl = InicioDaLinha(texto, piso, ConsultaInicioDecl(q, d));
    uint32_t linha = InicioDoBloco(texto, piso, linha_decl);

    while (linha < linha_decl) {
        uint32_t fim = linha;
        while (texto[fim] != '\n') fim++;
        if (saida->tam > 0) BufAnexarN(saida, "\n", 1);
        AnexarLinhaDeComentario(saida, texto, linha, fim);
        linha = fim + 1;
    }
}

/* ---------- um item da API ---------- */

void ConsultaAssinaturaDaApi(Consulta *q, const Decl *d, Buf *saida)
{
    const Interner *nomes = &q->c->nomes;

    if (d->tipo_no == D_PROC) {
        ConsultaAssinaturaTexto(q, d, saida);
        return;
    }
    if (d->tipo_no == D_CONST) {
        BufFormatar(saida, "const %.*s: ", (int)InternerTam(nomes, d->nome), InternerTexto(nomes, d->nome));
        TipoAnexarTexto(&q->c->sem.tipos, d->tipo.tipo, saida);
        return;
    }
    BufFormatar(saida, "%s %.*s", IndiceCategoria(d), (int)InternerTam(nomes, d->nome), InternerTexto(nomes, d->nome));
}

void ConsultaApiItemJson(Consulta *q, const Decl *d)
{
    const char *categoria = IndiceCategoria(d);
    Buf assinatura = {0}, comentario = {0};

    ConsultaComentarioAcima(q, d, &comentario);
    ConsultaAssinaturaDaApi(q, d, &assinatura);
    ConsultaAbrirObj(q);
    ConsultaChaveNome(q, "nome", d->nome);
    ConsultaChaveTexto(q, "categoria", categoria, strlen(categoria));
    ConsultaChaveId(q, "id", d);
    ConsultaChaveBool(q, "exportado", d->exportado);
    ConsultaChaveArquivoSePacote(q, d->pos);
    ConsultaChavePos(q, "pos", ConsultaInicioDecl(q, d));
    ConsultaChaveTexto(q, "assinatura", assinatura.dados, assinatura.tam);
    ConsultaChaveTexto(q, "comentario", comentario.tam > 0 ? comentario.dados : NULL, comentario.tam);
    ConsultaFecharObj(q);
    BufLiberar(&assinatura);
    BufLiberar(&comentario);
}

int ConsultaApiDoModuloJson(Consulta *q, uint8_t modulo)
{
    int total = 0;

    ConsultaAbrirLista(q, "api");
    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        if (d->modulo != modulo || !ConsultaEhApi(d)) continue;
        ConsultaApiItemJson(q, d);
        total++;
    }
    ConsultaFecharLista(q);
    return total;
}

int ConsultarApi(Consulta *q, const PedidoConsulta *p)
{
    int modulo = ConsultaAcharModulo(q, p->arg);
    int total;

    if (modulo < 0) return ConsultaUsoErrado("api", p->arg, "nao e modulo do projeto (use o caminho do import, ex.: std:mem)");
    ConsultaAbrir(q, "api");
    ConsultaChaveModulo(q, "modulo", (uint8_t)modulo);
    ConsultaChaveArquivo(q, ConsultaOffsetDoModulo(q, (uint8_t)modulo));
    ConsultaChaveArquivos(q, (uint8_t)modulo);
    total = ConsultaApiDoModuloJson(q, (uint8_t)modulo);
    ConsultaChaveInt(q, "total", total);
    ConsultaFechar(q);
    return CONSULTAR_OK;
}

/* ---------- testes e entrada ---------- */

void ConsultaTesteJson(Consulta *q, const Decl *d)
{
    ConsultaAbrirObj(q);
    ConsultaChaveNome(q, "nome", d->nome);
    ConsultaChaveId(q, "id", d);
    ConsultaChaveModulo(q, "modulo", d->modulo);
    ConsultaChaveArquivo(q, d->pos);
    ConsultaChavePos(q, "pos", d->pos);
    ConsultaChavePos(q, "fim", AstFimDecl(&q->fim, d));
    ConsultaFecharObj(q);
}

/* Teste de modulo do projeto (o std embutido tem os dele). */
bool ConsultaEhTesteDoProjeto(const Consulta *q, const Decl *d)
{
    return d->tipo_no == D_TEST && q->c->modulos.itens[d->modulo].origem != MOD_STD;
}

int ConsultarTestes(Consulta *q, const PedidoConsulta *p)
{
    int64_t total = 0;

    (void)p;
    ConsultaAbrir(q, "testes");
    ConsultaAbrirLista(q, "testes");
    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        if (!ConsultaEhTesteDoProjeto(q, d)) continue;
        ConsultaTesteJson(q, d);
        total++;
    }
    ConsultaFecharLista(q);
    ConsultaChaveInt(q, "total", total);
    ConsultaFechar(q);
    return CONSULTAR_OK;
}

void ConsultaEntradaJson(Consulta *q, const Decl *d, const char *entrada)
{
    Buf assinatura = {0};

    ConsultaAssinaturaTexto(q, d, &assinatura);
    ConsultaAbrirObj(q);
    ConsultaChaveNome(q, "nome", d->nome);
    ConsultaChaveTexto(q, "entrada", entrada, strlen(entrada));
    ConsultaChaveId(q, "id", d);
    ConsultaChaveModulo(q, "modulo", d->modulo);
    ConsultaChaveArquivo(q, d->pos);
    ConsultaChavePos(q, "pos", ConsultaInicioDecl(q, d));
    ConsultaChaveTexto(q, "assinatura", assinatura.dados, assinatura.tam);
    ConsultaFecharObj(q);
    BufLiberar(&assinatura);
}

int ConsultarEntrada(Consulta *q, const PedidoConsulta *p)
{
    int64_t total = 0;

    (void)p;
    ConsultaAbrir(q, "entrada");
    ConsultaAbrirLista(q, "entradas");
    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        const char *entrada = ConsultaEntradaDe(q, d);
        if (entrada == NULL) continue;
        ConsultaEntradaJson(q, d, entrada);
        total++;
    }
    ConsultaFecharLista(q);
    ConsultaChaveInt(q, "total", total);
    ConsultaFechar(q);
    return CONSULTAR_OK;
}
