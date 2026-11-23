/* teste_terceiros.c - verificacoes de unidade da uniao de membros COFF de terceiros (TerceirosMontar, chamada pelo teste_nativo): 5 verificacoes do P23e. Os membros COFF sao montados
 * na mao aqui (secoes, simbolos com auxiliar, relocacoes, .drectve), sem gcc.
 * Versao: 0.4.0
 * Changelog: 0.4.0 - M6d P92c: +2 verificacoes (a familia malloc de lib contada vai para forja_libmem_<fn>_<slot>, definido e nao contado ficam intocados, tabela FLM1; sem libmem o objeto nao muda).
 *            0.3.0 - M3b P23y-2: +5 verificacoes (COMDAT com 2 externos repetido, EXACT_MATCH com alvo diferente, EXACT_MATCH com adendo, LARGEST anula as associadas do lider antigo, -aligncomm que nao cabe em 32 bits).
 *            0.2.0 - M3b P23f: +2 verificacoes (a tabela de construtores .CRT$XC* e .ctors na ordem de execucao, com forja_ctors_ini/forja_ctors_fim; secao .tls de membro = X0107).
 *            0.1.0 - M3b P23e: COMDAT (ANY fica com um, LARGEST com o maior, NODUPLICATES = X0106); secoes ".text$x" ordenadas pelo sufixo; WEAK_EXTERNAL (a forte de outro membro vence,
 *                    sem ela vale o padrao); simbolo comum (maior tamanho, -aligncomm); .drectve (defaultlib, -l, export, aviso do resto), tipos de relocacao e relocacao desconhecida = X0903.
 * Verificacao: (em language\compiler) construir.bat && teste_nativo.exe
 */
#include <stdio.h>
#include <string.h>

#include "common/arena.h"
#include "common/buf.h"
#include "common/diag.h"
#include "nativo/obj/coff_membro.h"
#include "nativo/obj/estatica.h"
#include "nativo/obj/objeto.h"
#include "nativo/obj/terceiros.h"

#define MAQUINA_AMD64 0x8664u
#define TAM_COFF 20u
#define TAM_SECAO 40u
#define TAM_SIMBOLO 18u
#define TAM_RELOC 10u
#define TAM_NOME 8u
#define CARAC_TEXTO 0x60100020u   /* codigo, executavel, leitura, alinhamento de 1 byte */
#define CARAC_COMDAT 0x00001000u
#define CARAC_DIRETIVA 0x00100A00u
#define CLASSE_EXTERNA 2u
#define CLASSE_ESTATICA 3u
#define CLASSE_FRACA 105u
#define SEL_NODUP 1u
#define SEL_QUALQUER 2u
#define SEL_MAIOR 6u
#define MAX_MEMBROS 3u

typedef struct {
    int *total;
    int *falhas;
} Contagem;

static void Confere(const Contagem *c, bool ok, const char *texto, int linha)
{
    (*c->total)++;
    if (ok)
        return;
    (*c->falhas)++;
    fprintf(stderr, "FALHOU (terceiros) linha %d: %s\n", linha, texto);
}

#define TERCEIROS_VERIFICA(c, cond) Confere((c), (cond), #cond, __LINE__)

/* ---------- montagem de COFF, na mao ---------- */

typedef struct { uint32_t va; uint32_t simbolo; uint16_t tipo; } RelT;
typedef struct { const char *nome; uint32_t carac; const char *dados; uint32_t tam; const RelT *rels; uint32_t nrels; } SecT;
typedef enum { AUX_NENHUM, AUX_SECAO, AUX_FRACO } AuxT;
/* a e b: AUX_SECAO = selecao do COMDAT e secao associada; AUX_FRACO = indice COFF do padrao. */
typedef struct { const char *nome; uint32_t valor; int16_t sec; uint8_t classe; AuxT aux; uint32_t a; uint32_t b; } SimT;
typedef struct { const SecT *secs; uint32_t nsec; const SimT *sims; uint32_t nsim; } MembroT;

static void Anexar16(Buf *b, uint32_t v)
{
    const char dois[2] = {(char)(v & 0xFFu), (char)((v >> 8) & 0xFFu)};

    BufAnexarN(b, dois, sizeof dois);
}

static void Anexar32(Buf *b, uint32_t v)
{
    Anexar16(b, v & 0xFFFFu);
    Anexar16(b, v >> 16);
}

static void AnexarNome(Buf *b, const char *nome)
{
    char campo[TAM_NOME] = {0};

    memcpy(campo, nome, strlen(nome));
    BufAnexarN(b, campo, sizeof campo);
}

static uint32_t TamCorpoSecoes(const MembroT *m)
{
    uint32_t tam = 0;

    for (uint32_t k = 0; k < m->nsec; k++)
        tam += m->secs[k].tam + TAM_RELOC * m->secs[k].nrels;
    return tam;
}

static uint32_t SlotsDeSimbolos(const MembroT *m)
{
    uint32_t n = 0;

    for (uint32_t k = 0; k < m->nsim; k++)
        n += m->sims[k].aux == AUX_NENHUM ? 1u : 2u;
    return n;
}

static void AnexarCabecalhoSecao(Buf *out, const SecT *s, uint32_t pos)
{
    AnexarNome(out, s->nome);
    Anexar32(out, 0);
    Anexar32(out, 0);
    Anexar32(out, s->tam);
    Anexar32(out, s->tam > 0 ? pos : 0);
    Anexar32(out, s->nrels > 0 ? pos + s->tam : 0);
    Anexar32(out, 0);
    Anexar16(out, s->nrels);
    Anexar16(out, 0);
    Anexar32(out, s->carac);
}

