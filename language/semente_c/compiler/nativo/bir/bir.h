/* bir.h - BIR: representacao intermediaria do backend nativo, proxima da maquina, construida a partir da IR (ir/ir.h 0.13.1).
 * Versao: 0.28.0
 * Changelog: 0.28.0 - M8 P2: BirEntrada.perfil e BirPrograma.perfil (--perfil: o passe `perfil` instrumenta as procs de usuario; nenhum op novo).
 *            0.27.0 - F5-a (Forja 0.21.0): BIR_VERSAO alinhado a este cabecalho (estava 0.14.0); so texto informativo, formato nao muda.
 *            0.27.0 - S2 (D0703): BIR_SPLAT (dst V128 = difundir o vreg escalar real a em todas as lanes; aux = BVEC_4XF32 com origem F32 ou BVEC_2XF64 com origem F64) e ADD SUB MUL DIV de V128 (forma 4XF32 ou 2XF64 em aux, operandos V128 de mesma forma); BIR_SPLAT entra depois de BIR_SELECIONA, antes de BIR_END_SLOT.
 *            0.26.0 - S1b (D0703): BC_V128, BirVetorForma (aux), BirClasseEhXmm; nada o produz ainda (COPIA/CARREGA/GRAVA validos; o resto recusa).
 *            0.25.0 - R6a-3 (D0704): BirPrograma.endereco_ligado (o `licm` so deixa o add x, imm de endereco quando o passe `endereco` roda; neutralidade do --sem-otim endereco). 0.24.0 - R5 (SIMD): BIR_SELECIONA (dst = (args[0] aux args[1]) ? a : b; passe `selecao`) e BirSelecionaMinMax (bir_selecao.c); a op entra antes de BIR_END_SLOT (as ops de memoria seguem a partir dele).
 *            0.23.0 - S1a (D0703): BirClasseEhReal (so F32 e F64 sao reais; comparar classe >= BC_F32 deixava um BC_V128 futuro virar real).
 *            0.22.0 - M7b F3: BirEntrada.simbolos_dbg e BirPrograma.simbolos_dbg (--simbolos: o passe `sroa` deixa os slots de BirLocalCasa como estao, o PDB/DWARF aponta para eles).
 *            0.21.0 -M7b F1: BirFuncao.div_const (passe `divconst`: DIV/REM por constante viram multiplicacao e deslocamento no isel pos-alocador).
 *            0.20.0 - M7a F5: BirFuncao.isel_otim (passe `isel`: o x64 usa a selecao pos-alocador, x64_isel_otim*.c).
 *            0.19.0 - M7a F4: BirFuncao.regs_salvos_xmm (mascara dos xmm6..15 que reg_vreg usa); o reg_vreg de vreg real e o numero do xmm.
 *            0.18.0 - M7a F3: BirFuncao.reg_vreg e regs_salvos (alocacao de registradores do passe `alocador`; NULL/0 = tudo em slot) e BIR_SEM_REG.
 *            0.17.0 - O0 P13: BirEntrada.checagem, BirPrograma.checagem (indice e nulo no release com --checagem sim; o predicado BirConfereLimites fica em bir_interno.h).
 *            0.16.0 - M6e P93k: BirEntrada.guarda e BirPrograma.guarda (--guarda-memoria).
 *            0.15.0 - M6 P65a (D-M6-13): BirLocalCasa, BirFuncao.casas/ncasas/nsem_casa e BirImprimirLocais (bir_locais.c).
 *            0.14.0 - M5b P39-B1 (D0654): BirNomesMem (nome do runtime que copia e zera bloco acima de BIR_COPIA_MEM_MAX) em BirEntrada.nomes_mem e BirPrograma.nomes_mem; o alvo linux chama forja_memcpy/forja_memset.
 *            0.13.0 - M5b P38b: BirEntradaSysv e BirPrograma.entradas_sysv (stub SysV fjt_ + wrapper fjtw_ de cada callback escalar no alvo linux). Nenhuma BirOp nova.
 *            0.12.0 - M5b P37: BirSysv (descritor SysV de um agregado por valor que cruza para foreign de fora no alvo linux: folhas INTEGER/SSE com
 *                    deslocamento e alinhamento) em BirOperando.sysv; BirEntrada.sysv e BirPrograma.sysv (alvo linux). Nenhuma BirOp nova; o dump nao muda.
 *            0.11.0 - M4 P24p: BirEntrada.poda (so os procs alcancaveis das raizes entram na BIR; bir_alcance.h) e BirPrograma.procs_total / procs_gerados (o registro "poda" do --debug-code).
 *            0.10.0 - M3 P22a: BirEntrada.dll (DLL: sem forja_programa e sem exigir main; um involucro de nome limpo por 'pub export proc' e forja_liberar) e BirPrograma.exportados/nexportados
 *                    (os nomes exportados, em ordem do fonte; vivem na arena). Nenhuma BirOp nova.
 *            0.9.0 - M3 P21v-c: BIR_CHAMA_IND (chamada pelo valor de um proc: b = vreg I64 com o endereco; args, dst e retorno agregado como no CHAMA), entre BIR_RAIZ e BIR_END_SLOT.
 *            0.8.0 - M3 P19: BS_IMPORT (simbolo "__imp_<simbolo C>" de um foreign de DLL; a CHAMA a ele vira lea rax,[rip+__imp_x] / call [rax]) e BirSimbolo.pos (offset do 1o uso).
 *            0.7.0 - Linguagem v1.11 L13: BIR_RAIZ (dst = sqrt(a), a e dst F32 ou F64) entre BIR_F_PARA_F e BIR_END_SLOT (as ops de memoria seguem a partir de BIR_END_SLOT).
 *            0.6.1 - P18a: BirEntrada.traco (nenhum op novo: o traco e CHAMA ao runtime).
 *            0.6.0 - M2 P12b: BirEntrada.teste (forja test): as funcoes 'test' entram na BIR e o forja_programa registra cada uma (forja_teste_registrar) e devolve forja_testes_rodar,
 *                    no lugar de chamar fj_main. Nenhum op novo.
 *            0.5.0 - M2 P11b: agregado por valor. BirOperando.tam (bytes do agregado; 0 = escalar) nos args de CHAMA/ARMADILHA, em RETORNA.a e em CHAMA.a; BirFuncao.param_tam[] (NULL = nenhum
 *                    parametro agregado) e retorno_tam. O agregado viaja como ENDERECO (vreg I64) + tam; quem decide registrador x ponteiro x ponteiro oculto e o x64_abi_win.c.
 *                    Arg de CHAMA: o endereco de uma memoria PRIVADA do chamado (o construtor copia antes; o chamado pode alterar). Parametro agregado: o vreg k e o endereco da
 *                    memoria do chamado. Retorno agregado: CHAMA.a = vreg I64 com o endereco de destino (dst = BIR_SEM_VREG, classe BC_NADA); RETORNA.a = endereco da origem.
 *            0.4.0 - M2 P11a: memoria explicita do quadro. BirFuncao.slots[] {tam, alinhamento}; BirInstr.desloc; BIR_END_SLOT (dst I64 = endereco do slot a = imed k), BIR_CARREGA
 *                    (dst = [a + desloc], classe do dst; I8/I16 carregam a largura e o EXT_S/EXT_Z alarga), BIR_GRAVA ([a + desloc] = b, classe de b), BIR_COPIA_MEM ([a] <- [b],
 *                    desloc = n bytes, 1..64) e BIR_ZERA_MEM ([a], desloc = n, 1..64); acima de 64 bytes o construtor chama memcpy/memset do runtime. O validador confere slots
 *                    (alinhamento potencia de 2 ate 16), classes e bases I64. Agregado = slot + vreg I64 com o endereco; ponteiro e I64.
 *            0.3.0 - M2 P10b: BC_F32 e BC_F64 ativas; ADD SUB MUL DIV NEG em F*; CMP com BCOND_FEQ..FGE (desordenado: so FNE e verdadeiro); BIR_I_PARA_F (aux
 *                    BIR_SINAL|BIR_SEM_SINAL; a = I32 ou I64, e I64 so com sinal: o uint de 64 bits o construtor expande em blocos) e BIR_F_PARA_F (F32 <-> F64);
 *                    BirSimbolo BS_DADO (bytes + alinhamento, deduplicado como o BS_TEXTO) para a mascara de sinal do NEG real. BIR_BITS nao entrou (nenhum
 *                    construtor do P10b o usa; vem com o transmute).
 *            0.2.0 - M2 P9: BIR_DIV, BIR_REM (aux BIR_SINAL|BIR_SEM_SINAL), BIR_SHL, BIR_SHR e BIR_NOT; BC_I16 ativa; *_OV em I8/I16; o validador exige a
 *                    guarda das pre-condicoes de DIV, REM, SHL e SHR (X0904 sem ela).
 *            0.1.0 - backend x64 P6: BirClasse, BirOperando, BirOp, BirInstr, BirBloco, BirFuncao, BirSimbolo, BirPrograma; BirConstruir (X0001),
 *                    BirValidar (X0904), BirImprimir, BirLiberar; REGRAS_IR[] (o que cada IrOp tem no backend nativo, lido pelo futuro --suporte).
 * Verificacao: (em language\compiler) construir.bat && teste_nativo.exe && python -I ..\tests\nativo\bir\verificar_bir.py
 *
 * Modelo: cada funcao e uma lista de blocos; cada bloco e uma lista de instrucoes que termina em UM terminador (SALTA, DESVIA, RETORNA
 * ou ARMADILHA). vreg = registrador virtual, NAO SSA (um vreg pode ser regravado: locais e o contador do 'para'). Vregs 0..nparams-1 sao os
 * parametros. A classe do vreg e a largura da maquina; o sinal NAO esta na classe (int e uint sao BC_I64), esta na operacao (BirCond, aux do *_OV).
 * Operacoes (dst <- a, b):
 *   CONST dst = a(imediato) | COPIA dst = a | ADD SUB MUL AND OR XOR dst = a op b (wrap na largura da classe) | NEG dst = -a
 *   CMP dst(BC_I8: 0/1) = a aux b, aux = BirCond, operandos de mesma classe
 *   EXT_S / EXT_Z dst = a estendido (com / sem sinal) para a classe de dst | TRUNC dst = os bits baixos de a
 *   ADD_OV SUB_OV MUL_OV NEG_OV dst = a op b; aux = BIR_SINAL ou BIR_SEM_SINAL; se estourar o resultado, desvia para o bloco frio alvo[0]
 *     (a instrucao NAO termina o bloco: sem estouro segue na proxima). NEG_OV usa (a, b) = (0, x), igual ao forja_sub(0, x) do runtime.
 *   NOT dst = ~a (na largura da classe)
 *   DIV REM dst = a / b, a % b; aux = BIR_SINAL ou BIR_SEM_SINAL. Pre-condicao: b != 0 e, com sinal, nunca (MIN, -1): quem garante e o construtor,
 *     com blocos explicitos (o validador confere o teste de b no unico predecessor de quem usa DIV/REM).
 *   SHL SHR dst = a << b, a >> b (SHR logico; so sem sinal). Pre-condicao: b < largura da classe (comparacao sem sinal), conferida como acima.
 *   Real (BC_F32 e BC_F64; o imediato real guarda os BITS do valor, 32 em F32): ADD SUB MUL DIV dst = a op b (sem checagem, sem FMA); CONST COPIA como nos
 *     inteiros; NEG dst = -a com b = o simbolo BS_DADO da mascara de sinal (xorps); CMP dst(BC_I8) = a aux b com aux = BCOND_F* (ucomis); I_PARA_F dst = (real) a;
 *     F_PARA_F dst = a convertido entre F32 e F64; RAIZ dst = sqrt(a) (L13; a e dst da mesma classe real; sqrtss/sqrtsd: negativo da NaN). Real para inteiro nao tem op: e CHAMA de forja_as_<S>_de_f64 do runtime (dev confere, release satura).
 *   Memoria (P11a): END_SLOT dst(BC_I64) = endereco do slot k (a = imediato k) | CARREGA dst = [a + desloc] | GRAVA [a + desloc] = b | COPIA_MEM [a] <- [b] (desloc = n bytes)
 *     | ZERA_MEM [a] (desloc = n bytes). A base a e sempre um vreg BC_I64; os bytes de padding do slot nao sao definidos.
 *   END_SIMBOLO dst(BC_I64) = endereco do simbolo a | CHAMA [dst] = simbolo(args) | CHAMA_IND [dst] = b(args), b = vreg I64 com o endereco da funcao (convencao Win64 da fronteira C)
 *   SALTA alvo[0] | DESVIA a(vreg BC_I8): verdadeiro alvo[0], falso alvo[1] | RETORNA [a] | ARMADILHA simbolo(args): CHAMA que nao volta.
 * SELECIONA (R5): dst = (args[0] aux args[1]) ? a : b; nargs = 2 (os operandos da comparacao, de mesma classe, aux = BirCond como no CMP), a e b da classe de dst (inteiro, ponteiro ou
 *   F32/F64). Pura: sem trap, sem leitura de memoria. Real so no padrao que o minss/maxss preserva bit a bit (BirSelecionaMinMax: a<b?a:b e a>b?a:b, com NaN ou +-0 vale o 2o operando).
 * Vetor (S2): SPLAT dst(V128) = a (vreg F32 com aux 4XF32, ou F64 com aux 2XF64) em todas as lanes; ADD SUB MUL DIV de V128 operam lane a lane (sem FMA, mesmo resultado do escalar SSE).
 * Operando BE_SINAL / BE_ZERO (so em args de CHAMA e ARMADILHA): o valor segue estendido a 64 bits para a funcao chamada.
 */
