/* teste_abi_sysv.c - confere AbiSysvClassificar e AbiSysvRetorno (x64_abi_sysv.c) com as 60 assinaturas que o gcc fez (abi_sysv.json, lido do objdump).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M5b P38a: AbiSysvPlanejar (AbiSysvPlano) contra as mesmas 60 assinaturas: registradores e pilha por argumento, retorno
 *                    MEMORY (RDI oculto), classe de cada eightbyte, AL (xmm dos argumentos) e bytes de pilha (regra: arredondado a 8).
 *            0.1.0 - M5 P34: 60 assinaturas contra o gcc (args por registrador, pilha, retorno e oculto); parser JSON minimo proprio (objeto, lista, texto sem escape, literal);
 *                    layout das structs tirado dos typedef de assinaturas.c (alinhamento natural de C); auto-checagens de AL variadica e void.
 * Verificacao: gcc -std=c11 -Wall -Wextra -I compiler tests\unidade\teste_abi_sysv.c compiler\nativo\x64\x64_abi_sysv.c -o %TEMP%\p34\teste_abi_sysv.exe
 *              %TEMP%\p34\teste_abi_sysv.exe tests\nativo\linux\abi_sysv\abi_sysv.json tests\nativo\linux\abi_sysv\assinaturas.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nativo/bir/x64_abi_sysv.h"

#define ASSINATURAS_ESPERADAS 60u
#define MAX_PARAMS 16
#define MAX_ESTRUTURAS 64
#define MAX_CAMPOS 16
#define MAX_NOS 32768
#define MAX_TEXTO (1u << 19)
#define MAX_LINHA 512

/* ---- parser JSON minimo: so o que o gerador (classificar_abi.py) escreve ---- */

typedef enum { JSON_OBJETO, JSON_LISTA, JSON_TEXTO, JSON_LITERAL } JsonTipo;

typedef struct JsonNo {
    JsonTipo tipo;
    const char *chave;       /* nome no objeto pai */
    const char *texto;       /* JSON_TEXTO: conteudo; JSON_LITERAL: numero, true, false ou null */
    struct JsonNo *primeiro; /* primeiro filho de objeto ou lista */
    struct JsonNo *irmao;    /* proximo filho do mesmo pai */
} JsonNo;

static JsonNo JSON_NOS[MAX_NOS];
static uint32_t JSON_USADOS;
static char JSON_TEXTOS[MAX_TEXTO];
static uint32_t JSON_TEXTO_USADO;

static const char *PularEspacos(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    return p;
}

static JsonNo *NovoNo(JsonTipo tipo, const char *chave)
{
    JsonNo *no;

    if (JSON_USADOS >= MAX_NOS)
        return NULL;
    no = &JSON_NOS[JSON_USADOS++];
    no->tipo = tipo;
    no->chave = chave;
    no->texto = NULL;
    no->primeiro = NULL;
    no->irmao = NULL;
    return no;
}

/* Copia len bytes para o acervo de textos e devolve o ponteiro guardado (NULL se estourar). */
static const char *GuardarTexto(const char *ini, size_t len)
{
    char *dst;

    if (JSON_TEXTO_USADO + len + 1 > MAX_TEXTO)
        return NULL;
    dst = &JSON_TEXTOS[JSON_TEXTO_USADO];
    memcpy(dst, ini, len);
    dst[len] = '\0';
    JSON_TEXTO_USADO += (uint32_t)len + 1u;
    return dst;
}

/* p aponta para a aspa de abertura. Escape (\) e erro: o gerador do json nao o usa, entao aparecer aqui e algo a checar. */
static const char *LerTexto(const char *p, const char **saida)
{
    const char *ini = p + 1;
    const char *fim = ini;

    while (*fim != '"') {
        if (*fim == '\0' || *fim == '\\')
            return NULL;
        fim++;
    }
    *saida = GuardarTexto(ini, (size_t)(fim - ini));
    return *saida ? fim + 1 : NULL;
}

static const char *ParseTexto(const char *p, const char *chave, JsonNo **saida)
{
    JsonNo *no = NovoNo(JSON_TEXTO, chave);

    if (!no)
        return NULL;
    p = LerTexto(p, &no->texto);
    if (p)
        *saida = no;
    return p;
}

