/* coff_ler.c - leitor de objeto COFF AMD64 (.obj do gcc/MinGW) para o Objeto do backend nativo; alimenta o ligador do P5.
 * Versao: 0.4.0
 * Changelog: 0.4.0 - M3b P23e: constantes, CoffLeitor e as pecas de leitura vao para coff_interno.h (o leitor de membros de terceiros as usa); tabela de relocacoes ganha REL32_1..5 (delta no adendo),
 *                    SECTION e SECREL; IMAGE_REL_AMD64_ABSOLUTE e ignorada; secao "<base>.<sufixo>" (.text.unlikely, .xdata.unlikely) tem o tipo da base.
 *            0.3.1 - M3 F9b-1: decisoes extraidas de CoffListarIndefinidos.
 *            0.3.0 - M3b P23d: CoffListarIndefinidos (varre so a tabela de simbolos; reaproveita o cabecalho, a tabela de strings e CoffNomeSimbolo do CoffLer).
 *            0.2.0 - M2 P17c: secao de grupo "<base>$<sufixo>" (.text$f, .rdata$t, .data$v, .pdata$f, .xdata$f: -ffunction-sections / -fdata-sections do gcc)
 *                    deixa de ser recusada; .pdata$* e .xdata$* ganham o tipo de .pdata e .xdata. Quem junta pelo nome-base e o ligador.
 *            0.1.1 - M2 P9a: CoffDefinirSimbolo recebe CoffSimboloLido (regra de <= 4 parametros).
 *            0.1.0 - backend x64 P5: CoffLer (cabecalho 0x8664, nomes longos "/n" e tabela de strings, simbolos auxiliares pulados,
 *                    secoes 0/-1/-2, BSS pelo SizeOfRawData, alinhamento dos bits 20-23, adendo REL32/ADDR32NB/ADDR64 lido e zerado).
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes) e python -I ..\tests\nativo\obj\verificar_obj.py
 */
#include <string.h>

#include "nativo/obj/coff.h"
#include "nativo/obj/coff_interno.h"

#define COFF_RELOC_ABSOLUTA 0x0000u

static const CoffRelocSuportada COFF_RELOCS[] = {
    {0x0001, OR_ADDR64, 8, 0},
    {0x0003, OR_ADDR32NB, 4, 0},
    {0x0004, OR_REL32, 4, 0},
    {0x0005, OR_REL32, 4, -1},
    {0x0006, OR_REL32, 4, -2},
    {0x0007, OR_REL32, 4, -3},
    {0x0008, OR_REL32, 4, -4},
    {0x0009, OR_REL32, 4, -5},
    {0x000A, OR_SECTION, 2, 0},
    {0x000B, OR_SECREL, 4, 0},
};

bool CoffCabe(size_t n, size_t off, size_t tam)
{
    return off <= n && tam <= n - off;
}

/* Le n bytes little-endian (n <= 8). Quem chama confere CoffCabe antes. */
uint64_t CoffLe(const uint8_t *p, uint32_t n)
{
    uint64_t v = 0;
    for (uint32_t k = 0; k < n; k++)
        v |= (uint64_t)p[k] << (k * COFF_BITS_BYTE);
    return v;
}

bool CoffFalha(Diag *erro, const char *motivo)
{
    return DiagRelatar(erro, X0903, 0, "COFF: %s", motivo);
}

/* Texto da tabela de strings no offset dado (o offset conta os 4 bytes do tamanho); NULL se nao acha o NUL. */
static const char *CoffTextoStrtab(const CoffLeitor *L, uint64_t off)
{
    if (off >= L->tam_strtab)
        return NULL;
    const uint8_t *p = L->b + L->ini_strtab + off;
    if (memchr(p, 0, L->tam_strtab - off) == NULL)
        return NULL;
    return (const char *)p;
}

/* Nome de 8 bytes sem tabela: copia ate o NUL ou ate 8 caracteres. */
static const char *CoffNomeEmLinha(const uint8_t *campo, char buf[COFF_TAM_NOME + 1])
{
    size_t len = 0;
    while (len < COFF_TAM_NOME && campo[len] != 0)
        len++;
    memcpy(buf, campo, len);
    buf[len] = '\0';
    return buf;
}