static void AnexarCorpo(Buf *out, const SecT *s)
{
    if (s->tam > 0)
        BufAnexarN(out, s->dados, s->tam);
    for (uint32_t r = 0; r < s->nrels; r++) {
        Anexar32(out, s->rels[r].va);
        Anexar32(out, s->rels[r].simbolo);
        Anexar16(out, s->rels[r].tipo);
    }
}

static void AnexarAuxiliar(Buf *out, const SimT *s)
{
    char zeros[TAM_SIMBOLO] = {0};

    if (s->aux == AUX_SECAO) {
        zeros[12] = (char)(s->b & 0xFFu);
        zeros[14] = (char)s->a;
    } else {
        zeros[0] = (char)(s->a & 0xFFu);
        zeros[4] = 3;
    }
    BufAnexarN(out, zeros, sizeof zeros);
}

/* Nome com mais de 8 bytes vai para a tabela de strings (zeros + offset a partir do inicio dela, que tem 4 bytes de tamanho). */
static void AnexarSimbolo(Buf *out, const SimT *s, Buf *longos)
{
    if (strlen(s->nome) > TAM_NOME) {
        Anexar32(out, 0);
        Anexar32(out, 4u + (uint32_t)longos->tam);
        BufAnexarN(longos, s->nome, strlen(s->nome) + 1);
    } else {
        AnexarNome(out, s->nome);
    }
    Anexar32(out, s->valor);
    Anexar16(out, (uint32_t)(uint16_t)s->sec);
    Anexar16(out, 0);
    BufAnexarN(out, &(char){(char)s->classe}, 1);
    BufAnexarN(out, &(char){s->aux == AUX_NENHUM ? 0 : 1}, 1);
    if (s->aux != AUX_NENHUM)
        AnexarAuxiliar(out, s);
}

/* Cabecalho + secoes (cabecalho, dados e relocacoes) + simbolos + tabela de strings vazia. Nomes de ate 8 caracteres. */
static void MontarCoff(Buf *out, const MembroT *m)
{
    uint32_t pos = TAM_COFF + TAM_SECAO * m->nsec;

    Anexar16(out, MAQUINA_AMD64);
    Anexar16(out, m->nsec);
    Anexar32(out, 0);
    Anexar32(out, pos + TamCorpoSecoes(m));
    Anexar32(out, SlotsDeSimbolos(m));
    Anexar16(out, 0);
    Anexar16(out, 0);
    for (uint32_t k = 0; k < m->nsec; k++) {
        AnexarCabecalhoSecao(out, &m->secs[k], pos);
        pos += m->secs[k].tam + TAM_RELOC * m->secs[k].nrels;
    }
    for (uint32_t k = 0; k < m->nsec; k++)
        AnexarCorpo(out, &m->secs[k]);
    Buf longos = {0};

    for (uint32_t k = 0; k < m->nsim; k++)
        AnexarSimbolo(out, &m->sims[k], &longos);
    Anexar32(out, 4u + (uint32_t)longos.tam);
    if (longos.tam > 0)
        BufAnexarN(out, longos.dados, longos.tam);
    BufLiberar(&longos);
}

typedef struct {
    Objeto o;
    Diag erro;
    TerceirosRelato relato;
    Vec exportados;
    bool ok;
} Resultado;

static const char *const NOMES_MEMBROS[MAX_MEMBROS] = {"m0.o", "m1.o", "m2.o"};

/* arquivos[k] = EstaticaMembro.arquivo do membro k (NULL = todos 0); lm = TerceirosPedido.libmem (NULL = sem contabilidade). */
static void MontarCom(const MembroT *ms, uint32_t n, const uint32_t *arquivos, const TerceirosLibmem *lm, Resultado *r)
{
    Buf bytes[MAX_MEMBROS] = {{0}};
    Vec membros;
    TerceirosPedido ped = {0};

    *r = (Resultado){0};
    VecIniciar(&membros, sizeof(EstaticaMembro));
    VecIniciar(&r->exportados, sizeof(const char *));
    r->relato.exportados = &r->exportados;
    for (uint32_t k = 0; k < n; k++) {
        MontarCoff(&bytes[k], &ms[k]);
        *(EstaticaMembro *)VecEmpurrar(&membros) = (EstaticaMembro){arquivos != NULL ? arquivos[k] : 0, NOMES_MEMBROS[k], (const uint8_t *)bytes[k].dados, bytes[k].tam};
    }
    ObjIniciar(&r->o);
    ped.membros = &membros;
    ped.libmem = lm;
    r->ok = TerceirosMontar(&ped, &r->o, &r->relato, &r->erro);
    VecLiberar(&membros);
    for (uint32_t k = 0; k < n; k++)
        BufLiberar(&bytes[k]);
}

static void Montar(const MembroT *ms, uint32_t n, Resultado *r)
{
    MontarCom(ms, n, NULL, NULL, r);
}

static void Liberar(Resultado *r)
{
    ObjLiberar(&r->o);
    VecLiberar(&r->exportados);
}

/* ---------- consultas ao Objeto montado ---------- */

static const ObjSimbolo *Simbolo(const Resultado *r, const char *nome)
{
    int64_t i = ObjAcharSimbolo(&r->o, nome);

    return i < 0 ? NULL : VecEm(&r->o.simbolos, (size_t)i);
}

static const ObjSecao *SecaoDoSimbolo(const Resultado *r, const char *nome)
{
    const ObjSimbolo *s = Simbolo(r, nome);

    return s != NULL && s->secao >= 0 ? VecEm(&r->o.secoes, (size_t)s->secao) : NULL;
}