static const char *ParseLiteral(const char *p, const char *chave, JsonNo **saida)
{
    const char *ini = p;
    JsonNo *no;

    while ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-' || *p == '+' || *p == '.')
        p++;
    if (p == ini)
        return NULL;
    no = NovoNo(JSON_LITERAL, chave);
    if (!no)
        return NULL;
    no->texto = GuardarTexto(ini, (size_t)(p - ini));
    if (!no->texto)
        return NULL;
    *saida = no;
    return p;
}

static const char *ParseValor(const char *p, const char *chave, JsonNo **saida);

static const char *ParseChaveDoObjeto(const char *p, const char **chave)
{
    if (*p != '"')
        return NULL;
    p = LerTexto(p, chave);
    if (!p)
        return NULL;
    p = PularEspacos(p);
    if (*p != ':')
        return NULL;
    return PularEspacos(p + 1);
}

/* Le os filhos (separados por virgula) ate o fecha; devolve o ponteiro depois dele. */
static const char *ParseFilhos(const char *p, JsonNo *no, char fecha)
{
    JsonNo **ultimo = &no->primeiro;

    for (;;) {
        const char *chave_filho = NULL;
        JsonNo *filho = NULL;

        p = PularEspacos(p);
        if (no->tipo == JSON_OBJETO) {
            p = ParseChaveDoObjeto(p, &chave_filho);
            if (!p)
                return NULL;
        }
        p = ParseValor(p, chave_filho, &filho);
        if (!p)
            return NULL;
        *ultimo = filho;
        ultimo = &filho->irmao;
        p = PularEspacos(p);
        if (*p == fecha)
            return p + 1;
        if (*p != ',')
            return NULL;
        p++;
    }
}

static const char *ParseContainer(const char *p, JsonTipo tipo, const char *chave, JsonNo **saida)
{
    char fecha = (tipo == JSON_OBJETO) ? '}' : ']';
    JsonNo *no = NovoNo(tipo, chave);

    if (!no)
        return NULL;
    p = PularEspacos(p + 1);
    if (*p == fecha) {
        *saida = no;
        return p + 1;
    }
    p = ParseFilhos(p, no, fecha);
    if (p)
        *saida = no;
    return p;
}

static const char *ParseValor(const char *p, const char *chave, JsonNo **saida)
{
    p = PularEspacos(p);
    switch (*p) {
    case '{':
        return ParseContainer(p, JSON_OBJETO, chave, saida);
    case '[':
        return ParseContainer(p, JSON_LISTA, chave, saida);
    case '"':
        return ParseTexto(p, chave, saida);
    default:
        return ParseLiteral(p, chave, saida);
    }
}

static bool ParseJson(const char *texto, JsonNo **raiz)
{
    const char *fim = ParseValor(texto, NULL, raiz);

    return fim != NULL && *PularEspacos(fim) == '\0';
}

static const JsonNo *JsonCampo(const JsonNo *obj, const char *chave)
{
    const JsonNo *f;

    if (!obj || obj->tipo != JSON_OBJETO)
        return NULL;
    for (f = obj->primeiro; f; f = f->irmao) {
        if (f->chave && strcmp(f->chave, chave) == 0)
            return f;
    }
    return NULL;
}

static const char *JsonTextoDe(const JsonNo *obj, const char *chave)
{
    const JsonNo *no = JsonCampo(obj, chave);

    return (no && no->texto) ? no->texto : "";
}

static uint32_t JsonQuantos(const JsonNo *lista)
{
    uint32_t n = 0;
    const JsonNo *f = lista ? lista->primeiro : NULL;

    for (; f; f = f->irmao)
        n++;
    return n;
}

/* ---- layout das structs: tirado dos typedef de assinaturas.c (o gcc compilou esse codigo) ---- */

typedef struct {
    const char *nome;
    uint32_t tam;
    AbiSysvClasse classe;
} TipoEscalar;

static const TipoEscalar TIPOS_C[] = {
    {"signed char", 1, ABI_SYSV_INTEIRO}, {"short", 2, ABI_SYSV_INTEIRO}, {"int", 4, ABI_SYSV_INTEIRO},
    {"long", 8, ABI_SYSV_INTEIRO}, {"float", 4, ABI_SYSV_SSE}, {"double", 8, ABI_SYSV_SSE},
};

