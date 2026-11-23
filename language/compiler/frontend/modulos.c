/* modulos.c - tabela de modulos: forma do caminho do import (E1001), deduplicacao por texto e apelidos por arquivo.
 * Versao: 0.12.0
 * Changelog: 0.12.0 - Linguagem v1.13 L1b: ModulosTrechosDoPacote (os trechos da Fonte de um pacote, para as ferramentas que mostram o arquivo certo).
 *            0.11.0 - Linguagem v1.13 L1a (modulo de pasta): apelidos por trecho da Fonte (uint32_t); mapa trecho -> pacote (ModulosRegistrarArquivo, ModulosPacoteDoArquivo) e ModulosApelidoNoPacote.
 *            0.10.0 - M6b P79: o E1001 de std: lista os modulos disponiveis (std + vendor da pasta do arquivo raiz, ModulosListarDisponiveis) e cita o parecido ("quis dizer std:text?", ModulosParecido); ContextoImport leva a pasta; ConferirForma e RelatarStdAusente tomam o contexto.
 *            0.9.0 - S3 P6: E1001 de "std:fs" leva a sugestao certa (troca por "std:file": o sistema de arquivos vive no modulo de arquivo).
 *            0.8.0 - IA1-S2e: E1001 de "std:locale", "std:intl", "std:number" e "std:currency" leva a sugestao certa (troca por "std:fmt").
 *            0.7.0 - IA1-S2b2a: E1001 de "std:strings", "std:strconv", "std:utf8" e "std:unicode" leva a sugestao certa (troca por "std:text").
 *            0.6.0 - IA1-L3d1: registra a pasta e o caminho antigos do import (RenomeadoNotarTabela) e guarda no apelido padrao o apelido da forma nova (ApelidoDeArquivo.novo, ModulosApelidoNovo) para o forja migrar.
 *            0.5.0 - IA1-L3c: "std:arquivo" vale como "std:file" (CanonizarImport, tabela de renomeados); o apelido padrao segue o caminho escrito; flag recusar -> E0110.
 *            0.4.1 - IA1-L2: o E1001 de "std:proc" leva a sugestao certa (troca por "std:os").
 *            0.4.0 - IA1-b01: E1006 (ultimo segmento do caminho e palavra reservada, sem 'as') e E1001 de "std:proc" com a forma certa std:os.
 *            0.3.0 - Linguagem v1.8b P6: "vendor:" e "engine:" valem (ConferirForma, OrigemDoCaminho); ConferirPrefixo perde o E1001 provisorio.
 *            0.2.0 - Linguagem v1.8 P2b: ModulosConferirCiclos (DFS, E1002) e ModulosOrdenarDecls (pos-ordem).
 *            0.1.0 -Linguagem v1.8 P2a: ModulosIniciar, ModulosRegistrar (forma do caminho, limite de 254, apelido unico), ModulosApelido, ModulosLiberar.
 * Verificacao: construir.bat && python ..\tests\sintaxe\verificar_sintaxe.py
 */
#include "frontend/modulos.h"
#include "common/buf.h"
#include "common/renomeados.h"
#include "frontend/token.h"

/* Texto depois do ultimo '/' ou ':' (apelido padrao). */
static int32_t ApelidoPadrao(Modulos *m, const char *texto, uint32_t tam)
{
    uint32_t ini = 0;
    for (uint32_t i = 0; i < tam; i++) {
        if (texto[i] == '/' || texto[i] == ':') ini = i + 1;
    }
    return Internar(m->nomes, texto + ini, tam - ini);
}

