/* x64_interno.h - contexto de geracao de uma funcao e primitivas compartilhadas por x64_*.c (nao e API publica; a API e x64_gerar.h).
 * Versao: 0.21.0
 * Changelog: 0.21.0 - S2 (D0703): IselCarregarVetor, IselGuardarVetor, IselContaVetorial, IselSplat, IselCopiaVetor, IselCarregaVetor e IselGravaVetor (x64_isel_real.c); AlocBits(BC_V128) = 128.
 *            0.20.0 - S1b (D0703): X64_VETOR_BYTES (home de 16 B do vreg BC_V128).
 *            0.19.0 - M7a F5: X64Ctx.usos/defs (so com BirFuncao.isel_otim), CC_DA_COND e FORMAS_ARIT exportadas, MI3.
 *            0.18.0 - M7a F4: Quadro.regs_salvos_xmm e salvos_xmm (xmm6..15 do alocador, 16 B cada, no quadro), AlocRegsSalvosXmm.
 *            0.17.0 - M7a F3: PrologoMarcas.salvo (fim de cada push de callee-saved), X64_MAX_SALVOS, AlocHomeDoParametro, QuadroAlocacao.
 *            0.16.0 - M7a F2: AlocOperando (AlocSlot sai), AlocCarregar/AlocGravar/AlocMemoria/AlocRegsSalvos; Quadro.regs_salvos.
 *            0.15.0 - M6e P93d: X64Ctx.guarda.
 *            0.14.0 - M6 P65b: X64Ctx.dbg (DbgPrograma) no lugar dos vetores rastro_*; X64RastroIniciar/Funcao saem; X64DbgLocais (x64_quadro.c).
 *            0.13.0 - M5b P38b: X64GerarEntradasSysv (x64_entrada_sysv.c).
 *            0.12.0 - M5b P37: bloco da chamada SysV (IselChamaSysv, SysvEhChamadaDoPrograma, SysvBytesDePilha; x64_chamada_sysv.c); Quadro.salva/salvos e QuadroCalcular(programa, funcao).
 *            0.11.0 - IA1-b12: X64Ctx.rastreio e os vetores da tabela de posicoes do --rastreio; X64RastroIniciar/Funcao/Chamada/Fechar (x64_rastro.c).
 *            0.10.0 - M4 P24a: X64Ctx.tempos e X64Ctx.marca (fatias de tempo do X64Gerar).
 *            0.9.0 - Linguagem v1.11 L13: IselRaiz (x64_isel_real.c).
 *            0.8.0 - M2 P18b: IselPlantio (plantio de divergencia para teste, x64_isel.c).
 *            0.7.0 - M2 P11b: AbiArg, AbiRetorno, AbiLocal.bytes, AbiWinClassificar(const AbiArg*, n, ...), AbiWinRetorno e os auxiliares do chamado; X64Ctx.cls vira AbiArg*; IselLerBytes, IselGravarBytes
 *                    e IselBlocoMem (x64_isel_mem.c).
 *            0.6.0 - M2 P11a: Quadro.agregados, X64Ctx.slot_off, QuadroPosicionarSlots e x64_isel_mem.c (IselEndSlot, IselCarrega, IselGrava, IselCopiaMem, IselZeraMem).
 *            0.5.0 - M2 P10b: MXmm; x64_isel_real.c (IselCarregarReal, IselGuardarReal, IselGuardarXmm, IselContaReal, IselNegReal, IselCmpReal, IselIParaF, IselFParaF); a ABI
 *                    aceita F32 e F64 (xmm por posicao: o argumento k < 4 real vai em xmm<k>; AbiLocal.reg guarda o numero do xmm).
 *            0.4.0 - M2 P9: IselOperacao, IselIndiceLargura, IselExtensaoDaConta e as regras de x64_isel_conta.c (IselOvCurto, IselDivisao, IselDeslocamento, IselNot).
 *            0.3.0 - backend x64 P8b: X64Ctx.origem (prologo, sondagem, codigo, frio, epilogo) e X64ListarInstr sem o parametro dos bytes.
 *            0.2.0 - backend x64 P8: PrologoMarcas, campos de unwind/listagem no X64Ctx, bloco_off com o laco da sondagem, X64UnwindRegistrar e X64Listar*.
 *            0.1.0 - backend x64 P7a: X64Ctx, Quadro, AbiLocal, construtores de MInstr, emissao, alocacao (degrau 0), quadro, ABI Windows e isel.
 * Verificacao: construir.bat (compila com -Werror) e teste_nativo.exe x64 <arq.fj> <saida.exe>
 *
 * Convencoes:
 *   - Rascunho do degrau 0: rax, rcx, rdx, r10, r11. Nada vive em registrador entre duas BirInstr: todo vreg tem um slot de 8 bytes.
 *   - MInstr em MO_SIMBOLO e em [rip+d] leva em .simbolo o indice do simbolo do OBJETO (nao da BIR); MO_BLOCO leva o indice do bloco
 *     da BIR em .imed e o indice nblocos para o epilogo.
 *   - Vreg de classe I8 e bool (0/1): o slot e lido sempre por movzx de 8 bits.
 */
