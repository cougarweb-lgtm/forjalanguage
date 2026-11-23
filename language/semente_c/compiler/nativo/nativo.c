/* nativo.c - backend nativo: IR -> BIR -> Objeto -> Ligar -> PE, sem gcc nem ld; o exe leva a assinatura honesta de quem o gerou.
 * Versao: 0.50.1
 * Changelog: 0.50.1 - G0d: item passes do --debug-code (Montagem.passes_txt; OtimExecutar recebe OtimRelatos); arquivo no teto, sem linha nova. 0.50.0 - M8 P2: --perfil (NativoPedido.perfil): BirEntrada.perfil, Montagem.perfil (rt_win_perfil.o/rt_linux_perfil.o, 16o objeto da ligacao so com a opcao; NativoPerfilLer), a tabela FPF1 no .rdata (NativoPerfilTabela, depois do NativoInterfaceDll) e DebugEntrada.programa (item perfil do --debug-code); arquivo no teto: sem linha nova, o include vem do nativo_debug.h. 0.49.2 - M8 H1: GerarCodigo chama NativoInterfaceDll (DLL: struct do forja_dll_info no .rdata) e DebugEntrada.pedido (item interface do --debug-code). 0.49.1 - M7b F3: BirEntrada.simbolos_dbg = NativoPedido.simbolos (o passe sroa respeita os slots de depuracao; sem linha nova, arquivo no teto). 0.49.0 - M7a F0: ConstruirBir roda OtimExecutar depois do BirValidar (so release; tabela vazia = neutro) e o item otim do --debug-code.
 *            0.48.1 - Ryu d2fixed P4: UsarRyuDoPrograma tambem decide o curto pelo RUNTIME (o R0002 de rt_real.c chama forja_curto_texto_f64); a lista de simbolos do Ryu nao muda. 0.48.0 - M6e P93k: BirEntrada.guarda = NativoPedido.guarda_memoria (o BIR chama forja_lista_crescer_alvo). 0.47.0 - M6e P93d: AlvoColheita.guarda = NativoPedido.guarda_memoria (a FRT1 ganha a subtabela de linhas; so a linha da colheita muda). 0.46.0 - M6d P92c: TerceirosPedido.libmem e NativoBibliotecasEntrada.libmem so em dev x64 nao DLL (arquivo no teto: sem linha nova). 0.45.3 - M6b P77b: GravarPdb passa a IR e os nomes (PdbParametros.ir e .nomes) para os LF_* do TPI.
 *            0.45.2 - M6b P77a: GravarPdb passa os tipos (PdbParametros.tipos) para o S_REGREL32 das variaveis simples.
 *            0.45.1 - M6b P76c: GravarPdb passa a Fonte do programa (PdbParametros.fonte) para o C13 de linhas.
 *            0.45.0 - M6b P76a: GravarPdb passa ao PdbEscrever o DbgPrograma, o exe, o nome do modulo e o tamanho do codigo do programa (procs no .pdb); o .pdb e gravado com ArquivoGravarAtomicoSemDescarga (regeneravel).
 *            0.44.0 - M6b P75: PE com --simbolos sim grava <saida sem extensao>.pdb ao lado do exe (PdbEscrever) e o RSDS do exe aponta para ele (Imagem.pdb_caminho, GUID = hash do exe).
 *            0.43.0 - M6 P69: UsaRastreio = modo dev (PE, ELF e DLL); RuntimeDoAlvo sem a variante de rastreio (o runtime dev ja a e); --rastreio fica aceito e redundante.
 *            0.42.0- M6 P68: UsaDwarf vale para o PE (so --simbolos decide); DwarfSemantica.com_frame = ELF (o .debug_frame e so dele).
 *            0.41.0 - M6 P67: DwarfEscrever recebe DwarfSemantica (fonte, tipos, IR e nomes do pedido) para os DIE de tipo e de variavel.
 *            0.40.0 - M6 P66: --simbolos no ELF: o DbgPrograma e colhido e DwarfEscrever acrescenta as secoes de debug ao Objeto do programa (UsaDwarf).
 *            0.39.0 - M6 P65b: Montagem.dbg (DbgPrograma) colhido com --rastreio ou item linhas; AlvoColheita.dbg e DebugEntrada.dbg.
 *            0.38.1 - M6 P65a: item locais do --debug-code (locais_txt, BirImprimirLocais).
 *            0.38.0 - M5c-b P59/P60: std:mem VirtualArena e std:net ligam no alvo x86_64-linux (LerVarena/LerNet leem rt_linux_varena.o e rt_linux_net.o; saem os 2 X0003 de LigarImagem).
 *            0.37.0 - M5c-b P58: std:thread, std:sync e std:atomic ligam no alvo x86_64-linux (LerThread/LerSync/LerAtomic leem rt_linux_thread.o, rt_linux_sync.o e rt_linux_atomic.o; saem os 3 X0003 de LigarImagem).
 *            0.36.0 - M5c-b P57: programa linux que referencia forja_thread_* (std:thread/std:async) liga a variante libc como o foreign (LerEntrada: NativoUsaThreadsNoElf); sem threads e sem foreign o ELF cru nao muda.
 *            0.35.1 - M5b P41: --pie chega ao LigarPedido.pie e a semente sai como rcrt1.o (SementeMuslCrt1 com pie).
 *            0.35.0 - M5b P39-B2 (D0654): foreign no alvo linux liga na variante libc: LerEntrada le rt_linux_entrada_libc.o (main) e o crt1.o da semente
 *                    (nativo_semente.c; sem a semente = X0003 com forma_certa); PedidoDeLigacao leva LigarPedido.semente; sem foreign o _start cru de antes.
 *            0.34.0 - M5b P39-B1 (D0654): LerCurto escolhe rt_linux_curto.o no ELF (RtNativoLinuxCurto) e rt_win_curto.o no PE.
 *            0.33.0 - M5b P39-B1 (D0654): ConstruirBir passa os nomes de copia e zera do alvo (BirEntrada.nomes_mem = Alvo.mem).
 *            0.32.0 - M5b P39: LerEntrada (rt_linux_entrada_cru.o, o _start do ELF) entra sempre no alvo linux como LigarPedido.entrada (14o objeto); PE sem mudanca.
 *            0.31.0 - M5b P37: sai o X0003 "foreign com biblioteca no alvo linux" e a escotilha FORJA_TESTE_FOREIGN_MS_LINUX (RecusaDoAlvo so recusa dll);
 *                    ConstruirBir passa BirEntrada.sysv = alvo ELF (a ABI SysV do foreign no alvo linux).
 *            0.30.0 - M5b P36: escotilha FORJA_TESTE_FOREIGN_MS_LINUX (so teste) no X0003 FOREIGN_LINUX de RecusaDoAlvo; sem a variavel, igual a antes.
 *            0.29.0 - M5 P53: LerNet (rt_win_net.o, 13o objeto so com forja_net_*, leva a WS2_32 do ligador); no alvo x86_64-linux o uso de std:net e X0003 "ainda nao liga no alvo x86_64-linux (M5c-b)", com o caminho do std:mem.
 *            0.28.0 - M5 P52: LerVarena (rt_win_varena.o, 12o objeto so com forja_varena_*); no alvo x86_64-linux o uso de std:mem VirtualArena e X0003 "ainda nao liga no alvo x86_64-linux (M5c-b)" (LigarImagem), com o mesmo caminho do std:atomic.
 *            0.27.0 - M5 P50: LerAtomic (rt_win_atomic.o, 11o objeto so com forja_atomic_*); no alvo x86_64-linux o uso de std:atomic e X0003 "ainda nao liga no alvo x86_64-linux (M5c-b)" (LigarImagem), com o mesmo caminho do std:sync.
 *            0.26.0 - M5 P49: LerSync (rt_win_sync.o, 10o objeto so com forja_sync_*); no alvo x86_64-linux o uso de std:sync e X0003 "ainda nao liga no alvo x86_64-linux (M5c-b)" (LigarImagem), com o mesmo caminho do std:thread.
 *            0.25.0 - M5 P47: LerThread (rt_win_thread.o, 9o objeto so com forja_thread_*); no alvo x86_64-linux o uso de std:thread e X0003 "ainda nao liga no alvo x86_64-linux (M5c-b)" (LigarImagem). 0.24.0 - M5 P28b-2: sai o X0003 interino "std:process ainda nao liga no Linux: P28b-2" (SIMBOLOS_PROCESSO_SEM_LINUX e UsaProcessoSemRuntimeLinux): o rt_linux_es.o agora define forja_processo_* e entra pelo mesmo RuntimeUsaRyu do std:file.
 *            0.23.0 - M5 P28b-1: LerEs escolhe o objeto pelo alvo (rt_linux_es.o no ELF); saem o X0003 do std:file e o SIMBOLOS_OS_SEM_LINUX (a A52 do std:os agora vive no runtime Linux); fica um X0003 interino
 *                      "std:process ainda nao liga no Linux: P28b-2", detectado pelos simbolos forja_processo_*.
 *            0.22.0 - M5 P28: simbolo da A52 do std:os (cwd, abs_path, exe_path, temp_dir, set_env, random_bytes) sem runtime Linux vira X0003 "ainda nao liga no Linux: M5b" (era X0903 interno por simbolo indefinido).
 *            0.21.0 - S3 P5b-1: LerEs le o handle e o fs do std:file (rt_win_es.o) e ele entra como o 8o objeto so quando o programa referencia forja_fs_* (UsarRyuDoPrograma, como o math); no ELF e X0003 com "M5".
 *            0.20.0 - L11a-2: LerMath le a libm do musl (rt_math.o) e ela entra na ligacao como o 7o objeto so quando o programa referencia forja_math_* (UsarRyuDoPrograma, como o curto); sem ela o exe e o de antes.
 * Changelog: 0.19.0 - IA1-LX: LerCurto le o curto tambem no alvo x86_64-linux (sem o retorno antecipado do ELF): o mesmo rt_win_curto.o liga no ELF quando o programa referencia simbolo dele.
 *            0.18.0 -IA1-L5a: o curto (rt_win_curto.o: print curto de real e eprint, LerCurto) e o 6o objeto da ligacao, so quando o programa referencia simbolo dele (UsarRyuDoPrograma); sem ele o exe e o de antes.
 *            0.17.0 - IA1-b12: --rastreio (NativoPedido.rastreio): UsaRastreio escolhe o runtime com o rastreio (RtNativoWinRastreio) e liga AlvoColheita.rastreio; so Windows, dev, exe.
 *            0.16.0 - M5 P27b: EscreverImagem escolhe o escritor pelo formato do alvo (ElfEscrever no x86_64-linux, PeEscrever no Windows); o X0003 "escritor ELF: P27b" sai (Alvo.sem_escritor do linux e NULL; o
 *                    campo e a checagem seguem para um alvo futuro). O PE segue byte a byte igual.
 *            0.15.0 - M5 P27a: o alvo x86_64-linux passa por X64Gerar e pelo ligador (LigarPedido.formato = IMAGEM_ELF; runtime RtNativoLinux e objeto comum RtNativoLinuxComum no lugar do Ryu; sem legado, sem versao do linker
 *                    nem .rsrc) e PARA em EscreverExe com o X0003 do Alvo.sem_escritor ("escritor ELF: P27b"); X0003 antes de gerar para foreign de fora ("foreign com biblioteca no alvo linux: M5b") e para dll sem alvo.dll
 *                    ("biblioteca .so: M5c"). O alvo x86_64-windows segue byte a byte igual.
 *            0.14.1 - M5 P26b: a decisao do Ryu pelo programa saiu de LigarImagem para UsarRyuDoPrograma (conferir_regras: 5 decisoes por funcao); sem mudanca de comportamento.
 *            0.14.0 - M5 P26b: LigarImagem tambem junta o objeto do Ryu (rt_win_ryu.o, que leva o std:proc: rt_proc.c) quando o PROGRAMA tem simbolo indefinido que ele define (forja_proc_*); antes so o runtime decidia.
 *            0.13.0 - M4 P24p: ConstruirBir liga BirEntrada.poda (B07: so os procs alcancaveis entram na BIR); o --debug-code leva procs_total e procs_gerados da BIR (registro "poda").
 *            0.12.0 - M4 P24a: tempos completos: o X64Gerar devolve as fatias isel/regs/codificar (AlvoColheita); NativoConstruirExe so zera o que e do nativo (lex, parse, check, ir e inicio_ns vem do
 *                    driver) e o item tempos do --debug-code fecha total_ns, outros_ns e linhas (NativoTemposFechar) antes de montar o registro.
 *            0.11.0 - M3b P23f: o legado (rt_win_legado.o, LerLegado) e provedor de simbolos de membros de estatica (ResolverBibliotecas: runtime e legado); entra na ligacao (LigarPedido.legado) so quando
 *                    BibliotecasResolvidas.provedores_usados o marca (stubs da toolchain, sprintf, sscanf).
 *            0.10.0 - M3b P23e: biblioteca estatica ligada: depois de ResolverBibliotecas, MontarTerceiros (terceiros.c) junta os membros que a selecao puxou num 4o Objeto (LigarPedido.terceiros); o aviso do
 *                    .drectve (diretiva ignorada) sai uma vez em stderr; os objetos do programa, do runtime e do Ryu vao em TerceirosPedido.outros (o que eles definem nao ganha thunk).
 *            0.9.1 - M3 F9b-1: decisoes extraidas de ConstruirBir e Montar.
 *            0.9.0 - M3 F5: o Ryu e um objeto a parte (RtNativoWinRyu): ConferirLayout o le (LerRyu) e ele entra no LigarPedido.ryu so se o runtime tem simbolo dele indefinido (RuntimeUsaRyu).
 *            0.8.1 - M3 P22-O3: a variante CRT do runtime vem de NativoUsaRuntimeCrt (toda DLL ou foreign de fora); TemForeignDeFora saiu.
 *            0.8.0 - M3 P22-O1a: o exe, a DLL e o <saida>.debug.* sao gravados por ArquivoGravarAtomico (X0302 ganha " (em uso)" quando o destino persiste ocupado); --entradas grava o .entradas (nativo_entradas.c) depois deles.
 *            0.7.0 - M3 P22a: NativoPedido.dll: a BIR leva dll (involucros de export, sem forja_programa) e o ligador leva a entrada forja_entrada_dll, o nome do arquivo e os exports (BirPrograma.exportados).
 *            0.6.1 - M3 P19c: ConferirLayout usa a variante CRT do runtime (RtNativoWinCrt) quando a BIR tem algum BS_IMPORT (foreign de fora); sem foreign o runtime e o de sempre.
 *            0.6.0 - M3 P19: LigarImagem resolve os 'link' do programa (ResolverBibliotecas: X0101, X0102, X0103, X0105) e liga com LigarComTabelas; o erro leva os campos extras (ErroOrigem.campos).
 *            0.5.1 - P18a: ConstruirBir passa NativoPedido.traco para a BirEntrada.
 *            0.5.0 - M2 P12b: ConstruirBir passa NativoPedido.teste para a BirEntrada.
 *            0.4.0 - M2 P11b: ETAPA_LAYOUT com produtor real (ConferirLayout: LayoutConferir e forja_rt_layout do runtime contra o layout\ de str, X0906); o COFF do runtime e lido nessa etapa
 *                    (antes da geracao) e o ligador usa o mesmo Objeto.
 *            0.3.4 - M2 P10b: o runtime embutido e o do modo do pedido (RtNativoWin(modo): dev confere a faixa de 'as' de real, release satura).
 *            0.3.3 - M2 P9a fase 2: PeAcrescentarRecursos devolve false quando a secao .rsrc e recusada (o erro segue pelo Montagem).
 *            0.3.2 - M2 P9a: a gravacao do exe e do .debug usa ArquivoGravarCalado; o X0302 (JSON) sai sozinho em stderr.
 *            0.3.1 - portao M1: versao do forja fora de maior.menor.patch e X0905 explicito (antes o linker caia em 0.0 calado); SAIDA_* vem do
 *                    nativo.h; ETAPA_LAYOUT anotada como RESERVADA (M2).
 *            0.3.0 - backend x64 P8b: erros pelo nativo_erro.c (campos extras, pacote de relato nos X09xx, FORJA_TESTE_INJETAR); --debug-code com "json" grava
 *                    <saida>.debug.jsonl; a BIR so morre no fim (o relato de um erro em qualquer etapa leva a BIR); etapa do pipeline em cada erro.
 *            0.2.0 - backend x64 P8: --debug-code (itens bir, asm, quadros, reloc, formato e tempos em <saida>.debug.txt; item desconhecido ou 'ir' = X0003).
 *            0.1.0 - backend x64 P7b: NativoConstruirExe (encadeamento do modo x64 do teste_nativo), tempos por etapa, X0302 ao gravar,
 *                    identificacao do exe: "Forja Native <versao>" em .rdata, versao do linker do PE e .rsrc (VERSIONINFO + manifesto).
 * Verificacao: construir.bat && python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/base.h"
#include "common/buf.h"
#include "common/fonte.h"
#include "nativo/x64/alvo.h"
#include "nativo/dbg/dwarf.h"
#include "nativo/dbg/pdb.h"
#include "nativo/fmt/elf.h"
#include "nativo/fmt/pe.h"
#include "nativo/fmt/pe_recursos.h"
#include "nativo/nativo.h"
#include "nativo/nativo_debug.h"
#include "nativo/nativo_biblioteca.h"
#include "nativo/nativo_entradas.h"
#include "nativo/nativo_erro.h"
#include "nativo/nativo_semente.h"
#include "nativo/nativo_layout.h"
#include "nativo/obj/coff.h"
#include "nativo/obj/ligar.h"
#include "nativo/obj/objeto.h"
#include "nativo/obj/terceiros.h"
#include "nativo/optimizer/passes.h"
#include "nativo/rt_embutido.h"

#define NATIVO_ASSINATURA "Forja Native "
/* Os provedores de simbolos de membros de estatica (BibliotecasProvedor), nesta ordem: o runtime (ja esta na ligacao) e o legado (so entra se for usado). */
#define PROVEDOR_RUNTIME 0
#define PROVEDOR_LEGADO 1
#define PROVEDOR_QTD 2