/* Secao: "/N" aponta para a tabela de strings (N em decimal); senao o nome vem em linha. */
const char *CoffNomeSecao(const CoffLeitor *L, const uint8_t *campo, char buf[COFF_TAM_NOME + 1])
{
    if (campo[0] != '/')
        return CoffNomeEmLinha(campo, buf);
    uint64_t off = 0;
    for (uint32_t k = 1; k < COFF_TAM_NOME && campo[k] >= '0' && campo[k] <= '9'; k++)
        off = off * 10 + (uint64_t)(campo[k] - '0');
    return CoffTextoStrtab(L, off);
}

/* Simbolo: quatro bytes zero no inicio marcam nome na tabela de strings, no offset que vem logo depois. */
const char *CoffNomeSimbolo(const CoffLeitor *L, const uint8_t *campo, char buf[COFF_TAM_NOME + 1])
{
    if (CoffLe(campo, COFF_TAM_CAMPO) != 0)
        return CoffNomeEmLinha(campo, buf);
    return CoffTextoStrtab(L, CoffLe(campo + COFF_TAM_CAMPO, COFF_TAM_CAMPO));
}

/* Alinhamento da secao: bits 20-23 valem k para 2^(k-1); zero quer dizer o padrao do COFF. */
uint32_t CoffAlinhamento(uint32_t carac)
{
    uint32_t k = (carac & COFF_CARAC_ALINHA_MASCARA) >> COFF_CARAC_ALINHA_SHIFT;
    if (k == 0)
        return COFF_ALINHA_PADRAO;
    return 1u << (k - 1);
}

/* O nome e a base, "base$sufixo" (o gcc cria um grupo por funcao e por dado com -ffunction-sections e -fdata-sections) ou "base.sufixo" (.text.unlikely). */
static bool CoffEhSecao(const char *nome, const char *base)
{
    size_t n = strlen(base);
    return strncmp(nome, base, n) == 0 && (nome[n] == '\0' || nome[n] == '$' || nome[n] == '.');
}

/* Tipo do Objeto pelo nome e pelas caracteristicas: .pdata e .xdata tem tipo proprio, o resto sai pelas flags. */
static ObjTipoSecao CoffTipoPorNome(const char *nome)
{
    if (CoffEhSecao(nome, ".pdata"))
        return OS_PDATA;
    if (CoffEhSecao(nome, ".xdata"))
        return OS_XDATA;
    return OS_RDATA;
}

ObjTipoSecao CoffTipo(const char *nome, uint32_t carac)
{
    if (carac & COFF_CARAC_CODIGO)
        return OS_TEXTO;
    if (carac & COFF_CARAC_BSS)
        return OS_BSS;
    if (carac & COFF_CARAC_GRAVAVEL)
        return OS_DATA;
    return CoffTipoPorNome(nome);
}

const uint8_t *CoffCabecalhoSecao(const CoffLeitor *L, uint32_t i)
{
    return L->b + L->ini_secoes + (size_t)i * COFF_TAM_SECAO;
}

/* BSS nao tem bytes no arquivo: o tamanho vem do SizeOfRawData (o gcc o grava assim) ou do VirtualSize. */
bool CoffPreencherSecao(CoffLeitor *L, uint32_t idx, const uint8_t *h)
{
    ObjSecao *s = VecEm(&L->o->secoes, idx);
    uint32_t vsz = (uint32_t)CoffLe(h + COFF_OFF_SEC_VSZ, COFF_TAM_CAMPO);
    uint32_t raw = (uint32_t)CoffLe(h + COFF_OFF_SEC_RAW, COFF_TAM_CAMPO);
    uint32_t ptr = (uint32_t)CoffLe(h + COFF_OFF_SEC_PTR, COFF_TAM_CAMPO);
    if (s->tipo == OS_BSS) {
        s->tam_bss = raw > vsz ? raw : vsz;
        return true;
    }
    if (raw == 0)
        return true;
    if (!CoffCabe(L->n, ptr, raw))
        return DiagRelatar(L->erro, X0903, 0, "COFF: dados da secao '%s' fora do arquivo", s->nome);
    BufAnexarN(&s->bytes, (const char *)(L->b + ptr), raw);
    return true;
}