static const TipoEscalar TIPOS_JSON[] = {
    {"i8", 1, ABI_SYSV_INTEIRO}, {"i16", 2, ABI_SYSV_INTEIRO}, {"i32", 4, ABI_SYSV_INTEIRO},
    {"i64", 8, ABI_SYSV_INTEIRO}, {"ptr", 8, ABI_SYSV_INTEIRO}, {"f32", 4, ABI_SYSV_SSE}, {"f64", 8, ABI_SYSV_SSE},
};

typedef struct {
    char nome[16];
    AbiSysvCampo campos[MAX_CAMPOS];
    uint32_t n_campos;
    uint32_t tam;
} EstruturaC;

static EstruturaC ESTRUTURAS[MAX_ESTRUTURAS];
static uint32_t N_ESTRUTURAS;

static const TipoEscalar *BuscarEscalar(const TipoEscalar *tab, size_t n, const char *nome)
{
    for (size_t i = 0; i < n; i++) {
        if (strcmp(tab[i].nome, nome) == 0)
            return &tab[i];
    }
    return NULL;
}

static uint32_t Alinhar(uint32_t valor, uint32_t alinhamento)
{
    return (valor + alinhamento - 1u) / alinhamento * alinhamento;
}

/* Acrescenta o campo "tipo nome" (ini/len do trecho entre ';') no alinhamento natural de C. Trecho vazio ou so com espacos e ignorado. */
static bool AdicionarCampoC(const char *ini, size_t len, EstruturaC *est, uint32_t *offset, uint32_t *alinha)
{
    char tipo[32];
    size_t k;
    const TipoEscalar *esc;

    while (len > 0 && *ini == ' ') {
        ini++;
        len--;
    }
    while (len > 0 && ini[len - 1] == ' ')
        len--;
    if (len == 0)
        return true;
    k = len;
    while (k > 0 && ini[k - 1] != ' ')
        k--;
    if (k < 2 || k - 1 >= sizeof tipo || est->n_campos >= MAX_CAMPOS)
        return false;
    memcpy(tipo, ini, k - 1);
    tipo[k - 1] = '\0';
    esc = BuscarEscalar(TIPOS_C, sizeof TIPOS_C / sizeof TIPOS_C[0], tipo);
    if (!esc)
        return false;
    *offset = Alinhar(*offset, esc->tam);
    est->campos[est->n_campos++] = (AbiSysvCampo){.classe = esc->classe, .deslocamento = *offset, .alinhamento = esc->tam};
    *offset += esc->tam;
    if (esc->tam > *alinha)
        *alinha = esc->tam;
    return true;
}

/* linha = "typedef struct S01 { signed char f0; } S01;" */
static bool LerUmaEstrutura(const char *linha, EstruturaC *est)
{
    const char *ini = linha + sizeof("typedef struct ") - 1;
    const char *abre = strchr(linha, '{');
    const char *fecha = strchr(linha, '}');
    const char *p;
    size_t nome_len;
    uint32_t offset = 0, alinha = 1;

    if (!abre || !fecha || fecha < abre)
        return false;
    nome_len = strcspn(ini, " {");
    if (nome_len == 0 || nome_len >= sizeof est->nome)
        return false;
    memcpy(est->nome, ini, nome_len);
    est->nome[nome_len] = '\0';
    for (p = abre + 1; p < fecha;) {
        const char *fim = p;

        while (fim < fecha && *fim != ';')
            fim++;
        if (!AdicionarCampoC(p, (size_t)(fim - p), est, &offset, &alinha))
            return false;
        p = fim + 1;
    }
    est->tam = Alinhar(offset, alinha);
    return true;
}

static bool LerEstruturasC(const char *texto)
{
    const char *p = texto;
    char linha[MAX_LINHA];

    N_ESTRUTURAS = 0;
    while ((p = strstr(p, "typedef struct S")) != NULL) {
        size_t len = strcspn(p, "\r\n");

        if (len >= MAX_LINHA || N_ESTRUTURAS >= MAX_ESTRUTURAS)
            return false;
        memcpy(linha, p, len);
        linha[len] = '\0';
        if (!LerUmaEstrutura(linha, &ESTRUTURAS[N_ESTRUTURAS]))
            return false;
        N_ESTRUTURAS++;
        p += len;
    }
    return N_ESTRUTURAS > 0;
}