/* Tudo o que a geracao de um exe aloca, para liberar num ponto so. */
typedef struct {
    const NativoPedido *pedido;
    const Alvo *alvo;
    NativoTempos *tempos;
    Objeto codigo, runtime, ryu, terceiros, legado, curto, math, es, thread, sync, atomic, varena, net, entrada, semente, perfil;
    bool tem_semente;           /* variante libc (foreign no alvo linux): o crt1.o da semente musl entra na ligacao como o 15o objeto */
    bool usa_ryu;               /* o runtime deixou simbolo do Ryu indefinido: o ryu entra na ligacao */
    bool usa_math;              /* o programa referencia forja_math_* (std:math): a libm do musl (rt_math.o) entra na ligacao como o 7o objeto */
    bool usa_es;                /* o programa referencia forja_fs_* (std:file, handle e fs): rt_win_es.o entra na ligacao como o 8o objeto */
    bool usa_thread;            /* o programa referencia forja_thread_* (std:thread): rt_win_thread.o entra na ligacao como o 9o objeto */
    bool usa_sync;              /* o programa referencia forja_sync_* (std:sync): rt_win_sync.o entra na ligacao como o 10o objeto */
    bool usa_atomic;            /* o programa referencia forja_atomic_* (std:atomic): rt_win_atomic.o entra na ligacao como o 11o objeto */
    bool usa_varena;            /* o programa referencia forja_varena_* (std:mem VirtualArena): rt_win_varena.o entra na ligacao como o 12o objeto */
    bool usa_net;               /* o programa referencia forja_net_* (std:net UDP): rt_win_net.o entra na ligacao como o 13o objeto, com a WS2_32 */
    bool usa_curto;             /* o programa referencia simbolo do curto (print curto de real, eprint): o curto entra na ligacao como o 6o objeto */
    bool usa_legado;            /* um membro de estatica pede simbolo do legado (stubs da toolchain, sprintf, sscanf): o legado entra na ligacao */
    bool usa_terceiros;         /* algum 'link' e estatica: os membros puxados entram na ligacao como o 4o objeto */
    Imagem imagem;
    Buf exe;
    Diag erro;
    uint32_t itens;             /* mascara do --debug-code (DEBUG_*); 0 = sem arquivo de depuracao */
    Buf bir_txt;                /* BirImprimir, colhido para o item bir */
    Buf locais_txt, otim_txt, passes_txt; /* (passes_txt: item passes, G0d) */ /* BirImprimirLocais (item locais, M6 P65a) e OtimExecutar (item otim, M7a F0), colhidos para o --debug-code */
    AlvoListagem lista;         /* funcoes e instrucoes, colhidas pelo gerador */
    DbgPrograma dbg;            /* registro neutro de depuracao (M6 P65b); so vale com dbg_vivo */
    bool dbg_vivo;              /* DbgIniciar ja rodou (DbgLiberar vale) */
    char pdb_caminho[1100];     /* M6b P75: caminho absoluto do .pdb (vazio = PE sem .pdb); o RSDS do exe aponta para ele */
    uint8_t pdb_guid[16];       /* o GUID que PeEscrever pos no RSDS (hash do exe): o PDB leva o mesmo */
    BirPrograma bir;          /* vive ate o fim: o relato de um erro de qualquer etapa leva a BIR */
    BibliotecasResolvidas libs; /* tabelas de imports das DLLs dos 'link' (os nomes vivem ate o ImagemLiberar) */
    BibliotecaFalha falha_lib;  /* campos extras do X01xx, se a resolucao dos 'link' falhou */
    bool bir_viva;              /* BirConstruir ja rodou (BirLiberar vale) */
    uint32_t bir_funcoes;       /* quantas funcoes da BIR estao completas */
    const char *etapa;          /* etapa em curso (ETAPA_*): o campo "etapa" do erro */
} Montagem;

