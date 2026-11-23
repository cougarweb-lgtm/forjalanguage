/* ligar.c - ligador do backend nativo (P5): junta o forja_programa e o runtime numa Imagem PE32+ para PeEscrever.
 * Versao: 0.18.0
 * Changelog: 0.18.0 - M8 P2: LIGAR_PERFIL (objeto 15, LigarPedido.perfil: rt_win_perfil.o / rt_linux_perfil.o, o runtime do --perfil); LIGAR_QTD_OBJETOS segue ALCANCE_QTD_OBJETOS (16); sem a opcao recebe objeto vazio (byte a byte igual).
 *            0.17.0 - M6 P68: LigarElfDebug roda tambem no PE (as secoes de debug do programa vao para Imagem.dbg; o PE as escreve no fim, fora do .reloc).
 *            0.16.0 - M6 P66: secoes de debug do programa (OS_DEBUG_*) ficam sem saida no mapa e LigarElfDebug (ligar_elf.c) as copia para Imagem.dbg com as
 *                    relocacoes ADDR64 absolutas (D-M6-4); a relocacao delas nunca vira RELATIVE.
 *            0.15.0 - M5c-b P61: saida LIGAR_SAIDA_TLS (.tdata com a cauda .tbss); o TLS sem musl recusa com X0113; LigarElfPlanejarTls e LigarElfMarcarTls (ligar_elf.c) fecham o bloco.
 *            0.14.5 - M5b P41: im->pie = pedido->pie (ELF static-pie; a base e 0 e o ligador_elf grava as relocacoes RELATIVE).
 *            0.14.4 - M5b P39-B2 (D0654): LIGAR_SEMENTE (objeto 14, LigarPedido.semente: crt1.o do musl); com ele o _start e o do crt1.o (raiz da entrada) e o main
 *                    do objeto de entrada libc e alcancado por ele; LIGAR_QTD_OBJETOS segue ALCANCE_QTD_OBJETOS (15).
 *            0.14.3 - M5b P39: sem LigarPedido.entrada no ELF (pedido sintetico dos testes) o _start continua vindo do runtime; a regra e a da 0.14.2.
 *            0.14.2 - M5b P39: LIGAR_ENTRADA (objeto 13, LigarPedido.entrada: o _start do ELF, rt_linux_entrada_cru.o); no ELF a entrada e dele, nao do runtime; LIGAR_QTD_OBJETOS segue ALCANCE_QTD_OBJETOS (14); PE recebe um objeto vazio no 13 (byte a byte igual).
 *            0.14.1 - M5b P37d: no ELF a saida da secao sai do tipo (LigarSaidaElf, ligar_elf.c), nao do nome (.rodata e .data.rel.ro davam X0903); TLS sem saida = X0111 (LigarElfSemSaida). Linha so trocada, sem crescer. 0.14.0 - M5b P36: o ELF sai para ligar_elf.c (relocacoes, celulas .got, simbolos do ligador, coleta e formato) e o estado para ligar_estado.h; saidas .init_array
 *                    (LIGAR_SAIDA_INIT_ARRAY, RW com bytes) e .got (LIGAR_SAIDA_GOT); a ordem do ELF poe R, RX, RW com bytes e RW sem bytes. PE byte a byte igual.
 *            0.13.8 - M5b P35: LigarValorReloc ganha default: (sem caso novo); os tipos ELF de objeto.h 0.4.0 caem no FalhaInterna do fim da funcao, e o ligar_elf do P36 os aplica.
 *            0.13.7 - M5 P53: LIGAR_NET (objeto 12, LigarPedido.net: rt_win_net.o, std:net UDP); a WS2_32 e a 3a tabela fixa so com ele (LigarTabelasFixas); a WS2_32 do 'link' (libcurl) funde-se nela (LigarJuntarTabelas, via ligar_tabelas.c), um descritor so; LIGAR_QTD_OBJETOS segue ALCANCE_QTD_OBJETOS (13).
 *            0.13.6 - M5 P52: LIGAR_VARENA (objeto 11, LigarPedido.varena: rt_win_varena.o, std:mem VirtualArena); LIGAR_QTD_OBJETOS segue ALCANCE_QTD_OBJETOS (12). Os imports VirtualAlloc e VirtualFree ja estao em LIGAR_KERNEL32_M1 (ligar_tabelas.c): so entram no .idata de quem os alcanca.
 *            0.13.5 - M5 P50: LIGAR_ATOMIC (objeto 10, LigarPedido.atomic: rt_win_atomic.o, std:atomic); LIGAR_QTD_OBJETOS segue ALCANCE_QTD_OBJETOS (11).
 *            0.13.4 - M5 P49: LIGAR_SYNC (objeto 9, LigarPedido.sync: rt_win_sync.o, std:sync); LIGAR_QTD_OBJETOS segue ALCANCE_QTD_OBJETOS (10).
 *            0.13.3 - M5 P47: LIGAR_THREAD (objeto 8, LigarPedido.thread: rt_win_thread.o, std:thread); LIGAR_QTD_OBJETOS segue ALCANCE_QTD_OBJETOS (9).
 * Changelog: 0.13.2 - M5 P43: as tabelas fixas de __imp_ (LIGAR_KERNEL32_M1, LIGAR_ADVAPI32_M1, LIGAR_TABELAS e as constantes) saem para ligar_tabelas.c/.h (o arquivo estava em 799 linhas); a KERNEL32 passa de 61 para 64 imports (TlsAlloc, TlsGetValue, TlsSetValue: slot do RtFio).
 *            0.13.1 - M5 P28b-2b: KERNEL32_M1 de 60 para 61 imports (K32GetProcessMemoryInfo, no fim da tabela: peak_memory do std:os); so entra no .idata de quem o referencia, e nao abre DLL nova. 0.13.0 - A52: KERNEL32_M1 de 55 para 60 imports (GetCurrentDirectoryW ... SetEnvironmentVariableW, no fim da tabela: std:os) e 2a tabela fixa, ADVAPI32.dll com SystemFunction036 (random_bytes); LIGAR_QTD_TABELAS 2 (IMAGEM_MAX_DLLS de 16 para 17 no pe.h: o teto de DLLs de 'link' do usuario segue 15); so entram no .idata de quem os referencia.
 *            0.12.0 e 0.12.1 - S3 P5b-1 e POS-Z A51: 8o objeto, o handle e o fs do std:file (LigarPedido.es: rt_win_es.o); KERNEL32_M1 de 28 para 55 imports (SetFilePointerEx ... PeekNamedPipe, no fim da tabela: std:file e std:process); so entram no .idata de quem os referencia.
 *            0.11.0 - L11a-2: 7o objeto, a libm do musl (LigarPedido.math: rt_math.o); sem ele a ligacao e a de antes (objeto vazio, byte a byte).
 *            0.10.1 e 0.10.2 - IA1: KERNEL32_M1 de 26 para 28 imports (Sleep e GetSystemTimePreciseAsFileTime, no fim da tabela: sleep_ms e wall_time_ns do std:os); so entram no .idata de quem os referencia.
 *            0.10.0 - IA1-L5a: 6o objeto, o curto (LigarPedido.curto: rt_win_curto.o, print curto de real e eprint); sem ele a ligacao e a de antes (objeto vazio, byte a byte).
 *            0.9.0 - M5 P27a: LigarPedido.formato. No IMAGEM_ELF: base 0x400000 (PeBaseDaImagem), entrada _start do runtime, secoes na ordem .rdata .text .data .bss, sem a KERNEL32 nem imports, descarta .pdata/.xdata (e as ADDR32NB
 *                    delas), nao registra DIR64 e devolve os simbolos vivos na Imagem; X0903 com tabelas de imports ou DLL. A ordenacao dos DIR64 e a conferencia do .pdata (so do PE) foram para ligar_pe.c; PE byte a byte igual.
 *            0.8.2 e 0.8.3 - M5 P26b/P26c: KERNEL32_M1 de 17 para 26 imports (GetCommandLineW ... GetLastError, no fim da tabela: std:proc e std:arquivo); so entram no .idata de quem importa o modulo.
 *            0.8.1- M3b P23y-2: LigarConferirLimites: mais de IMAGEM_MAX_DLLS tabelas ou de ALCANCE_MAX_IMPORTS nomes importados e X0109 (era FalhaInterna, alcancavel pelos 'link'/foreign do usuario);
 *                    comentario da tabela da KERNEL32 corrigido (nao e a unica fonte de __imp_).
 *            0.8.0 - M3b P23f: 5o objeto, o legado (LigarPedido.legado: stubs da toolchain, sprintf e sscanf sobre a UCRT); a entrada do PE (e a raiz do alcance) vem de LigarEscolherEntrada: a do runtime ou,
 *                    se o objeto de terceiros tem forja_ctors_ini (construtores), a forja_entrada_ctor / forja_entrada_dll_ctor do legado; as raizes saem de tabelas estaticas para LigacaoEstado.raizes.
 *            0.7.0 - M3b P23e: 4o objeto (LigarPedido.terceiros, a uniao dos membros de estaticas; sem ele entra um objeto vazio como no Ryu); __imp_X definido por outro objeto vence a tabela de imports;
 *                    relocacoes SECTION (2 bytes) e SECREL (4 bytes); secao "<base>.<sufixo>" (.text.unlikely) vai para a saida da base.
 *            0.6.0 e 0.6.1 - M3 F5: 3o objeto, o Ryu (LigarPedido.ryu, opcional), mapeado, resolvido e relocado como o runtime; simbolo externo vem do 1o outro objeto que o define; decisoes de LigarAplicarReloc, LigarExportar e LigarComTabelas extraidas.
 *            0.5.0 - M3 P22a: DLL (LigarPedido.dll): a entrada e a forja_entrada_dll, os ADDR64 levam a ImageBase da DLL (PeBaseDaImagem) e LigarExportar monta a .edata com os exports do pedido
 *                    (cada um tem de ser global definido no programa; X0903 se faltar ou repetir). Ligar de exe segue com os mesmos bytes.
 *            0.4.0 - M3 P19: LigarComTabelas(LigarPedido) junta as DLLs dos 'link' depois da KERNEL32 do runtime (Ligar fica como o pedido sem tabelas extras); imports_usados em bitmap
 *                    (AlcanceImportUsado) e teto de 256 imports; texto do X0903 de import fora das tabelas.
 *            0.3.0 - M3 P19a: ImportTabela no lugar do KERNEL32 fixo: LIGAR_TABELAS (hoje so a KERNEL32 do runtime) vira o "plano" de __imp_ aceitos (um indice por
 *                    simbolo, que e o bit do Alcance) e LigarImportar monta um descritor por DLL com algum import alcancavel (KERNEL32 primeiro); LigarImportacoes sai.
 *            0.2.1 e 0.2.2 - L16: KERNEL32_M1 de 15 para 17 imports (GetEnvironmentVariableA e AddVectoredExceptionHandler, no fim da tabela: o forja test); so entram no .idata de quem roda test.
 *            0.2.0 - M2 P17c (G4): descarta secao inalcancavel (ligar_alcance.c: raizes por tabela, relocacoes COFF, .pdata sobe com o codigo) e monta o .idata so dos
 *                    __imp_ alcancaveis; secao "<base>$<sufixo>" vai para a saida do nome-base; sem teto de 32 secoes por objeto (o runtime tem ~440);
 *                    achar simbolo por hash (era O(S^2)); LigarImportacoes passa a ler da Imagem (o .idata ja nao tem sempre os 15 nomes);
 *                    Imagem.runtime_texto_rva (onde o runtime comeca no .text, para o --debug-code).
 *            0.1.1 a 0.1.5 - M2 P9a e antes: LigarCriarSecao devolve bool (X0905), LigarPreparar recebe o estado preenchido, PeSecaoPedido, LIGAR_CARAC_* saem (PE_SECAO_* do pe.h), LigarImportacoes, KERNEL32_M1 de 12 para 15 imports (console em UTF-8).
 *             0.1.0 - backend x64 P5: Ligar (secoes na ordem do PE; __imp_ so pela tabela KERNEL32_M1; REL32, ADDR32NB e ADDR64 com DIR64; entrada em forja_entrada; .pdata ordenado ou X0905).
 * Verificacao: construir.bat && teste_nativo.exe obj <pasta> e python -I ..\tests\nativo\obj\verificar_obj.py
 */