static const EstruturaC *BuscarEstrutura(const char *nome)
{
    for (uint32_t i = 0; i < N_ESTRUTURAS; i++) {
        if (strcmp(ESTRUTURAS[i].nome, nome) == 0)
            return &ESTRUTURAS[i];
    }
    return NULL;
}

/* ---- montagem e classificacao das assinaturas ---- */

typedef struct {
    AbiSysvArg params[MAX_PARAMS];
    AbiSysvCampo escalares[MAX_PARAMS];
    uint32_t n;
    AbiSysvArg ret;
    AbiSysvCampo ret_escalar;
} Assinatura;

/* Escalar vira um campo unico em deslocamento 0; agregado usa o layout de assinaturas.c. escalar aponta para o campo guardado pelo chamador. */
static bool MontarArg(const char *tipo, AbiSysvArg *arg, AbiSysvCampo *escalar)
{
    const TipoEscalar *esc = BuscarEscalar(TIPOS_JSON, sizeof TIPOS_JSON / sizeof TIPOS_JSON[0], tipo);
    const EstruturaC *est;

    if (esc) {
        *escalar = (AbiSysvCampo){.classe = esc->classe, .deslocamento = 0, .alinhamento = esc->tam};
        *arg = (AbiSysvArg){.tam = esc->tam, .campos = escalar, .n_campos = 1};
        return true;
    }
    est = BuscarEstrutura(tipo);
    if (!est)
        return false;
    *arg = (AbiSysvArg){.tam = est->tam, .campos = est->campos, .n_campos = est->n_campos};
    return true;
}

static bool MontarAssinatura(const JsonNo *sig, Assinatura *a)
{
    const JsonNo *params = JsonCampo(sig, "params");
    const JsonNo *f;

    a->n = 0;
    if (!params || params->tipo != JSON_LISTA || JsonQuantos(params) > MAX_PARAMS)
        return false;
    for (f = params->primeiro; f; f = f->irmao) {
        if (f->tipo != JSON_TEXTO || !MontarArg(f->texto, &a->params[a->n], &a->escalares[a->n]))
            return false;
        a->n++;
    }
    return MontarArg(JsonTextoDe(sig, "ret"), &a->ret, &a->ret_escalar);
}

/* Contagem por registrador (indice = AbiSysvReg), mais se algum argumento foi para a pilha. Usada para a classificacao minha e a do gcc. */
typedef struct {
    uint32_t args[ABI_SYSV_NENHUM];
    uint32_t ret[ABI_SYSV_NENHUM];
    bool pilha;
} Contagem;

static void ContarRegistros(const AbiSysvLocal *loc, uint32_t *hist)
{
    for (uint32_t e = 0; e < loc->n_eightbytes; e++)
        hist[loc->reg[e]]++;
}

/* Retorno em memoria: o endereco volta em RAX (reg[0]); o registrador do tipo nao esta em n_eightbytes. */
static void ContarRetorno(const AbiSysvLocal *loc, uint32_t *hist)
{
    if (loc->na_memoria)
        hist[loc->reg[0]]++;
    else
        ContarRegistros(loc, hist);
}

static bool CalcularMinha(const Assinatura *a, Contagem *res)
{
    AbiSysvLocal locs[MAX_PARAMS];
    AbiSysvLocal loc_ret;
    uint32_t oculto, k;

    memset(res, 0, sizeof *res);
    if (!AbiSysvRetorno(&a->ret, &loc_ret))
        return false;
    oculto = loc_ret.na_memoria ? 1u : 0u;
    if (!AbiSysvClassificar(a->params, a->n, oculto, locs))
        return false;
    if (oculto)
        res->args[ABI_SYSV_RDI]++;
    for (k = 0; k < a->n; k++) {
        if (locs[k].na_memoria)
            res->pilha = true;
        ContarRegistros(&locs[k], res->args);
    }
    ContarRetorno(&loc_ret, res->ret);
    return true;
}

/* ---- leitura do gcc (json) ---- */