void ModulosIniciar(Modulos *m, Interner *nomes, const char *caminho_raiz)
{
    size_t tam = strlen(caminho_raiz), ini = 0;
    for (size_t i = 0; i < tam; i++) {
        if (caminho_raiz[i] == '/' || caminho_raiz[i] == '\\') ini = i + 1;
    }
    if (tam - ini > 3 && memcmp(caminho_raiz + tam - 3, ".fj", 3) == 0) tam -= 3;
    m->nomes = nomes;
    m->n = 1;
    if (ini == 0) snprintf(m->pasta, sizeof m->pasta, "%s", ".");
    else snprintf(m->pasta, sizeof m->pasta, "%.*s", (int)(ini - 1), caminho_raiz);
    m->itens[0] = (Modulo){.origem = MOD_RAIZ};
    m->itens[0].caminho = Internar(nomes, caminho_raiz + ini, (uint32_t)(tam - ini));
    m->itens[0].apelido = m->itens[0].caminho;
    VecIniciar(&m->apelidos, sizeof(ApelidoDeArquivo));
    VecIniciar(&m->pacotes, sizeof(uint8_t));
}

/* Outro ':' fora da 2a posicao, '\', caminho absoluto. false = diagnostico. */
static bool ConferirPrefixo(Diag *d, uint32_t pos, const char *t, uint32_t tam)
{
    for (uint32_t i = 0; i < tam; i++) {
        if (t[i] == ':' && i != 1) {
            return DiagRelatar(d, E1001, pos, "prefixo desconhecido no import: valem std: e caminho relativo");
        }
    }
    if (memchr(t, '\\', tam) != NULL) return DiagRelatar(d, E1001, pos, "use '/' no caminho do import");
    if (t[0] == '/' || (tam > 1 && t[1] == ':')) {
        return DiagRelatar(d, E1001, pos, "caminho absoluto nao vale: o caminho e relativo a pasta do arquivo raiz");
    }
    return true;
}

static bool SegmentoValido(const char *s, uint32_t tam)
{
    if (tam == 0 || s[0] < 'a' || s[0] > 'z') return false;
    for (uint32_t i = 1; i < tam; i++) {
        bool ok = (s[i] >= 'a' && s[i] <= 'z') || (s[i] >= '0' && s[i] <= '9') || s[i] == '_';
        if (!ok) return false;
    }
    return true;
}

/* Proximo segmento a partir de *i (ate '/' ou fim); avanca *i para depois da barra. */
static uint32_t ProximoSegmento(const char *t, uint32_t tam, uint32_t *i)
{
    uint32_t ini = *i, fim = ini;
    while (fim < tam && t[fim] != '/') fim++;
    *i = fim + 1;
    return fim - ini;
}

static bool ConferirSegmentos(Diag *d, uint32_t pos, const char *t, uint32_t tam)
{
    for (uint32_t i = 0; i <= tam;) {
        const char *s = t + i;
        uint32_t n = ProximoSegmento(t, tam, &i);
        if ((n == 1 && s[0] == '.') || (n == 2 && s[0] == '.' && s[1] == '.')) {
            return DiagRelatar(d, E1001, pos, "'.' e '..' nao valem no caminho do import (o caminho parte da pasta do arquivo raiz)");
        }
    }
    if (tam > 3 && memcmp(t + tam - 3, ".fj", 3) == 0) return DiagRelatar(d, E1001, pos, "o caminho do import vai sem extensao");
    for (uint32_t i = 0; i <= tam;) {
        const char *s = t + i;
        uint32_t n = ProximoSegmento(t, tam, &i);
        if (!SegmentoValido(s, n)) {
            return DiagRelatar(d, E1001, pos, "segmento '%.*s' fora do padrao snake_case (a-z, 0-9 e _, comecando por letra)", (int)n, s);
        }
    }
    return true;
}

/* Nomes que a IA costuma chutar para o que a Forja tem em std:text (IA1-S2b2a). */
static bool ChuteDeStdText(const char *nome, uint32_t tam)
{
    static const char *const CHUTES[] = {"strings", "strconv", "utf8", "unicode"};
    for (size_t k = 0; k < sizeof CHUTES / sizeof CHUTES[0]; k++) {
        if (strlen(CHUTES[k]) == tam && memcmp(CHUTES[k], nome, tam) == 0) return true;
    }
    return false;
}

