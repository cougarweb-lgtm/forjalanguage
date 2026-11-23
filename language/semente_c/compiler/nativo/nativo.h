/* nativo.h - backend nativo: IR -> BIR -> Objeto -> Ligar -> PE, sem gcc nem ld. Ponto de entrada do driver (forja build/run --backend x64).
 * Versao: 0.20.0
 * Changelog: 0.20.0 - Linguagem v1.13 L1b: NativoPacote e NativoPedido.pacotes (a pasta de cada modulo de pasta para o --entradas, linha "pasta").
 *            0.19.0 - M8 P2: NativoPedido.perfil (--perfil).
 *            0.18.0 - M7a F0: NativoPedido.sem_otim (--sem-otim).
 *            0.17.0 - O0 P13: NativoPedido.checagem (--checagem).
 *            0.16.0 - M6e P93d: NativoPedido.guarda_memoria (--guarda-memoria).
 *            0.15.0 - M6 P66: NativoPedido.simbolos (--simbolos; o programa ganha DWARF no ELF).
 *            0.14.0 - M5b P41: NativoPedido.pie (--pie) chega ao LigarPedido.pie e a semente (rcrt1.o).
 *            0.13.0 - M5b P37: sai NATIVO_X0003_FOREIGN_LINUX (foreign de fora no alvo linux liga pela ABI SysV).
 *            0.12.0 - B12: NativoCarga e NativoPedido.cargas (os #load para o --entradas).
 *            0.11.0 - IA1-b12: NativoPedido.rastreio (--rastreio).
 *            0.10.0 - M5 P27a: NATIVO_X0003_SO e NATIVO_X0003_FOREIGN_LINUX (as causas do X0003 do alvo x86_64-linux, usadas pelo nativo.c e pelo driver); NativoPedido.alvo aceita "x86_64-linux".
 *            0.9.0 - M4 P24a: NativoTempos com os 13 campos do registro "tempos" (lex, parse, check, ir, 6 do nativo, outros, total, linhas) mais inicio_ns; NATIVO_TEMPOS_CAMPOS,
 *                    NativoTemposAnexarJson, NativoTemposValor e NativoTemposFechar (nativo_tempos.c); regs_ns e codificar_ns passam a ser medidos.
 *            0.8.0 - M3 P22-O1a: NativoPedido.lib_pasta e NativoPedido.entradas (--lib-pasta e --entradas).
 *            0.7.0 - M3 P22a: NativoPedido.dll (build --modo dll: a saida e uma DLL com export directory, .reloc e o DllMain do runtime; sem main exigido).
 *            0.6.0 - M3 P19: NativoPedido.pasta_libs, pastas_extra e npastas_extra (onde achar as bibliotecas dos 'link'); exit 1 tambem para X0101, X0102, X0103 e X0105.
 *            0.5.0 - P18a: NativoPedido.traco.
 *            0.4.0 - M2 P12b: NativoPedido.teste (forja test --backend x64).
 *            0.3.1 - portao M1: enum SAIDA_* aqui (fonte unica); regs_ns/codificar_ns documentados como nao medidos.
 *            0.3.0 - backend x64 P8b: NativoPedido.comando, NATIVO_ALVO_PADRAO e NativoSuporteTabela/NativoSuportePrograma (--suporte --json).
 *            0.2.0 - backend x64 P8: debug_code implementado (lista separada por virgula); tempos: o isel_ns cobre selecao, alocacao e codificacao.
 *            0.1.0 - backend x64 P7b: NativoPedido, NativoTempos e NativoConstruirExe.
 * Verificacao: construir.bat && python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe
 */
#ifndef FORJA_NATIVO_NATIVO_H
#define FORJA_NATIVO_NATIVO_H

#include "common/buf.h"
#include "common/fonte.h"
#include "common/vec.h"
#include "nativo/bir/bir.h"

/* B12: um #load lido pelo driver: caminho absoluto do arquivo e Hash64 do conteudo (a linha "load" do --entradas). */
typedef struct {
    const char *caminho;
    uint64_t hash;
} NativoCarga;