#include <stdlib.h>
#include <string.h>

#include "nativo/obj/ligar_alcance.h"
#include "nativo/obj/ligar_elf.h"
#include "nativo/obj/ligar_pe.h"
#include "nativo/obj/ligar_tabelas.h"
#include "nativo/obj/terceiros_ctors.h"

#define LIGAR_SEPARADOR_GRUPO "$."
/* A entrada do ELF: o _start do runtime Linux (asm; chama forja_entrada pela convencao Win64, D-M5-3). */
#define LIGAR_ENTRADA_ELF "_start"
#define LIGAR_PREFIXO_IMP ALCANCE_PREFIXO_IMP
#define LIGAR_TAM_PREFIXO_IMP ALCANCE_TAM_PREFIXO_IMP
#define LIGAR_TAM_CAMPO_DIR64 8u
#define LIGAR_TAM_CAMPO_32 4u
#define LIGAR_TAM_CAMPO_16 2u
#define LIGAR_TERCEIROS 3
#define LIGAR_LEGADO 4
#define LIGAR_CURTO 5
#define LIGAR_MATH 6
#define LIGAR_ES 7
#define LIGAR_THREAD 8
#define LIGAR_SYNC 9
#define LIGAR_ATOMIC 10
#define LIGAR_VARENA 11
#define LIGAR_NET 12
#define LIGAR_ENTRADA 13 /* M5b P39: o objeto de entrada do ELF (rt_linux_entrada_cru.o ou rt_linux_entrada_libc.o); no PE e vazio */
#define LIGAR_SEMENTE 14 /* M5b P39-B2: crt1.o do musl (o _start da variante libc); no PE e no ELF sem libc e vazio */
#define LIGAR_PERFIL 15 /* M8 P2: o runtime do --perfil (rt_win_perfil.o / rt_linux_perfil.o); sem a opcao e vazio */
#define LIGAR_TAM_ZEROS 256
#define LIGAR_BITS_BYTE 8u