#ifndef FORJA_NATIVO_BIR_BIR_H
#define FORJA_NATIVO_BIR_BIR_H

#include "common/arena.h"
#include "common/buf.h"
#include "common/diag.h"
#include "common/fonte.h"
#include "common/intern.h"
#include "ir/ir.h"
#include "semantic/tipos.h"

#define BIR_VERSAO "0.28.0"
#define BIR_COPIA_MEM_MAX 64u /* acima disso o construtor chama memcpy/memset */
/* Nomes do forja_dll_info (M8 H1): a BirFuncao que o construtor emite e a struct que nativo_interface.c grava no .rdata. G3 G0b: vieram de nativo_interface.h
 * (quebra de ciclo bir->nativo; a versao do formato BIR nao muda). */
#define DLL_INFO_SIMBOLO_DADOS "forja_dll_info_dados"
#define DLL_INFO_NOME "forja_dll_info"

/* Nomes do runtime que o construtor chama para copiar e zerar bloco acima de BIR_COPIA_MEM_MAX. Windows: memcpy/memset; linux: forja_memcpy/forja_memset (D0654). */
typedef struct { const char *copia, *zera; } BirNomesMem;
#define BIR_SLOT_ALINHAMENTO_MAX 16u
#define BIR_SEM_VREG UINT32_MAX
#define BIR_SEM_POS UINT32_MAX
#define BIR_SEM_SIMBOLO UINT32_MAX
#define BIR_SEM_BLOCO UINT32_MAX
#define BIR_SEM_REG 0xFFu /* BirFuncao.reg_vreg: o vreg vive no slot */

