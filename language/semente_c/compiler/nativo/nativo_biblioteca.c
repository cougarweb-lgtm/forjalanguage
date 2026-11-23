/* nativo_biblioteca.c - cola do backend nativo com bibliotecas.h: os 'link' e os simbolos de foreign do programa viram tabelas de imports (P19 do M3).
 * Versao: 0.16.1
 * Changelog: 0.16.1 - Linguagem v1.13 L1a-ir: NbPosDoExterno usa IrExterno.arquivo (trecho) no lugar do modulo.
 *            0.16.0 - R10a (contrato_rede, D-REDE-7): NbColetarLinks nao passa ao resolvedor, no alvo x86_64-linux, os 'link' das libs de sistema so do Windows (ws2_32, crypt32, bcrypt, advapi32, secur32, iphlpapi): "nada a ligar", simetrico ao msvcrt -> libc.a do musl.
 *            0.15.0 - 0.21.0 F4 passo 2b: sai NativoConferirBibliotecasDoC e os auxiliares so dele (NbTemBibliotecas, NbFalhaDeterminante, NbConstruirBir, NbRelatarDoC); sem chamador desde a 2a.
 *            0.14.1 - M6d P93h (e): NbMontarLibmem monta LibmemEntrada e chama LibmemMontar(entrada, arena, saida).
 *            0.14.0 - M6d P92e: NbMontarLibmem passa os simbolos `ends` (IrPrograma.fins) ao LibmemMontar.
 *            0.13.0 - M6d P92c: com NativoBibliotecasEntrada.libmem, monta o mapa slot -> lib (LibmemMontar) depois do BibliotecasResolver.
 *            0.12.0 - M5c-b P63: as raizes da semente ganham pthread_attr_init, pthread_attr_setstacksize e pthread_attr_destroy (pilha de 1 MiB da filha, rt_thread_linux.o 0.2.0).
 *            0.11.0 - M5c-b P58: as raizes da semente ganham pthread_create e pthread_join (o rt_thread_linux.o os chama; sem eles a selecao da libc.a nao os traz).
 *            0.10.0 - M5c-b P57: NativoUsaThreadsNoElf (BS_RUNTIME forja_thread_*: o programa com std:thread liga a variante libc mesmo sem foreign) e as raizes da semente ganham pthread_key_create, pthread_getspecific e pthread_setspecific (o RtFio por thread do objeto de entrada libc).
 *            0.9.3 - M5b P41: SementeMuslCrt1 recebe pie e le rcrt1.o (o _start do musl static-pie, que faz a auto-relocacao) no lugar de crt1.o.
 *            0.9.2 - M5b P39-B2: SementeAjustarPc (o adendo PC-relativo do crt1.o ganha +4, a convencao de terceiros.c); sem isso o _start do musl sai 4 bytes deslocado.
 *            0.9.1 - M5b P39-B2: raizes da libc.a (__libc_start_main, exit, fwrite, fflush, stdout) entram na selecao com foreign no alvo linux (NbAcrescentarRaizesSemente); sem foreign a semente sai da busca.
 *            0.9.0 - M5b P39-B2 (D0654): BibliotecaBusca.semente (NbSemente: no alvo linux, a pasta da semente musl); "c", "m", "msvcrt" e "ucrt" acham o libc.a dela.
 *            0.8.1 - M5b P36: no alvo x86_64-linux os imports do UCRT (__acrt_iob_func, fwrite, fflush) nao entram (o runtime Linux nao os usa).
 *            0.8.0 - M5b P36: BibliotecaBusca.elf (alvo x86_64-linux: a busca dos 'link' vai para vendor\linux).
 *            0.7.0 - IA1-b03: NativoConferirBibliotecasDoC (paridade do backend C: a mesma resolucao do x64 antes do gcc, X0101/X0102 com procurei_em, sem alvo e etapa).
 *            0.6.0 - M3b P23f: NativoBibliotecas recebe NativoBibliotecasEntrada (pedido, BIR e os provedores: runtime, Ryu e legado satisfazem o que um membro de estatica pede).
 *            0.5.0 - M3b P23e: o X0101 de simbolo pedido por membro de biblioteca estatica (simbolo_faltando = -1) leva o nome em simbolo_faltando_nome (sem proc do programa).
 *            0.4.0 - M3 P22-O3: NativoUsaRuntimeCrt (predicado unico: DLL Forja pura OU foreign de fora); os imports CRT entram em toda DLL.
 *            0.3.0 - M3 P22-O1a: NativoPedido.lib_pasta (--lib-pasta) entra no fim das pastas de busca, como caminho absoluto.
 *            0.2.0 - M3 P19c: com algum foreign de fora, __acrt_iob_func, fwrite e fflush entram no fim dos simbolos (os imports da variante CRT do runtime, ucrtbase.dll).
 *            0.1.0 - M3 P19: NativoBibliotecas. Simbolos = os BS_IMPORT da BIR (sem o "__imp_"), na ordem da 1a chamada; pastas = a do .fj e as extras do pedido, o System32 por
 *                    ultimo (biblioteca.c). A posicao do erro e a linha do 'proc' no .fj do modulo dele (IrExterno.linha); sem foreign, a do primeiro.
 * Verificacao: construir.bat && python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe --pasta m3/foreign e python -I ..\tests\cli\verificar_cli.py --forja .\forja.exe
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/arena.h"
#include "nativo/x64/alvo.h"
#include "nativo/nativo.h"
#include "nativo/nativo_biblioteca.h"
#include "nativo/nativo_erro.h"
#include "nativo/nativo_json.h"
#include "nativo/nativo_semente.h"
#include "nativo/obj/elf_ler.h"

#define NB_TAM_PREFIXO_IMP 6u

/* O que a coleta junta do pedido e da BIR: tudo na arena, que morre ao fim de NativoBibliotecas. */
typedef struct {
    Arena arena;
    const NativoPedido *pedido;
    const char **links;
    const char **pastas;
    const char **simbolos;
    uint32_t *proc_do_simbolo;  /* por simbolo: posicao em IrPrograma.externos do 1o proc de fora que o chama (UINT32_MAX = nenhum) */
    BibliotecaBusca busca;
    BibliotecasPedido ped;
} Coleta;

