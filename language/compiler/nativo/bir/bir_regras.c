/* bir_regras.c - REGRAS_IR[]: o que cada IrOp (e variante) tem no backend nativo, e a busca da regra de uma instrucao.
 * Versao: 0.24.0
 * Changelog: 0.24.0 - M6d P92e: IR_LIB_ENCERRA SUPORTADO (BirRegraLibEncerra), marco "M2"; NOME_OP ganha o nome.
 *            0.23.0 - M6c P88: IR_HANDLE_NASCE e IR_HANDLE_SOLTA SUPORTADO (bir_construir_handle.c), marco "M2" (gerar_bir_verifica.c so aceita M1..M3); NOME_OP ganha os nomes.
 *            0.22.0 - K17: IR_HEAP SUPORTADO (BirRegraHeap, bir_construir_aloca.c), marco "M2"; NOME_OP ganha o nome.
 *            0.21.0 - K08b: IR_BUSCA_BINARIA SUPORTADO (BirRegraBuscaBinaria, bir_construir_aloca.c), marco "M2"; NOME_OP ganha o nome.
 *            0.20.0 - K08a: IR_ORDENA_GERAL (sort_by) SUPORTADO, marco "M2" (bir_construir_aloca.c).
 *            0.19.0 - IA1-L9a: IR_BINARIO SOMA_VOLTA, SUB_VOLTA e MUL_VOLTA SUPORTADO (marco "M2"); NOMES_OPERADOR ganha os tres.
 *            0.18.0 - IA1-L5b: IR_REDIMENSIONA, IR_ANEXA_FATIA e IR_ORDENA (SUPORTADO, marco "M2"; bir_construir_aloca.c).
 *            0.17.1 - M3 F9b-2: decisoes extraidas de BirVarianteDe (VarianteDaChamada e VarianteDoOperador).0.17.0 - M3 P21v-c: IR_CHAMADA_IND vira SUPORTADO (marco "M3"; BirRegraChamadaInd).
 *            0.16.0 - M3 P21v-b: IR_CHAMADA_IND (ir.h 0.16.0) entra em NOMES_IR_OP e como FUTURO (marco "M3"): a chamada por valor de proc segue X0001 real ate o P21v-c.
 *            0.15.0 -M3 P21: IR_CHAMADA "callback" (algum argumento V_PROC; o trampolim fjt_ de bir_construir_fronteira.c) vira SUPORTADO (marco "M3"); BirVarianteDe devolve "callback" nesse caso.
 *            0.14.0 - M3 P19: IR_CHAMADA "foreign" vira SUPORTADO (marco "M3"); a chamada a foreign de fora usa a regra do proc (BirRegraChamada).
 *            0.13.0 - Linguagem v1.11 L13: IR_UNARIO RAIZ (sqrt) SUPORTADO (marco "M2"); NOMES_OPERADOR ganha "RAIZ".
 *            0.12.0 - M2 P17b: IR_SOA_* (7) e IR_MAPA_* (7) viram SUPORTADO (marco "M2"); IR_LIBERA op_aux 4 e 5 e o len de map e soa dinamica vem pelas regras de IR_LIBERA e IR_TAMANHO_TEXTO.
 *            0.11.0 - M2 P16: IR_CONJUNTO (bit_set), IR_MATRIZ, IR_MATRIZ_ELEMENTO e IR_MATRIZ_END_ELEMENTO viram SUPORTADO (marco "M2").
 *            0.10.0 - M2 P15b: IR_FORMATA (f"...", format, format_c) vira SUPORTADO (marco "M2").
 *            0.9.0 - M2 P15a: IR_EMBRULHA, IR_OPC_TEM, IR_OPC_VALOR, IR_TUPLA e IR_TUPLA_ELEMENTO viram SUPORTADO (marco "M2"); IR_CHAVEIA cobre ?T, ?^T, ?cstr e union.
 *            0.8.0 - M2 P14b: IR_ALOCA, IR_LISTA_CRIA, IR_ANEXA, IR_LIBERA (op_aux 0..3; o 4 e o 5, de map e soa, seguem X0001 pelo marco M2) e IR_FRAME_RESET viram SUPORTADO (marco "M2");
 *                    IR_CHAMADA a foreign INTERNO da std (Arena e Pool) vale como "proc"; o foreign de fora segue FUTURO (M3).
 *            0.7.0 - M2 P13b: IR_ELEMENTO, IR_ENDERECO_ELEMENTO, IR_FATIA, IR_ARRAY (lista fica X0001 pelo tipo) e IR_FATIA_DE_PONTEIRO (slice_from) viram SUPORTADO (marco "M2"); IR_TAMANHO_TEXTO cobre a fatia.
 *            0.6.0 - M2 P13a: IR_TAMANHO_TEXTO vira SUPORTADO (marco "M2"; so o str, fatia e lista seguem X0001 pelo tipo).
 *            0.5.0 - M2 P12a: IR_CHAVEIA, IR_RES_OK, IR_RES_ERRO, IR_RES_E_ERRO, IR_RES_VALOR, IR_RES_ERRO_DE, IR_EXPECT, IR_ASSERT e IR_PANIC viram SUPORTADO (marco "M2").
 *            0.4.0 - M2 P11a: IR_ATRIBUICAO campo, OP_ENDERECO, OP_DEREF, IR_CAMPO, IR_STRUCT, IR_VERIFICA_FAIXA, IR_TRANSMUTA, IR_GRAVA_PONTEIRO e IR_ENDERECO_CAMPO viram SUPORTADO (marco "M2").
 *            0.3.0 - M2 P9: DIV RESTO BIT_E BIT_OU BIT_XOU DESLOCA_ESQ DESLOCA_DIR e BIT_NAO passam a SUPORTADO (marco "M2").
 *            0.2.1 - registra IR_FATIA_DE_PONTEIRO (slice_from, ir.h 0.14.0) em NOME_OP e FUTURO_M2 (cobertura x64 fica para o P13).
 *            0.2.0 - M2 P9a: SUPORTADO_EM(op, variante, fn, marco) como base; SUPORTADO fica como SUPORTADO_EM com "M1" (as linhas do M1 nao mudam).
 *            0.1.0 - backend x64 P6: M1 = locais, atribuicao, negar/nao, E/OU/comparacoes/soma/sub/mul, conversao i32-int-uint, chamada a proc,
 *                    retorna, se/enquanto/para/pare/continua e imprime; o resto sai X0001 (marco M2; chamada a foreign M3).
 * Verificacao: construir.bat && teste_nativo.exe bir <arq.fj>
 */
