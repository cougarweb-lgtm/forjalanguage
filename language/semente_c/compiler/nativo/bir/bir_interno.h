/* bir_interno.h - estado da construcao da BIR e primitivas compartilhadas por bir_base.c, bir_construir*.c (nao e API publica).
 * Versao: 0.32.0
 * Changelog: 0.32.0 - Linguagem v1.13 L1a-ir: BirArquivoDoModulo vira BirArquivoDoTrecho (uint32_t arquivo).
 *            0.31.1 - G3 G0c: comentario de BirImediatoReal (f32 por conversao direta).
 *            0.31.0 - M1 split: ChaveEhPorValor e ChamarMapaPorValor (bir_construir_soa_mapa_valor.c 0.1.0) e os tipos MapaRef, ServicoMapa e PedidoMapa (de bir_construir_soa_mapa.c) ficam publicos aqui.
 *            0.30.1 - P2: BirIndiceNoLimite (release: indice imediato em [0, limite) dispensa vreg e conferencia; dev segue igual).
 *            0.30.0 - O0 P13: BirConfereLimites (indice e nulo: dev ou BirPrograma.checagem).
 *            0.29.0 - M6c P88: BirRegraHandleNasce e BirRegraHandleSolta (bir_construir_handle.c).
 *            0.28.0 - M6 P65a: BirRetratarLocais (bir_locais.c).
 *            0.27.0 - M5b P38b: BirCtx.comparador_runtime e BirComparadorDoRuntime (bir_construir_fronteira.c).
 *            0.26.0 - M5b P37: BirDescritorSysv (bir_construir_sysv.c).
 *            0.25.0 - K17: BirRegraHeap (IR_HEAP, heap_push / heap_pop) e LISTA_MEMBRO_PTR (0, o ptr da lista).
 *            0.24.0 - K08a: BirRegraOrdenaGeral (IR_ORDENA_GERAL, sort_by).
 *            0.23.0 - M4 P24p: BirCtx.alcance (bir_alcance.h; NULL = todos os procs).
 *            0.22.0 - M3 P22a: BirConstruirExports (bir_construir_fronteira.c).
 *            0.21.0 - M3 P21v-c: BirRegraChamadaInd (IR_CHAMADA_IND).
 *            0.20.0 - M3 P21: BirOperandoProc e BirConstruirTrampolins (bir_construir_fronteira.c); BirEstenderEstreito, BirArquivoDoModulo, BirOrigemNulo e BirFalhaSeNulo (bir_construir_chamada.c) publicos.
 *            0.19.0 - M3 P19: BirSimboloImport.
 *            0.18.0 - Linguagem v1.11 L13: BirRaizReal (bir_construir_real.c).
 *            0.17.0 - P18a: BirTracar (bir_construir_traco.c) e BirCtx.funcao_ir, num e slot_traco.
 *            0.16.0 - M2 P17b: regras de bir_construir_soa_mapa.c (IR_SOA_*, IR_MAPA_*, IR_LIBERA op_aux 4 e 5: BirLiberaMapa e BirLiberaSoa) e BirTamanhoDeColecao (len de map e de soa dinamica).
 *            0.15.0 - M2 P16: BirClasseDoConjunto, BirRegraConjunto, BirRegraMatriz, BirRegraMatrizElemento e BirRegraMatrizEndElemento (bir_construir_v19.c), BirConferirIndice (publica, de bir_construir_array.c)
 *                    e BirNegarRealEm (de bir_construir_real.c).
 *            0.14.0 - M2 P15b: BirAlocadorDe (de bir_construir_aloca.c) e BirRegraFormata (bir_construir_formata.c, IR_FORMATA).
 *            0.13.0 - M2 P15a: regras de bir_construir_opcional.c (IR_EMBRULHA, IR_OPC_TEM, IR_OPC_VALOR, IR_TUPLA, IR_TUPLA_ELEMENTO), BirEhPonteiroOpcional, BirNuloAgregado, BirSlotDoDestino e
 *                    BirTempDoCampo (de bir_construir_resultado.c); BirEhAgregado cobre ?T (nao nicho), union e tupla; BirClasseDoTipo cobre ?cstr (BC_I64).
 *            0.12.0 - M2 P14b: BirListaRef e BirListaDe, BirTamanhoDeLista, BirInteiro64 (de bir_construir_array.c), BirChamarRuntime, BirExternoDe e as regras de bir_construir_aloca.c (IR_ALOCA, IR_LISTA_CRIA, IR_ANEXA,
 *                    IR_LIBERA, IR_FRAME_RESET); BirEhAgregado cobre [dynamic]T.
 *            0.11.0 - M2 P13b: regras de bir_construir_array.c (IR_ELEMENTO, IR_ENDERECO_ELEMENTO, IR_FATIA, IR_FATIA_DE_PONTEIRO, IR_ARRAY); BirEhAgregado cobre [N]T e []T.
 *            0.10.0 - M2 P13a: str como agregado de 16 B (BirEhAgregado cobre TC_TEXTO); BirArgumentoAgregado publica; regras de bir_construir_texto.c (literal de str e de cstr, print, ==, !=,
 *                     cstr as str, len); BirClasseDoTipo cobre cstr (BC_I64).
 *            0.9.0 - M2 P12b: BirProgramaTeste (bir_construir_teste.c).
 *            0.8.0 - M2 P12a: BirEhAgregado (struct e resultado) no lugar de BirEhStruct nos pontos de agregado por valor; BirCtx.main_tipo; BirInteiroDoValor (enum com valor); regras de
 *                    bir_construir_resultado.c (RES_*, main -> ! E), bir_construir_chaveia.c (CHAVEIA de enum e de resultado) e bir_construir_afirma.c (ASSERT, PANIC, EXPECT).
 *            0.7.0 - M2 P11b: BirTipoPassavel, BirPrepararAssinatura (bir_construir_chamada.c, que passa a abrigar chamada, retorno e assinatura) e BirAgregadosValidos.
 *            0.6.0 - M2 P11a: memoria (BirCtx.lay, slot_local/slot_temp, tipo_local, em_memoria; BirNovoSlot; helpers e regras de bir_construir_mem.c e bir_construir_ptr.c;
 *                    BirMemoriaValida de bir_validar_mem.c); BirVregTemp publica; BirClasseDoTipo cobre ^T, ?^T e rawptr (BC_I64).
 *            0.5.0 - M2 P10b: BirClasseDoTipo recebe os Tipos (f32, f64 e unidade); BirSimboloDado e BirCtx.n_dado; bir_construir_real.c (BirImediatoReal, BirContaReal,
 *                    BirNegarReal, BirConversaoReal).
 *            0.4.0 - M2 P9: BirRegraDivisao (bir_construir_divisao.c) e os auxiliares das contas que ela reaproveita (BirSufixoInteiro, BirEstendido64,
 *                    BirVregDoArquivo, BirComparar, BirFrioEstouro); BirClasseDoTipo cobre todos os inteiros.
 *            0.3.0 - M2 P9a: BirFalhaInconsistente recebe a causa ja formatada (sem varargs; regra 5.4 a).
 *            0.2.0 - backend x64 P8b: BirCtx.varredura, BirOcorrencia, BirVarrerSuporte e BirNomeDoTipo (a varredura do --suporte e o campo ir dos erros).
 *            0.1.0 - backend x64 P6: BirCtx, rotulos de bloco (criados no 1o salto), pilha de lacos, primitivas de bloco, vreg, simbolo e erro.
 * Verificacao: construir.bat (compila com -Werror) e teste_nativo.exe bir <arq.fj>
 *
 * A posicao (BirInstr.pos) de toda instrucao emitida e a da IrInstr em construcao (BirCtx.atual); sem atual (forja_programa, fecho
 * da funcao) e BIR_SEM_POS.
 */