static bool CoffAbrirSecao(CoffLeitor *L, uint32_t i)
{
    const uint8_t *h = CoffCabecalhoSecao(L, i);
    char buf[COFF_TAM_NOME + 1];
    const char *nome = CoffNomeSecao(L, h, buf);
    if (nome == NULL)
        return CoffFalha(L->erro, "nome de secao fora da tabela de strings");
    uint32_t carac = (uint32_t)CoffLe(h + COFF_OFF_SEC_CARAC, COFF_TAM_CAMPO);
    uint32_t idx = ObjSecaoAbrir(L->o, nome, CoffTipo(nome, carac), CoffAlinhamento(carac));
    if (idx != i)
        FalhaInterna("coff: secao %u abriu no indice %u", i, idx);
    return CoffPreencherSecao(L, idx, h);
}

static bool CoffLerSecoes(CoffLeitor *L)
{
    for (uint32_t i = 0; i < L->nsec; i++) {
        if (!CoffAbrirSecao(L, i))
            return false;
    }
    return true;
}

/* Relocacao: 10 bytes (VirtualAddress, indice do simbolo COFF, tipo). Tipo fora da tabela e X0903. */
bool CoffTipoReloc(uint16_t codigo, CoffRelocSuportada *achado)
{
    for (size_t k = 0; k < sizeof COFF_RELOCS / sizeof COFF_RELOCS[0]; k++) {
        if (COFF_RELOCS[k].codigo == codigo) {
            *achado = COFF_RELOCS[k];
            return true;
        }
    }
    return false;
}

/* Adendo: o valor que ja esta no campo (com sinal). A relocacao leva esse valor e o campo sai zerado. */
int64_t CoffAdendo(const uint8_t *p, uint32_t tam)
{
    uint64_t v = CoffLe(p, tam);
    if (tam == COFF_TAM_CAMPO)
        return (int32_t)(uint32_t)v;
    return (int64_t)v;
}

static bool CoffSimboloDaReloc(const CoffLeitor *L, uint32_t simb, uint32_t *alvo)
{
    if (simb >= L->mapa.n)
        return false;
    *alvo = *(const uint32_t *)VecEm(&L->mapa, simb);
    return *alvo != COFF_SIMBOLO_NAO_MAPEADO;
}

static bool CoffLerReloc(CoffLeitor *L, uint32_t sec, const uint8_t *r)
{
    uint32_t va = (uint32_t)CoffLe(r, COFF_TAM_CAMPO);
    uint32_t simb = (uint32_t)CoffLe(r + COFF_TAM_CAMPO, COFF_TAM_CAMPO);
    uint16_t codigo = (uint16_t)CoffLe(r + 2 * COFF_TAM_CAMPO, 2);
    CoffRelocSuportada tipo;
    uint32_t alvo;
    ObjSecao *s = VecEm(&L->o->secoes, sec);
    if (codigo == COFF_RELOC_ABSOLUTA)
        return true;
    if (!CoffTipoReloc(codigo, &tipo))
        return DiagRelatar(L->erro, X0903, 0, "COFF: relocacao de tipo 0x%x na secao '%s' (aceitas: ADDR64, ADDR32NB, REL32, REL32_1..5, SECTION, SECREL)", codigo, s->nome);
    if (!CoffSimboloDaReloc(L, simb, &alvo))
        return DiagRelatar(L->erro, X0903, 0, "COFF: relocacao na secao '%s' aponta para o simbolo %u, que nao entrou no objeto", s->nome, simb);
    if (s->tipo == OS_BSS || !CoffCabe(s->bytes.tam, va, tipo.tam))
        return DiagRelatar(L->erro, X0903, 0, "COFF: relocacao em 0x%x fora dos bytes da secao '%s'", va, s->nome);
    int64_t adendo = CoffAdendo((const uint8_t *)s->bytes.dados + va, tipo.tam);
    memset(s->bytes.dados + va, 0, tipo.tam);
    ObjRelocar(L->o, (ObjAlvoReloc){sec, va}, (ObjCorrecao){tipo.tipo, alvo, adendo + tipo.delta});
    return true;
}

static bool CoffLerRelocsDaSecao(CoffLeitor *L, uint32_t sec)
{
    const uint8_t *h = CoffCabecalhoSecao(L, sec);
    size_t ptr = (size_t)CoffLe(h + COFF_OFF_SEC_PTR_RELOC, COFF_TAM_CAMPO);
    uint32_t qtd = (uint32_t)CoffLe(h + COFF_OFF_SEC_QTD_RELOC, 2);
    if (!CoffCabe(L->n, ptr, (size_t)qtd * COFF_TAM_RELOC))
        return CoffFalha(L->erro, "tabela de relocacoes fora do arquivo");
    for (uint32_t k = 0; k < qtd; k++) {
        if (!CoffLerReloc(L, sec, L->b + ptr + (size_t)k * COFF_TAM_RELOC))
            return false;
    }
    return true;
}