/* As tabelas FIXAS de __imp_ (KERNEL32 e ADVAPI32 do runtime) moram em ligar_tabelas.c (P43). */
_Static_assert(LIGAR_QTD_IMPORTS <= ALCANCE_MAX_IMPORTS, "a tabela de imports tem de caber no bitmap do Alcance");
_Static_assert(ALCANCE_MAX_IMPORTS == IMAGEM_MAX_IMPORTS, "o bitmap do Alcance e a IAT da Imagem aceitam o mesmo numero de imports");
_Static_assert(LIGAR_QTD_TABELAS <= IMAGEM_MAX_DLLS, "as tabelas de imports tem de caber nos descritores da Imagem");

/* Raizes da alcancabilidade (G4), em LigacaoEstado.raizes (ligar_estado.h): o programa entra inteiro e a entrada entra com o que ela alcanca.
 * O .pdata/.xdata nao e raiz: sobe com o codigo que descreve. */

/* A entrada do PE, por [dll][ctor]: o runtime (forja_entrada; numa DLL o DllMain forja_entrada_dll, e o forja_programa que a forja_entrada chama nao entra) ou, quando um membro
 * de estatica tem construtor (forja_ctors_ini no objeto de terceiros), a entrada do legado que roda os construtores antes. */
static const char *const LIGAR_NOMES_ENTRADA[2][2] = {
    {"forja_entrada", "forja_entrada_ctor"},
    {"forja_entrada_dll", "forja_entrada_dll_ctor"},
};

/* Saidas possiveis das secoes (os indices LIGAR_SAIDA_*): .text .rdata .data .pdata .xdata .bss (o .idata vem depois, o .reloc sai do PeEscrever). */
typedef struct {
    const char *nome;
    uint32_t carac;
} LigarSaida;

static const LigarSaida LIGAR_SAIDAS[LIGAR_QTD_SAIDA] = {
    {".text", PE_SECAO_TEXTO},
    {".rdata", PE_SECAO_LEITURA},
    {".data", PE_SECAO_DADOS},
    {".pdata", PE_SECAO_LEITURA},
    {".xdata", PE_SECAO_LEITURA},
    {".bss", PE_SECAO_BSS},
    {".init_array", PE_SECAO_DADOS},
    {".got", PE_SECAO_DADOS},
    {".tdata", PE_SECAO_DADOS},
};

/* A ordem em que as saidas viram secoes (e ganham RVA), por formato: o PE segue a de sempre; o ELF poe a leitura antes do codigo, e as RW com bytes antes das sem bytes, para os 3 PT_LOAD (R, RX, RW) saiam sem pagina sobrando. */
static const int LIGAR_ORDEM_SAIDAS[2][LIGAR_QTD_SAIDA] = {
    [IMAGEM_PE] = {0, 1, 2, 3, 4, 5, 6, 7, 8},
    [IMAGEM_ELF] = {1, 0, 2, 6, 7, 8, 5, 3, 4},
};

static uint32_t LigarAlinhar(uint32_t v, uint32_t alinhamento)
{
    return (v + alinhamento - 1) / alinhamento * alinhamento;
}

static LigarContrib *LigarContribDe(const LigacaoEstado *L, int o, uint32_t sec)
{
    return VecEm(&L->contrib[o], sec);
}

/* Quantas tabelas fixas o formato traz: a KERNEL32 do runtime no PE; o ELF fala com o kernel por syscall e nao importa nada. */
static uint32_t LigarTabelasFixas(const LigarPedido *pedido)
{
    if (pedido->formato != IMAGEM_PE)
        return 0;
    return pedido->net != NULL ? LIGAR_QTD_TABELAS : LIGAR_QTD_TABELAS_BASE;
}

/* As tabelas em fila: LIGAR_TABELAS (a KERNEL32 do runtime, so no PE; a WS2_32 so com o std:net) e depois as do pedido (as DLLs dos 'link', na ordem dos link). */
static void LigarJuntarTabelas(LigacaoEstado *L, const LigarPedido *pedido)
{
    uint32_t fixas = LigarTabelasFixas(pedido);

    if (fixas + pedido->qtd_tabelas > IMAGEM_MAX_DLLS)
        FalhaInterna("ligar: %u tabelas de imports; a Imagem aceita %d DLLs", fixas + pedido->qtd_tabelas, IMAGEM_MAX_DLLS);
    memcpy(L->tabelas, LIGAR_TABELAS, fixas * sizeof *LIGAR_TABELAS);
    L->qtd_tabelas = fixas;
    for (uint32_t t = 0; t < pedido->qtd_tabelas; t++) {
        const ImportTabela *tabela = &pedido->tabelas[t];

        if (fixas == LIGAR_QTD_TABELAS && LigarMesmaDll(tabela->dll, LIGAR_NOME_DLL_WS2_32))
            LigarFundirWs2(&L->tabelas[LIGAR_QTD_TABELAS_BASE], tabela, L->fundidos);
        else
            L->tabelas[L->qtd_tabelas++] = *tabela;
    }
}