/* As classes F* ficam depois das inteiras, mas real e so BirClasseEhReal: nunca compare por >= BC_F32. BC_V128 (S1b) e um vetor de 128 bits: banco xmm, nao e real escalar. */
typedef enum { BC_NADA, BC_I8, BC_I16, BC_I32, BC_I64, BC_F32, BC_F64, BC_V128 } BirClasse;

/* Forma do vetor, guardada em BirInstr.aux da instrucao que le ou grava um vreg BC_V128 (COPIA, CARREGA, GRAVA, SPLAT e ADD SUB MUL DIV de 4XF32 ou 2XF64). */
typedef enum { BVEC_4XF32, BVEC_2XF64, BVEC_4XI32, BVEC_2XI64, BVEC_QTD } BirVetorForma;

/* Lista explicita, nao faixa: um BC_V128 acrescentado depois de BC_F64 nao e real. */
static inline bool BirClasseEhReal(BirClasse c)
{
    return c == BC_F32 || c == BC_F64;
}

/* Vive num registrador xmm: real escalar ou vetor de 128 bits. */
static inline bool BirClasseEhXmm(BirClasse c)
{
    return c == BC_F32 || c == BC_F64 || c == BC_V128;
}

typedef enum { BO_NADA, BO_VREG, BO_IMED, BO_SIMBOLO } BirOperandoTipo;
typedef enum { BE_NENHUMA, BE_SINAL, BE_ZERO } BirExtensao;