#ifndef FORJA_NATIVO_BIR_INTERNO_H
#define FORJA_NATIVO_BIR_INTERNO_H

#include "common/vec.h"
#include "ir/ir_numerar.h"
#include "ir/layout.h"
#include "nativo/bir/bir.h"
#include "nativo/bir/bir_alcance.h"

#define BIR_SIMBOLO_EXTERNO (UINT32_MAX - 1u) /* marca de "este nome e um proc de foreign" em BirCtx.simbolo_do_nome */

/* Rotulo: o bloco so existe depois do 1o salto para ele (alvo inalcancavel nao vira bloco vazio sem terminador). */
typedef struct { uint32_t bloco; } BirRotulo;

/* Laco aberto: onde 'continue' e 'break' saltam. Os rotulos vivem no quadro da funcao que construiu o laco. */
typedef struct { BirRotulo *continua, *pare; } BirLaco;

struct BirCtx {
    const BirEntrada *e;
    BirPrograma *p;
    Diag *d;
    const BirAlcance *alcance; /* P24p: procs e trampolins que entram na BIR; NULL = todos */
    const IrInstr *atual;    /* instrucao da IR em construcao (vira BirInstr.ir; posicao das instrucoes e dos erros) */
    BirFuncao *f;             /* funcao em construcao */
    uint32_t cur;             /* bloco corrente; BIR_SEM_BLOCO = depois de um terminador (codigo morto abre bloco novo sob demanda) */
    uint32_t cap_vregs;
    int32_t *vreg_local, *vreg_temp; /* numero da IR -> vreg; -1 = ainda nao definido */
    uint32_t cap_local, cap_temp; /* capacidade dos dois mapas */
    uint32_t n_local, n_temp;     /* tamanho na funcao corrente (nlocais, ntemps) */
    BirLaco *lacos; uint32_t nlacos, cap_lacos;
    uint32_t *simbolo_do_nome; /* nome internado do proc -> indice do simbolo (BIR_SIMBOLO_EXTERNO: foreign) */
    uint32_t n_nomes;
    uint32_t *hash; uint32_t cap_hash, n_hash; /* indice (+1) dos simbolos por (tipo, bytes) */
    uint32_t n_texto, n_dado;
    bool comparador_runtime;  /* P38b (alvo linux): algum sort_by/heap_*_by passou um comparador ao runtime; os stubs SysV ganham a entrada Win64 (fjtm_) */
    bool tem_main;
    uint32_t main_simbolo;    /* fj_main: alvo do CHAMA de forja_programa */
    uint8_t main_retorno;     /* classe do retorno de fj_main (o resultado e descartado) */
    TipoId main_tipo;         /* tipo do retorno de fj_main: T ! E pede o R0004 em forja_programa */
    Vec *varredura;           /* NULL = para no 1o erro; senao (--suporte) cada X0001 vira uma BirOcorrencia aqui e a construcao segue */
    Layouts lay;              /* tamanho e offsets dos tipos (layout/): calculado uma vez por programa, na 1a vez que a memoria precisa */
    bool lay_pronto;
    int32_t *slot_local, *slot_temp; /* numero da IR -> indice do slot (agregado ou escalar em memoria); -1 = sem slot */
    TipoId *tipo_local;       /* tipo declarado de cada local da funcao (0 = ainda nao declarado) */
    uint8_t *em_memoria;      /* local escalar com '&' tomado: vive num slot e todo acesso e CARREGA/GRAVA */
    const IrFuncao *funcao_ir; /* P18a (so com BirEntrada.traco): a funcao da IR em construcao, para o nome no forja_traco */
    IrNumeracao num;          /* P18a: numeracao da funcao em construcao (IrNumerarFuncao) */
    int32_t slot_traco;       /* P18a: slot de 8 bytes que passa os bits de um real ao forja_traco; -1 = ainda nao criado nesta funcao */
};

/* Uma construcao recusada (X0001) da varredura do --suporte: onde (IrInstr.pos) e a causa do diagnostico. */
typedef struct { uint32_t pos; char causa[512]; } BirOcorrencia;

/* Constroi a BIR so para listar TODOS os X0001 do programa em ocorrencias (Vec de BirOcorrencia, na ordem do programa). Nao devolve BIR.
 * Limite: o corpo de uma construcao recusada (ex.: um 'if' com condicao f32) nao e varrido. */