static uint32_t SecoesChamadas(const Resultado *r, const char *nome)
{
    uint32_t n = 0;

    for (size_t k = 0; k < r->o.secoes.n; k++)
        n += strcmp(((const ObjSecao *)VecEm(&r->o.secoes, k))->nome, nome) == 0 ? 1u : 0u;
    return n;
}

static size_t BytesDasSecoes(const Resultado *r, const char *nome)
{
    size_t tam = 0;

    for (size_t k = 0; k < r->o.secoes.n; k++) {
        const ObjSecao *s = VecEm(&r->o.secoes, k);

        tam += strcmp(s->nome, nome) == 0 ? s->bytes.tam : 0;
    }
    return tam;
}

/* ---------- 1: COMDAT ---------- */

static const SecT SEC_F4 = {".text$f", CARAC_TEXTO | CARAC_COMDAT, "\x90\x90\x90\xC3", 4, NULL, 0};
static const SecT SEC_F8 = {".text$f", CARAC_TEXTO | CARAC_COMDAT, "\x90\x90\x90\x90\x90\x90\x90\xC3", 8, NULL, 0};

#define SIMS_F(sel) {{".text$f", 0, 1, CLASSE_ESTATICA, AUX_SECAO, (sel), 0}, {"f", 0, 1, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}}

static const SimT SIMS_F_QUALQUER[2] = SIMS_F(SEL_QUALQUER);
static const SimT SIMS_F_MAIOR[2] = SIMS_F(SEL_MAIOR);
static const SimT SIMS_F_NODUP[2] = SIMS_F(SEL_NODUP);

static bool ComdatQualquerFicaComUm(void)
{
    const MembroT ms[2] = {{&SEC_F4, 1, SIMS_F_QUALQUER, 2}, {&SEC_F4, 1, SIMS_F_QUALQUER, 2}};
    Resultado r;
    bool ok;

    Montar(ms, 2, &r);
    ok = r.ok && SecoesChamadas(&r, ".text$f") == 2 && BytesDasSecoes(&r, ".text$f") == 4;
    ok = ok && SecaoDoSimbolo(&r, "f") != NULL && SecaoDoSimbolo(&r, "f")->bytes.tam == 4;
    Liberar(&r);
    return ok;
}

static bool ComdatMaiorVenceENodupRecusa(void)
{
    const MembroT maior[2] = {{&SEC_F4, 1, SIMS_F_MAIOR, 2}, {&SEC_F8, 1, SIMS_F_MAIOR, 2}};
    const MembroT nodup[2] = {{&SEC_F4, 1, SIMS_F_NODUP, 2}, {&SEC_F4, 1, SIMS_F_NODUP, 2}};
    Resultado r;
    bool ok;

    Montar(maior, 2, &r);
    ok = r.ok && BytesDasSecoes(&r, ".text$f") == 8 && SecaoDoSimbolo(&r, "f") != NULL && SecaoDoSimbolo(&r, "f")->bytes.tam == 8;
    Liberar(&r);
    Montar(nodup, 2, &r);
    ok = ok && !r.ok && r.erro.codigo == X0106 && strstr(r.erro.causa, "'f'") != NULL && strstr(r.erro.causa, "m1.o") != NULL;
    Liberar(&r);
    return ok;
}

/* ---------- 2: secoes com '$' ordenadas pelo sufixo ---------- */

static bool SecoesComCifraveOrdenadas(void)
{
    const SecT secs[3] = {{".text$b", CARAC_TEXTO, "B", 1, NULL, 0}, {".text$a", CARAC_TEXTO, "A", 1, NULL, 0}, {".text", CARAC_TEXTO, "X", 1, NULL, 0}};
    const SimT sims[3] = {{"fb", 0, 1, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}, {"fa", 0, 2, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}, {"fx", 0, 3, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}};
    const MembroT ms[1] = {{secs, 3, sims, 3}};
    Resultado r;
    bool ok;

    Montar(ms, 1, &r);
    ok = r.ok && r.o.secoes.n == 3;
    ok = ok && strcmp(((const ObjSecao *)VecEm(&r.o.secoes, 0))->nome, ".text") == 0 && strcmp(((const ObjSecao *)VecEm(&r.o.secoes, 1))->nome, ".text$a") == 0;
    ok = ok && strcmp(((const ObjSecao *)VecEm(&r.o.secoes, 2))->nome, ".text$b") == 0;
    ok = ok && Simbolo(&r, "fx")->secao == 0 && Simbolo(&r, "fa")->secao == 1 && Simbolo(&r, "fb")->secao == 2;
    ok = ok && SecaoDoSimbolo(&r, "fa")->bytes.dados[0] == 'A' && SecaoDoSimbolo(&r, "fb")->bytes.dados[0] == 'B';
    Liberar(&r);
    return ok;
}

/* ---------- 3: WEAK_EXTERNAL ---------- */

/* Membro 0: os padroes ".weak.w" (secao 1, 'a') e ".weak.v" (secao 2, 'b') e os fracos w e v apontando para eles (indices COFF 0 e 1). Membro 1: a definicao forte de w ('S'). */
static const SecT SECS_FRACO_A[2] = {{".text$w", CARAC_TEXTO, "a", 1, NULL, 0}, {".text$v", CARAC_TEXTO, "b", 1, NULL, 0}};
static const SimT SIMS_FRACO_A[4] = {{".weak.w", 0, 1, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}, {".weak.v", 0, 2, CLASSE_EXTERNA, AUX_NENHUM, 0, 0},
                                    {"w", 0, 0, CLASSE_FRACA, AUX_FRACO, 0, 0}, {"v", 0, 0, CLASSE_FRACA, AUX_FRACO, 1, 0}};