#include <string.h>

#include "nativo/bir/bir_interno.h"

#define NOME_OP(x) [x] = #x

static const char *const NOMES_IR_OP[] = {
    NOME_OP(IR_LOCAL), NOME_OP(IR_UNARIO), NOME_OP(IR_BINARIO), NOME_OP(IR_CONVERSAO), NOME_OP(IR_CHAMADA), NOME_OP(IR_CAMPO),
    NOME_OP(IR_STRUCT), NOME_OP(IR_ATRIBUICAO), NOME_OP(IR_VERIFICA_FAIXA), NOME_OP(IR_IMPRIME), NOME_OP(IR_SE), NOME_OP(IR_ENQUANTO),
    NOME_OP(IR_PARA), NOME_OP(IR_CHAVEIA), NOME_OP(IR_RETORNA), NOME_OP(IR_PARE), NOME_OP(IR_CONTINUA), NOME_OP(IR_RES_OK),
    NOME_OP(IR_RES_ERRO), NOME_OP(IR_RES_E_ERRO), NOME_OP(IR_RES_VALOR), NOME_OP(IR_RES_ERRO_DE), NOME_OP(IR_EXPECT), NOME_OP(IR_ASSERT),
    NOME_OP(IR_PANIC), NOME_OP(IR_TRANSMUTA), NOME_OP(IR_GRAVA_PONTEIRO), NOME_OP(IR_TAMANHO_TEXTO), NOME_OP(IR_ELEMENTO),
    NOME_OP(IR_ENDERECO_ELEMENTO), NOME_OP(IR_ENDERECO_CAMPO), NOME_OP(IR_FATIA), NOME_OP(IR_ARRAY), NOME_OP(IR_ALOCA),
    NOME_OP(IR_LISTA_CRIA), NOME_OP(IR_ANEXA), NOME_OP(IR_LIBERA), NOME_OP(IR_FRAME_RESET), NOME_OP(IR_EMBRULHA), NOME_OP(IR_OPC_TEM),
    NOME_OP(IR_OPC_VALOR), NOME_OP(IR_TUPLA), NOME_OP(IR_TUPLA_ELEMENTO), NOME_OP(IR_FORMATA), NOME_OP(IR_CONJUNTO), NOME_OP(IR_MATRIZ),
    NOME_OP(IR_MATRIZ_ELEMENTO), NOME_OP(IR_MATRIZ_END_ELEMENTO), NOME_OP(IR_SOA_ELEMENTO), NOME_OP(IR_SOA_CAMPO),
    NOME_OP(IR_SOA_END_CAMPO), NOME_OP(IR_SOA_GRAVA), NOME_OP(IR_SOA_COLUNA), NOME_OP(IR_SOA_CRIA), NOME_OP(IR_SOA_ANEXA),
    NOME_OP(IR_MAPA_CRIA), NOME_OP(IR_MAPA_INSERE), NOME_OP(IR_MAPA_ACHA), NOME_OP(IR_MAPA_PONTEIRO), NOME_OP(IR_MAPA_TEM),
    NOME_OP(IR_MAPA_REMOVE), NOME_OP(IR_MAPA_ITEM), NOME_OP(IR_FATIA_DE_PONTEIRO), NOME_OP(IR_CHAMADA_IND),
    NOME_OP(IR_REDIMENSIONA), NOME_OP(IR_ANEXA_FATIA), NOME_OP(IR_ORDENA), NOME_OP(IR_ORDENA_GERAL), NOME_OP(IR_BUSCA_BINARIA),
    NOME_OP(IR_HEAP), NOME_OP(IR_HANDLE_NASCE), NOME_OP(IR_HANDLE_SOLTA), NOME_OP(IR_LIB_ENCERRA),
};