/* Diagnostico do pipeline: JSON com a linha do .fj e os campos extras em stdout. X0001 (programa fora do M1) e diagnostico; o resto e erro interno. */
static int RelatarErro(const Montagem *m)
{
    ErroOrigem origem = {.etapa = m->erro.codigo == X0902 ? ETAPA_CODIFICAR : m->etapa, .bir = m->bir_viva ? &m->bir : NULL,
                         .bir_funcoes = m->bir_funcoes, .campos = m->falha_lib.campos.dados};

    return NativoRelatarErro(m->pedido, &m->erro, &origem);
}

static int RecusarComX0003(const char *causa)
{
    DiagImprimirJsonSemFonte(X0003, causa, stderr);
    return SAIDA_USO;
}

/* Entra na etapa e, se FORJA_TESTE_INJETAR mira nela, falha com o codigo injetado (so teste). true = falhou. */
static bool Injetou(Montagem *m, const char *etapa)
{
    m->etapa = etapa;
    return NativoInjetarFalha(m->pedido, etapa, &m->erro);
}

/* Algum global definido no Ryu esta indefinido no runtime? (so entao o Ryu entra na ligacao) */
static bool RuntimeUsaRyu(const Objeto *runtime, const Objeto *ryu)
{
    for (size_t k = 0; k < ryu->simbolos.n; k++) {
        const ObjSimbolo *def = VecEm(&ryu->simbolos, k);
        int64_t i = def->global && def->secao >= 0 ? ObjAcharSimbolo(runtime, def->nome) : -1;
        if (i >= 0 && ((const ObjSimbolo *)VecEm(&runtime->simbolos, (size_t)i))->secao == OBJ_SECAO_INDEFINIDA)
            return true;
    }
    return false;
}