/* Uma folha do agregado (um escalar no deslocamento dele): sse = 1 para real (SSE), 0 para inteiro ou ponteiro (INTEGER). */
typedef struct { uint8_t sse; uint32_t deslocamento, alinhamento; } BirFolhaSysv;
/* Descritor SysV de um agregado por valor com tam bytes; folhas[0..n) em ordem de memoria. Preenchido so no alvo linux, em argumento e retorno de foreign de fora. */
typedef struct { uint32_t tam, n; const BirFolhaSysv *folhas; } BirSysv;

/* id = numero do vreg (BO_VREG) ou indice em BirPrograma.simbolos (BO_SIMBOLO); imed vale em BO_IMED (ja estendido a 64 bits conforme a classe).
 * tam > 0: o vreg (classe BC_I64) e o endereco de um agregado de tam bytes. sysv: NULL, salvo argumento ou retorno agregado de foreign de fora no alvo linux. */
typedef struct { uint8_t tipo, classe, extensao; uint32_t id; int64_t imed; uint32_t tam; const BirSysv *sysv; } BirOperando;

typedef enum {
    BIR_CONST, BIR_COPIA, BIR_ADD, BIR_SUB, BIR_MUL, BIR_AND, BIR_OR, BIR_XOR, BIR_NEG, BIR_CMP, BIR_EXT_S, BIR_EXT_Z, BIR_TRUNC,
    BIR_ADD_OV, BIR_SUB_OV, BIR_MUL_OV, BIR_NEG_OV, BIR_END_SIMBOLO, BIR_CHAMA,
    BIR_SALTA, BIR_DESVIA, BIR_RETORNA, BIR_ARMADILHA, BIR_DIV, BIR_REM, BIR_SHL, BIR_SHR, BIR_NOT, BIR_I_PARA_F, BIR_F_PARA_F,
    BIR_RAIZ, BIR_CHAMA_IND, BIR_SELECIONA, BIR_SPLAT,
    BIR_END_SLOT, BIR_CARREGA, BIR_GRAVA, BIR_COPIA_MEM, BIR_ZERA_MEM, BIR_QTD
} BirOp;