void BirVarrerSuporte(const BirEntrada *entrada, Vec *ocorrencias);

/* Pedido de instrucao de valor: dst = a op b (aux: BirCond ou BirSinal). A classe da instrucao e a do dst. */
typedef struct { uint8_t op, aux; uint32_t dst; BirOperando a, b; } BirPedido;

/* ---- bir_base.c: blocos, vregs, simbolos, operandos ---- */
/* O0 P13: indice/fatia (R0006) e nulo (R0005) conferidos? Dev sempre; release so com --checagem sim. As outras checagens de BM_DEV (estouro, faixa, divisao, handle, alocacao, epoca) seguem so em dev. */
#define BirConfereLimites(c) ((c)->p->modo == BM_DEV || (c)->p->checagem)

uint32_t BirNovoBloco(BirCtx *c, bool frio);
uint32_t BirNovoVreg(BirCtx *c, uint8_t classe);
BirInstr *BirEmitir(BirCtx *c, BirOp op, uint8_t classe);
BirInstr *BirEmitirPedido(BirCtx *c, BirPedido pedido);
uint32_t BirContaNova(BirCtx *c, uint8_t classe, BirPedido pedido); /* vreg novo como dst */
void BirMover(BirCtx *c, uint32_t dst, BirOperando origem); /* CONST se imediato, senao COPIA */
void BirSalta(BirCtx *c, uint32_t alvo);
void BirDesvia(BirCtx *c, BirOperando cond, uint32_t verdade, uint32_t falso);
void BirRetorna(BirCtx *c, const BirOperando *valor);
void BirArmadilha(BirCtx *c, uint32_t simbolo, const BirOperando *args, uint32_t nargs);
uint32_t BirRotuloAlvo(BirCtx *c, BirRotulo *r);   /* bloco do rotulo, criado na 1a referencia */
void BirRotuloSalta(BirCtx *c, BirRotulo *r);      /* fim de ramo: salta ao rotulo se o bloco corrente esta vivo */
void BirRotuloColoca(BirCtx *c, BirRotulo *r);     /* continua a construcao no rotulo (morto se ninguem saltou a ele) */
void BirEntrarLaco(BirCtx *c, BirRotulo *continua, BirRotulo *pare);
void BirSairLaco(BirCtx *c);

uint32_t BirSimboloFuncao(BirCtx *c, const char *nome_sem_prefixo);
uint32_t BirSimboloGlobal(BirCtx *c, const char *nome); /* funcao do programa com nome exato (forja_programa), sem copia */
uint32_t BirSimboloRuntime(BirCtx *c, const char *nome);
uint32_t BirSimboloImport(BirCtx *c, const char *simbolo, uint32_t pos); /* BS_IMPORT "__imp_<simbolo>" de foreign de DLL; pos = 1o uso */
uint32_t BirSimboloTexto(BirCtx *c, const char *bytes, uint32_t tam);
/* Dado de .rdata (tam bytes, copiados) no alinhamento dado; o mesmo conteudo devolve o mesmo simbolo (com o maior alinhamento pedido). */
uint32_t BirSimboloDado(BirCtx *c, const char *bytes, uint32_t tam, uint32_t alinhamento);
BirOperando *BirArgs(BirCtx *c, uint32_t n);   /* vetor de operandos na arena */

static inline BirOperando OpVreg(uint32_t id, uint8_t classe)
{
    return (BirOperando){.tipo = BO_VREG, .classe = classe, .id = id};
}

static inline BirOperando OpImed(int64_t valor, uint8_t classe)
{
    return (BirOperando){.tipo = BO_IMED, .classe = classe, .imed = valor};
}

/* P2: so no release (dev confere sempre). Indice imediato em [0, limite) nunca cai fora: dispensa o vreg e a conferencia. */
static inline bool BirIndiceNoLimite(BirCtx *c, BirOperando indice, int64_t limite)
{
    return c->p->modo != BM_DEV && indice.tipo == BO_IMED && indice.imed >= 0 && indice.imed < limite;
}

static inline BirOperando OpSimbolo(uint32_t simbolo)
{
    return (BirOperando){.tipo = BO_SIMBOLO, .classe = BC_I64, .id = simbolo};
}

static inline BirOperando OpComExtensao(BirOperando o, uint8_t extensao)
{
    o.extensao = extensao;
    return o;
}

/* ---- bir_regras.c ---- */
const char *BirNomeDoOp(uint8_t op);
const char *BirVarianteDe(const BirCtx *c, const IrInstr *i);
const BirRegra *BirAcharRegra(uint8_t op, const char *variante);

/* ---- erros e valores da IR (bir_construir.c) ---- */
/* Nome Forja do tipo ("i32", "f32", ...) ou, se nao ha nome fixo, o da classe ("struct", "lista", ...). */
const char *BirNomeDoTipo(const Tipos *tipos, TipoId tipo);
/* partes = {op, variante, tipo}: "-" e NULL somem do texto. Devolve false (X0001). */
bool BirFalhaX0001(BirCtx *c, uint32_t pos, const char *const partes[3], const char *marco);
bool BirFalhaTipo(BirCtx *c, const IrInstr *i, const char *variante, TipoId tipo); /* X0001 pelo tipo que a BIR ainda nao cobre */
bool BirFalhaInconsistente(BirCtx *c, const char *causa); /* X0904 na posicao atual; causa ja formatada por quem chama */

bool BirClasseDoTipo(const Tipos *tipos, TipoId tipo, uint8_t *classe); /* bool, inteiros, f32, f64 e unidade (f32); false nos demais */
bool BirOperandoDe(BirCtx *c, const IrInstr *i, const Valor *v, BirOperando *saida);
bool BirOperandoVreg(BirCtx *c, BirOperando o, BirOperando *saida); /* imediato vira CONST num vreg */
bool BirDefinirDestino(BirCtx *c, uint8_t classe, uint32_t *vreg);  /* vreg do destino (temporario) da instrucao atual */
bool BirVregLocal(BirCtx *c, int32_t local, uint32_t *vreg);
bool BirVregDeclarar(BirCtx *c, int32_t local, uint8_t classe, uint32_t *vreg); /* declara (ou reaproveita) o vreg do local */
bool BirConstruirBloco(BirCtx *c, const IrBloco *bloco);