static bool CoffLerRelocacoes(CoffLeitor *L)
{
    for (uint32_t i = 0; i < L->nsec; i++) {
        if (!CoffLerRelocsDaSecao(L, i))
            return false;
    }
    return true;
}

static void CoffMapear(CoffLeitor *L, uint32_t i, uint32_t idx)
{
    *(uint32_t *)VecEm(&L->mapa, i) = idx;
}

/* Um simbolo definido ja lido: i e a posicao na tabela, sec a secao COFF (1-based) e valor o offset dentro dela. */
typedef struct {
    uint32_t i;
    const char *nome;
    int16_t sec;
    uint32_t valor;
} CoffSimboloLido;

static bool CoffDefinirSimbolo(CoffLeitor *L, const CoffSimboloLido *s)
{
    if (s->sec > (int32_t)L->nsec)
        return DiagRelatar(L->erro, X0903, 0, "COFF: simbolo '%s' aponta para a secao %d, que nao existe", s->nome, s->sec);
    uint32_t idx = ObjSimboloReferir(L->o, s->nome);
    ObjSimbolo *obj = VecEm(&L->o->simbolos, idx);
    if (obj->secao != OBJ_SECAO_INDEFINIDA)
        return DiagRelatar(L->erro, X0903, 0, "COFF: simbolo '%s' definido duas vezes", s->nome);
    ObjSimboloDefinir(L->o, s->nome, (int32_t)s->sec - 1, s->valor);
    CoffMapear(L, s->i, idx);
    return true;
}

/* Um simbolo COFF: secao 0 e referencia (indefinido); -1 (absoluto) e -2 (.file e depuracao) nao entram no objeto. */
static bool CoffLerSimbolo(CoffLeitor *L, uint32_t i, const uint8_t *e)
{
    char buf[COFF_TAM_NOME + 1];
    const char *nome = CoffNomeSimbolo(L, e, buf);
    int16_t sec = (int16_t)CoffLe(e + COFF_OFF_SIM_SECAO, 2);
    if (nome == NULL)
        return CoffFalha(L->erro, "nome de simbolo fora da tabela de strings");
    if (sec == COFF_SEC_NENHUMA) {
        CoffMapear(L, i, ObjSimboloReferir(L->o, nome));
        return true;
    }
    if (sec < 0)
        return true;
    CoffSimboloLido lido = {i, nome, sec, (uint32_t)CoffLe(e + COFF_OFF_SIM_VALOR, COFF_TAM_CAMPO)};
    return CoffDefinirSimbolo(L, &lido);
}

static bool CoffLerSimbolos(CoffLeitor *L)
{
    for (uint32_t i = 0; i < L->nsim; i++) {
        VecEmpurrar(&L->mapa);
        CoffMapear(L, i, COFF_SIMBOLO_NAO_MAPEADO);
    }
    for (uint32_t i = 0; i < L->nsim;) {
        const uint8_t *e = L->b + L->ini_simbolos + (size_t)i * COFF_TAM_SIMBOLO;
        uint32_t aux = e[COFF_OFF_SIM_AUX];
        if (i + 1 + aux > L->nsim)
            return CoffFalha(L->erro, "simbolo auxiliar passa do fim da tabela");
        if (!CoffLerSimbolo(L, i, e))
            return false;
        i += 1 + aux;
    }
    return true;
}

static bool CoffLerTabelaSimbolos(CoffLeitor *L)
{
    if (!CoffCabe(L->n, L->ini_simbolos, (size_t)L->nsim * COFF_TAM_SIMBOLO))
        return CoffFalha(L->erro, "tabela de simbolos fora do arquivo");
    return CoffLerSimbolos(L);
}

/* A tabela de strings vem logo depois dos simbolos; sem ela (ou com tamanho invalido) so os nomes curtos funcionam. */
void CoffLerTabelaStrings(CoffLeitor *L)
{
    L->ini_strtab = L->ini_simbolos + (size_t)L->nsim * COFF_TAM_SIMBOLO;
    L->tam_strtab = 0;
    if (!CoffCabe(L->n, L->ini_strtab, COFF_TAM_CAMPO))
        return;
    uint32_t tam = (uint32_t)CoffLe(L->b + L->ini_strtab, COFF_TAM_CAMPO);
    if (CoffCabe(L->n, L->ini_strtab, tam))
        L->tam_strtab = tam;
}