/* Le o Ryu embutido (rt_win_ryu.o; no Linux, o rt_linux_comum.o: heap dlmalloc + Ryu) e decide se entra: o ola mundo nao o recebe se o runtime dele nao o referencia. */
static bool LerRyu(Montagem *m)
{
    size_t n = 0;
    const uint8_t *bytes = m->alvo->formato == IMAGEM_ELF ? RtNativoLinuxComum(&n) : RtNativoWinRyu(&n);

    if (!CoffLer(bytes, n, &m->ryu, &m->erro))
        return false;
    m->usa_ryu = RuntimeUsaRyu(&m->runtime, &m->ryu);
    return true;
}

/* Le o legado embutido (rt_win_legado.o): so entra na ligacao se um membro de estatica pede simbolo dele (ResolverBibliotecas decide). O Linux nao tem legado (sem estaticas de terceiros no M5). */
static bool LerLegado(Montagem *m)
{
    size_t n = 0;
    const uint8_t *bytes = RtNativoWinLegado(&n);

    if (m->alvo->formato == IMAGEM_ELF)
        return true;
    return CoffLer(bytes, n, &m->legado, &m->erro);
}

/* Le o curto embutido: rt_win_curto.o no PE; no ELF, rt_linux_curto.o (P39-B1, D0654: o mesmo COFF com memcpy renomeado para forja_memcpy). So entra na ligacao se o programa referencia simbolo dele (UsarRyuDoPrograma). Os simbolos externos dele (forja_fmt_*, forja_panic, memcpy e rt_saida_stderr) o runtime de cada alvo define. */
static bool LerCurto(Montagem *m)
{
    size_t n = 0;
    const uint8_t *bytes = m->alvo->formato == IMAGEM_ELF ? RtNativoLinuxCurto(&n) : RtNativoWinCurto(&n);

    return CoffLer(bytes, n, &m->curto, &m->erro);
}

/* Le a libm do musl embutida (rt_math.o, L11a-2): so entra na ligacao se o programa referencia forja_math_* (UsarRyuDoPrograma). Neutra de alvo, como o curto. */
static bool LerMath(Montagem *m)
{
    size_t n = 0;
    const uint8_t *bytes = RtNativoMath(&n);

    return CoffLer(bytes, n, &m->math, &m->erro);
}

/* O handle e o fs do std:file e o stdin do std:os: rt_win_es.o no PE, rt_linux_es.o (P28b-1) no ELF. So entra na ligacao se o programa referencia forja_fs_* ou forja_stdin_* (UsarRyuDoPrograma). */
static bool LerEs(Montagem *m)
{
    size_t n = 0;
    const uint8_t *bytes = m->alvo->formato == IMAGEM_ELF ? RtNativoLinuxEs(&n) : RtNativoWinEs(&n);

    return CoffLer(bytes, n, &m->es, &m->erro);
}

/* M5b P39-B2 (D-R3, D-R4): foreign no alvo linux = variante libc: o objeto de entrada com o main (rt_linux_entrada_libc.o) e o crt1.o da semente musl.
 * Sem a semente, X0003 com a forma certa (ela fica FORA do forja.exe, em third\musl\x86_64). */
static bool LerEntradaLibc(Montagem *m)
{
    size_t n = 0;
    const uint8_t *bytes = RtNativoLinuxEntradaLibc(&n);
    char pasta[SEMENTE_TAM_PASTA];

    if (!CoffLer(bytes, n, &m->entrada, &m->erro))
        return false;
    if (!SementeMuslPasta(pasta, sizeof pasta) || !SementeMuslCompleta(pasta)) {
        DiagRelatar(&m->erro, X0003, 0, "foreign no alvo x86_64-linux pede a semente musl (crt1.o e libc.a) em '%s', e ela nao esta la", pasta);
        DiagSugerir(&m->erro.sug, DIAG_CERTA, "restaure language\\third\\musl\\x86_64 (LEIAME.txt e construir_musl.sh no WSL alpine-build) ou compile sem foreign");
        return false;
    }
    if (!SementeMuslCrt1(pasta, m->pedido->pie, &m->semente)) {
        DiagRelatar(&m->erro, X0003, 0, "crt1.o da semente musl em '%s' nao le como ELF x86-64", pasta);
        return false;
    }
    m->tem_semente = true;
    return true;
}

/* M5b P39 (D-R3): o _start do Linux (rt_linux_entrada_cru.o) e o objeto de entrada do ELF sem foreign. No PE nao ha objeto de entrada: nada a ler. */
static bool LerEntrada(Montagem *m)
{
    size_t n = 0;
    const uint8_t *bytes;

    if (m->alvo->formato != IMAGEM_ELF)
        return true;
    if (NativoUsaRuntimeCrt(m->pedido, &m->bir) || NativoUsaThreadsNoElf(&m->bir))
        return LerEntradaLibc(m);
    bytes = m->pedido->pie ? RtNativoLinuxEntradaPie(&n) : RtNativoLinuxEntrada(&n);
    return CoffLer(bytes, n, &m->entrada, &m->erro);
}

/* M5 P47: std:thread (rt_win_thread.o; no ELF, rt_linux_thread.o, P58): so entra na ligacao se o programa referencia forja_thread_* (RuntimeUsaRyu). */
static bool LerThread(Montagem *m)
{
    size_t n = 0;
    const uint8_t *bytes = m->alvo->formato == IMAGEM_ELF ? RtNativoLinuxThread(&n) : RtNativoWinThread(&n);

    return CoffLer(bytes, n, &m->thread, &m->erro);
}