static const char *NbTexto(const NativoPedido *p, int32_t id)
{
    return InternerTexto(p->nomes, id);
}

/* --lib-pasta como caminho absoluto, copiado na arena (relativo ao diretorio de onde o forja foi chamado, como todo caminho da linha de comando). */
static const char *NbCaminhoAbsoluto(Coleta *c, const char *pasta)
{
    char absoluto[BIBLIOTECA_TAM_CAMINHO];

    if (_fullpath(absoluto, pasta, sizeof absoluto) == NULL)
        return pasta;
    return ArenaCopiarTexto(&c->arena, absoluto, strlen(absoluto));
}

/* O alvo do pedido e ELF (x86_64-linux): a busca dos 'link' vai para vendor\linux (M5b P36). */
static bool NbAlvoEhElf(const NativoPedido *p)
{
    const Alvo *alvo = AlvoPorNome(p->alvo != NULL ? p->alvo : NATIVO_ALVO_PADRAO);

    return alvo != NULL && alvo->formato == IMAGEM_ELF;
}

/* 0.16.0 (R10a, D-REDE-7): libs de sistema que so existem no Windows. No alvo linux o 'link' delas e "nada a ligar" (simetrico ao msvcrt -> libc.a do musl): o mesmo bloco foreign
 * serve aos 2 alvos. Nome exato, em minusculas, como o .fj escreve. */
static const char *const NB_LIBS_SO_WINDOWS[] = {"ws2_32", "crypt32", "bcrypt", "advapi32", "secur32", "iphlpapi"};

static bool NbEhLibSoWindows(const char *link)
{
    for (size_t k = 0; k < sizeof NB_LIBS_SO_WINDOWS / sizeof NB_LIBS_SO_WINDOWS[0]; k++) {
        if (strcmp(link, NB_LIBS_SO_WINDOWS[k]) == 0)
            return true;
    }
    return false;
}

static void NbColetarLinks(Coleta *c)
{
    const IrPrograma *ir = c->pedido->ir;
    bool elf = NbAlvoEhElf(c->pedido);
    uint32_t n = 0;

    c->links = ArenaAlocar(&c->arena, ((size_t)ir->nbibliotecas + 1) * sizeof *c->links);
    for (uint32_t k = 0; k < ir->nbibliotecas; k++) {
        const char *link = NbTexto(c->pedido, ir->bibliotecas[k]);

        if (!elf || !NbEhLibSoWindows(link))
            c->links[n++] = link;
    }
    c->ped.links = c->links;
    c->ped.qtd_links = n;
}

/* ---- semente musl (M5b P39-B2, D-R4) ----
 * Fica nesta unidade (e nao em nativo_semente.c) porque construir.bat junta os .c por glob e a linha do cmd (8191 chars) ja esta no limite. */