bool CoffLerCabecalho(CoffLeitor *L)
{
    if (!CoffCabe(L->n, 0, COFF_TAM_CABECALHO))
        return CoffFalha(L->erro, "arquivo menor que o cabecalho COFF");
    if (CoffLe(L->b, 2) != COFF_MAQUINA_AMD64)
        return DiagRelatar(L->erro, X0903, 0, "COFF: maquina 0x%x, o backend x64 aceita so 0x8664", (unsigned)CoffLe(L->b, 2));
    L->nsec = (uint32_t)CoffLe(L->b + COFF_OFF_CAB_SECOES, 2);
    L->nsim = (uint32_t)CoffLe(L->b + COFF_OFF_CAB_NSIMBOLOS, COFF_TAM_CAMPO);
    L->ini_simbolos = (size_t)CoffLe(L->b + COFF_OFF_CAB_SIMBOLOS, COFF_TAM_CAMPO);
    L->ini_secoes = COFF_TAM_CABECALHO + (size_t)CoffLe(L->b + COFF_OFF_CAB_TAM_OPC, 2);
    if (!CoffCabe(L->n, L->ini_secoes, (size_t)L->nsec * COFF_TAM_SECAO))
        return CoffFalha(L->erro, "tabela de secoes fora do arquivo");
    return true;
}

bool CoffLer(const uint8_t *b, size_t n, Objeto *o, Diag *erro)
{
    CoffLeitor L = {b, n, o, erro, 0, 0, 0, 0, 0, 0, {0}};
    VecIniciar(&L.mapa, sizeof(uint32_t));
    bool ok = CoffLerCabecalho(&L);
    if (ok) {
        CoffLerTabelaStrings(&L);
        ok = CoffLerSecoes(&L) && CoffLerTabelaSimbolos(&L) && CoffLerRelocacoes(&L);
    }
    VecLiberar(&L.mapa);
    return ok;
}

/* Indefinido de verdade: externo, secao 0 e valor 0 (valor > 0 em secao 0 e simbolo comum, que e definicao). */
static bool CoffEhIndefinido(const uint8_t *e)
{
    return e[COFF_OFF_SIM_CLASSE] == COFF_CLASSE_EXTERNA && CoffLe(e + COFF_OFF_SIM_SECAO, 2) == 0 && CoffLe(e + COFF_OFF_SIM_VALOR, COFF_TAM_CAMPO) == 0;
}

static bool CoffAnotarIndefinido(const CoffLeitor *L, const uint8_t *e, const CoffDestinoNomes *destino)
{
    char buf[COFF_TAM_NOME + 1];
    const char *nome = CoffNomeSimbolo(L, e, buf);
    if (nome == NULL)
        return CoffFalha(L->erro, "nome de simbolo fora da tabela de strings");
    *(const char **)VecEmpurrar(destino->lista) = ArenaCopiarTexto(destino->arena, nome, strlen(nome));
    return true;
}

/* Varre a tabela de simbolos (pulando os auxiliares) e anota os indefinidos no destino. */
static bool CoffAnotarIndefinidos(const CoffLeitor *L, const CoffDestinoNomes *destino)
{
    for (uint32_t i = 0; i < L->nsim;) {
        const uint8_t *e = L->b + L->ini_simbolos + (size_t)i * COFF_TAM_SIMBOLO;
        uint32_t aux = e[COFF_OFF_SIM_AUX];
        if (i + 1 + aux > L->nsim)
            return CoffFalha(L->erro, "simbolo auxiliar passa do fim da tabela");
        if (CoffEhIndefinido(e) && !CoffAnotarIndefinido(L, e, destino))
            return false;
        i += 1 + aux;
    }
    return true;
}

bool CoffListarIndefinidos(const uint8_t *b, size_t n, const CoffDestinoNomes *destino, Diag *erro)
{
    CoffLeitor L = {b, n, NULL, erro, 0, 0, 0, 0, 0, 0, {0}};
    if (!CoffLerCabecalho(&L))
        return false;
    CoffLerTabelaStrings(&L);
    if (!CoffCabe(L.n, L.ini_simbolos, (size_t)L.nsim * COFF_TAM_SIMBOLO))
        return CoffFalha(erro, "tabela de simbolos fora do arquivo");
    return CoffAnotarIndefinidos(&L, destino);
}