#ifndef FORJA_NATIVO_X64_X64_INTERNO_H
#define FORJA_NATIVO_X64_X64_INTERNO_H

#include "common/vec.h"
#include "nativo/x64/alvo.h"
#include "nativo/dbg/dbg.h"
#include "nativo/bir/bir.h"
#include "nativo/obj/objeto.h"
#include "nativo/x64/x64.h"

#define X64_ALINHAMENTO_FUNCAO 16u
#define X64_ALINHAMENTO_UNWIND 4u /* RUNTIME_FUNCTION e UNWIND_INFO ficam em limite de DWORD */
#define X64_BYTE_PADDING 0xCC
#define X64_PAGINA 4096u
#define X64_TAM_MAX_INSTR 15u
#define X64_SLOT_BYTES 8u
#define X64_VETOR_BYTES 16u /* home de despejo de um vreg BC_V128 (S1b) */

/* ---- ABI Windows x64 (x64_abi_win.c) ---- */
#define ABI_WIN_REGS_ARG 4u
#define ABI_WIN_SOMBRA 32u        /* home area de 32 bytes reservada pelo chamador */
#define ABI_WIN_REG_RETORNO X64_RAX
#define ABI_WIN_BYTES_ARG 8u

/* bytes: agregado de 1/2/4/8 bytes, que viaja nos proprios bytes (o registrador/slot recebe os bytes lidos do endereco do vreg); 0 = o proprio valor do vreg (escalar ou ponteiro). */
typedef struct { bool na_pilha; X64Reg reg; int32_t desloc; uint8_t bytes; } AbiLocal; /* desloc: a partir do rsp do chamador, no ponto do call */
/* classe e tam de um argumento: tam > 0 = agregado (classe BC_I64 = o endereco). */
typedef struct { BirClasse classe; uint32_t tam; } AbiArg;
typedef enum { ABI_RET_VAZIO, ABI_RET_RAX, ABI_RET_XMM0, ABI_RET_OCULTO } AbiRetorno;

/* Classifica os n argumentos (k<4 em RCX, RDX, R8, R9 ou, se real, em xmm<k> POR POSICAO; k>=4 em [rsp+32+8(k-4)]). AbiLocal.reg de um real
 * e o numero do xmm. Agregado: sempre o inteiro da posicao (tam 1/2/4/8: os bytes; outro tam: o ponteiro). false se alguma classe nao e inteira nem real. */