static const SecT SEC_FRACO_B = {".text$s", CARAC_TEXTO, "S", 1, NULL, 0};
static const SimT SIM_FRACO_B = {"w", 0, 1, CLASSE_EXTERNA, AUX_NENHUM, 0, 0};

static bool FracoForteVenceSemForteVaiParaOPadrao(void)
{
    const MembroT ms[2] = {{SECS_FRACO_A, 2, SIMS_FRACO_A, 4}, {&SEC_FRACO_B, 1, &SIM_FRACO_B, 1}};
    Resultado r;
    bool ok;

    Montar(ms, 2, &r);
    ok = r.ok && SecaoDoSimbolo(&r, "w") != NULL && SecaoDoSimbolo(&r, "v") != NULL;
    ok = ok && SecaoDoSimbolo(&r, "w")->bytes.dados[0] == 'S' && SecaoDoSimbolo(&r, "v")->bytes.dados[0] == 'b';
    Liberar(&r);
    return ok;
}

/* ---------- 4: simbolo comum ---------- */

static const SecT SEC_DIRETIVA_ALINHA = {".drectve", CARAC_DIRETIVA, " -aligncomm:\"c\",5", 17, NULL, 0};
static const SimT SIMS_COMUM_A[2] = {{"c", 8, 0, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}, {"d", 4, 0, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}};
static const SimT SIMS_COMUM_B[1] = {{"c", 16, 0, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}};

static bool ComumTemOMaiorTamanhoEOAlinhamentoDaDiretiva(void)
{
    const MembroT ms[2] = {{&SEC_DIRETIVA_ALINHA, 1, SIMS_COMUM_A, 2}, {&SEC_DIRETIVA_ALINHA, 1, SIMS_COMUM_B, 1}};
    Resultado r;
    const ObjSecao *c;
    const ObjSecao *d;
    bool ok;

    Montar(ms, 2, &r);
    c = SecaoDoSimbolo(&r, "c");
    d = SecaoDoSimbolo(&r, "d");
    ok = r.ok && c != NULL && d != NULL && c->tipo == OS_BSS && c->tam_bss == 16 && c->alinhamento == 32;
    ok = ok && d->tipo == OS_BSS && d->tam_bss == 4 && d->alinhamento == 4 && SecoesChamadas(&r, ".bss$comum") == 2;
    Liberar(&r);
    return ok;
}

/* ---------- 5: .drectve e relocacoes ---------- */

#define TEXTO_DIRETIVAS "/DEFAULTLIB:foo.lib -lbar /EXPORT:ex,DATA -aligncomm:\"c\",3 /FOO:x"

static bool DiretivasSeparamBibliotecaExportaAlinhaEIgnora(void)
{
    Arena arena = {0};
    Vec itens;
    const CoffDiretiva *d;
    bool ok;

    VecIniciar(&itens, sizeof(CoffDiretiva));
    CoffDiretivasLer(TEXTO_DIRETIVAS, strlen(TEXTO_DIRETIVAS), &arena, &itens);
    ok = itens.n == 5;
    d = VecEm(&itens, 0);
    ok = ok && d[0].tipo == DIRETIVA_BIBLIOTECA && strcmp(d[0].nome, "foo.lib") == 0 && d[1].tipo == DIRETIVA_BIBLIOTECA && strcmp(d[1].nome, "bar") == 0;
    ok = ok && d[2].tipo == DIRETIVA_EXPORTA && strcmp(d[2].nome, "ex") == 0 && d[3].tipo == DIRETIVA_ALINHA_COMUM && strcmp(d[3].nome, "c") == 0 && d[3].valor == 3;
    ok = ok && d[4].tipo == DIRETIVA_IGNORADA && strcmp(d[4].nome, "/FOO:x") == 0;
    VecLiberar(&itens);
    ArenaLiberar(&arena);
    return ok;
}

/* O .drectve de um membro vira exportados e o aviso do que o ligador nao usa; o -l e o defaultlib nao contam como ignorados. */
static bool DiretivasDoMembroChegamAoRelato(void)
{
    const SecT secs[1] = {{".drectve", CARAC_DIRETIVA, TEXTO_DIRETIVAS, (uint32_t)sizeof TEXTO_DIRETIVAS - 1, NULL, 0}};
    const MembroT ms[1] = {{secs, 1, NULL, 0}};
    Resultado r;
    bool ok;

    Montar(ms, 1, &r);
    ok = r.ok && r.relato.ignoradas == 1 && strcmp(r.relato.primeira_ignorada, "/FOO:x") == 0;
    ok = ok && r.exportados.n == 1 && strcmp(*(const char *const *)VecEm(&r.exportados, 0), "ex") == 0;
    Liberar(&r);
    return ok;
}

static const char CORPO_RELOCS[16] = {0};
static const RelT RELS_TIPOS[4] = {{0, 0, 6}, {4, 0, 0x0B}, {8, 0, 0x0A}, {12, 0, 3}};
static const RelT REL_DESCONHECIDA[1] = {{0, 0, 0x7F}};
static const SimT SIMS_ALVO[1] = {{"t", 0, 0, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}};