/* BCOND_F*: comparacao de real (aux de CMP com operandos F32 ou F64); com NaN so FNE e verdadeiro. */
typedef enum {
    BCOND_EQ, BCOND_NE, BCOND_LT_S, BCOND_LE_S, BCOND_GT_S, BCOND_GE_S, BCOND_LT_U, BCOND_LE_U, BCOND_GT_U, BCOND_GE_U,
    BCOND_FEQ, BCOND_FNE, BCOND_FLT, BCOND_FLE, BCOND_FGT, BCOND_FGE
} BirCond;

typedef enum { BIR_SINAL, BIR_SEM_SINAL } BirSinal;

typedef struct {
    uint8_t op, aux, classe; /* classe = a do dst (ou do resultado, em CHAMA sem dst) */
    uint32_t dst;
    BirOperando a, b;
    BirOperando *args; uint32_t nargs;
    uint32_t simbolo;        /* CHAMA, ARMADILHA */
    uint32_t alvo[2];        /* blocos: SALTA, DESVIA e o frio dos *_OV */
    uint32_t pos;            /* offset na fonte (IrInstr.pos) ou BIR_SEM_POS */
    const IrInstr *ir;       /* instrucao de origem; NULL nas sinteticas */
    int32_t desloc;          /* CARREGA, GRAVA: deslocamento da base; COPIA_MEM, ZERA_MEM: numero de bytes */
} BirInstr;