bool AbiWinClassificar(const AbiArg *args, uint32_t n, AbiLocal *saida);
/* Retorno: inteiro em RAX, real em XMM0, agregado de 1/2/4/8 bytes em RAX, outro agregado por ponteiro oculto (RCX; os visiveis andam uma posicao). */
AbiRetorno AbiWinRetorno(BirClasse classe, uint32_t tam);
bool AbiWinTamEmRegistro(uint32_t tam);
uint32_t AbiWinOcultoDaChamada(const BirInstr *i);
/* Onde a posicao k mora para quem foi chamado: [rbp+16+8k] (home area para k<4, pilha do chamador para k>=4). O ponteiro oculto ocupa a posicao 0. */
int32_t AbiWinParamRbp(uint32_t k);
/* Lado do chamado: 1 se o retorno e por ponteiro oculto (os parametros andam uma posicao); parametro agregado de 1/2/4/8 bytes (os bytes chegam no registrador/slot:
 * o vreg ganha um slot de ponteiro proprio); quantos parametros anteriores a k sao assim. */
uint32_t AbiWinOculto(const BirFuncao *f);
bool AbiWinParamEmBytes(const BirFuncao *f, uint32_t k);
uint32_t AbiWinOrdemEmBytes(const BirFuncao *f, uint32_t k);

/* ---- quadro (x64_quadro.c) ---- */
/* slots = bytes dos slots de vreg; agregados = bytes da area de memoria (BirFuncao.slots) logo abaixo deles, com o alinhamento de cada slot. */
typedef struct { uint32_t slots, agregados, maxargs, tam; bool sondar; bool salva; uint32_t salvos; uint32_t regs_salvos; uint32_t empurrados; uint32_t regs_salvos_xmm; uint32_t salvos_xmm; } Quadro; /* salva/salvos: rsi, rdi e xmm6..15 guardados (M5b P37); regs_salvos: mascara dos callee-saved do alocador (M7a F2); empurrados: bytes dos pushes dela, ja contidos em slots (M7a F3) */

typedef struct { uint32_t fim; uint32_t bloco; } SaltoLocal; /* rel32 em fim-4, a corrigir para o inicio do bloco */

/* Onde o prologo termina, em bytes desde o inicio da funcao: depois do push rbp, do mov rbp,rsp, de cada push de callee-saved do alocador (salvo[k], na ordem dos
 * registradores da mascara) e do sub rsp,N. Alimenta o UNWIND_INFO. */
#define X64_MAX_SALVOS 7u /* rbx, rsi, rdi, r12..r15 */
typedef struct { uint32_t push, fpreg, alocacao; uint32_t salvo[X64_MAX_SALVOS]; } PrologoMarcas;

typedef struct X64Ctx {
    const BirPrograma *p;
    const BirFuncao *f;
    Objeto *o;
    Diag *erro;
    AlvoListagem *lista;        /* NULL = sem --debug-code */
    uint32_t texto;             /* indice da secao .text no Objeto */
    uint32_t pdata, xdata;      /* secoes .pdata e .xdata no Objeto */
    uint32_t sim_xdata;         /* simbolo local no offset 0 de .xdata: base das relocacoes ADDR32NB dos UNWIND_INFO */
    uint32_t func_sim;          /* simbolo do Objeto da funcao em geracao */
    uint32_t inicio;            /* offset em .text do inicio da funcao em geracao */
    PrologoMarcas prologo;
    const uint32_t *obj_sim;    /* indice de BirSimbolo -> simbolo do Objeto */
    Quadro quadro;
    const uint32_t *slot_off;   /* BirFuncao.slots[k] mora em [rbp - slot_off[k]] (QuadroPosicionarSlots) */
    uint32_t *ordem;           /* ordem de emissao: blocos quentes, epilogo (= nblocos), blocos frios */
    uint32_t nordem, pos_ordem;
    uint32_t *bloco_off;        /* nblocos + 2 entradas: inicio de cada bloco, do epilogo (nblocos) e do laco da sondagem (nblocos + 1) */
    Vec saltos;                 /* SaltoLocal */
    AbiArg *cls;                /* maxargs entradas: classe e tam dos argumentos do CHAMA corrente (com o ponteiro oculto na frente, se houver) */
    AbiLocal *abi;              /* maxargs entradas */
    const BirInstr *instr;      /* instrucao em selecao (posicao dos erros) */
    uint8_t origem;             /* ListaOrigem: de onde vem a instrucao que sai agora (alimenta so a listagem do --debug-code) */
    AlvoTempos *tempos;         /* NULL = sem medicao; senao as fatias do X64Gerar (x64_gerar.c) */
    uint64_t marca;             /* ultima leitura do relogio (ns); so vale com tempos != NULL */
    bool rastreio;              /* --rastreio: a tabela de posicoes (x64_rastro.c) sai no .rdata; false = nenhum byte a mais */
    bool guarda;                /* M6e P93d: --guarda-memoria: a tabela do rastreio leva a subtabela de linhas e o bit de guarda; o .text nao muda */
    DbgPrograma *dbg;           /* M6 P65b: registro de depuracao (procs, linhas, locais, chamadas); NULL = sem coleta (custo zero) */
    const uint8_t *usos, *defs; /* M7a F5: por vreg, leituras e escritas na funcao inteira (satura em 255); NULL sem BirFuncao.isel_otim */
} X64Ctx;