static bool RelocacoesViramTiposDoObjetoEDesconhecidaPara(void)
{
    const SecT tipos[1] = {{".text$r", CARAC_TEXTO, CORPO_RELOCS, 16, RELS_TIPOS, 4}};
    const SecT estranha[1] = {{".text$r", CARAC_TEXTO, CORPO_RELOCS, 16, REL_DESCONHECIDA, 1}};
    const MembroT bom[1] = {{tipos, 1, SIMS_ALVO, 1}};
    const MembroT mau[1] = {{estranha, 1, SIMS_ALVO, 1}};
    Resultado r;
    const ObjReloc *rel;
    bool ok;

    Montar(bom, 1, &r);
    rel = r.ok && r.o.relocs.n == 4 ? VecEm(&r.o.relocs, 0) : NULL;
    ok = rel != NULL && rel[0].correcao.tipo == OR_REL32 && rel[0].correcao.adendo == -2 && rel[1].correcao.tipo == OR_SECREL && rel[2].correcao.tipo == OR_SECTION;
    ok = ok && rel[3].correcao.tipo == OR_ADDR32NB && rel[3].alvo.off == 12;
    Liberar(&r);
    Montar(mau, 1, &r);
    ok = ok && !r.ok && r.erro.codigo == X0903 && strstr(r.erro.causa, "0x7f") != NULL && strstr(r.erro.causa, "m0.o") != NULL;
    Liberar(&r);
    return ok;
}

/* ---------- 6: construtores viram a tabela forja_ctors_ini..forja_ctors_fim na ordem de execucao; .tls = X0107 (P23f) ---------- */

#define CARAC_DADOS 0xC0300040u /* dados inicializados, leitura e escrita, alinhamento de 4 bytes */
#define RELOC_ADDR64 1u
static const char OITO_ZEROS[8] = {0};
static const RelT REL_PARA_FA[1] = {{0, 0, RELOC_ADDR64}};
static const RelT REL_PARA_FB[1] = {{0, 1, RELOC_ADDR64}};
static const RelT REL_PARA_FC[1] = {{0, 2, RELOC_ADDR64}};
static const RelT REL_PARA_FD[1] = {{0, 3, RELOC_ADDR64}};
static const SimT SIMS_CTORS[4] = {{"fa", 0, 1, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}, {"fb", 0, 2, CLASSE_EXTERNA, AUX_NENHUM, 0, 0},
                                   {"fc", 0, 3, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}, {"fd", 0, 4, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}};

/* Os nomes dos simbolos alvo das relocacoes da tabela, em ordem de offset, separados por espaco (as relocacoes entram na ordem das entradas). */
static void AlvosDaTabela(const Resultado *r, uint32_t secao_tabela, char *saida, size_t cap)
{
    saida[0] = '\0';
    for (size_t k = 0; k < r->o.relocs.n; k++) {
        const ObjReloc *rel = VecEm(&r->o.relocs, k);

        if (rel->alvo.secao != secao_tabela)
            continue;
        strncat(saida, ((const ObjSimbolo *)VecEm(&r->o.simbolos, rel->correcao.simbolo))->nome, cap - strlen(saida) - 2);
        strncat(saida, " ", cap - strlen(saida) - 1);
    }
}

/* .CRT$XCU (fd) roda primeiro; os .ctors saem na ordem inversa da lista do ld: .ctors.2 (fa), .ctors.1 (fb) e por ultimo o .ctors sem sufixo (fc). */
static bool ConstrutoresViramTabelaNaOrdemDeExecucao(void)
{
    const SecT secs[8] = {{".text$fa", CARAC_TEXTO, "A", 1, NULL, 0}, {".text$fb", CARAC_TEXTO, "B", 1, NULL, 0}, {".text$fc", CARAC_TEXTO, "C", 1, NULL, 0},
                          {".text$fd", CARAC_TEXTO, "D", 1, NULL, 0}, {".ctors", CARAC_DADOS, OITO_ZEROS, 8, REL_PARA_FC, 1},
                          {".ctors.1", CARAC_DADOS, OITO_ZEROS, 8, REL_PARA_FB, 1}, {".ctors.2", CARAC_DADOS, OITO_ZEROS, 8, REL_PARA_FA, 1},
                          {".CRT$XCU", CARAC_DADOS, OITO_ZEROS, 8, REL_PARA_FD, 1}};
    const MembroT ms[1] = {{secs, 8, SIMS_CTORS, 4}};
    char alvos[64];
    Resultado r;
    const ObjSimbolo *ini;
    bool ok;

    Montar(ms, 1, &r);
    ini = r.ok ? Simbolo(&r, "forja_ctors_ini") : NULL;
    ok = ini != NULL && r.relato.tem_ctors && Simbolo(&r, "forja_ctors_fim")->off == 32 && Simbolo(&r, "forja_ctors_fim")->secao == ini->secao && ini->off == 0;
    ok = ok && strcmp(SecaoDoSimbolo(&r, "forja_ctors_ini")->nome, ".rdata$ctors") == 0 && SecaoDoSimbolo(&r, "forja_ctors_ini")->bytes.tam == 32;
    if (ok) {
        AlvosDaTabela(&r, (uint32_t)ini->secao, alvos, sizeof alvos);
        ok = strcmp(alvos, "fd fa fb fc ") == 0;
    }
    Liberar(&r);
    return ok;
}

static bool TlsEMembroRecusadoComX0107(void)
{
    const SecT secs[2] = {{".text$fa", CARAC_TEXTO, "A", 1, NULL, 0}, {".tls", CARAC_DADOS, OITO_ZEROS, 8, NULL, 0}};
    const SimT sims[1] = {{"fa", 0, 1, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}};
    const MembroT ms[1] = {{secs, 2, sims, 1}};
    Resultado r;
    bool ok;

    Montar(ms, 1, &r);
    ok = !r.ok && r.erro.codigo == X0107 && strstr(r.erro.causa, "m0.o") != NULL && strstr(r.erro.causa, ".tls") != NULL;
    Liberar(&r);
    return ok;
}