/* Texto de runtime (nome da operacao, arquivo): END_SIMBOLO num vreg novo no bloco corrente. */
uint32_t BirVregDoTexto(BirCtx *c, const char *bytes, uint32_t tam);
/* Arquivo e linha (locais ao trecho do .fj) do offset: o par FORJA_ARQ/linha. */
void BirArquivoELinha(const BirCtx *c, uint32_t pos, const char **caminho, uint32_t *linha);

/* ---- auxiliares das contas (bir_construir_conta.c) ---- */
const char *BirSufixoInteiro(TipoId tipo);                 /* "i8" "i16" "i32" "i64" (int) "u8" "u16" "u32" "u64" (uint) */
uint32_t BirComparar(BirCtx *c, uint8_t cond, BirOperando a, BirOperando b);  /* vreg bool novo = a cond b */
BirOperando BirEstendido64(BirOperando o, TipoId tipo);    /* argumento de 64 bits da falha, estendido pelo sinal do TIPO */
BirOperando BirVregDoArquivo(BirCtx *c, uint32_t pos, uint32_t *linha); /* vreg com o texto do arquivo; linha local ao trecho do .fj */
/* forja_falha_estouro[_u]("<operacao> <sufixo>", a, b, arquivo, linha) no bloco frio dado (o tipo e o de c->atual->a.tipo). */
void BirFrioEstouro(BirCtx *c, uint32_t frio, const char *operacao, const BirOperando ab[2]);

/* ---- memoria (bir_base.c, bir_construir_mem.c, bir_construir_ptr.c) ---- */
uint32_t BirNovoSlot(BirCtx *c, uint32_t tam, uint32_t alinhamento);  /* indice do slot na funcao corrente */
bool BirVregTemp(BirCtx *c, int32_t id, uint32_t *vreg);              /* vreg do temporario (ou X0904 se ainda nao definido) */
bool BirEhStruct(const BirCtx *c, TipoId tipo);
bool BirEhAgregado(const Tipos *tipos, TipoId tipo);                  /* struct ou resultado (T ! E): vive num slot e viaja como endereco */
const Layouts *BirLayouts(BirCtx *c);                                 /* calcula na 1a chamada */
void BirMemPrepararFuncao(BirCtx *c, const IrFuncao *irf);            /* mapas de slot, tipos de local e o pre-passo dos '&' */
void BirMemLiberar(BirCtx *c);
void BirRetratarLocais(BirCtx *c, const IrFuncao *irf);                /* M6 P65a: casa de cada local nomeado, tirada no fim da funcao (bir_locais.c) */
void BirRegistrarTipoLocal(BirCtx *c, int32_t local, TipoId tipo);     /* tipo declarado do local (parametros entram em BirMemPrepararFuncao) */
bool BirLerLocalEmMemoria(BirCtx *c, const Valor *v, BirOperando *saida); /* CARREGA do slot do local; false com X0904 se nao declarado */
bool BirEnderecoDe(BirCtx *c, const IrInstr *i, const Valor *v, uint32_t *endereco); /* vreg com o endereco de uma struct (local ou temp) */
bool BirForma(BirCtx *c, TipoId tipo, BirSlot *forma);                /* tamanho e alinhamento do layout/ (X0904 se o tipo nao tem forma em C) */
const BirSysv *BirDescritorSysv(BirCtx *c, TipoId tipo);              /* M5b P37: folhas SysV do agregado (bir_construir_sysv.c); NULL = forma sem descritor (o chamador da X0001) */

typedef struct { bool temporario; int32_t id; BirSlot forma; } BirPedidoSlot;  /* agregado ou escalar em memoria: local (temporario = false) ou temporario da IR */
typedef struct { uint32_t offset; TipoId tipo; } BirCampoInfo;                  /* offset e tipo de um campo (layout/) */
typedef struct { uint32_t endereco; TipoId tipo_struct; } BirBaseCampo;          /* vreg com o endereco da struct e o tipo dela */
typedef struct { uint32_t base, offset; TipoId tipo; } BirAlvoMem;               /* onde gravar: [base + offset], com o tipo do que mora ali */

bool BirDefinirSlot(BirCtx *c, const BirPedidoSlot *pedido, uint32_t *vreg);  /* slot na 1a definicao e END_SLOT em toda definicao */
bool BirCampoDe(BirCtx *c, TipoId tipo_struct, int32_t nome, BirCampoInfo *info);
bool BirBaseDoCampo(BirCtx *c, const IrInstr *i, const Valor *base, BirBaseCampo *saida); /* struct por valor ou ^Struct */
bool BirGravarEm(BirCtx *c, const IrInstr *i, const BirAlvoMem *alvo, const Valor *valor); /* GRAVA (escalar) ou COPIA_MEM (struct) */
uint32_t BirEmitirEndSlot(BirCtx *c, uint32_t slot, uint32_t dst);
void BirEmitirCarrega(BirCtx *c, uint32_t dst, uint32_t base, int32_t desloc);
void BirEmitirGrava(BirCtx *c, uint32_t base, int32_t desloc, BirOperando valor);
void BirZeraBloco(BirCtx *c, uint32_t destino, uint32_t tam);        /* ZERA_MEM, ou memset do runtime acima de 64 bytes */
void BirZeraDinamico(BirCtx *c, uint32_t destino, BirOperando bytes);                  /* memset do runtime com o tamanho em um operando (IA1-L5b) */
void BirCopiaDinamica(BirCtx *c, uint32_t destino, uint32_t origem, BirOperando bytes); /* memcpy do runtime com o tamanho em um operando (IA1-L5b) */
uint32_t BirEnderecoMais(BirCtx *c, uint32_t base, uint32_t off);    /* vreg com base + off (a propria base se off = 0) */
void BirCopiaBloco(BirCtx *c, uint32_t destino, uint32_t origem, uint32_t tam);      /* COPIA_MEM, ou memcpy do runtime acima de 64 bytes */
bool BirRegraLocalMem(BirCtx *c, const IrInstr *i);       /* IR_LOCAL de struct ou de escalar com '&' (BirLocalEmMemoria diz se vale) */
bool BirLocalEmMemoria(const BirCtx *c, const IrInstr *i);
bool BirRegraAtribuicaoMem(BirCtx *c, const IrInstr *i);  /* campo, struct inteira ou escalar em memoria */
bool BirAtribuicaoEmMemoria(const BirCtx *c, const IrInstr *i);
bool BirRegraCampo(BirCtx *c, const IrInstr *i);
bool BirRegraStruct(BirCtx *c, const IrInstr *i);
bool BirRegraEndereco(BirCtx *c, const IrInstr *i);
bool BirRegraDeref(BirCtx *c, const IrInstr *i);
bool BirRegraGravaPonteiro(BirCtx *c, const IrInstr *i);
bool BirRegraEnderecoCampo(BirCtx *c, const IrInstr *i);
bool BirRegraTransmuta(BirCtx *c, const IrInstr *i);
bool BirRegraVerificaFaixa(BirCtx *c, const IrInstr *i);
bool BirConversaoPonteiro(BirCtx *c, const IrInstr *i);   /* 'as' entre ^T, ?^T e rawptr */
bool BirEhConversaoPonteiro(const BirCtx *c, const IrInstr *i);