/* v1.13 L1b: a pasta de um modulo de pasta que o driver carregou: caminho (a pasta, como o driver a achou) e o Hash64 da lista dos nomes dos .fj dela (PastaHashNomes): a linha "pasta" do --entradas. */
typedef struct {
    const char *caminho;
    uint64_t hash;
} NativoPacote;

/* Codigos de saida do forja (fonte unica: nativo.c, nativo_erro.c e driver/main.c). */
enum { SAIDA_OK = 0, SAIDA_DIAGNOSTICO = 1, SAIDA_USO = 2, SAIDA_INTERNO = 3 };

/* Tempo (ns) do build inteiro: UMA struct para o --tempos do driver e o registro "tempos" do .debug.jsonl (a tabela NATIVO_TEMPOS_CAMPOS e a
 * unica lista de nomes). As 10 primeiras sao as etapas, na ordem do pipeline; depois vem o resto (outros = total - soma das etapas), o total e as linhas
 * do programa inteiro (raiz e modulos). O X64Gerar funde selecao, alocacao (degrau 0: um slot por vreg) e codificacao instrucao a instrucao, entao
 * as 3 fatias dele sao por funcao (alvo.h AlvoTempos): regs = preparo e alocacao, isel = prologo e corpo (selecao e codificacao fundidas),
 * codificar = fechamento e montagem do Objeto. O driver preenche lex, parse, check e ir; o nativo, o resto. */
typedef struct {
    uint64_t lex_ns, parse_ns, check_ns, ir_ns, bir_ns, isel_ns, regs_ns, codificar_ns, ligar_ns, escrever_ns, outros_ns, total_ns, linhas;
    uint64_t inicio_ns;         /* relogio (RelogioNs) no inicio do build, para o total_ns; 0 = o NativoConstruirExe marca na entrada. Fora do registro. */
} NativoTempos;

/* Um campo do registro "tempos": o nome no JSON (o do membro) e o deslocamento dele em NativoTempos. */
typedef struct { const char *nome; size_t deslocamento; } NativoTempoCampo;

#define NATIVO_TEMPOS_QTD 13        /* campos do registro */
#define NATIVO_TEMPOS_ETAPAS 10     /* os primeiros: lex_ns .. escrever_ns */
extern const NativoTempoCampo NATIVO_TEMPOS_CAMPOS[NATIVO_TEMPOS_QTD];

/* Anexa a b os 13 campos como ", \"<nome>\": <valor>" (sem chaves: quem chama abre e fecha o objeto JSON). */
void NativoTemposAnexarJson(Buf *b, const NativoTempos *t);

/* Valor do campo k da tabela (0 <= k < NATIVO_TEMPOS_QTD). */
uint64_t NativoTemposValor(const NativoTempos *t, size_t k);

/* Fecha o registro: total_ns = agora - inicio_ns e outros_ns = total_ns - soma das etapas. Soma maior que o total e FalhaInterna com os numeros
 * (as etapas sao intervalos disjuntos dentro do total: so um relogio ou um campo errado quebra isso). */
void NativoTemposFechar(NativoTempos *t);

/* --tempos do build x64: fecha o registro (NativoTemposFechar), conta as linhas de 'fonte' e grava em 'caminho' uma linha JSON com emit_ns (0 no x64)
 * e os 13 campos da tabela. false = nao gravou o arquivo (a mensagem sai em stderr). */
bool NativoTemposGravar(const char *caminho, uint64_t emit_ns, NativoTempos *t, const Fonte *fonte);

