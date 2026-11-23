/* ar_ler.c - leitor minimo de arquivo ar: membros e nomes (P19b do M3).
 * Versao: 0.3.0
 * Changelog: 0.3.0 - M5b P35: ArEhElf e ArRelatarMembroElf (ver ar_ler.h).
 *            0.2.1 - M3 F9b-1: decisoes extraidas de AriNomeLongo e AriAcharIndices.
 *            0.2.0 - M3b P23d: ArMembro.off e ArLerIndice (1o linker member GNU/MS em big-endian: N, N offsets, N nomes; 2o linker member MS em little-endian: M offsets de membro, N, N numeros
 *                    de membro de 16 bits base 1, N nomes). O 2o vence o 1o; sem "/" nenhum, "/SYM64/" ou indice torto = X0105.
 *            0.1.0 - M3 P19b: ArIniciar e ArProximo. Todo acesso passa por AriCabe (nada fora de bytes[0..n) e lido); o tamanho do membro vem de 10 digitos decimais.
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes)
 */
#include <string.h>

#include "nativo/obj/ar_ler.h"

#define ARI_MAGICA_ELF "\x7f" "ELF"
#define ARI_TAM_MAGICA_ELF 4u

#define ARI_ASSINATURA "!<arch>\n"
#define ARI_TAM_ASSINATURA 8u
#define ARI_TAM_CABECALHO 60u
#define ARI_TAM_NOME 16u
#define ARI_OFF_TAMANHO 48u
#define ARI_TAM_CAMPO_TAMANHO 10u
#define ARI_OFF_FIM 58u
#define ARI_INDICE_64 "/SYM64/"
#define ARI_TAM_CONTADOR 4u
#define ARI_TAM_INDICE_MS 2u

static bool AriCabe(const ArIterador *it, size_t off, size_t len)
{
    return off <= it->n && len <= it->n - off;
}

static bool AriMalformado(Diag *erro, const char *motivo, size_t pos)
{
    return DiagRelatar(erro, X0105, 0, "arquivo ar malformado no offset %zu: %s", pos, motivo);
}

bool ArIniciar(ArIterador *it, const uint8_t *bytes, size_t n, Diag *erro)
{
    *it = (ArIterador){.bytes = bytes, .n = n, .pos = ARI_TAM_ASSINATURA};
    if (n < ARI_TAM_ASSINATURA || memcmp(bytes, ARI_ASSINATURA, ARI_TAM_ASSINATURA) != 0)
        return DiagRelatar(erro, X0105, 0, "a biblioteca nao e um arquivo ar (falta a assinatura \"!<arch>\\n\")");
    return true;
}

/* Tamanho do membro: 10 digitos decimais, a direita com espacos. Falso se nao ha digito nenhum ou ha lixo no campo. */
static bool AriTamanho(const uint8_t *cab, size_t *tam)
{
    size_t valor = 0, i = 0;
    for (; i < ARI_TAM_CAMPO_TAMANHO && cab[ARI_OFF_TAMANHO + i] >= '0' && cab[ARI_OFF_TAMANHO + i] <= '9'; i++)
        valor = valor * 10 + (size_t)(cab[ARI_OFF_TAMANHO + i] - '0');
    for (size_t j = i; j < ARI_TAM_CAMPO_TAMANHO; j++) {
        if (cab[ARI_OFF_TAMANHO + j] != ' ')
            return false;
    }
    *tam = valor;
    return i > 0;
}

/* Nome comum do GNU ("nome/") ou do MSVC ("python313.dll/"): ate a barra; sem barra, ate os espacos do fim. */
static void AriNomeCurto(const uint8_t *cab, ArMembro *m)
{
    size_t n = 0;
    while (n < ARI_TAM_NOME && cab[n] != '/')
        n++;
    if (n == ARI_TAM_NOME) {
        while (n > 0 && cab[n - 1] == ' ')
            n--;
    }
    m->nome = (const char *)cab;
    m->nome_tam = n;
}