/* SELECIONA real (dst = (x aux y) ? t : f, classe real): e o resultado de minss/maxss? aux FLT/FGT e (t, f) = (x, y) ou (y, x), cada um o mesmo operando (vreg de mesmo id ou imediato de mesmos bits).
 * Se sim, *maximo diz maxss (senao minss) e a instrucao e "op *primeiro, *segundo" (minss p, s = p < s ? p : s; maxss p, s = p > s ? p : s; NaN ou +-0 devolve s). */
bool BirSelecionaMinMax(const BirInstr *i, bool *maximo, const BirOperando **primeiro, const BirOperando **segundo);

/* Memoria do quadro: um slot por agregado ou por escalar com endereco tomado. alinhamento e potencia de 2 (ate BIR_SLOT_ALINHAMENTO_MAX). */
typedef struct { uint32_t tam, alinhamento; } BirSlot;

typedef struct { BirInstr *instrs; uint32_t n, cap; bool frio; } BirBloco;

/* M6 P65a (D-M6-13): onde mora um local nomeado do usuario no fim da construcao da funcao. slot >= 0 (agregado ou escalar com '&'): o dado esta no slot
 * (o vreg e so o endereco dele) e vence o vreg; senao vreg >= 0: o valor mora no home do vreg. indireto: parametro agregado, o home guarda um PONTEIRO
 * para o dado (param_tam[k] > 0). nome e tipo_nome sao copias na arena (tipo_nome = classe de BirNomeDoTipo). */
typedef struct { uint32_t local_id; TipoId tipo; const char *nome, *tipo_nome; int32_t vreg, slot; bool indireto; } BirLocalCasa;

typedef struct {
    uint32_t simbolo;        /* indice em BirPrograma.simbolos */
    uint8_t retorno;         /* BirClasse; BC_NADA = vazio */
    uint32_t nparams, nvregs;
    uint8_t *classe_vreg;    /* nvregs entradas */
    BirBloco *blocos; uint32_t nblocos, cap_blocos; /* blocos[0] = entrada */
    uint32_t pos;            /* offset da 1a instrucao do corpo (IrFuncao nao tem pos) ou BIR_SEM_POS */
    bool sintetica;          /* forja_programa */
    BirSlot *slots; uint32_t nslots, cap_slots; /* memoria explicita do quadro (END_SLOT a = indice) */
    uint32_t *param_tam;     /* nparams entradas: bytes do agregado do parametro k (0 = escalar); NULL = nenhum agregado */
    uint32_t retorno_tam;    /* > 0: o retorno e um agregado desse tamanho (retorno = BC_I64: o endereco) */
    BirLocalCasa *casas; uint32_t ncasas; /* M6 P65a: locais do usuario com casa, em ordem de local_id (na arena); NULL/0 em funcao sem tabela de nomes */
    uint32_t nsem_casa;      /* M6 P65a: locais nomeados sem vreg nem slot (nunca definidos); a depuracao exige 0 */
    uint8_t *reg_vreg;       /* M7a F3: nvregs entradas, numero de hardware do registrador x64 do vreg ou BIR_SEM_REG; NULL = tudo em slot (dev e passe desligado); malloc, BirLiberar libera */
    uint32_t regs_salvos;    /* M7a F3: mascara (bit = numero do registrador) dos callee-saved inteiros que reg_vreg usa */
    uint32_t regs_salvos_xmm; /* M7a F4: mascara (bit = numero do xmm) dos xmm6..15 que reg_vreg usa; reg_vreg de vreg real = numero do xmm */
    bool isel_otim;          /* M7a F5: passe `isel` ligado nesta funcao (so release): o x64 coalesce, funde cmp+desvia e monta enderecos com indice */
    bool div_const;          /* M7b F1: passe `divconst` ligado nesta funcao (so release): o isel pos-alocador troca DIV/REM por imediato por mul magico e deslocamentos */
} BirFuncao;