#define SEMENTE_SUBPASTA "\\..\\third\\musl\\x86_64"
#define SEMENTE_CRT1 "\\crt1.o"
#define SEMENTE_RCRT1 "\\rcrt1.o" /* P41: o _start do static-pie (faz a auto-relocacao antes do main) */
#define SEMENTE_LIBC "\\libc.a"

bool SementeMuslPasta(char *saida, size_t tam)
{
    char exe[SEMENTE_TAM_PASTA];
    char *barra;

    if (_pgmptr == NULL || strlen(_pgmptr) >= sizeof exe)
        return false;
    snprintf(exe, sizeof exe, "%s", _pgmptr);
    barra = strrchr(exe, '\\');
    if (barra == NULL)
        return false;
    *barra = '\0';
    if (strlen(exe) + strlen(SEMENTE_SUBPASTA) + 1 > tam)
        return false;
    snprintf(saida, tam, "%s" SEMENTE_SUBPASTA, exe);
    return true;
}

/* Um arquivo da semente existe e abre para leitura (fopen; nada de stat). */
static bool SementeArquivoExiste(const char *pasta, const char *sufixo)
{
    char caminho[SEMENTE_TAM_PASTA + 16];
    FILE *f;

    if (snprintf(caminho, sizeof caminho, "%s%s", pasta, sufixo) >= (int)sizeof caminho)
        return false;
    f = fopen(caminho, "rb");
    if (f == NULL)
        return false;
    fclose(f);
    return true;
}

bool SementeMuslCompleta(const char *pasta)
{
    return SementeArquivoExiste(pasta, SEMENTE_CRT1) && SementeArquivoExiste(pasta, SEMENTE_LIBC);
}

/* Le o arquivo inteiro para memoria. NULL se falhar; *tam recebe o tamanho lido. */
static uint8_t *SementeLerTudo(const char *caminho, size_t *tam)
{
    FILE *f = fopen(caminho, "rb");
    long n;
    uint8_t *dados;

    *tam = 0;
    if (f == NULL)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) <= 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    dados = malloc((size_t)n);
    if (dados != NULL && fread(dados, 1, (size_t)n, f) == (size_t)n)
        *tam = (size_t)n;
    else {
        free(dados);
        dados = NULL;
    }
    fclose(f);
    return dados;
}

/* A convencao do Objeto neutro (terceiros.c, TrCopiarRelocs): o campo PC-relativo acaba 4 bytes depois do P, entao o adendo do ELF (que traz o -4) ganha +4.
 * O crt1.o nao passa pela uniao de terceiros: sem isto sai 4 bytes deslocado (call e lea para _start_c e main). */
static void SementeAjustarPc(Objeto *o)
{
    for (size_t r = 0; r < o->relocs.n; r++) {
        ObjReloc *rel = VecEm(&o->relocs, r);

        if (rel->correcao.tipo == OR_REL32 || rel->correcao.tipo == OR_GOTPCREL || rel->correcao.tipo == OR_GOTPCRELX)
            rel->correcao.adendo += 4;
    }
}

bool SementeMuslCrt1(const char *pasta, bool pie, Objeto *o)
{
    char caminho[SEMENTE_TAM_PASTA + 16];
    size_t n = 0;
    uint8_t *bytes;
    ElfLErro erro = {0};
    bool ok;

    if (snprintf(caminho, sizeof caminho, "%s%s", pasta, pie ? SEMENTE_RCRT1 : SEMENTE_CRT1) >= (int)sizeof caminho)
        return false;
    bytes = SementeLerTudo(caminho, &n);
    if (bytes == NULL)
        return false;
    ok = ElfLerObjeto(bytes, n, o, &erro) == ELFL_OK;
    free(bytes);
    if (ok)
        SementeAjustarPc(o);
    return ok;
}

/* M5b P39-B2: no alvo linux, a pasta da semente musl (o libc.a das libs "c", "m", "msvcrt" e "ucrt"); NULL fora dele ou sem o caminho do forja.exe. */
static const char *NbSemente(Coleta *c)
{
    char pasta[SEMENTE_TAM_PASTA];

    if (!NbAlvoEhElf(c->pedido) || !SementeMuslPasta(pasta, sizeof pasta))
        return NULL;
    return ArenaCopiarTexto(&c->arena, pasta, strlen(pasta));
}