/* Os digitos decimais depois da "/" de "/<n>": o offset n do nome na tabela "//". */
static size_t AriOffsetDoNomeLongo(const uint8_t *cab)
{
    size_t off = 0;
    for (size_t i = 1; i < ARI_TAM_NOME && cab[i] >= '0' && cab[i] <= '9'; i++)
        off = off * 10 + (size_t)(cab[i] - '0');
    return off;
}

/* O fim do nome que comeca em off na tabela "//": a "/" ou "\n" seguinte, ou o fim da tabela. */
static size_t AriFimDoNomeLongo(const ArIterador *it, size_t off)
{
    size_t fim = off;
    while (fim < it->nomes_tam && it->nomes_longos[fim] != '/' && it->nomes_longos[fim] != '\n')
        fim++;
    return fim;
}

/* "/<n>": o nome comeca no offset n da tabela "//" e vai ate "/\n" (ou a "\n", ou o fim da tabela). */
static bool AriNomeLongo(const ArIterador *it, const uint8_t *cab, ArMembro *m, Diag *erro)
{
    size_t off = AriOffsetDoNomeLongo(cab);
    if (it->nomes_longos == NULL || off >= it->nomes_tam)
        return AriMalformado(erro, "nome longo sem a tabela \"//\" ou fora dela", it->pos);
    size_t fim = AriFimDoNomeLongo(it, off);
    m->nome = (const char *)it->nomes_longos + off;
    m->nome_tam = fim - off;
    return true;
}

/* "/" e "/SYM64/" (indices de simbolos) e "//" (tabela de nomes longos). */
static bool AriEspecial(const uint8_t *cab, ArTipo *tipo)
{
    if (cab[1] == ' ' || memcmp(cab, ARI_INDICE_64, sizeof ARI_INDICE_64 - 1) == 0) {
        *tipo = AR_INDICE;
        return true;
    }
    if (cab[1] == '/' && cab[2] == ' ') {
        *tipo = AR_NOMES;
        return true;
    }
    return false;
}

/* Classifica o membro pelo nome do cabecalho e resolve o nome dos membros de objeto. */
static bool AriNomear(const ArIterador *it, const uint8_t *cab, ArMembro *m, Diag *erro)
{
    if (cab[0] != '/') {
        m->tipo = AR_OBJETO;
        AriNomeCurto(cab, m);
        return true;
    }
    if (AriEspecial(cab, &m->tipo))
        return true;
    m->tipo = AR_OBJETO;
    if (cab[1] < '0' || cab[1] > '9')
        return AriMalformado(erro, "nome de membro que comeca com '/' e nao e indice, \"//\" nem \"/<n>\"", it->pos);
    return AriNomeLongo(it, cab, m, erro);
}

/* Cabecalho do membro na posicao atual: marca final, tamanho e corpo dentro do arquivo. */
static bool AriConferirCabecalho(const ArIterador *it, const uint8_t *cab, size_t *tam, Diag *erro)
{
    if (!AriCabe(it, it->pos, ARI_TAM_CABECALHO))
        return AriMalformado(erro, "cabecalho de membro truncado", it->pos);
    if (cab[ARI_OFF_FIM] != '`' || cab[ARI_OFF_FIM + 1] != '\n' || !AriTamanho(cab, tam))
        return AriMalformado(erro, "cabecalho de membro invalido (tamanho ou marca final)", it->pos);
    if (!AriCabe(it, it->pos + ARI_TAM_CABECALHO, *tam))
        return AriMalformado(erro, "membro maior que o arquivo", it->pos);
    return true;
}