/* ---- construtores de MInstr ---- */
static inline MOperando MReg(X64Reg reg, uint8_t largura)
{
    MOperando o = {0};
    o.tipo = MO_REG;
    o.base = reg;
    o.largura = largura;
    return o;
}

static inline MOperando MMem(X64Reg base, int32_t desloc, uint8_t largura)
{
    MOperando o = MReg(base, largura);
    o.tipo = MO_MEM;
    o.desloc = desloc;
    return o;
}

static inline MOperando MXmm(uint8_t numero)
{
    MOperando o = {0};
    o.tipo = MO_XMM;
    o.base = (X64Reg)numero;
    o.largura = 128;
    return o;
}

static inline MOperando MImed(int64_t valor)
{
    MOperando o = {0};
    o.tipo = MO_IMED;
    o.imed = valor;
    return o;
}

static inline MOperando MBloco(uint32_t bloco)
{
    MOperando o = {0};
    o.tipo = MO_BLOCO;
    o.imed = bloco;
    return o;
}

static inline MOperando MSimbolo(uint32_t simbolo)
{
    MOperando o = {0};
    o.tipo = MO_SIMBOLO;
    o.simbolo = simbolo;
    return o;
}

static inline MInstr MI0(uint16_t forma)
{
    MInstr mi = {0};
    mi.forma = forma;
    return mi;
}

static inline MInstr MI1(uint16_t forma, MOperando a)
{
    MInstr mi = MI0(forma);
    mi.nops = 1;
    mi.op[0] = a;
    return mi;
}

static inline MInstr MI2(uint16_t forma, MOperando a, MOperando b)
{
    MInstr mi = MI1(forma, a);
    mi.nops = 2;
    mi.op[1] = b;
    return mi;
}

static inline MInstr MI3(uint16_t forma, MOperando a, MOperando b, MOperando c)
{
    MInstr mi = MI2(forma, a, b);
    mi.nops = 3;
    mi.op[2] = c;
    return mi;
}

static inline MInstr MICc(uint16_t forma, X64Cond cond, MOperando a)
{
    MInstr mi = MI1(forma, a);
    mi.cond = cond;
    return mi;
}

/* ---- emissao (x64_emitir.c) ---- */
/* Codifica, anexa em .text e registra a relocacao (call, lea [rip]) ou o salto local. false = X0902. */
bool X64Emitir(X64Ctx *c, MInstr mi);
uint32_t X64Offset(const X64Ctx *c);
void X64CorrigirSaltos(X64Ctx *c);
void X64Padding(X64Ctx *c);
/* X0901 com o nome da operacao e o motivo, na posicao da instrucao em selecao. */
bool X64SemRegra(const X64Ctx *c, const char *nome, const char *motivo);