/* M5 P49: std:sync (rt_win_sync.o; no ELF, rt_linux_sync.o, P58): so entra na ligacao se o programa referencia forja_sync_* (RuntimeUsaRyu). */
static bool LerSync(Montagem *m)
{
    size_t n = 0;
    const uint8_t *bytes = m->alvo->formato == IMAGEM_ELF ? RtNativoLinuxSync(&n) : RtNativoWinSync(&n);

    return CoffLer(bytes, n, &m->sync, &m->erro);
}

/* M5 P50: std:atomic (rt_win_atomic.o; no ELF, rt_linux_atomic.o, P58): so entra na ligacao se o programa referencia forja_atomic_* (RuntimeUsaRyu). */
static bool LerAtomic(Montagem *m)
{
    size_t n = 0;
    const uint8_t *bytes = m->alvo->formato == IMAGEM_ELF ? RtNativoLinuxAtomic(&n) : RtNativoWinAtomic(&n);

    return CoffLer(bytes, n, &m->atomic, &m->erro);
}

/* M5 P52: std:mem VirtualArena (rt_win_varena.o; no ELF, rt_linux_varena.o, P59): so entra na ligacao se o programa referencia forja_varena_* (RuntimeUsaRyu). */
static bool LerVarena(Montagem *m)
{
    size_t n = 0;
    const uint8_t *bytes = m->alvo->formato == IMAGEM_ELF ? RtNativoLinuxVarena(&n) : RtNativoWinVarena(&n);

    return CoffLer(bytes, n, &m->varena, &m->erro);
}

/* M5 P53: std:net UDP (rt_win_net.o; no ELF, rt_linux_net.o, P60): so entra na ligacao se o programa referencia forja_net_* (RuntimeUsaRyu). */
static bool LerNet(Montagem *m)
{
    size_t n = 0;
    const uint8_t *bytes = m->alvo->formato == IMAGEM_ELF ? RtNativoLinuxNet(&n) : RtNativoWinNet(&n);

    return CoffLer(bytes, n, &m->net, &m->erro);
}

/* M6 P69 (D-M6-6): em dev a pilha .fj e padrao nos 2 SOs (a tabela FRT1 vai no exe e o runtime dev a le); em release nada disso existe. --rastreio ficou redundante em dev.
 * A DLL dev tambem leva a tabela (o runtime dev a referencia), mas sem pilha: so a thread principal do exe marca o topo. */
static bool UsaRastreio(const Montagem *m)
{
    return m->pedido->modo == BM_DEV;
}

/* O runtime embutido do alvo e do modo: no Windows, a variante CRT quando ha foreign de fora; no Linux, so dev e release (sem foreign de fora, M5b). Dev = com rastreio. */
static const uint8_t *RuntimeDoAlvo(const Montagem *m, size_t *n)
{
    bool crt = m->alvo->formato != IMAGEM_ELF && NativoUsaRuntimeCrt(m->pedido, &m->bir);

    if (m->alvo->formato == IMAGEM_ELF)
        return RtNativoLinux(m->pedido->modo, n);
    return crt ? RtNativoWinCrt(m->pedido->modo, n) : RtNativoWin(m->pedido->modo, n);
}

/* Etapa layout (X0906): le o runtime do modo (COFF; o Objeto lido serve depois ao ligador) e confere o layout\ de si mesmo e contra o forja_rt_layout dele. */
static bool ConferirLayout(Montagem *m)
{
    size_t n = 0;
    const uint8_t *bytes = RuntimeDoAlvo(m, &n);

    m->etapa = ETAPA_LIGAR; /* uma falha ao ler o COFF e do runtime embutido, nao do layout */
    if (!CoffLer(bytes, n, &m->runtime, &m->erro) || !LerRyu(m) || !LerLegado(m) || !LerCurto(m) || !LerMath(m) || !LerEs(m) || !LerThread(m) || !LerSync(m) || !LerAtomic(m) || !LerVarena(m) || !LerNet(m) || !LerEntrada(m) || !NativoPerfilLer(m->pedido, m->alvo->formato == IMAGEM_ELF, &m->perfil, &m->erro))
        return false;
    m->etapa = ETAPA_LAYOUT;
    return NativoLayoutConferir(m->pedido, &m->runtime, &m->erro);
}

/* Depois da BIR validada: injecao da etapa BIR, layout do runtime e injecao da etapa layout, nessa ordem. */
static bool ConferirDepoisDaBir(Montagem *m)
{
    return !Injetou(m, ETAPA_BIR) && ConferirLayout(m) && !Injetou(m, ETAPA_LAYOUT);
}

/* IR -> BIR validada. tempos->bir_ns cobre construcao e validacao. */
static bool ConstruirBir(Montagem *m)
{
    const NativoPedido *p = m->pedido;
    BirEntrada entrada = {.ir = p->ir, .tipos = p->tipos, .nomes = p->nomes, .fonte = p->fonte, .modo = p->modo, .teste = p->teste,
                          .traco = p->traco, .dll = p->dll, .poda = true, .sysv = m->alvo->formato == IMAGEM_ELF, .nomes_mem = m->alvo->mem, .guarda = p->guarda_memoria, .checagem = p->checagem, .simbolos_dbg = p->simbolos, .perfil = p->perfil};
    uint64_t t0 = RelogioNs();
    bool ok;

    m->etapa = ETAPA_BIR;
    ok = BirConstruir(&entrada, &m->bir, &m->erro);
    m->bir_viva = true;
    m->bir_funcoes = ok || m->bir.nfuncoes == 0 ? m->bir.nfuncoes : m->bir.nfuncoes - 1; /* a funcao que falhou fica incompleta */
    ok = ok && BirValidar(&m->bir, &m->erro) && OtimExecutar(&m->bir, p->sem_otim, &(OtimRelatos){(m->itens & DEBUG_OTIM) != 0 ? &m->otim_txt : NULL, (m->itens & DEBUG_PASSES) != 0 ? &m->passes_txt : NULL}, &m->erro);
    m->tempos->bir_ns = RelogioNs() - t0;
    return ok && ConferirDepoisDaBir(m);
}

/* Anexa "Forja Native <versao>" (com NUL) ao fim do .rdata do programa; abre a secao se o programa nao tem nenhuma. */
static void AssinarRdata(Objeto *o, const char *versao)
{
    uint32_t rdata = UINT32_MAX;
    ObjSecao *s;

    for (uint32_t k = 0; k < o->secoes.n && rdata == UINT32_MAX; k++) {
        if (strcmp(((ObjSecao *)VecEm(&o->secoes, k))->nome, ".rdata") == 0)
            rdata = k;
    }
    if (rdata == UINT32_MAX)
        rdata = ObjSecaoAbrir(o, ".rdata", OS_RDATA, 1);
    s = VecEm(&o->secoes, rdata);
    BufAnexar(&s->bytes, NATIVO_ASSINATURA);
    BufAnexarN(&s->bytes, versao, strlen(versao) + 1);
}

/* M6 P66/P68 (D-M6-5): o DWARF do programa sai com --simbolos (padrao sim em dev), no ELF e no PE. */
static bool UsaDwarf(const Montagem *m)
{
    return m->pedido->simbolos;
}

/* O gerador colhe o DbgPrograma quando o --rastreio, o item linhas do --debug-code ou o DWARF (--simbolos) o pedem (a FRT1, o dump e o DWARF leem dele). */
static bool PedeColetaDbg(const Montagem *m)
{
    return UsaRastreio(m) || (m->itens & DEBUG_LINHAS) != 0 || UsaDwarf(m);
}