/* ---- bir_validar_mem.c: forma das 5 operacoes de memoria e dos slots da funcao ---- */
bool BirMemoriaValida(const BirFuncao *f, const BirInstr *i);
bool BirSlotsValidos(const BirFuncao *f);
bool BirAgregadosValidos(const BirFuncao *f);                /* param_tam e retorno_tam na classe BC_I64 */

/* ---- agregado por valor (bir_construir_chamada.c) ---- */
bool BirTipoPassavel(const Tipos *tipos, TipoId tipo);       /* classe escalar da BIR ou agregado (struct, resultado, str): o que cruza a fronteira de um proc */
bool BirArgumentoAgregado(BirCtx *c, const IrInstr *i, const Valor *v, BirOperando *saida); /* copia privada do agregado + endereco com tam */
bool BirPrepararAssinatura(BirCtx *c, const IrFuncao *irf);  /* retorno, parametros (vregs 0..n-1), param_tam e retorno_tam da funcao */
BirOperando BirEstenderEstreito(BirOperando op, TipoId tipo); /* bool/i8/i16/u8/u16 seguem estendidos (sinal em i8/i16, zero nos demais); os demais iguais */
const char *BirArquivoDoTrecho(const BirCtx *c, uint32_t arquivo);  /* caminho do arquivo (trecho da Fonte; 0 = raiz) */
/* Texto do R0005 de um ponteiro nulo que cruza a fronteira C: o que era (oque), o arquivo do modulo e a linha do 'proc'. */
typedef struct { const char *oque, *arquivo; uint32_t linha; } BirOrigemNulo;
void BirFalhaSeNulo(BirCtx *c, uint32_t valor, const BirOrigemNulo *origem); /* valor (vreg I64) == 0 -> forja_falha_nulo no bloco frio; segue em bloco novo */

/* ---- fronteira C -> Forja (bir_construir_fronteira.c, P21) ---- */
bool BirOperandoProc(BirCtx *c, const Valor *v, BirOperando *saida);  /* V_PROC: END_SIMBOLO de fjt_<proc> num vreg I64 */
/* Comparador (V_PROC ou proc em variavel) que o RUNTIME chama em Win64 (sort_by, heap_*_by), num vreg I64. No alvo linux o valor e o fjt_ SysV: a entrada Win64 do stub fica
 * ABI_SYSV_ENTRADA_WIN64_ANTES bytes antes dele, e o comparador e fjt_ menos isso. */
bool BirComparadorDoRuntime(BirCtx *c, const IrInstr *i, const Valor *v, BirOperando *saida);
bool BirConstruirTrampolins(BirCtx *c);                                /* uma BirFuncao fjt_<proc> por IrTrampolim */
bool BirConstruirExports(BirCtx *c);                                   /* DLL (P22a): uma BirFuncao de nome limpo por 'pub export proc' (BirPrograma.exportados) e o forja_liberar */

/* ---- resultado, chaveia e afirmacoes (bir_construir_resultado.c, bir_construir_chaveia.c, bir_construir_afirma.c) ---- */
typedef enum { RES_PARTE_FLAG, RES_PARTE_VALOR, RES_PARTE_ERRO } BirParteResultado;

/* Offset e tipo de uma parte do resultado {bool e_erro; T valor; E erro} (sem 'valor' quando T e vazio), na ordem do layout/. */
bool BirParteDoResultado(BirCtx *c, TipoId resultado, BirParteResultado parte, BirCampoInfo *info);
bool BirMainResultado(BirCtx *c);   /* corpo de forja_programa quando main devolve T ! E: R0004 e saida 1 se veio erro */
bool BirProgramaTeste(BirCtx *c);   /* corpo de forja_programa no forja test (bir_construir_teste.c): registra cada 'test' e devolve forja_testes_rodar */
/* Inteiro do valor da IR; um membro de enum vira o valor C do membro (IrEnum.valores) em vez do indice. */
int64_t BirInteiroDoValor(const BirCtx *c, const Valor *v);
bool BirRegraResOk(BirCtx *c, const IrInstr *i);
bool BirRegraResErro(BirCtx *c, const IrInstr *i);
bool BirRegraResEErro(BirCtx *c, const IrInstr *i);
bool BirRegraResValor(BirCtx *c, const IrInstr *i);
bool BirRegraResErroDe(BirCtx *c, const IrInstr *i);
bool BirRegraChaveia(BirCtx *c, const IrInstr *i);
bool BirRegraAssert(BirCtx *c, const IrInstr *i);
bool BirRegraPanic(BirCtx *c, const IrInstr *i);
bool BirRegraExpect(BirCtx *c, const IrInstr *i);