/* Poe os simbolos de todas as tabelas em fila (a ordem das tabelas e a de dentro de cada uma): a 1a tabela com o nome vence, como o ld. */
static void LigarPlanejarImports(LigacaoEstado *L)
{
    for (uint32_t t = 0; t < L->qtd_tabelas; t++) {
        for (uint32_t s = 0; s < L->tabelas[t].qtd; s++) {
            if (L->qtd_plano == ALCANCE_MAX_IMPORTS)
                FalhaInterna("ligar: mais de %d imports nas tabelas", ALCANCE_MAX_IMPORTS);
            L->plano[L->qtd_plano++] = L->tabelas[t].simbolos[s];
        }
    }
}

/* Entrada do exe/DLL: a do runtime, ou a do legado se os membros de estatica registraram construtores (a tabela forja_ctors_ini/forja_ctors_fim de terceiros_ctors.c); no ELF, o _start do runtime. */
static void LigarEscolherEntrada(LigacaoEstado *L)
{
    const Objeto *terceiros = L->obj[LIGAR_TERCEIROS];
    bool ctor = ObjAcharSimbolo(terceiros, TERCEIROS_SIMBOLO_CTORS_INI) >= 0;

    L->objeto_entrada = ctor ? LIGAR_LEGADO : LIGAR_RUNTIME;
    L->nome_entrada = LIGAR_NOMES_ENTRADA[L->pedido->dll ? 1 : 0][ctor ? 1 : 0];
    if (L->pedido->formato == IMAGEM_ELF) {
        L->nome_entrada = LIGAR_ENTRADA_ELF;
        if (L->pedido->entrada != NULL) /* sem objeto de entrada (pedido sintetico dos testes) o _start vem do runtime, como antes */
            L->objeto_entrada = LIGAR_ENTRADA;
        if (L->pedido->semente != NULL) /* variante libc: o _start e o do crt1.o do musl; o main vem do objeto de entrada por alcance */
            L->objeto_entrada = LIGAR_SEMENTE;
    }
    L->raizes[0] = (AlcanceRaiz){LIGAR_PROGRAMA, NULL};
    L->raizes[1] = (AlcanceRaiz){L->objeto_entrada, L->nome_entrada};
}

/* Prepara o estado: programa, runtime, im, erro e tabelas ja vem gravados em L (quem chama os preenche ao zerar o estado). Calcula a alcancabilidade. */
static void LigarPreparar(LigacaoEstado *L)
{
    LigarPlanejarImports(L);
    LigarEscolherEntrada(L);
    VecIniciar(&L->celulas, sizeof(LigarCelula));
    for (int o = 0; o < LIGAR_QTD_OBJETOS; o++) {
        VecIniciar(&L->rva_sim[o], sizeof(uint32_t));
        VecIniciar(&L->contrib[o], sizeof(LigarContrib));
        for (size_t s = 0; s < L->obj[o]->secoes.n; s++)
            ((LigarContrib *)VecEmpurrar(&L->contrib[o]))->saida = LIGAR_SEM_SAIDA;
    }
    L->entrada.raizes = L->raizes;
    L->entrada.qtd_raizes = LIGAR_QTD_RAIZES;
    L->entrada.imports = L->plano;
    L->entrada.qtd_imports = L->qtd_plano;
    memcpy(L->entrada.obj, L->obj, sizeof L->obj);
    L->texto_runtime_off = LIGAR_SEM_RVA;
    AlcanceCalcular(&L->entrada, &L->alcance);
}

static void LigarLiberar(LigacaoEstado *L)
{
    for (int o = 0; o < LIGAR_QTD_OBJETOS; o++) {
        VecLiberar(&L->rva_sim[o]);
        VecLiberar(&L->contrib[o]);
    }
    VecLiberar(&L->celulas);
    AlcanceLiberar(&L->alcance);
}

/* Simbolo que uma secao viva cita e ninguem define: so agora e erro (referencia de secao descartada nao conta). */
static bool LigarConferirAlcance(const LigacaoEstado *L)
{
    if (L->alcance.indefinido != NULL)
        return DiagRelatar(L->erro, X0903, 0, "COFF: simbolo '%s' indefinido (nao esta no programa nem no runtime)", L->alcance.indefinido);
    if (L->alcance.import_fora != NULL)
        return DiagRelatar(L->erro, X0903, 0, "COFF: simbolo '%s' importado fora das tabelas de imports (KERNEL32 do runtime e DLLs dos 'link')", L->alcance.import_fora);
    return true;
}

/* ---------- secoes: ordem e alinhamento das contribuicoes ---------- */

/* A saida pelo nome-base: ".text$f" vai para ".text" (grupo de secao do gcc com -ffunction-sections e -fdata-sections) e ".text.unlikely" tambem. */
static int LigarIndiceSaida(const char *nome)
{
    for (int k = 0; k < LIGAR_SAIDA_TLS; k++) { /* o TLS (k 8) so vem do ELF, por tipo (LigarSaidaElf), nunca pelo nome */
        size_t base = strlen(LIGAR_SAIDAS[k].nome);
        if (strncmp(LIGAR_SAIDAS[k].nome, nome, base) == 0 && (nome[base] == '\0' || strchr(LIGAR_SEPARADOR_GRUPO, nome[base]) != NULL))
            return k;
    }
    return -1;
}

static uint32_t LigarTamanhoSecao(const ObjSecao *s)
{
    return s->tipo == OS_BSS || s->tipo == OS_TBSS ? s->tam_bss : (uint32_t)s->bytes.tam;
}

/* Guarda onde o runtime comeca no .text (so o --debug-code usa, para separar a faixa "runtime" do programa). */
static void LigarAnotarTextoRuntime(LigacaoEstado *L, int o, int k, uint32_t off)
{
    if (o == LIGAR_RUNTIME && k == LIGAR_SAIDA_TEXTO && L->texto_runtime_off == LIGAR_SEM_RVA)
        L->texto_runtime_off = off;
}

/* O .pdata e o .xdata (RUNTIME_FUNCTION e UNWIND_INFO do Win64) so existem no PE. */
static bool LigarSaidaSoDoPe(const LigacaoEstado *L, int k)
{
    return L->pedido->formato == IMAGEM_ELF && (k == LIGAR_SAIDA_PDATA || k == LIGAR_SAIDA_XDATA);
}