static int RegDoNome(const char *nome)
{
    for (uint32_t r = 0; r < ABI_SYSV_NENHUM; r++) {
        if (strcmp(AbiSysvNomeReg((AbiSysvReg)r), nome) == 0)
            return (int)r;
    }
    return -1;
}

static bool HistogramaDoJson(const JsonNo *lista, uint32_t *hist)
{
    const JsonNo *f;

    if (!lista || lista->tipo != JSON_LISTA)
        return false;
    for (f = lista->primeiro; f; f = f->irmao) {
        int reg = RegDoNome(f->texto);

        if (reg < 0)
            return false;
        hist[reg]++;
    }
    return true;
}

static bool ObservadoDoJson(const JsonNo *sig, Contagem *obs)
{
    const JsonNo *obs_no = JsonCampo(sig, "observado");

    memset(obs, 0, sizeof *obs);
    if (!obs_no)
        return false;
    obs->pilha = strcmp(JsonTextoDe(obs_no, "pilha"), "true") == 0;
    return HistogramaDoJson(JsonCampo(obs_no, "args"), obs->args) && HistogramaDoJson(JsonCampo(obs_no, "ret"), obs->ret);
}

/* Mesma regra do classificar_abi.py (Comparar): args iguais em multiconjunto (variadica: os meus presentes no gcc, pilha nao entra);
 * retorno meu presente no gcc; o que o gcc tem a mais no retorno e so contado em sobra. */
static bool CompararComGcc(const Contagem *mine, const Contagem *gcc, bool variadica, uint32_t *sobra)
{
    bool args_ok = variadica || mine->pilha == gcc->pilha;
    bool ret_ok = true;

    for (uint32_t r = 0; r < ABI_SYSV_NENHUM; r++) {
        if (variadica)
            args_ok = args_ok && mine->args[r] <= gcc->args[r];
        else
            args_ok = args_ok && mine->args[r] == gcc->args[r];
        ret_ok = ret_ok && mine->ret[r] <= gcc->ret[r];
        if (gcc->ret[r] > mine->ret[r])
            *sobra += gcc->ret[r] - mine->ret[r];
    }
    return args_ok && ret_ok;
}

/* ---- relato ---- */

typedef struct {
    uint32_t total;
    uint32_t iguais;
    uint32_t concordam_json;
    uint32_t sobra;
} Relatorio;

static void ImprimirRegistros(const char *rotulo, const uint32_t *hist)
{
    printf("  %s:", rotulo);
    for (uint32_t r = 0; r < ABI_SYSV_NENHUM; r++) {
        for (uint32_t c = 0; c < hist[r]; c++)
            printf(" %s", AbiSysvNomeReg((AbiSysvReg)r));
    }
    printf("\n");
}

static void ImprimirDivergencia(const char *nome, const Contagem *mine, const Contagem *gcc)
{
    printf("DIVERGE %s\n", nome);
    ImprimirRegistros("meu args", mine->args);
    ImprimirRegistros("gcc  args", gcc->args);
    printf("  pilha meu=%d gcc=%d\n", mine->pilha ? 1 : 0, gcc->pilha ? 1 : 0);
    ImprimirRegistros("meu ret", mine->ret);
    ImprimirRegistros("gcc  ret", gcc->ret);
}

/* Confere uma assinatura; devolve true se bate com o gcc. */
static bool ConferirUma(const JsonNo *sig, uint32_t *sobra)
{
    Assinatura a;
    Contagem mine, gcc;
    const char *nome = JsonTextoDe(sig, "nome");
    bool variadica = strcmp(JsonTextoDe(sig, "tipo"), "variadica") == 0;
    bool ok;

    if (!MontarAssinatura(sig, &a) || !CalcularMinha(&a, &mine) || !ObservadoDoJson(sig, &gcc)) {
        printf("ERRO %s: tipo desconhecido ou classificacao recusada\n", nome);
        return false;
    }
    ok = CompararComGcc(&mine, &gcc, variadica, sobra);
    if (!ok)
        ImprimirDivergencia(nome, &mine, &gcc);
    return ok;
}