/* ---- ?T, ?^T, ?cstr, union e tupla (bir_construir_opcional.c; BirSlotDoDestino e BirTempDoCampo vivem em bir_construir_resultado.c) ---- */
#define OPC_MEMBRO_TEM 0u        /* ?T no layout/: {bool tem; T v} */
#define OPC_MEMBRO_VALOR 1u
#define UNIAO_MEMBRO_TAG 0u      /* union no layout/: {u32 tag; union interna} */
#define UNIAO_MEMBRO_VARIANTE 1u

bool BirEhPonteiroOpcional(const Tipos *tipos, TipoId tipo);                   /* ?^T e ?cstr: o proprio ponteiro (BC_I64, nulo = none) */
bool BirNuloAgregado(BirCtx *c, TipoId tipo, uint32_t *endereco);              /* none de ?T: slot novo zerado; devolve o vreg do endereco */
bool BirSlotDoDestino(BirCtx *c, const IrInstr *i, uint32_t *base);            /* slot do temporario de destino (tipo da instrucao), com END_SLOT */
bool BirTempDoCampo(BirCtx *c, const IrInstr *i, uint32_t agregado, const BirCampoInfo *membro); /* destino da instrucao = o membro em [agregado + offset] (agregado = vreg do endereco): CARREGA ou COPIA_MEM */
bool BirRegraEmbrulha(BirCtx *c, const IrInstr *i);        /* IR_EMBRULHA: ?T (tem + v), ?^T/?cstr (o ponteiro), union (tag + variante) */
bool BirRegraOpcTem(BirCtx *c, const IrInstr *i);          /* IR_OPC_TEM: tem valor (op_aux 0) ou e none (op_aux 1) */
bool BirRegraOpcValor(BirCtx *c, const IrInstr *i);        /* IR_OPC_VALOR: nao confere */
bool BirRegraTupla(BirCtx *c, const IrInstr *i);           /* IR_TUPLA */
bool BirRegraTuplaElemento(BirCtx *c, const IrInstr *i);   /* IR_TUPLA_ELEMENTO */

/* ---- texto: str (16 B, {ptr, len}) e cstr (bir_construir_texto.c) ---- */
bool BirTextoLiteral(BirCtx *c, const Valor *v, uint32_t *endereco);       /* literal de str: slot {END_SIMBOLO do texto, len}; devolve o vreg do endereco do slot */
bool BirTextoCstr(BirCtx *c, const Valor *v, BirOperando *saida);          /* literal de cstr: END_SIMBOLO do texto num vreg I64 */
bool BirImprimeStr(BirCtx *c, const IrInstr *i);                           /* IR_IMPRIME de str: forja_print_str */
bool BirRegraStrIgual(BirCtx *c, const IrInstr *i);                        /* IR_BINARIO IGUAL e DIFERENTE de str: forja_str_igual */
bool BirCstrParaStr(BirCtx *c, const IrInstr *i);                          /* 'as' de cstr para str: forja_str_de_cstr */
bool BirRegraTamanhoTexto(BirCtx *c, const IrInstr *i);                    /* IR_TAMANHO_TEXTO de str: CARREGA do len */

/* ---- arrays [N]T, fatias []T e slice_from (bir_construir_array.c) ---- */
bool BirRegraElemento(BirCtx *c, const IrInstr *i);            /* IR_ELEMENTO: copia do elemento (CARREGA ou COPIA_MEM); dev confere o indice (R0006) */
bool BirRegraEnderecoElemento(BirCtx *c, const IrInstr *i);    /* IR_ENDERECO_ELEMENTO: base + i * tam */
bool BirRegraFatia(BirCtx *c, const IrInstr *i);               /* IR_FATIA: {ptr + lo * tam, len}; dev chama forja_fatia_confere */
bool BirRegraFatiaDePonteiro(BirCtx *c, const IrInstr *i);     /* IR_FATIA_DE_PONTEIRO (slice_from): {ptr, len}; dev chama forja_vetor_c_confere */
bool BirRegraArray(BirCtx *c, const IrInstr *i);               /* IR_ARRAY: literal de [N]T (ou zero) e a fatia vazia */

/* ---- listas [dynamic]T (bir_construir_array.c) e alocacao (bir_construir_aloca.c) ---- */
#define LISTA_MEMBRO_PTR 0u     /* K17: o ptr do [dynamic]T (membro 0 de {ptr, len, cap, alocador, epoca}; nativo_layout.c) */
#define LISTA_MEMBRO_LEN 1u     /* membros do [dynamic]T no layout/: {ptr, len, cap, alocador, epoca} */
#define LISTA_MEMBRO_CAP 2u
#define LISTA_MEMBRO_ALOCADOR 3u
#define LISTA_MEMBRO_EPOCA 4u

/* A lista de um valor [dynamic]T (por valor) ou ^[dynamic]T: endereco = vreg I64 com o bloco de 32 B, tipo = o tipo [dynamic]T (nunca o ponteiro). */
typedef struct { uint32_t endereco; TipoId tipo; } BirListaRef;
bool BirListaDe(BirCtx *c, const IrInstr *i, const Valor *v, BirListaRef *lista);
uint32_t BirOffsetDaLista(BirCtx *c, const BirListaRef *lista, uint32_t membro);   /* offset do membro; 0 = ptr */
void BirChamarListaViva(BirCtx *c, const IrInstr *i, uint32_t epoca);              /* dev: forja_lista_viva(epoca vreg I32, arquivo, linha) */
bool BirTamanhoDeLista(BirCtx *c, const IrInstr *i);       /* IR_TAMANHO_TEXTO de [dynamic]T e ^[dynamic]T: CARREGA do len */
bool BirInteiro64(BirCtx *c, const IrInstr *i, const Valor *v, BirOperando *saida); /* inteiro de 64 bits (indice, limite, cap): vreg curto alarga pelo sinal do TIPO */