/* ---- tabela de posicoes do --rastreio (x64_rastro.c); so chamados com c->rastreio (que exige c->dbg) ---- */
void X64RastroChamada(X64Ctx *c, const MInstr *mi);        /* depois de emitir mi: se for call de proc .fj, guarda o endereco de retorno e a posicao */
void X64RastroFechar(X64Ctx *c);                           /* fim do X64Gerar: grava a tabela no .rdata e define forja_rastro_tabela e forja_rastro_texto */

/* ---- unwind (x64_unwind.c) e listagem do --debug-code (x64_listar.c) ---- */
/* Fim da funcao (sem o padding): grava o UNWIND_INFO em .xdata e o RUNTIME_FUNCTION em .pdata, com as 3 relocacoes ADDR32NB. */
void X64UnwindRegistrar(X64Ctx *c);
void X64ListarFuncao(X64Ctx *c);
void X64ListarInstr(X64Ctx *c, const MInstr *mi, uint32_t tam);
void X64ListarQuadro(const X64Ctx *c, uint32_t fim, const uint8_t *unwind, uint32_t tam_unwind);

/* ---- alocacao, degrau 0 (x64_alocar.c) ---- */
uint8_t AlocBits(uint8_t classe);                 /* 8, 16, 32 ou 64 (F32 = 32, F64 = 64, V128 = 128); 0 = classe fora do M2 */
/* Onde vive um vreg: registrador (em_reg, reg) ou slot [rbp+desloc]; largura = tamanho do acesso em bits. Hoje sempre memoria. */
typedef struct { bool em_reg; X64Reg reg; int32_t desloc; uint8_t largura; } AlocOperando;
AlocOperando AlocOperandoDe(const BirFuncao *f, uint32_t vreg, uint8_t largura);
MOperando AlocMemoria(const AlocOperando *o);                                              /* so com em_reg == false */
bool AlocCarregar(X64Ctx *c, uint16_t forma, MOperando destino, const AlocOperando *o);    /* forma de leitura (RM): destino = o */
bool AlocGravar(X64Ctx *c, uint16_t forma, const AlocOperando *o, MOperando origem);       /* forma de escrita (MR): o = origem */
uint32_t AlocRegsSalvos(const BirFuncao *f);                                               /* mascara de X64Reg callee-saved usados */
uint32_t AlocRegsSalvosXmm(const BirFuncao *f);                                            /* mascara de xmm6..15 usados (bit = numero do xmm) */
uint32_t AlocBytesEmpurrados(const BirFuncao *f);                                         /* 8 por callee-saved que o prologo empurra abaixo do rbp */
MOperando AlocHomeDoParametro(const BirFuncao *f, uint32_t k, uint8_t largura);            /* [rbp+16+8k] do parametro k: de onde o prologo o poe no registrador */
uint32_t AlocBytesSlots(const BirFuncao *f);
bool AlocClassesSuportadas(const X64Ctx *c);

/* ---- quadro (x64_quadro.c) ---- */
Quadro QuadroCalcular(const BirPrograma *p, const BirFuncao *f);
/* Bytes do "sub rsp,N" do prologo: o quadro menos o que os pushes de callee-saved ja tomaram. */
uint32_t QuadroAlocacao(const Quadro *q);
/* Distancia de cada slot de memoria ate o rbp (abaixo dos slots de vreg, cada um alinhado ao seu alinhamento); saida pode ser NULL. Devolve o fim da area
 * (bytes desde o rbp, contando os slots de vreg): o quadro continua multiplo de 16 e rbp e multiplo de 16, entao [rbp - off] respeita o alinhamento. */
uint32_t QuadroPosicionarSlots(const BirFuncao *f, uint32_t *saida);
/* M6 P65b: entrega ao DbgPrograma (c->dbg) o nome, o tipo e o deslocamento em rbp de cada local do usuario da funcao; chamar depois de c->slot_off pronto. */
void X64DbgLocais(const X64Ctx *c);
void QuadroOrdenar(const BirFuncao *f, uint32_t *ordem);   /* ordem tem nblocos + 1 entradas */
bool QuadroPrologo(X64Ctx *c);
bool QuadroEpilogo(X64Ctx *c);
uint32_t QuadroProximoBloco(const X64Ctx *c);              /* proximo na ordem de emissao ou BIR_SEM_BLOCO */