/* Cada contribuicao viva entra no alinhamento dela, na saida que o nome da secao escolhe; a morta fica sem saida. */
static bool LigarMapearSecao(LigacaoEstado *L, int o, uint32_t sec)
{
    if (!AlcanceVivo(&L->alcance, o, sec))
        return true;
    const ObjSecao *s = VecEm(&L->obj[o]->secoes, sec);
    if (s->tipo >= OS_DEBUG_ABBREV) /* M6 P66 (D-M6-4): secao de debug nao tem saida; LigarElfDebug a copia para Imagem.dbg */
        return true;
    int k = L->pedido->formato == IMAGEM_ELF ? LigarSaidaElf(s) : LigarIndiceSaida(s->nome);
    if (k < 0)
        return L->pedido->formato == IMAGEM_ELF ? LigarElfSemSaida(L, s) : DiagRelatar(L->erro, X0903, 0, "COFF: secao '%s' sem lugar no PE (use .text, .rdata, .data, .pdata, .xdata ou .bss)", s->nome);
    if (k == LIGAR_SAIDA_BSS && s->tipo != OS_BSS)
        return DiagRelatar(L->erro, X0903, 0, "COFF: secao '%s' com conteudo dentro de .bss", s->nome);
    if (k == LIGAR_SAIDA_TLS && !LigarElfTlsPermitido(L, s))
        return false;
    if (LigarSaidaSoDoPe(L, k))
        return true; /* ELF: o unwind do Win64 nao tem lugar (D-M5-7); a secao fica sem saida e as relocacoes dela (ADDR32NB) nao se aplicam */
    LigarContrib *c = LigarContribDe(L, o, sec);
    c->saida = k;
    c->off = LigarAlinhar(L->tam[k], s->alinhamento);
    L->tam[k] = c->off + LigarTamanhoSecao(s);
    LigarAnotarTextoRuntime(L, o, k, c->off);
    return true;
}

static bool LigarMapearObjeto(LigacaoEstado *L, int o)
{
    const Vec *secs = &L->obj[o]->secoes;
    for (uint32_t s = 0; s < secs->n; s++) {
        if (!LigarMapearSecao(L, o, s))
            return false;
    }
    return true;
}

static void LigarZeros(Buf *b, size_t n)
{
    static const char zeros[LIGAR_TAM_ZEROS] = {0};
    while (n > 0) {
        size_t parte = n < LIGAR_TAM_ZEROS ? n : LIGAR_TAM_ZEROS;
        BufAnexarN(b, zeros, parte);
        n -= parte;
    }
}

/* Preenche ate o offset da contribuicao e anexa o conteudo (BSS vira zeros). */
static void LigarPegar(const LigacaoEstado *L, int o, uint32_t sec, Buf *b)
{
    const ObjSecao *s = VecEm(&L->obj[o]->secoes, sec);
    if (s->tipo == OS_TBSS) /* M5c-b P61: o .tbss so ocupa memoria (cauda do bloco TLS), nao tem bytes */
        return;
    LigarZeros(b, LigarContribDe(L, o, sec)->off - b->tam);
    if (s->tipo == OS_BSS)
        LigarZeros(b, s->tam_bss);
    else if (s->bytes.tam > 0)
        BufAnexarN(b, s->bytes.dados, s->bytes.tam);
}

static void LigarJuntar(const LigacaoEstado *L, int k, Buf *b)
{
    for (int o = 0; o < LIGAR_QTD_OBJETOS; o++) {
        for (uint32_t sec = 0; sec < L->obj[o]->secoes.n; sec++) {
            if (LigarContribDe(L, o, sec)->saida == k)
                LigarPegar(L, o, sec, b);
        }
    }
}

static bool LigarCriarSecao(LigacaoEstado *L, int k)
{
    Buf b = {0};
    bool bss = k == LIGAR_SAIDA_BSS;
    if (!bss)
        LigarJuntar(L, k, &b);
    if (k == LIGAR_SAIDA_GOT) /* celulas .got: zeros aqui, o endereco entra depois de resolver (LigarElfPreencherGot) */
        LigarZeros(&b, L->tam[k]);
    PeSecaoPedido pedido = {LIGAR_SAIDAS[k].nome, LIGAR_SAIDAS[k].carac, bss ? NULL : &b, L->tam[k]};
    uint32_t rva = ImagemSecao(L->im, &pedido, L->erro);
    BufLiberar(&b);
    if (rva == 0)
        return false;
    L->rva[k] = rva;
    L->idx_im[k] = (uint32_t)(L->im->qtd_secoes - 1);
    return true;
}

/* Os imports alcancaveis, em fila (DLL por DLL): n deles, e k = indice do plano ja visto. */
typedef struct {
    const char *usados[ALCANCE_MAX_IMPORTS];
    uint32_t n, k;
} LigarUsados;

static void LigarSelecionarTabela(LigacaoEstado *L, const ImportTabela *tabela, LigarUsados *u)
{
    for (uint32_t s = 0; s < tabela->qtd; s++, u->k++) {
        L->slot_import[u->k] = -1;
        if (!AlcanceImportUsado(&L->alcance, u->k))
            continue;
        L->slot_import[u->k] = (int32_t)u->n;
        u->usados[u->n++] = L->plano[u->k];
    }
}

/* So os __imp_ alcancaveis entram no .idata (G4): um descritor por DLL que tem algum, na ordem das tabelas; slot_import diz onde cada um ficou. */
static void LigarImportar(LigacaoEstado *L)
{
    LigarUsados u = {.n = 0};
    PeImportDll dlls[IMAGEM_MAX_DLLS];
    uint32_t n_dlls = 0;
    for (uint32_t t = 0; t < L->qtd_tabelas; t++) {
        uint32_t ini = u.n;
        LigarSelecionarTabela(L, &L->tabelas[t], &u);
        if (u.n > ini)
            dlls[n_dlls++] = (PeImportDll){L->tabelas[t].dll, &u.usados[ini], u.n - ini};
    }
    if (n_dlls > 0 && !PeReservarImportacoesDlls(L->im, dlls, n_dlls))
        FalhaInterna("ligar: os %u imports alcancaveis nao couberam no .idata", u.n);
}

static bool LigarMapearObjetos(LigacaoEstado *L)
{
    for (int o = 0; o < LIGAR_QTD_OBJETOS; o++) {
        if (!LigarMapearObjeto(L, o))
            return false;
    }
    return true;
}