/* Nomes que a IA costuma chutar para a formatacao por regiao, que a Forja tem em std:fmt (IA1-S2e). */
static bool ChuteDeStdFmt(const char *nome, uint32_t tam)
{
    static const char *const CHUTES[] = {"locale", "intl", "number", "currency"};
    for (size_t k = 0; k < sizeof CHUTES / sizeof CHUTES[0]; k++) {
        if (strlen(CHUTES[k]) == tam && memcmp(CHUTES[k], nome, tam) == 0) return true;
    }
    return false;
}

/* P79: o que o E1001 de modulo precisa: o modulo (pasta do arquivo raiz), o diagnostico e a posicao do texto do import. */
typedef struct { const Modulos *m; Diag *d; uint32_t pos; } ContextoImport;

/* A causa do diagnostico tem 512 bytes: a lista de modulos fica em ate 440 (o resto da frase cabe). */
enum { LISTA_MODULOS_TAM = 440, IMPORT_TAM = 128 };

/* Parecido de um import std: sem o 'std:' este nome nao e chute conhecido (ChuteDeStdText/Fmt): "quis dizer std:text?" pela tabela de ModulosParecido. */
static bool RelatarStdParecido(const ContextoImport *cx, const char *imp, const char *lista)
{
    char parecido[IMPORT_TAM];

    if (!ModulosParecido(cx->m->pasta, imp, parecido, sizeof parecido)) {
        return DiagRelatar(cx->d, E1001, cx->pos, "%s nao existe; disponiveis: %s", imp, lista);
    }
    return DiagRelatar(cx->d, E1001, cx->pos, "%s nao existe; quis dizer %s? (disponiveis: %s)", imp, parecido, lista);
}

/* E1001 de modulo std ausente. "std:proc" foi renomeado (IA1-b01): a causa traz a forma certa, import "std:os". */
static bool RelatarStdAusente(const ContextoImport *cx, const char *nome, uint32_t tam)
{
    Diag *d = cx->d;
    uint32_t pos = cx->pos;
    char lista[LISTA_MODULOS_TAM], imp[IMPORT_TAM];

    ModulosListarDisponiveis(cx->m->pasta, lista, sizeof lista);
    snprintf(imp, sizeof imp, "std:%.*s", (int)tam, nome);
    if (ChuteDeStdText(nome, tam)) {
        DiagRelatar(d, E1001, pos, "std:%.*s nao existe: texto e bytes ficam em std:text; escreva import \"std:text\" (disponiveis: %s)", (int)tam, nome, lista);
        DiagSugerir(&d->sug, DIAG_CERTA, "troque \"std:%.*s\" por \"std:text\": a Forja tem um so modulo de texto", (int)tam, nome);
        DiagEditar(&d->sug, pos, pos + 6 + tam, "\"std:text\""); /* aspas + "std:" + nome */
        return false;
    }
    if (ChuteDeStdFmt(nome, tam)) {
        DiagRelatar(d, E1001, pos, "std:%.*s nao existe: numero, moeda e data por regiao ficam em std:fmt; escreva import \"std:fmt\" (disponiveis: %s)", (int)tam, nome, lista);
        DiagSugerir(&d->sug, DIAG_CERTA, "troque \"std:%.*s\" por \"std:fmt\": a Forja tem um so modulo de formatacao por regiao", (int)tam, nome);
        DiagEditar(&d->sug, pos, pos + 6 + tam, "\"std:fmt\""); /* aspas + "std:" + nome */
        return false;
    }
    if (tam == 2 && memcmp(nome, "fs", 2) == 0) {
        DiagRelatar(d, E1001, pos, "std:fs nao existe: arquivo, pasta e caminho ficam em std:file; escreva import \"std:file\" (disponiveis: %s)", lista);
        DiagSugerir(&d->sug, DIAG_CERTA, "troque \"std:fs\" por \"std:file\": o sistema de arquivos esta no modulo de arquivo");
        DiagEditar(&d->sug, pos, pos + 6 + tam, "\"std:file\""); /* aspas + "std:" + nome */
        return false;
    }
    if (tam == 4 && memcmp(nome, "proc", 4) == 0) {
        DiagRelatar(d, E1001, pos, "std:proc nao existe mais: o modulo agora e std:os; escreva import \"std:os\" (disponiveis: %s)", lista);
        DiagSugerir(&d->sug, DIAG_CERTA, "troque \"std:proc\" por \"std:os\": o modulo foi renomeado"); /* IA1-L2: pos = a aspa de abertura do texto */
        DiagEditar(&d->sug, pos, pos + 10, "\"std:os\"");
        return false;
    }
    return RelatarStdParecido(cx, imp, lista);
}