bool ArProximo(ArIterador *it, ArMembro *membro, Diag *erro)
{
    size_t tam = 0;
    if (it->pos >= it->n)
        return false;
    const uint8_t *cab = it->bytes + it->pos;
    if (!AriConferirCabecalho(it, cab, &tam, erro))
        return false;
    *membro = (ArMembro){.dados = cab + ARI_TAM_CABECALHO, .tam = tam, .off = it->pos};
    if (!AriNomear(it, cab, membro, erro))
        return false;
    if (membro->tipo == AR_NOMES) {
        it->nomes_longos = membro->dados;
        it->nomes_tam = tam;
    }
    it->pos += ARI_TAM_CABECALHO + tam + (tam & 1u);
    return true;
}

/* ---------- indice de simbolos ---------- */

static uint32_t AriLe32Be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint32_t AriLe32Le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t AriLe16Le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}

/* O nome do simbolo comeca em m->dados[*pos] e vai ate o NUL; *pos passa para depois dele. Falso se o NUL nao existe (quem chama relata o X0105). */
static bool AriEmpurrarNome(const ArMembro *m, size_t *pos, uint32_t membro_off, Vec *simbolos)
{
    const uint8_t *ini = m->dados + *pos;
    const uint8_t *fim = memchr(ini, 0, m->tam - *pos);
    if (fim == NULL)
        return false;
    ArSimbolo *s = VecEmpurrar(simbolos);
    s->nome = (const char *)ini;
    s->nome_tam = (size_t)(fim - ini);
    s->membro_off = membro_off;
    *pos += s->nome_tam + 1;
    return true;
}

/* 1o linker member (GNU e MS): N, N offsets de membro (um por simbolo) e N nomes, tudo big-endian. Vazio (0 bytes) = nenhum simbolo. */
static bool AriIndiceBe(const ArMembro *m, Vec *simbolos, Diag *erro)
{
    if (m->tam == 0)
        return true;
    if (m->tam < ARI_TAM_CONTADOR)
        return AriMalformado(erro, "indice de simbolos menor que o contador", m->off);
    uint32_t qtd = AriLe32Be(m->dados);
    size_t pos = ARI_TAM_CONTADOR + (size_t)qtd * ARI_TAM_CONTADOR;
    if (pos > m->tam)
        return AriMalformado(erro, "indice de simbolos truncado (offsets)", m->off);
    for (uint32_t i = 0; i < qtd; i++) {
        uint32_t off = AriLe32Be(m->dados + ARI_TAM_CONTADOR + (size_t)i * ARI_TAM_CONTADOR);
        if (!AriEmpurrarNome(m, &pos, off, simbolos))
            return AriMalformado(erro, "nome de simbolo do indice sem o NUL final", m->off);
    }
    return true;
}

/* Posicoes do 2o linker member: tabela de offsets de membro, N, tabela de numeros de membro (16 bits) e nomes. */
typedef struct {
    uint32_t membros;
    uint32_t simbolos;
    size_t pos_numeros;
    size_t pos_nomes;
} AriMs2;

static bool AriMs2Ler(const ArMembro *m, AriMs2 *t, Diag *erro)
{
    if (m->tam < ARI_TAM_CONTADOR)
        return AriMalformado(erro, "2o indice de simbolos menor que o contador", m->off);
    t->membros = AriLe32Le(m->dados);
    size_t pos_qtd = ARI_TAM_CONTADOR + (size_t)t->membros * ARI_TAM_CONTADOR;
    if (pos_qtd > m->tam || m->tam - pos_qtd < ARI_TAM_CONTADOR)
        return AriMalformado(erro, "2o indice de simbolos truncado (offsets dos membros)", m->off);
    t->simbolos = AriLe32Le(m->dados + pos_qtd);
    t->pos_numeros = pos_qtd + ARI_TAM_CONTADOR;
    t->pos_nomes = t->pos_numeros + (size_t)t->simbolos * ARI_TAM_INDICE_MS;
    if (t->pos_nomes > m->tam)
        return AriMalformado(erro, "2o indice de simbolos truncado (numeros de membro)", m->off);
    return true;
}