/* ---------- 7: COMDAT com 2 externos, EXACT_MATCH com relocacoes e LARGEST com associadas (P23y-2) ---------- */

#define SEL_EXATO 4u
#define SEL_ASSOC 5u
#define RELOC_REL32 4u
#define SIMS_FG(sel) {{".text$f", 0, 1, CLASSE_ESTATICA, AUX_SECAO, (sel), 0}, {"f", 0, 1, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}, {"g", 2, 1, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}}

static const SimT SIMS_FG_QUALQUER[3] = SIMS_FG(SEL_QUALQUER);
static const SimT SIMS_FG_MAIOR[3] = SIMS_FG(SEL_MAIOR);

/* O 2o membro repete a secao COMDAT que tem DOIS externos (f e g, como o C1/C2 de um construtor): o perdedor so mapeia para os simbolos do 1o, o vencedor do LARGEST os reaponta. */
static bool ComdatComDoisExternosDuplicadoNaoDaX0106(void)
{
    const MembroT qualquer[2] = {{&SEC_F8, 1, SIMS_FG_QUALQUER, 3}, {&SEC_F8, 1, SIMS_FG_QUALQUER, 3}};
    const MembroT maior[2] = {{&SEC_F4, 1, SIMS_FG_MAIOR, 3}, {&SEC_F8, 1, SIMS_FG_MAIOR, 3}};
    Resultado r;
    bool ok;

    Montar(qualquer, 2, &r);
    ok = r.ok && BytesDasSecoes(&r, ".text$f") == 8 && SecaoDoSimbolo(&r, "f") != NULL && SecaoDoSimbolo(&r, "g") == SecaoDoSimbolo(&r, "f") && Simbolo(&r, "g")->off == 2;
    Liberar(&r);
    Montar(maior, 2, &r);
    ok = ok && r.ok && BytesDasSecoes(&r, ".text$f") == 8 && SecaoDoSimbolo(&r, "f") != NULL && SecaoDoSimbolo(&r, "f")->bytes.tam == 8;
    ok = ok && SecaoDoSimbolo(&r, "g") == SecaoDoSimbolo(&r, "f") && Simbolo(&r, "g")->off == 2;
    Liberar(&r);
    return ok;
}

static const char CORPO_CALL[6] = {(char)0xE8, 0, 0, 0, 0, (char)0xC3};
static const char CORPO_CALL_ADENDO4[6] = {(char)0xE8, 4, 0, 0, 0, (char)0xC3};
static const char CORPO_CALL_ADENDO8[6] = {(char)0xE8, 8, 0, 0, 0, (char)0xC3};
static const RelT REL_CALL_TA[1] = {{1, 3, RELOC_REL32}};
static const RelT REL_CALL_TB[1] = {{1, 4, RELOC_REL32}};
static const SimT SIMS_EXATO[4] = {{".text$f", 0, 1, CLASSE_ESTATICA, AUX_SECAO, SEL_EXATO, 0}, {"f", 0, 1, CLASSE_EXTERNA, AUX_NENHUM, 0, 0},
                                   {"ta", 0, 0, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}, {"tb", 0, 0, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}};

static bool ExatoDuasFuncoesComAlvosDiferentesRecusa(void)
{
    const SecT a[1] = {{".text$f", CARAC_TEXTO | CARAC_COMDAT, CORPO_CALL, 6, REL_CALL_TA, 1}};
    const SecT b[1] = {{".text$f", CARAC_TEXTO | CARAC_COMDAT, CORPO_CALL, 6, REL_CALL_TB, 1}};
    const MembroT ms[2] = {{a, 1, SIMS_EXATO, 4}, {b, 1, SIMS_EXATO, 4}};
    Resultado r;
    bool ok;

    Montar(ms, 2, &r);
    ok = !r.ok && r.erro.codigo == X0903 && strstr(r.erro.causa, "EXACT_MATCH") != NULL && strstr(r.erro.causa, "'f'") != NULL && strstr(r.erro.causa, "m1.o") != NULL;
    Liberar(&r);
    return ok;
}

/* Mesmo alvo e mesmo adendo (nos bytes da secao, ate a relocacao ser lida) = iguais e a 2a some; adendo diferente = recusa. */
static bool ExatoComMesmoAlvoEAdendoAceitaEAdendoDiferenteRecusa(void)
{
    const SecT a4[1] = {{".text$f", CARAC_TEXTO | CARAC_COMDAT, CORPO_CALL_ADENDO4, 6, REL_CALL_TA, 1}};
    const SecT b4[1] = {{".text$f", CARAC_TEXTO | CARAC_COMDAT, CORPO_CALL_ADENDO4, 6, REL_CALL_TA, 1}};
    const SecT b8[1] = {{".text$f", CARAC_TEXTO | CARAC_COMDAT, CORPO_CALL_ADENDO8, 6, REL_CALL_TA, 1}};
    const MembroT iguais[2] = {{a4, 1, SIMS_EXATO, 4}, {b4, 1, SIMS_EXATO, 4}};
    const MembroT difere[2] = {{a4, 1, SIMS_EXATO, 4}, {b8, 1, SIMS_EXATO, 4}};
    Resultado r;
    bool ok;

    Montar(iguais, 2, &r);
    ok = r.ok && BytesDasSecoes(&r, ".text$f") == 6 && SecaoDoSimbolo(&r, "f") != NULL;
    Liberar(&r);
    Montar(difere, 2, &r);
    ok = ok && !r.ok && r.erro.codigo == X0903 && strstr(r.erro.causa, "EXACT_MATCH") != NULL;
    Liberar(&r);
    return ok;
}