static bool LigarMontarSecoes(LigacaoEstado *L)
{
    if (!LigarMapearObjetos(L))
        return false;
    LigarElfPlanejarGot(L); /* ELF: o tamanho da .got sai das relocacoes vivas, antes de criar as secoes */
    LigarElfPlanejarTls(L); /* ELF: .tdata e .tbss no bloco TLS (offsets e tamanho), antes de criar a secao */
    for (int i = 0; i < LIGAR_QTD_SAIDA; i++) {
        int k = LIGAR_ORDEM_SAIDAS[L->pedido->formato][i];
        if (L->tam[k] > 0 && !LigarCriarSecao(L, k))
            return false;
    }
    LigarElfMarcarTls(L); /* ELF: a Imagem guarda a secao TLS e o p_align do PT_TLS */
    if (L->texto_runtime_off != LIGAR_SEM_RVA)
        L->im->runtime_texto_rva = L->rva[LIGAR_SAIDA_TEXTO] + L->texto_runtime_off;
    LigarImportar(L);
    return true;
}

/* ---------- simbolos: RVA de cada um ---------- */

/* RVA de um simbolo definido: a secao de saida, mais o offset da contribuicao, mais o offset do simbolo. */
static uint32_t LigarRvaDefinido(const LigacaoEstado *L, int o, const ObjSimbolo *s)
{
    const LigarContrib *c = LigarContribDe(L, o, (uint32_t)s->secao);
    if (c->saida == LIGAR_SEM_SAIDA || L->tam[c->saida] == 0)
        return LIGAR_SEM_RVA;
    return L->rva[c->saida] + c->off + s->off;
}

/* Global definido com esse nome no objeto o; NULL se nao tem. Locais (nome com '.') nunca casam entre objetos. */
static const ObjSimbolo *LigarAcharDefinido(const LigacaoEstado *L, int o, const char *nome)
{
    int64_t i = ObjAcharSimbolo(L->obj[o], nome);
    if (i < 0)
        return NULL;
    const ObjSimbolo *s = VecEm(&L->obj[o]->simbolos, (size_t)i);
    return s->global && s->secao != OBJ_SECAO_INDEFINIDA ? s : NULL;
}

/* __imp_X: o slot da IAT desse nome no plano das tabelas, ou LIGAR_SEM_RVA se nenhuma secao viva o usa (o erro de nome fora das tabelas vem do Alcance). */
static uint32_t LigarRvaImport(const LigacaoEstado *L, const ObjSimbolo *s)
{
    const char *nome = s->nome + LIGAR_TAM_PREFIXO_IMP;
    for (uint32_t k = 0; k < L->qtd_plano; k++) {
        if (strcmp(L->plano[k], nome) == 0 && L->slot_import[k] >= 0)
            return L->im->importacao.iat_rva[L->slot_import[k]];
    }
    return LIGAR_SEM_RVA;
}

/* Indefinido que nao e __imp_: o global definido em outro objeto (o 1o na ordem programa, runtime, Ryu); sem ele LIGAR_SEM_RVA (o erro vem do Alcance se uma secao viva o cita). */
static uint32_t LigarRvaExterno(const LigacaoEstado *L, int o, const ObjSimbolo *s)
{
    for (int outro = 0; outro < LIGAR_QTD_OBJETOS; outro++) {
        const ObjSimbolo *achado = outro == o ? NULL : LigarAcharDefinido(L, outro, s->nome);
        if (achado != NULL)
            return LigarRvaDefinido(L, outro, achado);
    }
    return LIGAR_SEM_RVA;
}

static bool LigarDuplicado(const LigacaoEstado *L, int o, const ObjSimbolo *s)
{
    for (int outro = 0; outro < LIGAR_QTD_OBJETOS; outro++) {
        if (outro != o && s->global && LigarAcharDefinido(L, outro, s->nome) != NULL)
            return true;
    }
    return false;
}

static bool LigarResolverSimbolo(const LigacaoEstado *L, int o, const ObjSimbolo *s, uint32_t *rva)
{
    if (s->secao >= 0) {
        if (LigarDuplicado(L, o, s))
            return DiagRelatar(L->erro, X0903, 0, "COFF: simbolo '%s' definido nos dois objetos", s->nome);
        *rva = LigarRvaDefinido(L, o, s);
        return true;
    }
    *rva = LigarRvaExterno(L, o, s);
    if (*rva == LIGAR_SEM_RVA && strncmp(s->nome, LIGAR_PREFIXO_IMP, LIGAR_TAM_PREFIXO_IMP) == 0)
        *rva = LigarRvaImport(L, s);
    if (*rva == LIGAR_SEM_RVA) /* ELF: __init_array_start e similares, dados pelo proprio ligador */
        LigarElfSimboloDoLigador(L, s->nome, rva);
    return true;
}

static bool LigarResolverObjeto(LigacaoEstado *L, int o)
{
    const Vec *sims = &L->obj[o]->simbolos;
    for (size_t j = 0; j < sims->n; j++) {
        uint32_t rva = 0;
        if (!LigarResolverSimbolo(L, o, VecEm(sims, j), &rva))
            return false;
        *(uint32_t *)VecEmpurrar(&L->rva_sim[o]) = rva;
    }
    return true;
}

static bool LigarResolverSimbolos(LigacaoEstado *L)
{
    for (int o = 0; o < LIGAR_QTD_OBJETOS; o++) {
        if (!LigarResolverObjeto(L, o))
            return false;
    }
    return true;
}

/* ---------- relocacoes ---------- */

/* REL32 = S+A-(P+4); ADDR32NB = S+A (RVA); ADDR64 = ImageBase+S+A (VA absoluta, com entrada DIR64). base = a ImageBase da imagem (exe ou DLL).
 * SECTION e SECREL dependem das secoes de saida e saem de LigarValorSecao. */
static uint64_t LigarValorReloc(const ObjCorrecao *c, uint32_t alvo, uint32_t p, uint64_t base)
{
    int64_t s = alvo;
    int64_t a = c->adendo;
    switch (c->tipo) {
    case OR_REL32:
        return (uint64_t)(s + a - ((int64_t)p + 4));
    case OR_ADDR32NB:
        return (uint64_t)(s + a);
    case OR_ADDR64:
        return (uint64_t)((int64_t)base + s + a);
    case OR_SECTION:
    case OR_SECREL:
    default:
        break;
    }
    FalhaInterna("ligar: tipo de relocacao %d desconhecido", (int)c->tipo);
}

/* O campo da relocacao: 8 bytes (ADDR64), 2 (SECTION) ou 4. */
static uint32_t LigarTamanhoCampo(ObjTipoReloc tipo)
{
    switch (tipo) {
    case OR_ADDR64:
        return LIGAR_TAM_CAMPO_DIR64;
    case OR_SECTION:
        return LIGAR_TAM_CAMPO_16;
    default:
        return LIGAR_TAM_CAMPO_32;
    }
}