/* ---- selecao (x64_isel.c, x64_isel_fluxo.c) ---- */
typedef bool (*IselFn)(X64Ctx *c, const BirInstr *i);
typedef struct { const char *nome; IselFn fn; } RegraIsel;
extern const RegraIsel REGRAS_ISEL[BIR_QTD];

/* BirCond (as 10 inteiras) -> condicao do setcc/jcc: iguais, com sinal (l, le, g, ge) e sem sinal (b, be, a, ae). */
extern const X64Cond CC_DA_COND[10];

/* Formas da operacao de dois operandos "rax = rax op b", indexadas por [0] = 32 bits e [1] = 64 bits (I8 e I32 operam em 32); ri = FX_QTD quando nao ha forma com imediato. */
typedef struct { uint16_t rr[2]; uint16_t ri[2]; } FormasAritmeticas;
extern const FormasAritmeticas FORMAS_ARIT[BIR_QTD];

bool X64Selecionar(X64Ctx *c, const BirInstr *i);
/* SELECIONA (R5, x64_isel_otim_fluxo.c): cmp + cmovcc em inteiro, minss/maxss em real. */
bool IselSeleciona(X64Ctx *c, const BirInstr *i);
/* Carrega o operando (vreg ou imediato) em reg; ext (BirExtensao) escolhe o alargamento a 64 bits. */
bool IselCarregar(X64Ctx *c, X64Reg reg, const BirOperando *o, uint8_t ext);
bool IselGuardar(X64Ctx *c, uint32_t vreg, X64Reg reg);
bool IselSalto(X64Ctx *c, uint32_t bloco);                 /* jmp, que some se o bloco e o proximo */
/* rax = rax <base> b (ADD SUB AND OR XOR CMP MUL), operandos alargados por ext; a largura vem da classe de i->a. */
bool IselOperacao(X64Ctx *c, const BirInstr *i, uint8_t base, uint8_t ext);
uint8_t IselIndiceLargura(uint8_t classe);                 /* 1 = 64 bits, 0 = 32 (I8, I16 e I32 operam em 32) */
uint8_t IselExtensaoDaConta(uint8_t classe, bool com_sinal); /* BE_SINAL so para I8 e I16 com sinal; senao BE_NENHUMA */
/* So teste (FORJA_TESTE_PLANTAR=1|2|3): o plantio (1..3) que vale para esta instrucao; 0 = nenhum. Unico ponto que le a variavel. */
uint8_t IselPlantio(const X64Ctx *c, const BirInstr *i);

/* x64_isel_conta.c: o que o M2 acrescenta as contas */
bool IselOvCurto(X64Ctx *c, const BirInstr *i, uint8_t base); /* *_OV de I8 e I16: conta em 32 bits e faixa do tipo */
bool IselDivisao(X64Ctx *c, const BirInstr *i);            /* DIV e REM (cdq/cqo + idiv, ou xor edx + div) */
bool IselDeslocamento(X64Ctx *c, const BirInstr *i);       /* SHL e SHR por cl ou por imm8 */
bool IselNot(X64Ctx *c, const BirInstr *i);