/* Forma do caminho na ordem do contrato (2.3). */
static bool ConferirForma(const ContextoImport *cx, const char *t, uint32_t tam)
{
    Diag *d = cx->d;
    uint32_t pos = cx->pos;

    if (tam == 0) return DiagRelatar(d, E1001, pos, "caminho de import vazio");
    if (tam >= 4 && memcmp(t, "std:", 4) == 0) {
        return ModulosStdTem(t + 4, tam - 4) || RelatarStdAusente(cx, t + 4, tam - 4);
    }
    if (tam >= 7 && (memcmp(t, "vendor:", 7) == 0 || memcmp(t, "engine:", 7) == 0)) {
        return ConferirSegmentos(d, pos, t + 7, tam - 7);
    }
    return ConferirPrefixo(d, pos, t, tam) && ConferirSegmentos(d, pos, t, tam);
}

static ModuloOrigem OrigemDoCaminho(const char *t)
{
    if (strncmp(t, "std:", 4) == 0) return MOD_STD;
    if (strncmp(t, "vendor:", 7) == 0) return MOD_VENDOR;
    if (strncmp(t, "engine:", 7) == 0) return MOD_ENGINE;
    return MOD_ARQUIVO;
}

/* Acha o modulo de mesmo texto (raiz inclusive) ou abre um novo. */
static bool AcharOuCriar(Modulos *m, const RegistroImport *r, Diag *d, uint8_t *indice)
{
    for (uint32_t k = 0; k < m->n; k++) {
        if (m->itens[k].caminho == r->texto) { *indice = (uint8_t)k; return true; }
    }
    if (m->n == MODULOS_MAX) return DiagRelatar(d, E1001, r->pos_texto, "mais de 254 modulos importados");
    const char *t = InternerTexto(m->nomes, r->texto);
    Modulo *novo = &m->itens[m->n];
    *novo = (Modulo){.caminho = r->texto, .importador = r->arquivo, .pos_import = r->pos_texto};
    novo->origem = (uint8_t)OrigemDoCaminho(t);
    novo->apelido = ApelidoPadrao(m, t, InternerTam(m->nomes, r->texto));
    *indice = (uint8_t)m->n++;
    return true;
}

/* Sem 'as', o apelido padrao (ultimo segmento) nao pode ser palavra reservada: os ids das palavras-chave no Interner sao 0..KW_QTD-1 (token.h). */
static bool ConferirApelidoPadrao(Modulos *m, const RegistroImport *r, Diag *d)
{
    if (r->apelido >= 0) return true;
    int32_t padrao = ApelidoPadrao(m, InternerTexto(m->nomes, r->texto), InternerTam(m->nomes, r->texto));
    if (padrao >= KW_QTD) return true;
    return DiagRelatar(d, E1006, r->pos_texto, "o ultimo segmento do caminho \"%s\" e a palavra reservada '%s': escreva import \"%s\" as <nome>",
                       InternerTexto(m->nomes, r->texto), InternerTexto(m->nomes, padrao), InternerTexto(m->nomes, r->texto));
}