/* 2o linker member (MS, little-endian): o membro do simbolo i e o de numero base 1 gravado em pos_numeros + 2i, que indexa a tabela de offsets. */
static bool AriIndiceMs2(const ArMembro *m, Vec *simbolos, Diag *erro)
{
    AriMs2 t = {0};
    if (!AriMs2Ler(m, &t, erro))
        return false;
    size_t pos = t.pos_nomes;
    for (uint32_t i = 0; i < t.simbolos; i++) {
        uint32_t numero = AriLe16Le(m->dados + t.pos_numeros + (size_t)i * ARI_TAM_INDICE_MS);
        if (numero == 0 || numero > t.membros)
            return AriMalformado(erro, "2o indice de simbolos com numero de membro fora da tabela", m->off);
        uint32_t off = AriLe32Le(m->dados + ARI_TAM_CONTADOR + (size_t)(numero - 1) * ARI_TAM_CONTADOR);
        if (!AriEmpurrarNome(m, &pos, off, simbolos))
            return AriMalformado(erro, "nome de simbolo do 2o indice sem o NUL final", m->off);
    }
    return true;
}

/* Os dois primeiros "/" do arquivo (antes do primeiro objeto); dados fica NULL no que nao existe. */
typedef struct {
    ArMembro primeiro;
    ArMembro segundo;
} AriIndices;

/* Guarda o membro de indice no 1o lugar livre (os dois primeiros valem; os outros sao ignorados). */
static void AriGuardarIndice(AriIndices *achados, const ArMembro *m)
{
    if (achados->primeiro.dados == NULL)
        achados->primeiro = *m;
    else if (achados->segundo.dados == NULL)
        achados->segundo = *m;
}

static bool AriAcharIndices(const uint8_t *bytes, size_t n, AriIndices *achados, Diag *erro)
{
    ArIterador it;
    ArMembro m;
    if (!ArIniciar(&it, bytes, n, erro))
        return false;
    while (ArProximo(&it, &m, erro) && m.tipo != AR_OBJETO) {
        if (m.tipo != AR_INDICE)
            continue;
        AriGuardarIndice(achados, &m);
    }
    return !erro->tem;
}

bool ArEhElf(const uint8_t *dados, size_t n)
{
    return n >= ARI_TAM_MAGICA_ELF && memcmp(dados, ARI_MAGICA_ELF, ARI_TAM_MAGICA_ELF) == 0;
}

bool ArRelatarMembroElf(Diag *erro, const char *membro, const ElfLErro *e)
{
    if (e->codigo == ELFL_RELOC_FORA_DO_CENSO)
        return DiagRelatar(erro, X0110, 0, "biblioteca estatica, membro '%s': relocacao de tipo %u (%s) fora do censo do ligador nativo (o libc.a ja usa so o censo)", membro,
                           (unsigned)e->tipo_reloc, ElfLNomeReloc(e->tipo_reloc));
    return DiagRelatar(erro, X0105, 0, "biblioteca estatica, membro '%s': %s", membro, e->mensagem);
}

bool ArLerIndice(const uint8_t *bytes, size_t n, Vec *simbolos, Diag *erro)
{
    AriIndices achados = {{0}, {0}};
    VecIniciar(simbolos, sizeof(ArSimbolo));
    if (!AriAcharIndices(bytes, n, &achados, erro))
        return false;
    if (achados.primeiro.dados == NULL)
        return DiagRelatar(erro, X0105, 0, "a biblioteca estatica nao tem indice de simbolos (falta rodar 'ar s' ou ranlib); sem ele nao ha como escolher os membros");
    if (memcmp(bytes + achados.primeiro.off, ARI_INDICE_64, sizeof ARI_INDICE_64 - 1) == 0)
        return AriMalformado(erro, "indice \"/SYM64/\" (offsets de 64 bits) nao e suportado", achados.primeiro.off);
    return achados.segundo.dados != NULL ? AriIndiceMs2(&achados.segundo, simbolos, erro) : AriIndiceBe(&achados.primeiro, simbolos, erro);
}