/* x64_isel_real.c: reais. O registrador de IselCarregar/IselGuardar vale como numero de xmm quando o operando e real. */
bool IselCarregarReal(X64Ctx *c, uint8_t xmm, const BirOperando *o);
bool IselGuardarReal(X64Ctx *c, uint32_t vreg, uint8_t xmm);                 /* xmm -> slot do vreg */
bool IselGuardarXmm(X64Ctx *c, uint8_t classe, MOperando memoria, uint8_t xmm); /* movss/movsd na memoria dada (largura 32 ou 64) */
bool IselContaReal(X64Ctx *c, const BirInstr *i);                            /* ADD SUB MUL DIV */
bool IselNegReal(X64Ctx *c, const BirInstr *i);                              /* xorps com a mascara de sinal (BIR b = simbolo BS_DADO) */
bool IselCmpReal(X64Ctx *c, const BirInstr *i);                              /* ucomis + setcc, com a paridade para o NaN */
bool IselIParaF(X64Ctx *c, const BirInstr *i);
bool IselFParaF(X64Ctx *c, const BirInstr *i);
bool IselRaiz(X64Ctx *c, const BirInstr *i);
/* S2 (vetor V128, x64_isel_real.c): o vetor vive em xmm (registrador do alocador) ou no home de 16 B; movups e a forma de ida e volta. */
bool IselCarregarVetor(X64Ctx *c, uint8_t xmm, uint32_t vreg);                /* vreg -> xmm (movups do home ou movaps do registrador) */
bool IselGuardarVetor(X64Ctx *c, uint32_t vreg, uint8_t xmm);                 /* xmm -> vreg (movups ao home ou movaps ao registrador) */
bool IselContaVetorial(X64Ctx *c, const BirInstr *i);                         /* ADD SUB MUL DIV de V128 (addps..divpd) */
bool IselSplat(X64Ctx *c, const BirInstr *i);                                 /* SPLAT: shufps (f32) ou unpcklpd (f64) */
bool IselCopiaVetor(X64Ctx *c, const BirInstr *i);                            /* COPIA de V128 */
bool IselCarregaVetor(X64Ctx *c, const BirInstr *i);                          /* CARREGA de V128: movups xmm, [rcx+desloc] */
bool IselGravaVetor(X64Ctx *c, const BirInstr *i);                            /* GRAVA de V128: movups [rcx+desloc], xmm */

/* x64_isel_mem.c: END_SLOT (lea rax,[rbp-off]), CARREGA, GRAVA (base em rcx, valor em rax ou xmm0), COPIA_MEM e ZERA_MEM (movs de 8 bytes ate 64 + cauda 4/2/1) */
bool IselEndSlot(X64Ctx *c, const BirInstr *i);
bool IselCarrega(X64Ctx *c, const BirInstr *i);
bool IselGrava(X64Ctx *c, const BirInstr *i);
bool IselCopiaMem(X64Ctx *c, const BirInstr *i);
bool IselZeraMem(X64Ctx *c, const BirInstr *i);
/* Agregado por valor (P11b): le/grava 1, 2, 4 ou 8 bytes de [base] em/de reg (a leitura zera o resto do registrador) e copia n bytes de [rdx] para [rcx]
 * (ate 64: movs de 8 bytes + cauda; acima, mov r8d,n e call memcpy do runtime; destroi rax, rcx, rdx, r8..r11). */
bool IselLerBytes(X64Ctx *c, X64Reg reg, X64Reg base, uint32_t bytes);
bool IselGravarBytes(X64Ctx *c, X64Reg base, X64Reg reg, uint32_t bytes);
bool IselBlocoMem(X64Ctx *c, uint32_t n);

bool IselChama(X64Ctx *c, const BirInstr *i);

/* ---- chamada SysV de foreign de fora no alvo linux (x64_chamada_sysv.c, M5b P37) ---- */
bool IselChamaSysv(X64Ctx *c, const BirInstr *i);
bool SysvEhChamadaDoPrograma(const BirPrograma *p, const BirInstr *i); /* CHAMA a BS_IMPORT com o programa no alvo linux */
uint32_t SysvBytesDePilha(const BirInstr *i);                          /* bytes de pilha da chamada SysV (0 se nao fecha) */
bool X64GerarEntradasSysv(X64Ctx *c);                                  /* stubs fjt_ dos callbacks escalares (x64_entrada_sysv.c, M5b P38b) */
bool IselArmadilha(X64Ctx *c, const BirInstr *i);
bool IselSalta(X64Ctx *c, const BirInstr *i);
bool IselDesvia(X64Ctx *c, const BirInstr *i);
bool IselRetorna(X64Ctx *c, const BirInstr *i);

#endif