/* L3c2: a pasta (1o segmento) de um import "vendor:" na forma antiga da tabela (REN_PACOTE: banco -> db) vira a nova; 'saida' leva o texto canonico. Devolve false so no E0110 (recusar). */
static bool CanonizarPasta(Modulos *m, const RegistroImport *r, Diag *d, RegistroImport *saida)
{
    const char *t = InternerTexto(m->nomes, r->texto);
    uint32_t tam = InternerTam(m->nomes, r->texto), fim = 7;
    bool era_antigo = false;
    char pasta[64], canonico[512];

    while (fim < tam && t[fim] != '/') fim++;
    if (fim - 7 >= sizeof pasta || tam >= sizeof canonico) return true;
    snprintf(pasta, sizeof pasta, "%.*s", (int)(fim - 7), t + 7);
    const Renomeado *ren = RenomeadoAchar(REN_PACOTE, "import", pasta, &era_antigo);
    if (ren == NULL || !era_antigo) return true;
    RenomeadoNotarTabela(d, r->pos_texto + 1 + 7, fim - 7, ren);
    if (RenomeadoRecusa(ren)) return RenomeadoRecusar(d, r->pos_texto + 1 + 7, fim - 7, ren);
    snprintf(canonico, sizeof canonico, "vendor:%s%.*s", ren->novo, (int)(tam - fim), t + fim);
    saida->texto = Internar(m->nomes, canonico, (uint32_t)strlen(canonico));
    return true;
}

/* L3c: "std:arquivo" (forma antiga da tabela de renomeados) vale como o modulo canonico "std:file"; 'saida' leva o texto canonico. L3c2: o mesmo para o caminho inteiro de um vendor
 * ("vendor:banco/banco" -> "vendor:db/db") e, se esse nao casa, para a pasta do pacote (CanonizarPasta). Com recusar (L3f) a forma antiga e E0110. */
static bool CanonizarImport(Modulos *m, const RegistroImport *r, Diag *d, RegistroImport *saida)
{
    const char *t = InternerTexto(m->nomes, r->texto);
    bool era_antigo = false;
    bool de_modulo = strncmp(t, "std:", 4) == 0 || strncmp(t, "vendor:", 7) == 0;
    const Renomeado *ren = de_modulo ? RenomeadoAchar(REN_MODULO, "import", t, &era_antigo) : NULL;

    *saida = *r;
    if (ren == NULL && strncmp(t, "vendor:", 7) == 0) return CanonizarPasta(m, r, d, saida);
    if (ren == NULL || !era_antigo) return true;
    RenomeadoNotarTabela(d, r->pos_texto + 1, InternerTam(m->nomes, r->texto), ren);
    if (RenomeadoRecusa(ren)) return RenomeadoRecusar(d, r->pos_texto + 1, InternerTam(m->nomes, r->texto), ren);
    saida->texto = Internar(m->nomes, ren->novo, (uint32_t)strlen(ren->novo));
    return true;
}

/* L3c: o apelido padrao e o ultimo segmento do caminho ESCRITO (arquivo para "std:arquivo"); com recusar na entrada de qualificador (L3f) e E0110 no segmento. */
static bool ConferirQualificador(Modulos *m, const RegistroImport *r, Diag *d)
{
    const char *t = InternerTexto(m->nomes, r->texto);
    uint32_t tam = InternerTam(m->nomes, r->texto);
    bool era_antigo = false;
    int32_t padrao;
    const Renomeado *q;

    if (r->apelido >= 0 || RenomeadoBuscar(REN_MODULO, t) == NULL) return true;
    padrao = ApelidoPadrao(m, t, tam);
    q = RenomeadoAchar(REN_QUALIFICADOR, NULL, InternerTexto(m->nomes, padrao), &era_antigo);
    if (q == NULL || !era_antigo || !RenomeadoRecusa(q)) return true;
    return RenomeadoRecusar(d, r->pos_texto + 1 + (tam - InternerTam(m->nomes, padrao)), InternerTam(m->nomes, padrao), q);
}

/* L3d1: sem 'as', o apelido padrao segue o caminho ESCRITO; se o caminho vai mudar (forma antiga) e o ultimo segmento muda junto, devolve o apelido da forma nova (o forja migrar
 * troca os usos 'antigo.' por 'novo.'); -1 = o apelido fica. */
