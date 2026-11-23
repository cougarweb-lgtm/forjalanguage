/* consultar_modulos.c - 'forja consultar imports' e a tabela de modulos que o 'forja mapa' tambem usa (ver consultar_interno.h).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - Linguagem v1.13 L1b (modulo de pasta): o modulo e a lista de trechos (arquivos) dele, nao o trecho de mesmo indice; ConsultaOffsetDoModulo e ConsultaLinhasDoModulo contam do 1o ao ultimo arquivo do pacote;
 *            ConsultaTrechoDoOffset, ConsultaInicioDoArquivo, ConsultaFimDoArquivo, ConsultaModuloDoOffset, ConsultaQtdArquivos; "arquivos" (e "arquivo" por item) so saem num pacote de 2+ arquivos.
 *            0.1.0 - IA1-I3: primeira versao. Um modulo = caminho, origem, arquivo, linhas, imports (com apelido), quem o importa e os 'link' de foreign; a ordem e a raiz primeiro e
 *            depois as dependencias na ordem do programa (a mesma de 'simbolos').
 * Verificacao: construir.bat && python tests\consultar\verificar_mapa.py --forja <forja.exe>
 */
#include <string.h>

#include "driver/consultar_interno.h"

const char *ConsultaOrigemDoModulo(const Consulta *q, uint8_t modulo)
{
    static const char *const NOMES[] = {"raiz", "arquivo", "std", "vendor", "engine"};
    return NOMES[q->c->modulos.itens[modulo].origem];
}

uint32_t ConsultaQtdArquivos(const Consulta *q, uint8_t modulo)
{
    uint32_t primeiro;

    return ModulosTrechosDoPacote(&q->c->modulos, q->c->fonte.ntrechos, modulo, &primeiro);
}

uint32_t ConsultaOffsetDoModulo(const Consulta *q, uint8_t modulo)
{
    const Fonte *f = &q->c->fonte;
    uint32_t primeiro;

    ModulosTrechosDoPacote(&q->c->modulos, f->ntrechos, modulo, &primeiro);
    return f->ntrechos == 0 ? 0 : f->trechos[primeiro].inicio;
}

uint32_t ConsultaTrechoDoOffset(const Consulta *q, uint32_t off)
{
    return FonteTrechoDoOffset(&q->c->fonte, off);
}

uint32_t ConsultaInicioDoArquivo(const Consulta *q, uint32_t off)
{
    const Fonte *f = &q->c->fonte;

    return f->ntrechos == 0 ? 0 : f->trechos[FonteTrechoDoOffset(f, off)].inicio;
}

uint32_t ConsultaFimDoArquivo(const Consulta *q, uint32_t off)
{
    const Fonte *f = &q->c->fonte;

    return f->ntrechos == 0 ? (uint32_t)f->tam : f->trechos[FonteTrechoDoOffset(f, off)].fim;
}

uint8_t ConsultaModuloDoOffset(const Consulta *q, uint32_t off)
{
    return ModulosPacoteDoArquivo(&q->c->modulos, FonteTrechoDoOffset(&q->c->fonte, off));
}

/* Linhas do texto [ini, fim): cada LF fecha uma, e o resto depois do ultimo LF tambem conta. */
static uint64_t LinhasDoTexto(const Fonte *f, uint32_t ini, uint32_t fim)
{
    uint64_t linhas = 0;

    for (uint32_t i = ini; i < fim; i++) {
        if (f->texto[i] == '\n') linhas++;
    }
    if (fim > ini && f->texto[fim - 1] != '\n') linhas++;
    return linhas;
}

uint64_t ConsultaLinhasDoModulo(const Consulta *q, uint8_t modulo)
{
    const Fonte *f = &q->c->fonte;
    uint32_t primeiro, n = ModulosTrechosDoPacote(&q->c->modulos, f->ntrechos, modulo, &primeiro);
    uint64_t linhas = 0;

    if (f->ntrechos == 0) return LinhasDoTexto(f, 0, (uint32_t)f->tam);
    for (uint32_t t = primeiro; t < primeiro + n; t++) linhas += LinhasDoTexto(f, f->trechos[t].inicio, f->trechos[t].fim);
    return linhas;
}

uint64_t ConsultaBytesDoModulo(const Consulta *q, uint8_t modulo)
{
    const Fonte *f = &q->c->fonte;
    uint32_t primeiro, n = ModulosTrechosDoPacote(&q->c->modulos, f->ntrechos, modulo, &primeiro);
    uint64_t bytes = 0;

    if (f->ntrechos == 0) return f->tam;
    for (uint32_t t = primeiro; t < primeiro + n; t++) bytes += f->trechos[t].fim - f->trechos[t].inicio;
    return bytes;
}

void ConsultaChaveArquivoSePacote(Consulta *q, uint32_t off)
{
    if (ConsultaQtdArquivos(q, ConsultaModuloDoOffset(q, off)) > 1) ConsultaChaveArquivo(q, off);
}