/* SECTION = numero (1-based) da secao do PE que contem o alvo; SECREL = distancia do alvo (mais o adendo) ate o inicio dessa secao. */
static bool LigarValorSecao(const LigacaoEstado *L, const ObjCorrecao *c, uint32_t alvo, uint64_t *valor)
{
    for (int k = 0; k < LIGAR_QTD_SAIDA; k++) {
        if (L->tam[k] == 0 || alvo < L->rva[k] || alvo - L->rva[k] >= L->tam[k])
            continue;
        *valor = c->tipo == OR_SECTION ? (uint64_t)L->idx_im[k] + 1 : (uint64_t)((int64_t)(alvo - L->rva[k]) + c->adendo);
        return true;
    }
    return DiagRelatar(L->erro, X0903, 0, "COFF: relocacao SECTION/SECREL para um simbolo fora das secoes de saida (RVA 0x%x)", alvo);
}

/* O RVA do alvo e o RVA do campo a corrigir. */
typedef struct {
    uint32_t alvo;
    uint32_t campo;
} LigarPonto;

static bool LigarValorDe(const LigacaoEstado *L, const ObjReloc *r, LigarPonto ponto, uint64_t *valor)
{
    if (r->correcao.tipo == OR_SECTION || r->correcao.tipo == OR_SECREL)
        return LigarValorSecao(L, &r->correcao, ponto.alvo, valor);
    *valor = LigarValorReloc(&r->correcao, ponto.alvo, ponto.campo, PeBaseDaImagem(L->im));
    return true;
}

static void LigarEscreverCampo(uint8_t *p, uint32_t n, uint64_t v)
{
    for (uint32_t k = 0; k < n; k++)
        p[k] = (uint8_t)(v >> (k * LIGAR_BITS_BYTE));
}

/* O campo de 'tam' bytes em sec_off tem de caber na secao de saida k; senao e erro interno do ligador. */
static void LigarConferirCampoNaSecao(const LigacaoEstado *L, int k, uint32_t sec_off, uint32_t tam)
{
    const PeSecao *ps = &L->im->secoes[L->idx_im[k]];

    if (L->tam[k] == 0 || sec_off + tam > ps->bytes.tam)
        FalhaInterna("ligar: relocacao em 0x%x fora da secao de saida %d", sec_off, k);
}

/* Relocacao de secao descartada nao se aplica (a secao nao esta no exe). */
static bool LigarAplicarReloc(LigacaoEstado *L, int o, const ObjReloc *r)
{
    const LigarContrib *c = LigarContribDe(L, o, r->alvo.secao);
    if (c->saida == LIGAR_SEM_SAIDA)
        return true;
    if (L->pedido->formato == IMAGEM_ELF) /* o ELF tem as proprias relocacoes (ABS32, GOTPCRELX, celulas .got...): ligar_elf.c */
        return LigarElfRelocar(L, o, r);
    int k = c->saida;
    uint32_t sec_off = c->off + r->alvo.off;
    uint32_t alvo = *(const uint32_t *)VecEm(&L->rva_sim[o], r->correcao.simbolo);
    if (alvo == LIGAR_SEM_RVA)
        return DiagRelatar(L->erro, X0903, 0, "COFF: relocacao aponta para o simbolo '%s', que cai numa secao vazia",
                           ((const ObjSimbolo *)VecEm(&L->obj[o]->simbolos, r->correcao.simbolo))->nome);
    PeSecao *ps = &L->im->secoes[L->idx_im[k]];
    bool dir64 = r->correcao.tipo == OR_ADDR64;
    uint32_t tam = LigarTamanhoCampo(r->correcao.tipo);
    LigarConferirCampoNaSecao(L, k, sec_off, tam);
    uint32_t p = L->rva[k] + sec_off;
    uint64_t valor = 0;
    if (!LigarValorDe(L, r, (LigarPonto){alvo, p}, &valor))
        return false;
    LigarEscreverCampo((uint8_t *)ps->bytes.dados + sec_off, tam, valor);
    if (dir64 && L->pedido->formato == IMAGEM_PE) /* o ELF ET_EXEC tem endereco fixo: nada a relocar na carga */
        *(uint32_t *)VecEmpurrar(&L->im->dir64) = p;
    return true;
}

static bool LigarAplicarRelocs(LigacaoEstado *L)
{
    LigarElfPreencherGot(L);
    LigarElfDebug(L); /* M6 P66/P68: as secoes de debug do programa (ELF e PE), com os enderecos ja resolvidos; sem secao de debug, nao faz nada */
    for (int o = 0; o < LIGAR_QTD_OBJETOS; o++) {
        const Vec *rels = &L->obj[o]->relocs;
        for (size_t i = 0; i < rels->n; i++) {
            if (!LigarAplicarReloc(L, o, VecEm(rels, i)))
                return false;
        }
    }
    return true;
}

/* ---------- entrada e simbolos ---------- */

static bool LigarEntrada(LigacaoEstado *L)
{
    const char *nome = L->nome_entrada;
    const ObjSimbolo *e = LigarAcharDefinido(L, L->objeto_entrada, nome);
    if (e == NULL)
        return DiagRelatar(L->erro, X0903, 0, "COFF: simbolo '%s' indefinido no runtime (sem entrada)", nome);
    uint32_t rva = LigarRvaDefinido(L, L->objeto_entrada, e);
    if (rva == LIGAR_SEM_RVA)
        return DiagRelatar(L->erro, X0903, 0, "COFF: simbolo '%s' cai numa secao vazia", nome);
    L->im->entrada_rva = rva;
    return true;
}

/* ---------- exports da DLL ---------- */

/* Cada export do pedido vira um PeExporta em 'simbolos'; para no 1o que nao esta definido no programa (X0903). */
static bool LigarColetarExports(LigacaoEstado *L, Vec *simbolos)
{
    const LigarPedido *p = L->pedido;
    bool ok = true;

    for (uint32_t k = 0; k < p->qtd_exports && ok; k++) {
        const ObjSimbolo *s = LigarAcharDefinido(L, LIGAR_PROGRAMA, p->exports[k]);
        uint32_t rva = s != NULL ? LigarRvaDefinido(L, LIGAR_PROGRAMA, s) : LIGAR_SEM_RVA;

        if (rva == LIGAR_SEM_RVA)
            ok = DiagRelatar(L->erro, X0903, 0, "COFF: export '%s' nao esta definido no programa", p->exports[k]);
        else
            *(PeExporta *)VecEmpurrar(simbolos) = (PeExporta){p->exports[k], rva};
    }
    return ok;
}