/* Dev: indice (vreg I64) >= n (sem sinal) desvia ao frio com forja_falha_indice (R0006); segue em bloco novo. */
void BirConferirIndice(BirCtx *c, const IrInstr *i, BirOperando indice, BirOperando n);

/* CHAMA a funcao do runtime: retorno BC_NADA descarta; senao devolve o vreg do resultado (BIR_SEM_VREG sem retorno). args vem de BirArgs. */
typedef struct { const char *funcao; uint8_t retorno; uint32_t nargs; } BirChamadaRt;
uint32_t BirChamarRuntime(BirCtx *c, const BirChamadaRt *chamada, BirOperando *args);
const IrExterno *BirExternoDe(const BirCtx *c, int32_t nome);                          /* foreign declarado com o nome (NULL se nao e foreign) */
bool BirRegraAloca(BirCtx *c, const IrInstr *i);           /* IR_ALOCA: forja_mem_alocar */
bool BirRegraListaCria(BirCtx *c, const IrInstr *i);       /* IR_LISTA_CRIA: forja_lista_criar e {ptr, 0, cap, alocador} */
bool BirRegraAnexa(BirCtx *c, const IrInstr *i);           /* IR_ANEXA: forja_lista_crescer quando len == cap, grava o item, len += 1 */
bool BirRegraRedimensiona(BirCtx *c, const IrInstr *i);    /* IR_REDIMENSIONA (resize): forja_lista_crescer ate caber, memset dos novos, len = n (IA1-L5b) */
bool BirRegraAnexaFatia(BirCtx *c, const IrInstr *i);      /* IR_ANEXA_FATIA (append items =): cresce ate caber, memcpy, len += fatia.len (IA1-L5b) */
bool BirRegraOrdena(BirCtx *c, const IrInstr *i);          /* IR_ORDENA (sort): forja_ordenar(ptr, len, ORDENA_*) do runtime (IA1-L5b) */
bool BirRegraOrdenaGeral(BirCtx *c, const IrInstr *i);     /* IR_ORDENA_GERAL (sort_by, K08a; modo CHAVE = sort_by_key, K08b): forja_ordenar_estavel / forja_ordc_ordenar do runtime */
bool BirRegraBuscaBinaria(BirCtx *c, const IrInstr *i);    /* IR_BUSCA_BINARIA (binary_search, K08b): forja_ordc_posicao / forja_ordc_achou do runtime */
bool BirRegraHandleNasce(BirCtx *c, const IrInstr *i);      /* IR_HANDLE_NASCE (M6c P88, bir_construir_handle.c): dev chama forja_handle_nasce; release nao emite nada */
bool BirRegraLibEncerra(BirCtx *c, const IrInstr *i);       /* IR_LIB_ENCERRA (M6d P92e): dev chama forja_libmem_encerrou(simbolo); release nao emite nada */
bool BirRegraHandleSolta(BirCtx *c, const IrInstr *i);      /* IR_HANDLE_SOLTA: dev chama forja_handle_solta; release nao emite nada */
bool BirRegraHeap(BirCtx *c, const IrInstr *i);           /* IR_HEAP (heap_push / heap_pop, K17): forja_heap_subir / forja_heap_retira_topo do runtime (rt_ordenar.c) */
bool BirRegraLibera(BirCtx *c, const IrInstr *i);          /* IR_LIBERA op_aux 0..3: forja_mem_liberar, forja_lista_liberar, forja_pool_devolver */
bool BirRegraFrameReset(BirCtx *c, const IrInstr *i);      /* IR_FRAME_RESET: forja_frame_reset */
/* Alocador de IR_ALOCA, IR_LISTA_CRIA, IR_LIBERA op_aux 3 e IR_FORMATA: V_INT (1 heap, 2 frame) vira imediato u32; handle de Arena/Pool e um valor u32 (vreg I32). */
bool BirAlocadorDe(BirCtx *c, const IrInstr *i, const Valor *v, BirOperando *saida);
bool BirRegraFormata(BirCtx *c, const IrInstr *i);         /* IR_FORMATA: forja_fmt_inicio, uma forja_fmt_<tipo> por par e o final (imprime, str ou cstr) */

/* ---- reais (bir_construir_real.c) ---- */
/* Valor da IR (V_REAL, ou V_INT/V_BOOL numa classe real) -> imediato real: f32 = (float) do double (conversao direta), f64 = o double. */
bool BirImediatoReal(const Valor *v, uint8_t classe, BirOperando *saida);
/* SOMA SUB MUL DIV e as seis comparacoes sobre reais; ab = operandos ja construidos. */
bool BirContaReal(BirCtx *c, const IrInstr *i, const BirOperando ab[2]);
bool BirNegarReal(BirCtx *c, BirOperando a);
bool BirRaizReal(BirCtx *c, BirOperando a);                    /* dst = sqrt(a) (BIR_RAIZ; a real, F32 ou F64) */
void BirNegarRealEm(BirCtx *c, BirOperando a, uint32_t dst);   /* dst = -a (a e dst da mesma classe real), sem tocar no destino da instrucao */
/* 'as' em que a origem ou o destino e real (inteiro <-> real, f32 <-> f64). */
bool BirConversaoReal(BirCtx *c, const IrInstr *i);

/* ---- bit_set e matrix (bir_construir_v19.c) ---- */
bool BirClasseDoConjunto(const Tipos *tipos, TipoId tipo, uint8_t *classe);   /* bit_set: BC_I8/I16/I32/I64 pelo numero de membros (<= 8, 16, 32, 64) */
bool BirRegraConjunto(BirCtx *c, const IrInstr *i);            /* IR_CONJUNTO op_aux 2..8: diferenca, tem, insere, remove, conta (forja_rt_conta_bits), adiciona, membro */
bool BirRegraMatriz(BirCtx *c, const IrInstr *i);              /* IR_MATRIZ op_aux 0..8 desenrolado em contas escalares f32/f64 sobre o slot (por colunas) */
bool BirRegraMatrizElemento(BirCtx *c, const IrInstr *i);      /* IR_MATRIZ_ELEMENTO: dev confere a linha e depois a coluna (R0006) */
bool BirRegraMatrizEndElemento(BirCtx *c, const IrInstr *i);   /* IR_MATRIZ_END_ELEMENTO: o endereco do elemento, mesmas conferencias */