typedef enum { BS_FUNCAO, BS_RUNTIME, BS_TEXTO, BS_DADO, BS_IMPORT } BirSimboloTipo;

/* BS_FUNCAO e BS_RUNTIME: nome. BS_TEXTO: bytes com NUL no fim (tam = bytes sem o NUL), ordem = numero na ordem do 1o uso (s0, s1...).
 * BS_DADO: tam bytes de .rdata no alinhamento dado (potencia de 2), ordem = numero na ordem do 1o uso (d0, d1...); deduplicado por conteudo.
 * BS_IMPORT: nome = "__imp_<simbolo C do foreign>" (o slot da IAT); pos = offset da 1a IrInstr que o chama (0 nos demais tipos). A CHAMA a um
 * BS_IMPORT e uma chamada INDIRETA pelo slot, nunca um call rel32. */
typedef struct { const char *nome; uint8_t tipo; uint32_t tam; const char *bytes; uint32_t ordem; uint32_t alinhamento; uint32_t pos; } BirSimbolo;

typedef enum { BM_DEV, BM_RELEASE } BirModo;

/* Entrada SysV de um callback (alvo linux, assinatura so de escalares): o stub fjt_<proc> (x64_entrada_sysv.c, nao e BirFuncao) guarda os registradores SysV num
 * bloco e chama o wrapper fjtw_<proc> (uma BirFuncao Win64 de 2 parametros: bloco*, pilha*). Os dois sao indices de BirPrograma.simbolos.
 * win64 != BIR_SEM_SIMBOLO: o simbolo da ponte Win64 fjtm_<proc> (BirFuncao), para onde o stub leva uma entrada Win64 16 bytes antes de si (comparador do runtime). */
typedef struct { uint32_t stub, wrapper, win64; } BirEntradaSysv;

typedef struct {
    BirFuncao *funcoes; uint32_t nfuncoes;
    BirSimbolo *simbolos; uint32_t nsimbolos, cap_simbolos;
    uint8_t modo;            /* BirModo */
    const Fonte *fonte;      /* so para BirImprimir (linha:coluna) */
    const char **exportados; uint32_t nexportados;  /* DLL: nome (global) de cada involucro de export, mais "forja_liberar"; NULL/0 no exe */
    uint32_t procs_total, procs_gerados;            /* P24p: procs do programa (sem os 'test' fora do modo teste) e os que entraram na BIR; iguais sem BirEntrada.poda */
    bool sysv;               /* M5b P37: alvo linux; o foreign de fora usa a ABI SysV e BirOperando.sysv vem preenchido */
    bool guarda;             /* M6e P93k: --guarda-memoria; o append/resize/reserve chamam forja_lista_crescer_alvo (todo append de lista de heap move) */
    bool checagem;           /* O0 P13: BirEntrada.checagem; o predicado e BirConfereLimites (bir_interno.h) */
    bool simbolos_dbg;       /* M7b F3: --simbolos (BirEntrada.simbolos_dbg): o passe `sroa` nao escalariza slot de BirLocalCasa */
    bool perfil;             /* M8 P2: --perfil (BirEntrada.perfil): OtimExecutar roda o passe `perfil` (dev e release) */
    bool endereco_ligado;    /* M7b R6a-3: OtimExecutar diz se o passe `endereco` roda; so entao o `licm` deixa o add x, imm de endereco no corpo (--sem-otim endereco = byte a byte o de antes) */
    BirEntradaSysv *entradas_sysv; uint32_t nentradas_sysv; /* M5b P38b: um stub SysV por trampolim escalar (so com sysv); vive na arena */
    BirNomesMem nomes_mem;   /* P39-B1: os nomes de copia e zera do alvo (BirEntrada.nomes_mem, ou o padrao do Windows) */
    Arena arena;
} BirPrograma;