static int32_t ApelidoDaFormaNova(Modulos *m, const RegistroImport *escrito, const RegistroImport *canonico, int32_t apelido)
{
    int32_t novo;

    if (escrito->apelido >= 0 || canonico->texto == escrito->texto) return -1;
    novo = ApelidoPadrao(m, InternerTexto(m->nomes, canonico->texto), InternerTam(m->nomes, canonico->texto));
    return novo == apelido ? -1 : novo;
}

bool ModulosRegistrar(Modulos *m, const RegistroImport *escrito, Diag *d, uint8_t *indice)
{
    RegistroImport canonico;
    const RegistroImport *r = &canonico;

    if (!CanonizarImport(m, escrito, d, &canonico)) return false;
    ContextoImport cx = {m, d, r->pos_texto};

    if (!ConferirForma(&cx, InternerTexto(m->nomes, r->texto), InternerTam(m->nomes, r->texto))) return false;
    if (!ConferirApelidoPadrao(m, escrito, d) || !ConferirQualificador(m, escrito, d)) return false;
    if (!AcharOuCriar(m, r, d, indice)) return false;
    int32_t apelido = escrito->apelido;
    if (apelido < 0) {
        apelido = ApelidoPadrao(m, InternerTexto(m->nomes, escrito->texto), InternerTam(m->nomes, escrito->texto));
        m->itens[*indice].apelido = apelido; /* o parser le este campo logo em seguida (Decl.nome_a) */
    }
    for (size_t i = 0; i < m->apelidos.n; i++) {
        const ApelidoDeArquivo *a = VecEm(&m->apelidos, i);
        if (a->arquivo != r->arquivo) continue;
        if (a->modulo == *indice) {
            return DiagRelatar(d, E1001, r->pos_texto, "o modulo \"%s\" ja foi importado neste arquivo", InternerTexto(m->nomes, r->texto));
        }
        if (a->nome == apelido) {
            return DiagRelatar(d, E1001, r->pos_apelido, "apelido '%s' ja usado neste arquivo; troque com 'as'", InternerTexto(m->nomes, apelido));
        }
    }
    ApelidoDeArquivo *novo = VecEmpurrar(&m->apelidos);
    *novo = (ApelidoDeArquivo){.arquivo = r->arquivo, .modulo = *indice, .nome = apelido, .novo = ApelidoDaFormaNova(m, escrito, r, apelido)};
    return true;
}

uint8_t ModulosApelido(const Modulos *m, uint32_t arquivo, int32_t nome)
{
    for (size_t i = 0; i < m->apelidos.n; i++) {
        const ApelidoDeArquivo *a = VecEm(&m->apelidos, i);
        if (a->nome == nome && a->arquivo == arquivo) return a->modulo;
    }
    return 0;
}

void ModulosRegistrarArquivo(Modulos *m, uint32_t arquivo, uint8_t pacote)
{
    while (m->pacotes.n <= arquivo) VecEmpurrar(&m->pacotes);
    *(uint8_t *)VecEm(&m->pacotes, arquivo) = pacote;
}

uint8_t ModulosPacoteDoArquivo(const Modulos *m, uint32_t arquivo)
{
    return arquivo < m->pacotes.n ? *(const uint8_t *)VecEm(&m->pacotes, arquivo) : 0;
}

uint32_t ModulosTrechosDoPacote(const Modulos *m, uint32_t ntrechos, uint8_t pacote, uint32_t *primeiro)
{
    uint32_t n = 0;

    *primeiro = 0;
    if (ntrechos == 0) return pacote == 0 ? 1 : 0;
    for (uint32_t t = 0; t < ntrechos; t++) {
        if (ModulosPacoteDoArquivo(m, t) != pacote) continue;
        if (n++ == 0) *primeiro = t;
    }
    return n;
}

bool ModulosApelidoNoPacote(const Modulos *m, uint8_t pacote, int32_t nome)
{
    for (size_t i = 0; i < m->apelidos.n; i++) {
        const ApelidoDeArquivo *a = VecEm(&m->apelidos, i);
        if (a->nome == nome && ModulosPacoteDoArquivo(m, a->arquivo) == pacote) return true;
    }
    return false;
}