/* Na ordem do enum Operador (ast.h). */
static const char *const NOMES_OPERADOR[] = {
    "OU", "E", "IGUAL", "DIFERENTE", "MENOR", "MENOR_IGUAL", "MAIOR", "MAIOR_IGUAL", "SOMA", "SUB", "MUL", "DIV", "RESTO",
    "NEGAR", "NAO", "ENDERECO", "BIT_E", "BIT_OU", "BIT_XOU", "DESLOCA_ESQ", "DESLOCA_DIR", "BIT_NAO", "DEREF", "RAIZ",
    "SOMA_VOLTA", "SUB_VOLTA", "MUL_VOLTA",
};

/* SUPORTADO_EM diz em que marco a familia entrou; as linhas de M2 em diante usam o marco certo. */
#define SUPORTADO_EM(op, variante, fn, marco) {(op), (variante), BR_SUPORTADO, (marco), (fn)}
#define SUPORTADO(op, variante, fn) SUPORTADO_EM((op), (variante), (fn), "M1")
#define FUTURO(op, variante, marco) {(op), (variante), BR_FUTURO, (marco), NULL}
#define FUTURO_M2(op) FUTURO((op), "-", "M2")

/* A busca para na 1a linha cujo op bate e cuja variante e igual (ou "-"). */
const BirRegra REGRAS_IR[] = {
    SUPORTADO(IR_LOCAL, "-", BirRegraLocal),
    SUPORTADO(IR_ATRIBUICAO, "local", BirRegraAtribuicao),
    SUPORTADO_EM(IR_ATRIBUICAO, "campo", BirRegraAtribuicao, "M2"),
    SUPORTADO(IR_UNARIO, "NEGAR", BirRegraUnario),
    SUPORTADO(IR_UNARIO, "NAO", BirRegraUnario),
    SUPORTADO_EM(IR_UNARIO, "ENDERECO", BirRegraEndereco, "M2"),
    SUPORTADO_EM(IR_UNARIO, "DEREF", BirRegraDeref, "M2"),
    SUPORTADO_EM(IR_UNARIO, "BIT_NAO", BirRegraUnario, "M2"),
    SUPORTADO_EM(IR_UNARIO, "RAIZ", BirRegraUnario, "M2"),
    SUPORTADO(IR_BINARIO, "E", BirRegraBinario),
    SUPORTADO(IR_BINARIO, "OU", BirRegraBinario),
    SUPORTADO(IR_BINARIO, "IGUAL", BirRegraBinario),
    SUPORTADO(IR_BINARIO, "DIFERENTE", BirRegraBinario),
    SUPORTADO(IR_BINARIO, "MENOR", BirRegraBinario),
    SUPORTADO(IR_BINARIO, "MENOR_IGUAL", BirRegraBinario),
    SUPORTADO(IR_BINARIO, "MAIOR", BirRegraBinario),
    SUPORTADO(IR_BINARIO, "MAIOR_IGUAL", BirRegraBinario),
    SUPORTADO(IR_BINARIO, "SOMA", BirRegraBinario),
    SUPORTADO(IR_BINARIO, "SUB", BirRegraBinario),
    SUPORTADO(IR_BINARIO, "MUL", BirRegraBinario),
    SUPORTADO_EM(IR_BINARIO, "DIV", BirRegraDivisao, "M2"),
    SUPORTADO_EM(IR_BINARIO, "RESTO", BirRegraDivisao, "M2"),
    SUPORTADO_EM(IR_BINARIO, "BIT_E", BirRegraBinario, "M2"),
    SUPORTADO_EM(IR_BINARIO, "BIT_OU", BirRegraBinario, "M2"),
    SUPORTADO_EM(IR_BINARIO, "BIT_XOU", BirRegraBinario, "M2"),
    SUPORTADO_EM(IR_BINARIO, "DESLOCA_ESQ", BirRegraDivisao, "M2"),
    SUPORTADO_EM(IR_BINARIO, "DESLOCA_DIR", BirRegraDivisao, "M2"),
    SUPORTADO_EM(IR_BINARIO, "SOMA_VOLTA", BirRegraBinario, "M2"),
    SUPORTADO_EM(IR_BINARIO, "SUB_VOLTA", BirRegraBinario, "M2"),
    SUPORTADO_EM(IR_BINARIO, "MUL_VOLTA", BirRegraBinario, "M2"),
    SUPORTADO(IR_CONVERSAO, "-", BirRegraConversao),
    SUPORTADO(IR_CHAMADA, "proc", BirRegraChamada),
    SUPORTADO_EM(IR_CHAMADA, "foreign", BirRegraChamada, "M3"),
    SUPORTADO_EM(IR_CHAMADA, "callback", BirRegraChamada, "M3"),
    SUPORTADO(IR_RETORNA, "-", BirRegraRetorna),
    SUPORTADO(IR_IMPRIME, "-", BirRegraImprime),
    SUPORTADO(IR_SE, "-", BirRegraSe),
    SUPORTADO(IR_ENQUANTO, "-", BirRegraEnquanto),
    SUPORTADO(IR_PARA, "-", BirRegraPara),
    SUPORTADO(IR_PARE, "-", BirRegraPare),
    SUPORTADO(IR_CONTINUA, "-", BirRegraContinua),
    SUPORTADO_EM(IR_CAMPO, "-", BirRegraCampo, "M2"), SUPORTADO_EM(IR_STRUCT, "-", BirRegraStruct, "M2"),
    SUPORTADO_EM(IR_VERIFICA_FAIXA, "-", BirRegraVerificaFaixa, "M2"), SUPORTADO_EM(IR_TRANSMUTA, "-", BirRegraTransmuta, "M2"),
    SUPORTADO_EM(IR_GRAVA_PONTEIRO, "-", BirRegraGravaPonteiro, "M2"), SUPORTADO_EM(IR_ENDERECO_CAMPO, "-", BirRegraEnderecoCampo, "M2"),
    SUPORTADO_EM(IR_CHAVEIA, "-", BirRegraChaveia, "M2"),
    SUPORTADO_EM(IR_RES_OK, "-", BirRegraResOk, "M2"), SUPORTADO_EM(IR_RES_ERRO, "-", BirRegraResErro, "M2"),
    SUPORTADO_EM(IR_RES_E_ERRO, "-", BirRegraResEErro, "M2"), SUPORTADO_EM(IR_RES_VALOR, "-", BirRegraResValor, "M2"),
    SUPORTADO_EM(IR_RES_ERRO_DE, "-", BirRegraResErroDe, "M2"),
    SUPORTADO_EM(IR_EXPECT, "-", BirRegraExpect, "M2"), SUPORTADO_EM(IR_ASSERT, "-", BirRegraAssert, "M2"),
    SUPORTADO_EM(IR_PANIC, "-", BirRegraPanic, "M2"),
    SUPORTADO_EM(IR_TAMANHO_TEXTO, "-", BirRegraTamanhoTexto, "M2"), SUPORTADO_EM(IR_ELEMENTO, "-", BirRegraElemento, "M2"),
    SUPORTADO_EM(IR_ENDERECO_ELEMENTO, "-", BirRegraEnderecoElemento, "M2"), SUPORTADO_EM(IR_FATIA, "-", BirRegraFatia, "M2"),
    SUPORTADO_EM(IR_ARRAY, "-", BirRegraArray, "M2"), SUPORTADO_EM(IR_ALOCA, "-", BirRegraAloca, "M2"),
    SUPORTADO_EM(IR_LISTA_CRIA, "-", BirRegraListaCria, "M2"), SUPORTADO_EM(IR_ANEXA, "-", BirRegraAnexa, "M2"),
    SUPORTADO_EM(IR_LIBERA, "-", BirRegraLibera, "M2"), SUPORTADO_EM(IR_FRAME_RESET, "-", BirRegraFrameReset, "M2"),
    SUPORTADO_EM(IR_EMBRULHA, "-", BirRegraEmbrulha, "M2"), SUPORTADO_EM(IR_OPC_TEM, "-", BirRegraOpcTem, "M2"), SUPORTADO_EM(IR_OPC_VALOR, "-", BirRegraOpcValor, "M2"),
    SUPORTADO_EM(IR_TUPLA, "-", BirRegraTupla, "M2"), SUPORTADO_EM(IR_TUPLA_ELEMENTO, "-", BirRegraTuplaElemento, "M2"), SUPORTADO_EM(IR_FORMATA, "-", BirRegraFormata, "M2"),
    SUPORTADO_EM(IR_CONJUNTO, "-", BirRegraConjunto, "M2"), SUPORTADO_EM(IR_MATRIZ, "-", BirRegraMatriz, "M2"),
    SUPORTADO_EM(IR_MATRIZ_ELEMENTO, "-", BirRegraMatrizElemento, "M2"), SUPORTADO_EM(IR_MATRIZ_END_ELEMENTO, "-", BirRegraMatrizEndElemento, "M2"),
    SUPORTADO_EM(IR_SOA_ELEMENTO, "-", BirRegraSoaElemento, "M2"), SUPORTADO_EM(IR_SOA_CAMPO, "-", BirRegraSoaCampo, "M2"),
    SUPORTADO_EM(IR_SOA_END_CAMPO, "-", BirRegraSoaEndCampo, "M2"), SUPORTADO_EM(IR_SOA_GRAVA, "-", BirRegraSoaGrava, "M2"),
    SUPORTADO_EM(IR_SOA_COLUNA, "-", BirRegraSoaColuna, "M2"), SUPORTADO_EM(IR_SOA_CRIA, "-", BirRegraSoaCria, "M2"),
    SUPORTADO_EM(IR_SOA_ANEXA, "-", BirRegraSoaAnexa, "M2"), SUPORTADO_EM(IR_MAPA_CRIA, "-", BirRegraMapaCria, "M2"),
    SUPORTADO_EM(IR_MAPA_INSERE, "-", BirRegraMapaInsere, "M2"), SUPORTADO_EM(IR_MAPA_ACHA, "-", BirRegraMapaAcha, "M2"),
    SUPORTADO_EM(IR_MAPA_PONTEIRO, "-", BirRegraMapaPonteiro, "M2"), SUPORTADO_EM(IR_MAPA_TEM, "-", BirRegraMapaTem, "M2"),
    SUPORTADO_EM(IR_MAPA_REMOVE, "-", BirRegraMapaRemove, "M2"), SUPORTADO_EM(IR_MAPA_ITEM, "-", BirRegraMapaItem, "M2"),
    SUPORTADO_EM(IR_FATIA_DE_PONTEIRO, "-", BirRegraFatiaDePonteiro, "M2"),
    SUPORTADO_EM(IR_CHAMADA_IND, "-", BirRegraChamadaInd, "M3"),
    SUPORTADO_EM(IR_REDIMENSIONA, "-", BirRegraRedimensiona, "M2"), SUPORTADO_EM(IR_ANEXA_FATIA, "-", BirRegraAnexaFatia, "M2"),
    SUPORTADO_EM(IR_ORDENA, "-", BirRegraOrdena, "M2"), SUPORTADO_EM(IR_ORDENA_GERAL, "-", BirRegraOrdenaGeral, "M2"),
    SUPORTADO_EM(IR_BUSCA_BINARIA, "-", BirRegraBuscaBinaria, "M2"),
    SUPORTADO_EM(IR_HEAP, "-", BirRegraHeap, "M2"),
    SUPORTADO_EM(IR_HANDLE_NASCE, "-", BirRegraHandleNasce, "M2"), SUPORTADO_EM(IR_HANDLE_SOLTA, "-", BirRegraHandleSolta, "M2"),
    SUPORTADO_EM(IR_LIB_ENCERRA, "-", BirRegraLibEncerra, "M2"),
};