/* Confere todas; e conta se o veredito bate com o ok do json (o json e a comparacao feita pelo python). */
static void ConferirTodas(const JsonNo *sigs, Relatorio *rel)
{
    const JsonNo *sig;

    memset(rel, 0, sizeof *rel);
    for (sig = sigs->primeiro; sig; sig = sig->irmao) {
        bool ok = ConferirUma(sig, &rel->sobra);
        bool ok_json = strcmp(JsonTextoDe(sig, "ok"), "true") == 0;

        rel->total++;
        if (ok)
            rel->iguais++;
        if (ok == ok_json)
            rel->concordam_json++;
    }
}

/* Auto-checagens que o json nao cobre (ele nao tem AL nem void): AL de variadica e retorno void. Conferidas pela regra SysV, nao pelo gcc. */
static bool AutoChecagens(void)
{
    AbiSysvCampo dbl = {.classe = ABI_SYSV_SSE, .deslocamento = 0, .alinhamento = 8};
    AbiSysvCampo inteiro = {.classe = ABI_SYSV_INTEIRO, .deslocamento = 0, .alinhamento = 4};
    AbiSysvArg arg_dbl = {.tam = 8, .campos = &dbl, .n_campos = 1};
    AbiSysvArg arg_int = {.tam = 4, .campos = &inteiro, .n_campos = 1};
    AbiSysvArg vazio = {.tam = 0, .campos = NULL, .n_campos = 0};
    AbiSysvLocal loc_dbl, loc_int, loc_ret;

    if (!AbiSysvClassificar(&arg_dbl, 1, 0, &loc_dbl) || AbiSysvAlVariadica(&loc_dbl, 1) != 1)
        return false;
    if (!AbiSysvClassificar(&arg_int, 1, 0, &loc_int) || AbiSysvAlVariadica(&loc_int, 1) != 0)
        return false;
    if (!AbiSysvRetorno(&vazio, &loc_ret))
        return false;
    return !loc_ret.na_memoria && loc_ret.n_eightbytes == 0;
}

/* ---- AbiSysvPlanejar (P38a): o plano que o call site usa, conferido com o mesmo gcc ---- */

/* Histograma do plano no formato de CalcularMinha (args, retorno, pilha), para comparar com o gcc. */
static void ContarPlano(const AbiSysvPlano *plano, uint32_t n, Contagem *res)
{
    memset(res, 0, sizeof *res);
    if (plano->oculto)
        res->args[ABI_SYSV_RDI]++;
    for (uint32_t k = 0; k < n; k++) {
        if (plano->arg[k].na_memoria)
            res->pilha = true;
        ContarRegistros(&plano->arg[k], res->args);
    }
    ContarRetorno(&plano->ret, res->ret);
}

/* Cada eightbyte cai num registrador da propria classe: INTEGER em registrador inteiro (antes de XMM0), SSE em xmm. */
static bool LocalCoerente(const AbiSysvLocal *loc)
{
    for (uint32_t e = 0; e < loc->n_eightbytes; e++) {
        bool registrador_inteiro = loc->reg[e] < ABI_SYSV_XMM0;

        if (registrador_inteiro != (loc->classe[e] == ABI_SYSV_INTEIRO))
            return false;
    }
    return true;
}

static bool PlanoCoerente(const AbiSysvPlano *plano, uint32_t n)
{
    for (uint32_t k = 0; k < n; k++) {
        if (!LocalCoerente(&plano->arg[k]))
            return false;
    }
    return LocalCoerente(&plano->ret);
}

/* Cada argumento na pilha ocupa o proprio tamanho arredondado a 8, em sequencia a partir de 0. O json nao traz o total da pilha, entao a regra SysV e conferida aqui. */
static bool PilhaBateComRegra(const Assinatura *a, const AbiSysvPlano *plano)
{
    uint32_t total = 0;

    for (uint32_t k = 0; k < a->n; k++) {
        if (!plano->arg[k].na_memoria)
            continue;
        if (plano->off[k] != total)
            return false;
        total += (a->params[k].tam + 7u) / 8u * 8u;
    }
    return total == plano->bytes_pilha;
}

static uint32_t SomaXmm(const uint32_t *hist)
{
    uint32_t soma = 0;

    for (uint32_t r = ABI_SYSV_XMM0; r <= ABI_SYSV_XMM7; r++)
        soma += hist[r];
    return soma;
}