/* BIR -> Objeto (isel, alocacao e codificacao fundidos no X64Gerar; as fatias regs, isel e codificar vem medidas por funcao) e a assinatura no .rdata. */
static bool GerarCodigo(Montagem *m)
{
    AlvoTempos fatias = {0};
    AlvoColheita colheita = {.lista = (m->itens & (DEBUG_ASM | DEBUG_QUADROS)) != 0 ? &m->lista : NULL, .tempos = &fatias, .rastreio = UsaRastreio(m), .guarda = m->pedido->guarda_memoria};
    bool ok;

    if (Injetou(m, ETAPA_ISEL) || Injetou(m, ETAPA_CODIFICAR))
        return false;
    if (PedeColetaDbg(m)) {
        DbgIniciar(&m->dbg);
        m->dbg_vivo = true;
        colheita.dbg = &m->dbg;
    }
    ok = m->alvo->gerar(&m->bir, &m->codigo, &m->erro, &colheita) && NativoInterfaceDll(&m->codigo, m->pedido) && NativoPerfilTabela(&m->codigo, &m->bir, m->pedido);
    if (ok && UsaDwarf(m)) {
        DwarfSemantica sem = {.fonte = m->pedido->fonte, .tipos = m->pedido->tipos, .ir = m->pedido->ir, .nomes = m->pedido->nomes,
                              .com_frame = m->alvo->formato == IMAGEM_ELF};

        DwarfEscrever(&m->codigo, &m->dbg, &sem);
    }
    m->tempos->isel_ns = fatias.isel_ns;
    m->tempos->regs_ns = fatias.regs_ns;
    m->tempos->codificar_ns = fatias.codificar_ns;
    if (ok)
        AssinarRdata(&m->codigo, m->pedido->versao);
    return ok;
}

/* "maior.menor.patch" -> MajorLinkerVersion / MinorLinkerVersion do cabecalho PE (a versao do forja, nao a de um ligador de fora).
 * Versao fora do formato e erro explicito (X0905), nao um linker 0.0 calado. */
static bool VersaoDoLinker(Montagem *m)
{
    unsigned maior = 0, menor = 0;
    const char *versao = m->pedido->versao;

    if (sscanf(versao, "%u.%u", &maior, &menor) != 2 || maior > UINT8_MAX || menor > UINT8_MAX)
        return DiagRelatar(&m->erro, X0905, 0, "versao do forja '%s' fora de maior.menor.patch (0..255) para o cabecalho do PE", versao);
    m->imagem.linker_maior = (uint8_t)maior;
    m->imagem.linker_menor = (uint8_t)menor;
    return true;
}

/* Os 'link' do programa viram as tabelas de imports das DLLs (X0101, X0102, X0103 e X0105 na falha, com os campos extras do JSON). */
static bool ResolverBibliotecas(Montagem *m)
{
    const BibliotecasProvedor provedores[PROVEDOR_QTD] = {[PROVEDOR_RUNTIME] = {&m->runtime, false}, [PROVEDOR_LEGADO] = {&m->legado, true}};
    NativoBibliotecasEntrada entrada = {.pedido = m->pedido, .bir = &m->bir, .provedores = provedores, .qtd_provedores = PROVEDOR_QTD, .libmem = m->pedido->modo == BM_DEV && !m->pedido->dll};

    if (!NativoBibliotecas(&entrada, &m->libs, &m->falha_lib)) {
        m->erro = m->falha_lib.diag;
        return false;
    }
    m->usa_legado = (m->libs.provedores_usados & (1u << PROVEDOR_LEGADO)) != 0;
    return true;
}

/* O nome do arquivo (sem a pasta) de um caminho: o campo Name do export directory da DLL. */
static const char *NomeDoArquivo(const char *caminho)
{
    const char *barra = strrchr(caminho, '/');
    const char *invertida = strrchr(caminho, '\\');

    if (invertida != NULL && (barra == NULL || invertida > barra))
        barra = invertida;
    return barra != NULL ? barra + 1 : caminho;
}

/* Aviso unico: o .drectve dos membros trouxe diretiva que o ligador nao usa (so aligncomm, defaultlib, -l e export valem). */
static void AvisarDiretivas(const TerceirosRelato *relato)
{
    if (relato->ignoradas > 0)
        fprintf(stderr, "aviso: %u diretiva(s) do .drectve de bibliotecas estaticas ignorada(s); a primeira: '%s'\n", relato->ignoradas, relato->primeira_ignorada);
}

/* Os membros das estaticas viram o 4o objeto: slots __imp_ das raizes que uma estatica define e thunks das funcoes de DLL que o codigo de terceiros chama. */
static bool MontarTerceiros(Montagem *m)
{
    TerceirosRelato relato = {0};
    TerceirosPedido ped;
    const Objeto *outros[4] = {&m->codigo, &m->runtime};
    uint32_t qtd_outros = 2;

    if (!m->libs.tem_estaticas)
        return true;
    if (m->usa_ryu)
        outros[qtd_outros++] = &m->ryu;
    if (m->usa_legado)
        outros[qtd_outros++] = &m->legado;
    ped = (TerceirosPedido){.membros = &m->libs.selecao.membros, .celulas = (const char *const *)m->libs.celulas.dados, .qtd_celulas = (uint32_t)m->libs.celulas.n,
                            .tabelas = m->libs.tabelas, .qtd_tabelas = m->libs.qtd_tabelas, .outros = outros, .qtd_outros = qtd_outros, .formato = m->alvo->formato, .libmem =m->pedido->modo == BM_DEV && !m->pedido->dll ? &m->libs.libmem : NULL};
    if (!TerceirosMontar(&ped, &m->terceiros, &relato, &m->erro))
        return false;
    m->usa_terceiros = true;
    m->usa_legado = m->usa_legado || relato.tem_ctors; /* construtor: a entrada do exe passa a ser a forja_entrada_ctor do legado */
    AvisarDiretivas(&relato);
    return true;
}

static LigarPedido PedidoDeLigacao(const Montagem *m)
{
    return (LigarPedido){.programa = &m->codigo, .runtime = &m->runtime, .ryu = m->usa_ryu ? &m->ryu : NULL, .terceiros = m->usa_terceiros ? &m->terceiros : NULL,
                         .legado = m->usa_legado ? &m->legado : NULL, .curto = m->usa_curto ? &m->curto : NULL, .math = m->usa_math ? &m->math : NULL, .es = m->usa_es ? &m->es : NULL, .thread = m->usa_thread ? &m->thread : NULL, .sync = m->usa_sync ? &m->sync : NULL, .atomic = m->usa_atomic ? &m->atomic : NULL, .varena = m->usa_varena ? &m->varena : NULL, .net = m->usa_net ? &m->net : NULL, .entrada = m->alvo->formato == IMAGEM_ELF ? &m->entrada : NULL, .semente = m->tem_semente ? &m->semente : NULL, .perfil = m->pedido->perfil ? &m->perfil : NULL, .tabelas = m->libs.tabelas, .qtd_tabelas = m->libs.qtd_tabelas,
                         .dll = m->pedido->dll, .nome_dll = NomeDoArquivo(m->pedido->saida), .exports = m->bir.exportados,
                         .qtd_exports = m->bir.nexportados, .formato = m->alvo->formato, .pie = m->pedido->pie};
}