const uint32_t REGRAS_IR_QTD = sizeof REGRAS_IR / sizeof REGRAS_IR[0];

const char *BirNomeDoOp(uint8_t op)
{
    if (op >= sizeof NOMES_IR_OP / sizeof NOMES_IR_OP[0] || NOMES_IR_OP[op] == NULL)
        return "IR_?";
    return NOMES_IR_OP[op];
}

/* Foreign de fora (M3): o interno da std (Arena e Pool) chama o simbolo do proprio runtime e vale como proc. */
static bool ProcEhExterno(const BirCtx *c, int32_t nome)
{
    if (nome < 0 || (uint32_t)nome >= c->n_nomes || c->simbolo_do_nome[nome] != BIR_SIMBOLO_EXTERNO)
        return false;
    return !BirExternoDe(c, nome)->interno;
}

/* P21: chamada com algum proc da Forja como argumento (V_PROC): o trampolim fjt_ entra pela regra "callback". */
static bool TemArgumentoProc(const IrInstr *i)
{
    for (uint32_t k = 0; k < i->nargs; k++) {
        if (i->args[k].tipo_no == V_PROC)
            return true;
    }
    return false;
}

/* Variante da chamada: callback, foreign ou proc. */
static const char *VarianteDaChamada(const BirCtx *c, const IrInstr *i)
{
    if (TemArgumentoProc(i))
        return "callback";
    return ProcEhExterno(c, i->nome) ? "foreign" : "proc";
}

/* Nome do Operador (unario e binario) pelo op_aux; "?" fora da tabela. */
static const char *VarianteDoOperador(const IrInstr *i)
{
    return i->op_aux < sizeof NOMES_OPERADOR / sizeof NOMES_OPERADOR[0] ? NOMES_OPERADOR[i->op_aux] : "?";
}

/* Variante da instrucao: o nome do Operador (unario e binario), local ou campo (atribuicao), callback, proc ou foreign (chamada); "-" nas demais. */
const char *BirVarianteDe(const BirCtx *c, const IrInstr *i)
{
    if (i->op == IR_UNARIO || i->op == IR_BINARIO)
        return VarianteDoOperador(i);
    if (i->op == IR_ATRIBUICAO)
        return i->nome >= 0 ? "campo" : "local";
    if (i->op == IR_CHAMADA)
        return VarianteDaChamada(c, i);
    return "-";
}

const BirRegra *BirAcharRegra(uint8_t op, const char *variante)
{
    for (uint32_t k = 0; k < REGRAS_IR_QTD; k++) {
        const BirRegra *r = &REGRAS_IR[k];

        if (r->op == op && (r->variante[0] == '-' || strcmp(r->variante, variante) == 0))
            return r;
    }
    return NULL;
}