typedef struct {
    const IrPrograma *ir;
    const Tipos *tipos;
    const Interner *nomes;
    const Fonte *fonte;
    uint8_t modo;               /* BirModo: BM_DEV liga as checagens de estouro e conversao */
    const char *saida;          /* caminho do .exe */
    const char *debug_code;     /* NULL = sem arquivo de depuracao; senao itens separados por virgula (nativo_debug.h); desconhecido = X0003 */
    const char *alvo;           /* NULL = "x86_64-windows" */
    const char *versao;         /* versao do forja ("0.18.1"): vai para a string, o PE e o VERSIONINFO do exe */
    NativoTempos *tempos;       /* NULL = sem medicao */
    const char *comando;        /* a linha de comando inteira, so para o pacote de relato (NULL = vazia) */
    bool teste;                 /* forja test: o exe roda os 'test' (forja_testes_rodar) no lugar do main; modo sempre BM_DEV */
    bool traco;                 /* --traco (P18a): o exe escreve em stderr uma linha "@traco ..." a cada gravacao de local ou temporario escalar */
    bool dll;                   /* M3 P22a: build --modo dll (a saida e uma DLL; o chamador ja conferiu que ha ao menos um 'pub export proc') */
    const char *pasta_libs;            /* M3 P19: pasta absoluta do .fj raiz = 1a pasta de busca das bibliotecas de 'link' (NULL = so o System32) */
    const char *const *pastas_extra;   /* vendor\ e %FORJA_ENGINE%\lib (so com vendor:/engine:), depois da pasta do .fj */
    uint32_t npastas_extra;
    const char *lib_pasta;             /* M3 P22-O1a: --lib-pasta, acrescentada ao FIM das pastas de busca (antes do System32); NULL = nenhuma */
    const char *entradas;              /* M3 P22-O1a: --entradas, caminho do .entradas que o build grava (so em sucesso); NULL = nao grava */
    const Vec *cargas;                 /* B12: os #load lidos (NativoCarga: caminho absoluto e Hash64), entram no --entradas como linhas "load"; NULL = nenhum */
    const Vec *pacotes;                /* v1.13 L1b: as pastas de modulo de pasta carregadas (NativoPacote), entram no --entradas como linhas "pasta"; NULL = nenhuma */
    bool pie;                          /* M5b P41: --pie, ELF static-pie no alvo x86_64-linux (base 0, PT_DYNAMIC e .rela.dyn; com foreign a semente entra como rcrt1.o) */
    const char *sem_otim;              /* M7a F0: --sem-otim (passes desligados, ou "tudo"); NULL = todos os ligados; so vale em BM_RELEASE */
    bool checagem;                     /* O0 P13: indice e nulo conferidos tambem no release (--checagem sim); em BM_DEV sempre se confere (BirConfereLimites) */
    bool simbolos;                   /* M6 P66 (D-M6-5): DWARF do programa (secoes .debug_* no ELF); o PE entra no P68. Padrao: sim em dev, nao em release (OpcoesSimbolos) */
    bool guarda_memoria;               /* M6e P93d (D-M6e-6): --guarda-memoria (so dev, x64, exe): a FRT1 leva a subtabela de linhas e o bit de guarda; o .text nao muda */
    bool perfil;                       /* M8 P2 (D-M8-5): --perfil (so exe): o passe `perfil` instrumenta as procs, a tabela FPF1 vai no .rdata e rt_perfil entra na ligacao; sem ela o exe sai byte a byte igual */
    bool rastreio;                     /* IA1-b12: --rastreio; vale so no alvo Windows em dev e sem --modo dll (outro caso o ignora): o exe leva a tabela de posicoes e o runtime com o rastreio, e a linha R ganha coluna, valor e pilha .fj */
} NativoPedido;

#define NATIVO_ALVO_PADRAO "x86_64-windows"

/* Causas do X0003 que o alvo x86_64-linux devolve (D-M5-8): o ELF do M5 e estatico, sem libc e sem biblioteca de fora. */
#define NATIVO_X0003_SO "biblioteca .so: M5c"

/* Gera o exe em pedido->saida. Devolve o codigo de saida do forja: 0 ok | 1 diagnostico (X0001 em stdout) | 2 uso (X0003 e X0302 em stderr)
 * | 3 erro interno (X0901..X0906 em stdout). */
int NativoConstruirExe(const NativoPedido *pedido);

/* --suporte --json sem arquivo: a tabela REGRAS_IR (o que cada IrOp tem no backend nativo hoje) em uma linha JSON no stdout. Sempre exit 0. */
int NativoSuporteTabela(void);

/* --suporte --json com arquivo: cada construcao do programa que o backend nativo ainda recusa (X0001), numa linha JSON no stdout. Exit 0, com ou sem
 * ocorrencias (e um relatorio, nao um diagnostico). pedido->saida e debug_code nao sao usados. */
int NativoSuportePrograma(const NativoPedido *pedido);

#endif