/* O programa chama forja_proc_* (std:proc), que vive no mesmo objeto do Ryu: entao o Ryu entra na ligacao mesmo que o runtime nao o peca. */
static void UsarRyuDoPrograma(Montagem *m)
{
    m->usa_ryu = m->usa_ryu || RuntimeUsaRyu(&m->codigo, &m->ryu);
    m->usa_curto = RuntimeUsaRyu(&m->codigo, &m->curto) || RuntimeUsaRyu(&m->runtime, &m->curto);
    m->usa_math = RuntimeUsaRyu(&m->codigo, &m->math);
    m->usa_es = RuntimeUsaRyu(&m->codigo, &m->es);
    m->usa_thread = RuntimeUsaRyu(&m->codigo, &m->thread);
    m->usa_sync = RuntimeUsaRyu(&m->codigo, &m->sync);
    m->usa_atomic = RuntimeUsaRyu(&m->codigo, &m->atomic);
    m->usa_varena = RuntimeUsaRyu(&m->codigo, &m->varena);
    m->usa_net = RuntimeUsaRyu(&m->codigo, &m->net);
}

/* So o PE leva a versao do linker no cabecalho e o .rsrc (VERSIONINFO e manifesto); o ELF nao tem nenhum dos dois. */
static bool AcabarImagemDoPe(Montagem *m)
{
    return m->alvo->formato != IMAGEM_PE || (VersaoDoLinker(m) && PeAcrescentarRecursos(&m->imagem, m->pedido->versao, &m->erro));
}

/* Runtime nativo (lido em ConferirLayout) + programa + as DLLs dos 'link' -> Imagem do alvo (PE com a versao do linker e o .rsrc; ELF so com o que o ligador devolve). */
static bool LigarImagem(Montagem *m)
{
    uint64_t t0 = RelogioNs();
    bool ok;

    if (Injetou(m, ETAPA_LIGAR))
        return false;
    UsarRyuDoPrograma(m);
    if (!ResolverBibliotecas(m) || !MontarTerceiros(m))
        return false;
    LigarPedido pedido = PedidoDeLigacao(m); /* depois do ResolverBibliotecas: as tabelas ja estao em m->libs */
    ok = LigarComTabelas(&pedido, &m->imagem, &m->erro) && AcabarImagemDoPe(m);
    m->tempos->ligar_ns = RelogioNs() - t0;
    return ok;
}

/* X0302 (exit 2): nao gravou o arquivo. EACCES depois das tentativas e dos renomes do ArquivoGravarAtomico = o destino esta em uso. */
static int RelatarFalhaDeGravacao(const char *caminho, int err)
{
    char causa[1250];

    snprintf(causa, sizeof causa, "nao consegui gravar '%s'%s", caminho, err == EACCES ? " (em uso)" : "");
    DiagImprimirJsonSemFonte(X0302, causa, stderr);
    return SAIDA_USO;
}

/* M6b P75 (D-M6-5, D-M6-9): PE com --simbolos sim leva o RSDS e grava o .pdb; o caminho e absoluto e tem o nome base do exe (a extensao vira .pdb). */
static bool UsaPdb(const Montagem *m)
{
    return UsaDwarf(m) && m->alvo->formato == IMAGEM_PE;
}

static void PrepararCodeView(Montagem *m)
{
    char absoluto[sizeof m->pdb_caminho - 8];   /* folga para ".pdb" */
    const char *saida = m->pedido->saida;
    char *ponto, *barra;

    if (_fullpath(absoluto, saida, sizeof absoluto) == NULL)
        snprintf(absoluto, sizeof absoluto, "%s", saida);
    ponto = strrchr(absoluto, '.');
    barra = strrchr(absoluto, '\\');
    if (ponto != NULL && (barra == NULL || ponto > barra))
        *ponto = '\0';
    snprintf(m->pdb_caminho, sizeof m->pdb_caminho, "%s.pdb", absoluto);
    m->imagem.pdb_caminho = m->pdb_caminho;
    m->imagem.cv_guid = m->pdb_guid;
}

/* O escritor do formato do alvo: ELF64 no Linux, PE32+ no Windows. */
static bool EscreverImagem(Montagem *m)
{
    if (m->alvo->formato == IMAGEM_ELF)
        return ElfEscrever(&m->imagem, &m->exe, &m->erro);
    if (UsaPdb(m))
        PrepararCodeView(m);
    return PeEscrever(&m->imagem, &m->exe, &m->erro);
}

/* O .pdb ao lado do exe, casado com ele pelo GUID e pela idade do RSDS. */
static int GravarPdb(Montagem *m)
{
    char modulo[300];
    const char *base = strrchr(m->pdb_caminho, '\\');
    PdbParametros p = {.idade = PE_CV_IDADE, .dbg = &m->dbg, .tipos = m->pedido->tipos, .ir = m->pedido->ir, .nomes = m->pedido->nomes, .fonte = m->pedido->fonte, .exe = &m->exe, .modulo = modulo};
    Buf pdb = {0};
    int err = 0;
    int codigo = SAIDA_OK;

    base = base != NULL ? base + 1 : m->pdb_caminho;
    snprintf(modulo, sizeof modulo, "%.*s.obj", (int)(strlen(base) - 4), base);   /* o nome base do exe (sem o ".pdb") */
    p.codigo_tam = m->imagem.runtime_texto_rva - m->imagem.secoes[0].rva;
    memcpy(p.guid, m->pdb_guid, sizeof p.guid);
    if (!PdbEscrever(&p, &pdb))
        FalhaInterna("pdb: o PDB de '%s' nao coube no MSF", m->pdb_caminho);
    if (!ArquivoGravarAtomicoSemDescarga(m->pdb_caminho, pdb.dados, pdb.tam, &err))   /* regeneravel: sem FlushFileBuffers (custo do --simbolos sim) */
        codigo = RelatarFalhaDeGravacao(m->pdb_caminho, err);
    BufLiberar(&pdb);
    return codigo;
}

/* PE ou ELF em memoria -> arquivo, de forma atomica. Falha ao gravar e X0302 (exit 2); autoconferencia recusada e X0905 (exit 3). */
static int EscreverExe(Montagem *m)
{
    uint64_t t0 = RelogioNs();
    bool ok;
    int err = 0;
    int codigo = SAIDA_OK;

    if (m->alvo->sem_escritor != NULL)
        return RecusarComX0003(m->alvo->sem_escritor); /* o alvo chegou a Imagem (ligador pronto) mas o escritor do formato ainda nao existe */
    ok = !Injetou(m, ETAPA_ESCREVER) && EscreverImagem(m);
    if (!ok)
        return RelatarErro(m);
    if (!ArquivoGravarAtomico(m->pedido->saida, m->exe.dados, m->exe.tam, &err))
        codigo = RelatarFalhaDeGravacao(m->pedido->saida, err);
    m->tempos->escrever_ns = RelogioNs() - t0;
    return codigo;
}

/* Fecha o registro de tempos no instante em que o item tempos do --debug-code vai ser montado: total, outros e as linhas do programa inteiro. */
static void FecharTempos(Montagem *m)
{
    NativoTemposFechar(m->tempos);
    m->tempos->linhas = FonteContarLinhas(m->pedido->fonte);
}