/* "arquivos": [...] com o caminho de cada .fj do pacote, na ordem da carga; so num pacote de 2+ arquivos. */
void ConsultaChaveArquivos(Consulta *q, uint8_t modulo)
{
    const Fonte *f = &q->c->fonte;
    uint32_t primeiro, n = ModulosTrechosDoPacote(&q->c->modulos, f->ntrechos, modulo, &primeiro);

    if (n < 2) return;
    ConsultaAbrirLista(q, "arquivos");
    for (uint32_t t = primeiro; t < primeiro + n; t++) ConsultaItemCaminho(q, f->trechos[t].caminho);
    ConsultaFecharLista(q);
}

int ConsultaOrdemDosModulos(const Consulta *q, uint8_t *ordem)
{
    const Modulos *m = &q->c->modulos;
    int n = 1;

    ordem[0] = 0;
    for (uint32_t i = 0; m->n > 1 && i < m->n; i++) {
        if (m->ordem[i] != 0) ordem[n++] = m->ordem[i];
    }
    return n;
}

int ConsultaAcharModulo(const Consulta *q, const char *caminho)
{
    size_t n = strlen(caminho);

    for (uint32_t k = 0; k < q->c->modulos.n; k++) {
        int32_t nome = q->c->modulos.itens[k].caminho;
        if (InternerTam(&q->c->nomes, nome) == n && memcmp(InternerTexto(&q->c->nomes, nome), caminho, n) == 0) return (int)k;
    }
    return -1;
}

/* Apelido com que o import entra no arquivo: o 'as' escrito ou o ultimo segmento do caminho. */
static int32_t ApelidoDoImport(const Consulta *q, const Decl *imp)
{
    return imp->nome_a >= 0 ? imp->nome_a : q->c->modulos.itens[imp->indice].apelido;
}

static void ImportaJson(Consulta *q, uint8_t modulo)
{
    ConsultaAbrirLista(q, "importa");
    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        if (d->tipo_no != D_IMPORT || d->modulo != modulo) continue;
        ConsultaAbrirObj(q);
        ConsultaChaveModulo(q, "modulo", (uint8_t)d->indice);
        ConsultaChaveNome(q, "apelido", ApelidoDoImport(q, d));
        ConsultaChaveArquivoSePacote(q, d->pos);
        ConsultaChavePos(q, "pos", d->pos);
        ConsultaFecharObj(q);
    }
    ConsultaFecharLista(q);
}

/* O mesmo arquivo pode escrever dois imports do mesmo modulo (apelidos diferentes): 'importado_por' o conta uma vez. */
static bool JaImportouAntes(const Consulta *q, const Decl *imp)
{
    for (const Decl *d = q->c->sem.programa->primeira; d != imp; d = d->prox) {
        if (d->tipo_no == D_IMPORT && d->indice == imp->indice && d->modulo == imp->modulo) return true;
    }
    return false;
}

static void ImportadoPorJson(Consulta *q, uint8_t modulo)
{
    ConsultaAbrirLista(q, "importado_por");
    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        if (d->tipo_no == D_IMPORT && d->indice == modulo && !JaImportouAntes(q, d)) ConsultaItemModulo(q, d->modulo);
    }
    ConsultaFecharLista(q);
}

void ConsultaLinksJson(Consulta *q, const char *chave, int modulo)
{
    ConsultaAbrirLista(q, chave);
    for (const Decl *d = q->c->sem.programa->primeira; d != NULL; d = d->prox) {
        if (d->tipo_no != D_LINK || (modulo >= 0 && d->modulo != modulo)) continue;
        ConsultaAbrirObj(q);
        ConsultaChaveNome(q, "lib", d->nome);
        ConsultaChaveModulo(q, "modulo", d->modulo);
        ConsultaChaveArquivoSePacote(q, d->pos);
        ConsultaChavePos(q, "pos", d->pos);
        ConsultaFecharObj(q);
    }
    ConsultaFecharLista(q);
}

void ConsultaModuloJson(Consulta *q, uint8_t modulo)
{
    ConsultaAbrirObj(q);
    ConsultaChaveModulo(q, "modulo", modulo);
    ConsultaChaveTexto(q, "origem", ConsultaOrigemDoModulo(q, modulo), strlen(ConsultaOrigemDoModulo(q, modulo)));
    ConsultaChaveArquivo(q, ConsultaOffsetDoModulo(q, modulo));
    ConsultaChaveArquivos(q, modulo);
    ConsultaChaveInt(q, "linhas", (int64_t)ConsultaLinhasDoModulo(q, modulo));
    ImportaJson(q, modulo);
    ImportadoPorJson(q, modulo);
    ConsultaLinksJson(q, "links", modulo);
    ConsultaFecharObj(q);
}

int ConsultarImports(Consulta *q, const PedidoConsulta *p)
{
    uint8_t ordem[MODULOS_MAX];
    int n = ConsultaOrdemDosModulos(q, ordem);

    (void)p;
    ConsultaAbrir(q, "imports");
    ConsultaAbrirLista(q, "modulos");
    for (int i = 0; i < n; i++) ConsultaModuloJson(q, ordem[i]);
    ConsultaFecharLista(q);
    ConsultaLinksJson(q, "links", -1);
    ConsultaFechar(q);
    return CONSULTAR_OK;
}