/* Entrada da construcao: o que o front-end entrega depois do lowering. modo BM_DEV liga as checagens de estouro e de conversao. */
typedef struct {
    const IrPrograma *ir;
    const Tipos *tipos;
    const Interner *nomes;
    const Fonte *fonte;
    uint8_t modo;
    bool teste;              /* forja test: o programa e o executor dos 'test' (modo BM_DEV) */
    bool dll;                /* build --modo dll: sem forja_programa, main opcional e os 'pub export proc' ganham o involucro de nome limpo (M3 P22a) */
    bool traco;              /* --traco (P18a): depois de cada gravacao de local ou temporario escalar o programa CHAMA forja_traco; sem ele a BIR nao muda */
    bool poda;               /* P24p (B07): so os procs alcancaveis das raizes (main, exports da dll, 'test') e os trampolins que eles usam entram na BIR; a IR continua inteira */
    bool sysv;               /* M5b P37: o alvo e x86_64-linux; o foreign de fora usa a ABI SysV (BirPrograma.sysv) */
    bool guarda;             /* M6e P93k: --guarda-memoria (so dev, x64, exe): BirPrograma.guarda */
    bool checagem;           /* O0 P13: --checagem sim no release: indice e nulo conferidos (BirConfereLimites) */
    bool simbolos_dbg;       /* M7b F3: --simbolos sim (NativoPedido.simbolos): vira BirPrograma.simbolos_dbg */
    bool perfil;             /* M8 P2: --perfil (so exe): vira BirPrograma.perfil */
    BirNomesMem nomes_mem;   /* P39-B1: copia NULL = nomes do Windows (DLL, --suporte e testes, que nao escolhem alvo) */
} BirEntrada;

/* IR -> BIR. false = X0001 em *erro (off = IrInstr.pos) ou X0904 (IR inconsistente). Em falha o BirPrograma fica parcial: BirLiberar sempre. */
bool BirConstruir(const BirEntrada *entrada, BirPrograma *saida, Diag *erro);

/* Confere a forma da BIR (blocos terminados, alvos, vregs, classes, simbolos). false = X0904 com a causa. */
bool BirValidar(const BirPrograma *programa, Diag *erro);

/* Dump deterministico, uma linha por instrucao (formato no cabecalho de bir_imprimir.c). */
void BirImprimir(const BirPrograma *programa, Buf *saida);

/* M6 P65a: dump dos locais nomeados (item "locais" do --debug-code): por funcao "funcao <simbolo>", uma linha "  local ..." por casa e "  sem_casa=N". */
void BirImprimirLocais(const BirPrograma *programa, Buf *saida);

void BirLiberar(BirPrograma *programa);

/* ---- REGRAS_IR[]: o que cada IrOp tem no backend nativo (dados; o futuro --suporte le a tabela inteira) ---- */
typedef enum { BR_SUPORTADO, BR_FUTURO } BirEstado;

typedef struct BirCtx BirCtx;
typedef bool (*BirRegraFn)(BirCtx *ctx, const IrInstr *instr);

/* variante "-" = a op inteira; senao o nome do Operador ou "local"/"campo"/"proc"/"foreign". marco = "M1", "M2", "M3". fn = NULL em BR_FUTURO. */
typedef struct { uint8_t op; const char *variante; uint8_t estado; const char *marco; BirRegraFn fn; } BirRegra;

extern const BirRegra REGRAS_IR[];
extern const uint32_t REGRAS_IR_QTD;

#endif