/* ---- soa e map (bir_construir_soa_mapa.c) ---- */
bool BirRegraSoaElemento(BirCtx *c, const IrInstr *i);     /* IR_SOA_ELEMENTO: gather, a struct S sai no slot do temporario */
bool BirRegraSoaCampo(BirCtx *c, const IrInstr *i);        /* IR_SOA_CAMPO */
bool BirRegraSoaEndCampo(BirCtx *c, const IrInstr *i);     /* IR_SOA_END_CAMPO */
bool BirRegraSoaGrava(BirCtx *c, const IrInstr *i);        /* IR_SOA_GRAVA: scatter */
bool BirRegraSoaColuna(BirCtx *c, const IrInstr *i);       /* IR_SOA_COLUNA: a fatia {ptr da coluna, len} */
bool BirRegraSoaCria(BirCtx *c, const IrInstr *i);         /* IR_SOA_CRIA: forja_soa_criar */
bool BirRegraSoaAnexa(BirCtx *c, const IrInstr *i);        /* IR_SOA_ANEXA: forja_soa_crescer quando len == cap e o item espalhado pelas colunas */
bool BirRegraMapaCria(BirCtx *c, const IrInstr *i);        /* IR_MAPA_CRIA: forja_mapa_criar */
bool BirRegraMapaInsere(BirCtx *c, const IrInstr *i);      /* IR_MAPA_INSERE: forja_mapa_inserir e o valor em valores[indice] */
bool BirRegraMapaAcha(BirCtx *c, const IrInstr *i);        /* IR_MAPA_ACHA: forja_mapa_achar; ?V em nicho ou struct {tem; v} */
bool BirRegraMapaPonteiro(BirCtx *c, const IrInstr *i);    /* IR_MAPA_PONTEIRO */
bool BirRegraMapaTem(BirCtx *c, const IrInstr *i);         /* IR_MAPA_TEM */
bool BirRegraMapaRemove(BirCtx *c, const IrInstr *i);      /* IR_MAPA_REMOVE: forja_mapa_remover */
bool BirRegraMapaItem(BirCtx *c, const IrInstr *i);        /* IR_MAPA_ITEM: chave, valor ou endereco do valor do par denso b */
bool BirLiberaMapa(BirCtx *c, const IrInstr *i);           /* IR_LIBERA op_aux 4: forja_mapa_liberar */
bool BirLiberaSoa(BirCtx *c, const IrInstr *i);            /* IR_LIBERA op_aux 5: forja_soa_liberar e a soa volta a {0} mantendo o alocador */
bool BirTamanhoDeColecao(BirCtx *c, const IrInstr *i);     /* IR_TAMANHO_TEXTO de map, soa[dynamic]S e [dynamic]T (e os ponteiros para eles) */

/* ---- map por valor (bir_construir_soa_mapa_valor.c) e os tipos do map (bir_construir_soa_mapa.c) ---- */
/* O forja_mapa: endereco = vreg I64 do bloco de 56 B; chave e valor = os tipos K e V com a forma de cada um. */
typedef struct { uint32_t endereco; TipoId tipo, chave, valor; BirSlot forma_chave, forma_valor; bool texto; } MapaRef;
/* Servico do runtime sobre uma chave: a funcao (chave por slot) e as de chave por valor (k4, k8), o retorno e se recebe tam_valor. */
typedef struct { const char *funcao, *funcao_k4, *funcao_k8; uint8_t retorno; bool com_valor; } ServicoMapa;
/* A instrucao, o map e o servico de uma chamada ao runtime. */
typedef struct { const IrInstr *i; const MapaRef *mapa; const ServicoMapa *servico; } PedidoMapa;
bool ChaveEhPorValor(BirCtx *c, const MapaRef *r);                               /* chave inteira de 4 ou 8 bytes, sem texto: vai por valor (k4/k8) */
bool ChamarMapaPorValor(BirCtx *c, const PedidoMapa *p, uint32_t *resultado);    /* forja_mapa_<nome>_k4|k8: *resultado = vreg do indice denso (achar e inserir) ou BIR_SEM_VREG */

/* ---- traco do diferencial (bir_construir_traco.c, P18a) ---- */
/* Depois da instrucao que grava local ou temporario escalar: CHAMA forja_traco(funcao, instr, linha, coluna, bits). false = X0904 (ja relatado). */
bool BirTracar(BirCtx *c, const IrInstr *i);

/* ---- regras (bir_construir.c, bir_construir_conta.c, bir_construir_divisao.c, bir_construir_fluxo.c) ---- */
bool BirRegraDivisao(BirCtx *c, const IrInstr *i);         /* DIV, RESTO, DESLOCA_ESQ e DESLOCA_DIR com os blocos de guarda */
bool BirRegraLocal(BirCtx *c, const IrInstr *i);
bool BirRegraAtribuicao(BirCtx *c, const IrInstr *i);
bool BirRegraChamada(BirCtx *c, const IrInstr *i);
bool BirRegraChamadaInd(BirCtx *c, const IrInstr *i);
bool BirRegraRetorna(BirCtx *c, const IrInstr *i);
bool BirRegraImprime(BirCtx *c, const IrInstr *i);
bool BirRegraUnario(BirCtx *c, const IrInstr *i);
bool BirRegraBinario(BirCtx *c, const IrInstr *i);
bool BirRegraConversao(BirCtx *c, const IrInstr *i);
bool BirRegraSe(BirCtx *c, const IrInstr *i);
bool BirRegraEnquanto(BirCtx *c, const IrInstr *i);
bool BirRegraPara(BirCtx *c, const IrInstr *i);
bool BirRegraPare(BirCtx *c, const IrInstr *i);
bool BirRegraContinua(BirCtx *c, const IrInstr *i);

#endif