/* -aligncomm com valor de 33 bits (4294967300 = 2^32 + 4) nao pode virar 4: vira IGNORADA; 4294967295 ainda cabe e fica. */
static bool AlinhaComumQueNaoCabeEmUint32ViraIgnorada(void)
{
    static const char TEXTO[] = "-aligncomm:\"c\",4294967300 -aligncomm:\"d\",4294967295";
    Arena arena = {0};
    Vec itens;
    const CoffDiretiva *d;
    bool ok;

    VecIniciar(&itens, sizeof(CoffDiretiva));
    CoffDiretivasLer(TEXTO, sizeof TEXTO - 1, &arena, &itens);
    d = VecEm(&itens, 0);
    ok = itens.n == 2 && d[0].tipo == DIRETIVA_IGNORADA && d[1].tipo == DIRETIVA_ALINHA_COMUM && d[1].valor == 4294967295u;
    VecLiberar(&itens);
    ArenaLiberar(&arena);
    return ok;
}

/* O lider do 1o membro tem .xdata$f associada; o 2o membro traz a maior: a antiga E a associada dela ficam sem efeito (so um unwind vivo para f). */
static bool LargestPromovidoAnulaAsAssociadasDoLiderAntigo(void)
{
    const SecT a[2] = {{".text$f", CARAC_TEXTO | CARAC_COMDAT, "\x90\x90\x90\xC3", 4, NULL, 0}, {".xdata$f", CARAC_DADOS | CARAC_COMDAT, "XXXX", 4, NULL, 0}};
    const SimT sa[3] = {{".text$f", 0, 1, CLASSE_ESTATICA, AUX_SECAO, SEL_MAIOR, 0}, {"f", 0, 1, CLASSE_EXTERNA, AUX_NENHUM, 0, 0},
                        {".xdata$f", 0, 2, CLASSE_ESTATICA, AUX_SECAO, SEL_ASSOC, 1}};
    const MembroT ms[2] = {{a, 2, sa, 3}, {&SEC_F8, 1, SIMS_F_MAIOR, 2}};
    Resultado r;
    bool ok;

    Montar(ms, 2, &r);
    ok = r.ok && SecaoDoSimbolo(&r, "f") != NULL && SecaoDoSimbolo(&r, "f")->bytes.tam == 8;
    ok = ok && BytesDasSecoes(&r, ".text$f") == 8 && BytesDasSecoes(&r, ".xdata$f") == 0;
    Liberar(&r);
    return ok;
}

/* ---------- M6d P92c: redirecionamento da familia malloc por lib e tabela FLM1 ---------- */

static const char CORPO_TRES_CALLS[12] = {0};
static const RelT RELS_M0[3] = {{0, 0, RELOC_REL32}, {4, 1, RELOC_REL32}, {8, 3, RELOC_REL32}};
static const RelT REL_MALLOC[1] = {{0, 0, RELOC_REL32}};
static const RelT RELS_M1[2] = {{0, 0, RELOC_REL32}, {4, 2, RELOC_REL32}};
static const SimT SIMS_M0[4] = {{"malloc", 0, 0, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}, {"__imp__strdup", 0, 0, CLASSE_EXTERNA, AUX_NENHUM, 0, 0},
                                {"fa", 0, 1, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}, {"free", 0, 0, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}};
static const SimT SIMS_M1[3] = {{"malloc", 0, 0, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}, {"fb", 0, 1, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}, {"calloc", 8, 1, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}};
static const SimT SIMS_M2[3] = {{"malloc", 0, 0, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}, {"fc", 0, 1, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}, {"free", 4, 1, CLASSE_EXTERNA, AUX_NENHUM, 0, 0}};

/* O nome do simbolo a que a relocacao em secao + off aponta ("" se nao ha). */
static const char *AlvoDaRelocacao(const Resultado *r, const char *secao, uint32_t off)
{
    for (size_t k = 0; k < r->o.relocs.n; k++) {
        const ObjReloc *rel = VecEm(&r->o.relocs, k);

        if (rel->alvo.off == off && strcmp(((const ObjSecao *)VecEm(&r->o.secoes, rel->alvo.secao))->nome, secao) == 0)
            return ((const ObjSimbolo *)VecEm(&r->o.simbolos, rel->correcao.simbolo))->nome;
    }
    return "";
}

/* 3 membros: arquivos 0 e 1 contados (slots 0 e 1), o 2 nao (slot -1; define free). malloc e __imp__strdup do membro 0 vao para o slot 0, malloc do 1 para o slot 1; free definido pelo membro NAO
 * contado (o musl do Linux) tambem vai para o slot 0; calloc que o proprio membro 1 define e malloc do membro nao contado ficam intocados; a tabela FLM1 sai com os 2 slots. */