/* <saida>.debug.txt (ou .debug.jsonl, com o item json) ao lado do exe. Falha ao gravar e X0302 (exit 2), como a do exe. */
static int GravarDepuracao(Montagem *m)
{
    bool json = (m->itens & DEBUG_JSON) != 0;
    DebugEntrada entrada = {.mascara = m->itens, .versao = m->pedido->versao, .modo = m->pedido->modo == BM_RELEASE ? "release" : "dev",
                            .alvo = m->alvo->nome, .fonte = m->pedido->fonte, .bir = &m->bir_txt, .otim = &m->otim_txt, .passes = &m->passes_txt, .locais = &m->locais_txt, .dbg = m->dbg_vivo ? &m->dbg : NULL, .lista = &m->lista, .codigo = &m->codigo,
                            .runtime = &m->runtime, .imagem = &m->imagem, .tempos = m->tempos, .tam_exe = m->exe.tam,
                            .procs_total = m->bir.procs_total, .procs_gerados = m->bir.procs_gerados, .pedido = m->pedido, .programa = &m->bir};
    Buf texto = {0};
    char caminho[1100];
    int err = 0;
    int codigo = SAIDA_OK;

    if ((m->itens & DEBUG_TEMPOS) != 0)
        FecharTempos(m);
    snprintf(caminho, sizeof caminho, "%s.debug.%s", m->pedido->saida, json ? "jsonl" : "txt");
    if (json)
        DebugMontarJson(&entrada, &texto);
    else
        DebugMontar(&entrada, &texto);
    if (!ArquivoGravarAtomico(caminho, texto.dados, texto.tam, &err))
        codigo = RelatarFalhaDeGravacao(caminho, err);
    BufLiberar(&texto);
    return codigo;
}

/* --entradas: o .entradas do build (so em sucesso; a faxina e o estado do orquestrador moram fora). */
static int GravarEntradas(Montagem *m)
{
    int err = 0;

    if (NativoEntradasGravar(m->pedido, &m->libs, &err))
        return SAIDA_OK;
    return RelatarFalhaDeGravacao(m->pedido->entradas, err);
}

/* O que acompanha o exe depois que ele foi gravado: <saida>.debug.* (--debug-code) e o .entradas (--entradas). */
static int GravarComplementos(Montagem *m)
{
    int codigo = SAIDA_OK;

    if (UsaPdb(m))
        codigo = GravarPdb(m);
    if (codigo == SAIDA_OK && m->itens != 0)
        codigo = GravarDepuracao(m);
    if (codigo == SAIDA_OK && m->pedido->entradas != NULL)
        codigo = GravarEntradas(m);
    return codigo;
}

/* IR -> BIR -> codigo; com o item bir do --debug-code, deixa o texto da BIR pronto. */
static int MontarCodigo(Montagem *m)
{
    if (!ConstruirBir(m) || !GerarCodigo(m))
        return RelatarErro(m);
    if ((m->itens & DEBUG_BIR) != 0)
        BirImprimir(&m->bir, &m->bir_txt);
    if ((m->itens & DEBUG_LOCAIS) != 0)
        BirImprimirLocais(&m->bir, &m->locais_txt);
    return SAIDA_OK;
}

static int Montar(Montagem *m)
{
    int codigo = MontarCodigo(m);

    if (codigo == SAIDA_OK && !LigarImagem(m))
        codigo = RelatarErro(m);
    if (codigo == SAIDA_OK)
        codigo = EscreverExe(m);
    if (codigo == SAIDA_OK)
        codigo = GravarComplementos(m);
    return codigo;
}

static void LiberarMontagem(Montagem *m)
{
    if (m->bir_viva)
        BirLiberar(&m->bir);
    ObjLiberar(&m->codigo);
    ObjLiberar(&m->runtime);
    ObjLiberar(&m->ryu);
    ObjLiberar(&m->terceiros);
    ObjLiberar(&m->legado);
    ObjLiberar(&m->curto);
    ObjLiberar(&m->math);
    ObjLiberar(&m->es);
    ObjLiberar(&m->thread);
    ObjLiberar(&m->sync);
    ObjLiberar(&m->atomic);
    ObjLiberar(&m->varena);
    ObjLiberar(&m->net);
    ObjLiberar(&m->entrada);
    ObjLiberar(&m->semente), ObjLiberar(&m->perfil);
    ImagemLiberar(&m->imagem);
    BibliotecasLiberar(&m->libs);
    BibliotecaFalhaLiberar(&m->falha_lib);
    BufLiberar(&m->exe);
    BufLiberar(&m->bir_txt);
    BufLiberar(&m->locais_txt), BufLiberar(&m->otim_txt), BufLiberar(&m->passes_txt);
    AlvoListagemLiberar(&m->lista);
    if (m->dbg_vivo)
        DbgLiberar(&m->dbg);
}

/* Zera so o que o nativo preenche (bir..escrever, outros, total, linhas); lex, parse, check, ir e inicio_ns vem do driver. Sem inicio_ns, o build comeca aqui. */
static void IniciarTempos(NativoTempos *t)
{
    NativoTempos novo = {.lex_ns = t->lex_ns, .parse_ns = t->parse_ns, .check_ns = t->check_ns, .ir_ns = t->ir_ns, .inicio_ns = t->inicio_ns};

    if (novo.inicio_ns == 0)
        novo.inicio_ns = RelogioNs();
    *t = novo;
}

/* O que o alvo ainda nao faz, como causa do X0003; NULL = o pedido cabe. (M5b P37: foreign de fora no linux ja liga pela ABI SysV.) */
static const char *RecusaDoAlvo(const NativoPedido *pedido, const Alvo *alvo)
{
    if (pedido->dll && !alvo->dll)
        return NATIVO_X0003_SO;
    return NULL;
}

/* O alvo do pedido (NULL se o nome nao existe) e a causa do X0003 que o recusa (NULL = segue). */
static const Alvo *EscolherAlvo(const NativoPedido *pedido, const char **recusa)
{
    const Alvo *alvo = AlvoPorNome(pedido->alvo != NULL ? pedido->alvo : NATIVO_ALVO_PADRAO);

    *recusa = alvo == NULL ? "alvo desconhecido no backend nativo" : RecusaDoAlvo(pedido, alvo);
    return alvo;
}

int NativoConstruirExe(const NativoPedido *pedido)
{
    NativoTempos descarte = {0};
    Montagem m = {.pedido = pedido};
    const char *recusa;
    char causa[300];
    int codigo;

    if (pedido->debug_code != NULL && !DebugItens(pedido->debug_code, &m.itens, causa, sizeof causa))
        return RecusarComX0003(causa);
    m.alvo = EscolherAlvo(pedido, &recusa);
    if (recusa != NULL)
        return RecusarComX0003(recusa);
    m.tempos = pedido->tempos != NULL ? pedido->tempos : &descarte;
    IniciarTempos(m.tempos);
    ObjIniciar(&m.codigo);
    ObjIniciar(&m.runtime);
    ObjIniciar(&m.ryu);
    ObjIniciar(&m.terceiros);
    ObjIniciar(&m.legado);
    ObjIniciar(&m.curto);
    ObjIniciar(&m.math);
    ObjIniciar(&m.es);
    ObjIniciar(&m.thread);
    ObjIniciar(&m.sync);
    ObjIniciar(&m.atomic);
    ObjIniciar(&m.varena);
    ObjIniciar(&m.net);
    ObjIniciar(&m.entrada);
    ObjIniciar(&m.semente), ObjIniciar(&m.perfil);
    ImagemIniciar(&m.imagem);
    AlvoListagemIniciar(&m.lista);
    codigo = Montar(&m);
    LiberarMontagem(&m);
    return codigo;
}