/* Os exports do pedido viram a .edata: cada nome tem de ser global definido no programa (o wrapper do export). */
static bool LigarExportar(LigacaoEstado *L)
{
    const LigarPedido *p = L->pedido;
    Vec simbolos;
    bool ok;

    VecIniciar(&simbolos, sizeof(PeExporta));
    ok = LigarColetarExports(L, &simbolos);
    if (ok && !PeReservarExportacoes(L->im, p->nome_dll, simbolos.dados, (uint32_t)simbolos.n))
        ok = DiagRelatar(L->erro, X0903, 0, "COFF: a DLL nao tem export ou repete um nome de export");
    VecLiberar(&simbolos);
    return ok;
}

/* O Ryu do pedido ou, sem ele, um objeto vazio (o ligador sempre trabalha com os tres). */
static const Objeto *LigarRyuOuVazio(const LigarPedido *pedido, const Objeto *vazio)
{
    return pedido->ryu != NULL ? pedido->ryu : vazio;
}

/* Alcance, secoes, simbolos, entrada e relocacoes, nessa ordem; para na 1a que falha. */
static bool LigarEtapasDaImagem(LigacaoEstado *L)
{
    return LigarConferirAlcance(L) && LigarMontarSecoes(L) && LigarResolverSimbolos(L) && LigarEntrada(L) && LigarAplicarRelocs(L);
}

static uint32_t LigarNomesImportados(const LigarPedido *pedido)
{
    uint32_t n = 0;

    for (uint32_t t = 0; t < LigarTabelasFixas(pedido); t++)
        n += LIGAR_TABELAS[t].qtd;
    for (uint32_t t = 0; t < pedido->qtd_tabelas; t++)
        n += pedido->tabelas[t].qtd;
    return n;
}

/* Os tetos da Imagem (DLLs no .idata e nomes importados) dependem dos 'link' e do foreign do usuario: estourar e X0109, nao erro interno (os FalhaInterna abaixo ficam como invariante). */
static bool LigarConferirLimites(const LigarPedido *pedido, Diag *erro)
{
    uint32_t dlls = LigarTabelasFixas(pedido) + pedido->qtd_tabelas;
    uint32_t nomes = LigarNomesImportados(pedido);

    if (dlls > IMAGEM_MAX_DLLS)
        return DiagRelatar(erro, X0109, 0, "o programa pede %u DLLs com import, contando a KERNEL32 e a ADVAPI32 do runtime; o ligador nativo aceita no maximo %d", dlls, IMAGEM_MAX_DLLS);
    if (nomes > ALCANCE_MAX_IMPORTS)
        return DiagRelatar(erro, X0109, 0, "o programa pede %u nomes importados de DLL, contando os %d da KERNEL32 e da ADVAPI32 do runtime; o ligador nativo aceita no maximo %d", nomes,
                           LIGAR_QTD_IMPORTS + LIGAR_QTD_IMPORTS_ADVAPI32, ALCANCE_MAX_IMPORTS);
    return true;
}

/* PE: a ordem do .reloc e do .pdata (o indice da secao .pdata na Imagem, ou -1 se o exe nao a tem). */
static bool LigarFecharPe(const LigacaoEstado *L)
{
    int pdata = L->tam[LIGAR_SAIDA_PDATA] == 0 ? -1 : (int)L->idx_im[LIGAR_SAIDA_PDATA];

    return LigarPeFinalizar(L->im, pdata, L->erro);
}

/* Depois das relocacoes: o que fecha a Imagem de cada formato (a .edata da DLL; PE: ordem do .reloc/.pdata; ELF: os simbolos vivos). */
static bool LigarFechar(LigacaoEstado *L)
{
    if (L->pedido->dll && !LigarExportar(L))
        return false;
    if (L->pedido->formato == IMAGEM_ELF) {
        LigarElfColetarSimbolos(L);
        return true;
    }
    return LigarFecharPe(L);
}

static bool LigarImagem(const LigarPedido *pedido, Imagem *im, Diag *erro)
{
    Objeto sem_ryu;
    ObjIniciar(&sem_ryu);
    LigacaoEstado L = {.obj = {[LIGAR_PROGRAMA] = pedido->programa, [LIGAR_RUNTIME] = pedido->runtime,
                               [LIGAR_RYU] = LigarRyuOuVazio(pedido, &sem_ryu),
                               [LIGAR_TERCEIROS] = pedido->terceiros != NULL ? pedido->terceiros : &sem_ryu,
                               [LIGAR_LEGADO] = pedido->legado != NULL ? pedido->legado : &sem_ryu,
                               [LIGAR_CURTO] = pedido->curto != NULL ? pedido->curto : &sem_ryu,
                               [LIGAR_MATH] = pedido->math != NULL ? pedido->math : &sem_ryu,
                               [LIGAR_ES] = pedido->es != NULL ? pedido->es : &sem_ryu,
                               [LIGAR_THREAD] = pedido->thread != NULL ? pedido->thread : &sem_ryu,
                               [LIGAR_SYNC] = pedido->sync != NULL ? pedido->sync : &sem_ryu,
                               [LIGAR_ATOMIC] = pedido->atomic != NULL ? pedido->atomic : &sem_ryu,
                               [LIGAR_VARENA] = pedido->varena != NULL ? pedido->varena : &sem_ryu,
                               [LIGAR_NET] = pedido->net != NULL ? pedido->net : &sem_ryu,
                               [LIGAR_ENTRADA] = pedido->entrada != NULL ? pedido->entrada : &sem_ryu,
                               [LIGAR_SEMENTE] = pedido->semente != NULL ? pedido->semente : &sem_ryu,
                               [LIGAR_PERFIL] = pedido->perfil != NULL ? pedido->perfil : &sem_ryu},
                       .im = im, .erro = erro, .pedido = pedido};
    im->dll = pedido->dll;
    im->formato = pedido->formato;
    im->pie = pedido->pie;
    LigarJuntarTabelas(&L, pedido);
    LigarPreparar(&L);
    bool ok = LigarEtapasDaImagem(&L) && LigarFechar(&L);
    LigarLiberar(&L);
    ObjLiberar(&sem_ryu);
    return ok;
}

bool LigarComTabelas(const LigarPedido *pedido, Imagem *im, Diag *erro)
{
    return LigarElfConferirFormato(pedido, erro) && LigarConferirLimites(pedido, erro) && LigarImagem(pedido, im, erro);
}

bool Ligar(const Objeto *programa, const Objeto *runtime, Imagem *im, Diag *erro)
{
    return LigarComTabelas(&(LigarPedido){.programa = programa, .runtime = runtime}, im, erro);
}