static bool FamiliaDaLibContadaVaiParaOSlotEDefinidoFicaIntocado(void)
{
    const SecT a[1] = {{".text$a", CARAC_TEXTO, CORPO_TRES_CALLS, 12, RELS_M0, 3}};
    const SecT b[1] = {{".text$b", CARAC_TEXTO, CORPO_TRES_CALLS, 12, RELS_M1, 2}};
    const SecT c[1] = {{".text$c", CARAC_TEXTO, CORPO_TRES_CALLS, 12, REL_MALLOC, 1}};
    const MembroT ms[3] = {{a, 1, SIMS_M0, 4}, {b, 1, SIMS_M1, 3}, {c, 1, SIMS_M2, 3}};
    const uint32_t arquivos[3] = {0, 1, 2};
    const int8_t slots[3] = {0, 1, -1};
    TerceirosLibmem lm = {.qtd_slots = 2, .qtd_arquivos = 3, .slot_do_arquivo = slots, .nome = {"cjson", "z"}, .arquivo = {"libcjson.a", "libz.a"}};
    Resultado r;
    const ObjSecao *tabela;
    uint32_t qtd = 0;
    bool ok;

    MontarCom(ms, 3, arquivos, &lm, &r);
    ok = r.ok && strcmp(AlvoDaRelocacao(&r, ".text$a", 0), "forja_libmem_malloc_0") == 0 && strcmp(AlvoDaRelocacao(&r, ".text$a", 4), "__imp_forja_libmem__strdup_0") == 0;
    ok = ok && strcmp(AlvoDaRelocacao(&r, ".text$a", 8), "forja_libmem_free_0") == 0 && strcmp(AlvoDaRelocacao(&r, ".text$b", 0), "forja_libmem_malloc_1") == 0;
    ok = ok && strcmp(AlvoDaRelocacao(&r, ".text$b", 4), "calloc") == 0;
    ok = ok && strcmp(AlvoDaRelocacao(&r, ".text$c", 0), "malloc") == 0;
    ok = ok && Simbolo(&r, "__imp_forja_libmem__strdup_0") != NULL && Simbolo(&r, "__imp_forja_libmem__strdup_0")->secao >= 0 && SecoesChamadas(&r, ".rdata$imp") == 1;
    tabela = SecaoDoSimbolo(&r, "forja_libmem_tabela");
    ok = ok && tabela != NULL && strcmp(tabela->nome, ".rdata$libmem") == 0 && memcmp(tabela->bytes.dados, "FLM1", 4) == 0;
    if (tabela != NULL && tabela->bytes.tam > 8) {
        memcpy(&qtd, tabela->bytes.dados + 4, sizeof qtd);
        uint32_t offs[6];

        memcpy(offs, tabela->bytes.dados + 8, sizeof offs); /* {off_nome, off_arquivo, off_fins} x 2 */
        ok = ok && qtd == 2 && strcmp(tabela->bytes.dados + offs[0], "cjson") == 0 && strcmp(tabela->bytes.dados + offs[1], "libcjson.a") == 0 && offs[2] == 0;
        ok = ok && strcmp(tabela->bytes.dados + offs[3], "z") == 0 && strcmp(tabela->bytes.dados + offs[4], "libz.a") == 0 && offs[5] == 0;
    }
    Liberar(&r);
    return ok;
}

/* Sem libmem (NULL) ou com qtd_slots == 0 o objeto de terceiros e o de sempre: malloc fica malloc, nenhuma secao nova. */
static bool SemLibmemOObjetoNaoMuda(void)
{
    const SecT a[1] = {{".text$a", CARAC_TEXTO, CORPO_TRES_CALLS, 12, RELS_M0, 3}};
    const MembroT ms[1] = {{a, 1, SIMS_M0, 4}};
    const TerceirosLibmem vazio = {0};
    Resultado nulo, zero;
    bool ok;

    Montar(ms, 1, &nulo);
    MontarCom(ms, 1, NULL, &vazio, &zero);
    ok = nulo.ok && zero.ok && nulo.o.secoes.n == zero.o.secoes.n && nulo.o.simbolos.n == zero.o.simbolos.n && nulo.o.relocs.n == zero.o.relocs.n;
    ok = ok && strcmp(AlvoDaRelocacao(&nulo, ".text$a", 0), "malloc") == 0 && strcmp(AlvoDaRelocacao(&zero, ".text$a", 0), "malloc") == 0 && Simbolo(&nulo, "forja_libmem_tabela") == NULL;
    Liberar(&nulo);
    Liberar(&zero);
    return ok;
}

void TerceirosVerificacoes(int *total, int *falhas)
{
    Contagem c = {total, falhas};

    TERCEIROS_VERIFICA(&c, ComdatQualquerFicaComUm() && ComdatMaiorVenceENodupRecusa());
    TERCEIROS_VERIFICA(&c, SecoesComCifraveOrdenadas());
    TERCEIROS_VERIFICA(&c, FracoForteVenceSemForteVaiParaOPadrao());
    TERCEIROS_VERIFICA(&c, ComumTemOMaiorTamanhoEOAlinhamentoDaDiretiva());
    TERCEIROS_VERIFICA(&c, DiretivasSeparamBibliotecaExportaAlinhaEIgnora() && DiretivasDoMembroChegamAoRelato() && RelocacoesViramTiposDoObjetoEDesconhecidaPara());
    TERCEIROS_VERIFICA(&c, ConstrutoresViramTabelaNaOrdemDeExecucao());
    TERCEIROS_VERIFICA(&c, TlsEMembroRecusadoComX0107());
    TERCEIROS_VERIFICA(&c, ComdatComDoisExternosDuplicadoNaoDaX0106());
    TERCEIROS_VERIFICA(&c, ExatoDuasFuncoesComAlvosDiferentesRecusa());
    TERCEIROS_VERIFICA(&c, ExatoComMesmoAlvoEAdendoAceitaEAdendoDiferenteRecusa());
    TERCEIROS_VERIFICA(&c, LargestPromovidoAnulaAsAssociadasDoLiderAntigo());
    TERCEIROS_VERIFICA(&c, AlinhaComumQueNaoCabeEmUint32ViraIgnorada());
    TERCEIROS_VERIFICA(&c, FamiliaDaLibContadaVaiParaOSlotEDefinidoFicaIntocado());
    TERCEIROS_VERIFICA(&c, SemLibmemOObjetoNaoMuda());
}