static void NbColetarPastas(Coleta *c)
{
    const NativoPedido *p = c->pedido;
    uint32_t n = 0;

    c->pastas = ArenaAlocar(&c->arena, ((size_t)p->npastas_extra + 2) * sizeof *c->pastas);
    if (p->pasta_libs != NULL)
        c->pastas[n++] = p->pasta_libs;
    for (uint32_t k = 0; k < p->npastas_extra; k++)
        c->pastas[n++] = p->pastas_extra[k];
    if (p->lib_pasta != NULL)
        c->pastas[n++] = NbCaminhoAbsoluto(c, p->lib_pasta);
    c->busca = (BibliotecaBusca){.pastas = c->pastas, .qtd_pastas = n, .elf = NbAlvoEhElf(p), .semente = NbSemente(c)};
    c->ped.busca = &c->busca;
}

/* O 1o foreign de fora cujo simbolo C e esse (posicao em externos); UINT32_MAX se nenhum. */
static uint32_t NbProcDoSimbolo(const Coleta *c, const char *simbolo)
{
    const IrPrograma *ir = c->pedido->ir;

    for (uint32_t k = 0; k < ir->nexternos; k++) {
        if (!ir->externos[k].interno && strcmp(NbTexto(c->pedido, ir->externos[k].simbolo), simbolo) == 0)
            return k;
    }
    return UINT32_MAX;
}

/* Os imports do runtime CRT (rt_saida.c, FORJA_RT_CRT): o stdout do programa passa a ser o do UCRT. Entram no fim, so se o programa chama foreign de fora (e o mesmo caso em
 * que nativo.c escolhe a variante CRT), sem repetir o que o programa ja importa. */
static const char *const NB_IMPORTS_CRT[] = {"__acrt_iob_func", "fwrite", "fflush"};
#define NB_QTD_IMPORTS_CRT (sizeof NB_IMPORTS_CRT / sizeof NB_IMPORTS_CRT[0])

/* P22-O3: o produto usa a variante CRT do runtime? DLL Forja pura (todas dividem o FILE stdout do UCRT, entao a saida sai em ordem) ou programa com foreign de fora.
 * Unico lugar da decisao: nativo.c escolhe o runtime embutido e esta cola escolhe os imports pela mesma resposta. */
bool NativoUsaRuntimeCrt(const NativoPedido *pedido, const BirPrograma *bir)
{
    if (pedido->dll)
        return true;
    for (uint32_t k = 0; k < bir->nsimbolos; k++) {
        if (bir->simbolos[k].tipo == BS_IMPORT)
            return true;
    }
    return false;
}

/* M5c-b P57 (D-R2): o programa chama o runtime de threads (forja_thread_*, std:thread e, por cima dele, std:async)? No alvo linux ele liga a variante libc (o pthread do musl), como o foreign.
 * Separada de NativoUsaRuntimeCrt de proposito: no PE essa decide a variante CRT e nao pode mudar com std:thread. */
bool NativoUsaThreadsNoElf(const BirPrograma *bir)
{
    static const char prefixo[] = "forja_thread_";

    for (uint32_t k = 0; k < bir->nsimbolos; k++) {
        if (bir->simbolos[k].tipo == BS_RUNTIME && strncmp(bir->simbolos[k].nome, prefixo, sizeof prefixo - 1) == 0)
            return true;
    }
    return false;
}

static uint32_t NbAcrescentarImportsCrt(Coleta *c, uint32_t n, bool usa_crt)
{
    uint32_t base = n;

    if (!usa_crt)
        return n;
    for (size_t k = 0; k < NB_QTD_IMPORTS_CRT; k++) {
        bool ja_tem = false;

        for (uint32_t j = 0; j < base; j++)
            ja_tem = ja_tem || strcmp(c->simbolos[j], NB_IMPORTS_CRT[k]) == 0;
        if (ja_tem)
            continue;
        c->simbolos[n] = NB_IMPORTS_CRT[k];
        c->proc_do_simbolo[n++] = UINT32_MAX;
    }
    return n;
}

/* M5b P39-B2: o que o crt1.o e o objeto de entrada libc pedem da libc.a (o musl). Sem eles a selecao das estaticas nao puxa o musl (o alcance so ve o programa). */
static const char *const NB_RAIZES_SEMENTE[] = {"__libc_start_main", "exit", "fwrite", "fflush", "stdout", "pthread_key_create", "pthread_getspecific", "pthread_setspecific", "pthread_create", "pthread_join", "pthread_attr_init", "pthread_attr_setstacksize", "pthread_attr_destroy"};
#define NB_QTD_RAIZES_SEMENTE (sizeof NB_RAIZES_SEMENTE / sizeof NB_RAIZES_SEMENTE[0])