int32_t ModulosApelidoNovo(const Modulos *m, uint32_t arquivo, int32_t nome)
{
    for (size_t i = 0; i < m->apelidos.n; i++) {
        const ApelidoDeArquivo *a = VecEm(&m->apelidos, i);
        if (a->nome == nome && a->arquivo == arquivo) return a->novo;
    }
    return -1;
}

/* ---- ciclos (E1002) e ordem de dependencia ---- */

enum { ESTADO_NOVO, ESTADO_NA_PILHA, ESTADO_PRONTO };

typedef struct {
    Modulos *m;
    const Programa *p;
    Diag *d;
    uint32_t prof, nordem;
    uint8_t pilha[MODULOS_MAX], estado[MODULOS_MAX];
} Dfs;

/* "a -> b -> a": da posicao de 'alvo' na pilha ate o topo, e o alvo de novo (E1002 no texto do import que fecha o ciclo). */
static bool RelatarCiclo(Dfs *g, const Decl *imp, uint8_t alvo)
{
    Buf cadeia = {0};
    uint32_t ini = 0;
    while (g->pilha[ini] != alvo) ini++;
    for (uint32_t i = ini; i < g->prof; i++) {
        BufAnexar(&cadeia, InternerTexto(g->m->nomes, g->m->itens[g->pilha[i]].caminho));
        BufAnexar(&cadeia, " -> ");
    }
    BufAnexar(&cadeia, InternerTexto(g->m->nomes, g->m->itens[alvo].caminho));
    DiagRelatar(g->d, E1002, imp->pos_a, "ciclo de import: %s", cadeia.dados);
    BufLiberar(&cadeia);
    return false;
}

static bool Visitar(Dfs *g, uint8_t k);

/* Segue uma aresta k -> imp->indice. false = ciclo (diagnostico ja gravado). */
static bool SeguirImport(Dfs *g, const Decl *imp)
{
    uint8_t alvo = (uint8_t)imp->indice;
    if (g->estado[alvo] == ESTADO_NA_PILHA) return RelatarCiclo(g, imp, alvo);
    return g->estado[alvo] == ESTADO_PRONTO || Visitar(g, alvo);
}

static bool Visitar(Dfs *g, uint8_t k)
{
    g->estado[k] = ESTADO_NA_PILHA;
    g->pilha[g->prof++] = k;
    for (const Decl *d = g->p->primeira; d != NULL; d = d->prox) {
        if (d->tipo_no == D_IMPORT && d->modulo == k && !SeguirImport(g, d)) return false;
    }
    g->prof--;
    g->estado[k] = ESTADO_PRONTO;
    g->m->ordem[g->nordem++] = k;
    return true;
}

bool ModulosConferirCiclos(Modulos *m, const Programa *p, Diag *d)
{
    Dfs g = {.m = m, .p = p, .d = d};
    return Visitar(&g, 0);
}

void ModulosOrdenarDecls(const Modulos *m, Programa *p)
{
    Decl *cabeca[MODULOS_MAX] = {0}, *cauda[MODULOS_MAX] = {0}, *ultima = NULL;
    for (Decl *d = p->primeira, *prox; d != NULL; d = prox) {
        prox = d->prox;
        d->prox = NULL;
        if (cauda[d->modulo] == NULL) cabeca[d->modulo] = d; else cauda[d->modulo]->prox = d;
        cauda[d->modulo] = d;
    }
    p->primeira = NULL;
    for (uint32_t i = 0; i < m->n; i++) {
        uint8_t k = m->ordem[i];
        if (cabeca[k] == NULL) continue;
        if (ultima == NULL) p->primeira = cabeca[k]; else ultima->prox = cabeca[k];
        ultima = cauda[k];
    }
    p->ultima = ultima;
}

void ModulosLiberar(Modulos *m)
{
    VecLiberar(&m->apelidos);
    VecLiberar(&m->pacotes);
}