static bool PlanoBateComGcc(const Assinatura *a, const JsonNo *sig)
{
    AbiSysvPlano plano;
    Contagem mine, gcc;
    uint32_t sobra = 0;
    bool variadica = strcmp(JsonTextoDe(sig, "tipo"), "variadica") == 0;

    if (!ObservadoDoJson(sig, &gcc) || !AbiSysvPlanejar(a->params, a->n, &a->ret, &plano))
        return false;
    ContarPlano(&plano, a->n, &mine);
    /* Na variadica o observado do json lista os 8 xmm do prototipo, nao o AL: o AL dela e conferido na AutoChecagens. */
    return CompararComGcc(&mine, &gcc, variadica, &sobra) && PlanoCoerente(&plano, a->n) && PilhaBateComRegra(a, &plano)
           && (variadica || plano.al == SomaXmm(gcc.args));
}

/* Confere AbiSysvPlanejar nas assinaturas do json; devolve quantas batem com o gcc. */
static uint32_t ConferirPlanos(const JsonNo *sigs)
{
    const JsonNo *sig;
    uint32_t batem = 0;

    for (sig = sigs->primeiro; sig; sig = sig->irmao) {
        Assinatura a;

        if (MontarAssinatura(sig, &a) && PlanoBateComGcc(&a, sig))
            batem++;
        else
            printf("PLANO DIVERGE %s\n", JsonTextoDe(sig, "nome"));
    }
    return batem;
}

/* Carrega um arquivo inteiro num buffer terminado em NUL; NULL se falhar. */
static char *LerArquivo(const char *caminho)
{
    FILE *arq = fopen(caminho, "rb");
    long tam;
    char *buf;
    size_t lidos;

    if (!arq)
        return NULL;
    if (fseek(arq, 0, SEEK_END) != 0 || (tam = ftell(arq)) < 0 || fseek(arq, 0, SEEK_SET) != 0) {
        fclose(arq);
        return NULL;
    }
    buf = malloc((size_t)tam + 1u);
    if (!buf) {
        fclose(arq);
        return NULL;
    }
    lidos = fread(buf, 1, (size_t)tam, arq);
    fclose(arq);
    buf[lidos] = '\0';
    return buf;
}

int main(int argc, char **argv)
{
    char *texto_json, *texto_c;
    JsonNo *raiz = NULL;
    const JsonNo *sigs;
    Relatorio rel;
    bool auto_ok;
    uint32_t planos_ok;

    if (argc < 3) {
        fprintf(stderr, "uso: teste_abi_sysv <abi_sysv.json> <assinaturas.c>\n");
        return 2;
    }
    texto_json = LerArquivo(argv[1]);
    texto_c = LerArquivo(argv[2]);
    if (!texto_json || !texto_c || !ParseJson(texto_json, &raiz) || !LerEstruturasC(texto_c)) {
        fprintf(stderr, "nao consegui ler ou interpretar os arquivos de entrada\n");
        return 2;
    }
    sigs = JsonCampo(raiz, "assinaturas");
    if (!sigs || JsonQuantos(sigs) != ASSINATURAS_ESPERADAS) {
        fprintf(stderr, "o json nao tem as %u assinaturas esperadas\n", ASSINATURAS_ESPERADAS);
        return 2;
    }
    ConferirTodas(sigs, &rel);
    auto_ok = AutoChecagens();
    planos_ok = ConferirPlanos(sigs);
    printf("RESULTADO: %u/%u assinaturas iguais ao gcc; veredito igual ao ok do json: %u/%u; retorno a mais no gcc (tolerado): %u\n",
           rel.iguais, rel.total, rel.concordam_json, rel.total, rel.sobra);
    printf("AUTO-CHECAGENS (AL variadica e void; fora do json): %s\n", auto_ok ? "PASSOU" : "FALHOU");
    printf("PLANO AbiSysvPlanejar: %u/%u iguais ao gcc (registradores, pilha, classe, AL e bytes de pilha)\n", planos_ok, ASSINATURAS_ESPERADAS);
    free(texto_json);
    free(texto_c);
    return (rel.iguais == ASSINATURAS_ESPERADAS && rel.total == ASSINATURAS_ESPERADAS && auto_ok && planos_ok == ASSINATURAS_ESPERADAS) ? 0 : 1;
}