/* Raiz que o programa ja pediu (ex.: um foreign pthread_create) nao entra de novo: dois imports com o mesmo nome dao __imp_ definido duas vezes. */
static bool NbJaNaColeta(const Coleta *c, uint32_t n, const char *nome)
{
    for (uint32_t i = 0; i < n; i++) {
        if (strcmp(c->simbolos[i], nome) == 0)
            return true;
    }
    return false;
}

static uint32_t NbAcrescentarRaizesSemente(Coleta *c, uint32_t n)
{
    if (c->busca.semente == NULL)
        return n;
    for (uint32_t k = 0; k < NB_QTD_RAIZES_SEMENTE; k++) {
        if (NbJaNaColeta(c, n, NB_RAIZES_SEMENTE[k]))
            continue;
        c->simbolos[n] = NB_RAIZES_SEMENTE[k];
        c->proc_do_simbolo[n++] = UINT32_MAX;
    }
    return n;
}

static void NbColetarSimbolos(Coleta *c, const BirPrograma *bir)
{
    uint32_t n = 0;
    size_t teto = (size_t)bir->nsimbolos + NB_QTD_IMPORTS_CRT + NB_QTD_RAIZES_SEMENTE + 1;

    c->simbolos = ArenaAlocar(&c->arena, teto * sizeof *c->simbolos);
    c->proc_do_simbolo = ArenaAlocar(&c->arena, teto * sizeof *c->proc_do_simbolo);
    for (uint32_t k = 0; k < bir->nsimbolos; k++) {
        if (bir->simbolos[k].tipo != BS_IMPORT)
            continue;
        c->simbolos[n] = bir->simbolos[k].nome + NB_TAM_PREFIXO_IMP;
        c->proc_do_simbolo[n] = NbProcDoSimbolo(c, c->simbolos[n]);
        n++;
    }
    if (n == 0 && !NativoUsaThreadsNoElf(bir)) /* sem foreign nem threads o alvo linux nao usa a libc: a semente sai da busca e o programa segue o ELF cru */
        c->busca.semente = NULL;
    n = NbAcrescentarImportsCrt(c, n, NativoUsaRuntimeCrt(c->pedido, bir) && !NbAlvoEhElf(c->pedido));
    n = NbAcrescentarRaizesSemente(c, n);
    c->ped.simbolos = c->simbolos;
    c->ped.qtd_simbolos = n;
}

/* ---------- a falha ---------- */

/* Offset da linha do 'proc' no texto inteiro (os modulos dividem um texto so; linha_base leva a linha local do modulo para a global). 0 se nao da para saber. */
static uint32_t NbPosDoExterno(const Fonte *f, const IrExterno *e)
{
    uint32_t base = f->ntrechos > 1 ? f->trechos[e->arquivo].linha_base : 0;
    uint32_t linha = base + e->linha;

    if (f->inicio_linha == NULL || linha == 0 || linha > f->ninicios)
        return 0;
    return f->inicio_linha[linha - 1];
}

static const IrExterno *NbPrimeiroDeFora(const IrPrograma *ir)
{
    for (uint32_t k = 0; k < ir->nexternos; k++) {
        if (!ir->externos[k].interno)
            return &ir->externos[k];
    }
    return NULL;
}

/* "procurei_em": [...] a partir das linhas do Buf (um caminho por linha), com '/' no lugar de '\'. */
static void NbCampoProcureiEm(Buf *campos, const Buf *linhas)
{
    const char *p = linhas->dados;
    bool primeira = true;

    BufAnexar(campos, "\"procurei_em\": [");
    while (p != NULL && *p != '\0') {
        const char *fim = strchr(p, '\n');
        char caminho[BIBLIOTECA_TAM_CAMINHO];

        snprintf(caminho, sizeof caminho, "%.*s", (int)(fim != NULL ? (size_t)(fim - p) : strlen(p)), p);
        BufAnexar(campos, primeira ? "" : ", ");
        JsonCaminho(campos, caminho);
        primeira = false;
        p = fim != NULL ? fim + 1 : NULL;
    }
    BufAnexar(campos, "]");
}

static void NbCampoTexto(Buf *campos, const char *chave, const char *texto)
{
    BufFormatar(campos, "\"%s\": ", chave);
    JsonTexto(campos, texto, strlen(texto));
}

/* X0101: a causa cita o proc e o simbolo; a posicao e a do 'proc' que o chama. */
static const IrExterno *NbExternoDoFaltante(const Coleta *c, const BibliotecasResolvidas *r)
{
    uint32_t j = (uint32_t)r->simbolo_faltando;

    if (r->simbolo_faltando < 0 || c->proc_do_simbolo[j] == UINT32_MAX)
        return NULL;
    return &c->pedido->ir->externos[c->proc_do_simbolo[j]];
}

static void NbFalhaSimbolo(const Coleta *c, const BibliotecasResolvidas *r, BibliotecaFalha *falha)
{
    /* simbolo_faltando < 0: quem pediu foi um membro de estatica, so ha o nome do simbolo */
    const char *simbolo = r->simbolo_faltando >= 0 ? c->simbolos[r->simbolo_faltando] : r->simbolo_faltando_nome;
    const IrExterno *e = NbExternoDoFaltante(c, r);
    const char *proc = e != NULL ? NbTexto(c->pedido, e->nome) : simbolo;

    DiagRelatar(&falha->diag, X0101, e != NULL ? NbPosDoExterno(c->pedido->fonte, e) : 0, "simbolo '%s' do proc foreign '%s' nao encontrado nas bibliotecas do 'link'",
                simbolo, proc);
    NbCampoTexto(&falha->campos, "proc", proc);
    BufAnexar(&falha->campos, ", ");
    NbCampoTexto(&falha->campos, "simbolo", simbolo);
    BufAnexar(&falha->campos, ", ");
    NbCampoProcureiEm(&falha->campos, &r->procurei_em);
}

/* X0102, X0103 e X0105: o 'link' que falhou; a posicao e a do 1o foreign de fora (o 'link' em si nao tem posicao na IR). */
static void NbFalhaLink(const Coleta *c, const BibliotecasResolvidas *r, BibliotecaFalha *falha)
{
    const IrExterno *e = NbPrimeiroDeFora(c->pedido->ir);

    falha->diag.off = e != NULL ? NbPosDoExterno(c->pedido->fonte, e) : 0;
    NbCampoTexto(&falha->campos, "biblioteca", r->link_falho);
    if (falha->diag.codigo == X0102) {
        BufAnexar(&falha->campos, ", ");
        NbCampoProcureiEm(&falha->campos, &r->procurei_em);
    }
}

static void NbMontarFalha(const Coleta *c, const BibliotecasResolvidas *r, BibliotecaFalha *falha)
{
    if (falha->diag.codigo == X0101)
        NbFalhaSimbolo(c, r, falha);
    else
        NbFalhaLink(c, r, falha);
}

/* M6d P92e: o mapa slot -> lib com os simbolos C de todo foreign `ends` declarado (IrPrograma.fins); LibmemMontar acha, pelo indice dos .a, quem define cada um. */
static void NbMontarLibmem(Coleta *c, BibliotecasResolvidas *saida)
{
    const IrPrograma *ir = c->pedido->ir;
    const char **fins = ArenaAlocar(&c->arena, (ir->nfins + 1) * sizeof *fins);

    for (uint32_t k = 0; k < ir->nfins; k++)
        fins[k] = NbTexto(c->pedido, ir->fins[k]);
    LibmemEntrada entrada = {.estaticas = &saida->estaticas, .fins = fins, .qtd_fins = ir->nfins};
    LibmemMontar(&entrada, &saida->memoria, &saida->libmem);
}

bool NativoBibliotecas(const NativoBibliotecasEntrada *entrada, BibliotecasResolvidas *saida, BibliotecaFalha *falha)
{
    Coleta c = {.pedido = entrada->pedido};
    bool ok;

    *falha = (BibliotecaFalha){0};
    NbColetarLinks(&c);
    NbColetarPastas(&c);
    NbColetarSimbolos(&c, entrada->bir);
    c.ped.provedores = entrada->provedores;
    c.ped.qtd_provedores = entrada->qtd_provedores;
    ok = BibliotecasResolver(&c.ped, saida, &falha->diag);
    if (!ok)
        NbMontarFalha(&c, saida, falha);
    else if (entrada->libmem)
        NbMontarLibmem(&c, saida);
    ArenaLiberar(&c.arena);
    return ok;
}

void BibliotecaFalhaLiberar(BibliotecaFalha *falha)
{
    BufLiberar(&falha->campos);
}

