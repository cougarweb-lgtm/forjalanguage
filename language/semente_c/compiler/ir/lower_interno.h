/* lower_interno.h - contexto e interfaces entre os modulos do lowering (lower_*.c). CONGELADO: so o main altera este arquivo.
 * Versao: 0.16.0
 * Changelog: 0.16.0 - S4 P1: LowerCtx.seq_teste_modulo (contador dos test de modulo no forja test).
 *            0.15.0 - IA1-L5b: LwResize e LwSort (lower_memoria.c; so prototipos).
 *            0.14.0 - Linguagem v1.12 M3 P21v-b: LwChamadaInd (lower_call.c; so prototipo).
 *            0.13.0 -Linguagem v1.11 L15a: LwEhListaFixa, LwIndiceListaFixa, LwEnderecoListaFixa, LwFatiaListaFixa, LwTamanhoListaFixa, LwBaseListaFixa e LwAppendListaFixa (lower_lista_fixa.c; so prototipos).
 *            0.12.1 - Linguagem v1.11 L14b: LwProdutoQuat, LwGirarQuat e LwAbsQuat (lower_quat.c).
 *            0.12.0 - Linguagem v1.11 L14a: LwBinarioQuat, LwNegarQuat e LwConjQuat (lower_quat.c).
 *            0.11.0 -Linguagem v1.11 L13: PorElemento, LwPorElemento e LwElementoDoOperando (lower_vetor.c); LwMatematica (lower_matematica.c).
 *            0.10.0 -Linguagem v1.11 L12b: LwSwizzle (lower_vetor.c).
 *            0.9.0 - Linguagem v1.11 L12a: ContaVetor, LwContaVetor, LwBinarioVetor e LwUnarioVetor (lower_vetor.c).
 *            0.8.0 -Linguagem v1.11 L11: LwDepurar (lower_depurar.c).
 *            0.7.0 -Linguagem v1.11 L9: LowerCtx.layouts e a API de lower_layout.c (LwLayoutCalcular, LwLayoutValor, LwStaticAssert, LwAssertsDeTopo, LwLayoutChamada).
 *            0.6.0 - Linguagem v1.9 P11: LwEmbutidoMapa e LwForMapa (lower_mapa.c).
 *                    Linguagem v1.9 P8: LwIndiceSoa, LwEnderecoCampoSoa, LwCampoSoa, LwCampoDeSoa e LwForSoa (lower_soa.c).
 *                    Linguagem v1.9 P6: LwMatrizUnaria, LwIndiceMatriz, LwEnderecoMatriz (lower_v19.c).
 *                    Linguagem v1.9 P4: LwIndiceInt, LwBaseIndexada, LwEnderecavel (lower_array.c) e LwLiteralV19, LwBinarioV19, LwTamanhoV19,
 *                    LwStmtForV19, LwEmbutidoV19, LwEhV19 (lower_v19.c).
 *            0.5.0 - Linguagem v1.8 P6b: LwAlocador com LowerCtx nao const (Arena/Pool gera instrucao).
 *            0.4.0 - Linguagem v1.7 P8: LwAlocador (publico) e LwFormata (lower_formato.c).
 *            0.3.0- Linguagem v1.6 P7a: LwZeroDoTipo, LwEhLista, LwMemoria.
 *            0.1.2 - Linguagem v1.3 P6: LwValorTexto(nome, tipo).
 *            0.1.1 - Linguagem v1.3: LwValorNulo.
 *            0.1.0 - Fase 5d: LowerCtx, escopos de defer e a API de cada modulo.
 * Verificacao: construir.bat (compila com -Werror)
 *
 * Regras do lowering (valem para todos os modulos):
 *  - Toda IR vive na Arena (c->arena). Nunca reutilizar temporario: o emissor registra o tipo de cada temp na definicao.
 *  - Instrucao nova = LwNovaInstr(c, op, pos): ja entra no bloco c->atual. Para escrever em outro bloco (cond, corpo, senao de
 *    um IR_SE/IR_ENQUANTO/IR_PARA/IR_CHAVEIA) use LwTrocarBloco e restaure o anterior.
 *  - IrInstr.pos = Expr.pos_op (ou Stmt.pos quando nao ha expressao). Valor.tipo e sempre o tipo do operando.
 *  - Anotacao faltando na AST (ref == REF_NENHUM onde devia ter, tipo 0) = FalhaInterna com a posicao.
 *  - Funcao de ate 30 linhas; despacho por tabela {ExprTipo/StmtTipo -> funcao}.
 */
#ifndef FORJA_LOWER_INTERNO_H
#define FORJA_LOWER_INTERNO_H

#include "common/vec.h"
#include "ir/lower.h"
#include "ir/layout.h"

/* Um escopo de defer = um bloco { }. 'inicio' = indice em LowerCtx.defers onde comecam os defers deste escopo.
 * 'e_laco' = o bloco e o corpo de um while/for (break/continue expandem ate ele, inclusive). */
typedef struct { uint32_t inicio; bool e_laco; } LwEscopo;

typedef struct {
    Semantica *sem;
    Arena *arena;
    IrPrograma *prog;
    /* funcao em construcao */
    IrFuncao *funcao;
    const Decl *decl;           /* D_PROC ou D_TEST em lowering */
    IrBloco *atual;             /* bloco que recebe as instrucoes novas */
    uint32_t ntemps;            /* proximo temporario */
    uint32_t prox_local;        /* proximo local sintetico (comeca em Decl.nlocais) */
    Vec defers;                 /* Stmt* de ST_DEFER, pilha unica */
    Vec escopos;                /* LwEscopo, pilha */
    /* tabelas por Decl.indice (preenchidas por LwDeclaracoes) */
    Decl **procs, **structs, **consts, **enums;
    Valor *const_valor;         /* valor dobrado da const escalar (indexado por Decl.indice de const) */
    bool *const_escalar;        /* true = const escalar dobrada; false = const de struct (lowering do init a cada uso) */
    uint32_t seq_teste;         /* contador de test para o nome interno 0teste_<n> */
    uint32_t seq_teste_modulo;  /* S4 P1: contador dos test de modulo (0teste_m<k>_<n>); separado para a raiz manter 0teste_<n> */
    Layouts layouts;            /* v1.11 L9: layout dos tipos (so calculado se Semantica.usa_layout; lower_layout.c) */
} LowerCtx;

/* ---- lower_ir.c: construcao da IR ---- */
void *LwAlocar(LowerCtx *c, size_t n);                                  /* zerado, na arena */
IrInstr *LwNovaInstr(LowerCtx *c, IrOp op, uint32_t pos);               /* anexa em *c->atual; destino=-1, nome=-1 */
int32_t LwNovoTemp(LowerCtx *c);
int32_t LwNovoLocal(LowerCtx *c);                                       /* local sintetico */
IrBloco *LwTrocarBloco(LowerCtx *c, IrBloco *novo);                     /* devolve o anterior */
Valor LwValorTemp(int32_t id, TipoId tipo);
Valor LwValorLocal(int32_t id, TipoId tipo);
Valor LwValorInt(int64_t v, TipoId tipo);                               /* enum: inteiro = indice do membro */
Valor LwValorReal(double v, TipoId tipo);
Valor LwValorBool(bool v);
Valor LwValorTexto(int32_t nome_internado, TipoId tipo);                /* tipo = TIPO_TEXTO (str) ou TIPO_CSTR */
Valor LwValorNulo(TipoId tipo);                                         /* none: tipo = ?^T ou rawptr */
Valor LwValorNenhum(void);
Valor *LwArgs(LowerCtx *c, uint32_t n);                                 /* vetor de Valor zerado (V_NENHUM) */

/* ---- lower_expr.c / lower_call.c: expressoes (agente A) ---- */
/* Calcula a expressao, emite as instrucoes no bloco atual e devolve o operando simples com o resultado.
 * Arvore 'flexivel' ou so de literais/consts vira 1 Valor constante, sem instrucao. e/ou usa IR_SE (curto-circuito). */
Valor LwExpr(LowerCtx *c, const Expr *e);
/* Verifica a faixa do campo 'indice_campo' da struct (Decl D_STRUCT) para o valor v; nao emite nada se o campo nao tem faixa ou
 * se v e constante (V_INT) ja provada pela semantica (E0420). Usada pelo literal de struct (A) e pela atribuicao (B). */
void LwVerificaFaixa(LowerCtx *c, const Decl *estrutura, int32_t indice_campo, Valor v, uint32_t pos);
/* Chamada de proc/embutido (print, assert, panic): argumentos na ordem do fonte, gravados no slot Arg.indice; slot vazio =
 * Param.padrao avaliado NO CHAMADOR. Devolve o resultado (V_NENHUM se o retorno e vazio). */
Valor LwChamada(LowerCtx *c, const Expr *e);
Valor LwChamadaInd(LowerCtx *c, const Expr *e);  /* v1.12: EX_CHAMADA_IND (f(x), obj.campo(x)) = IR_CHAMADA_IND sobre o valor de tipo proc */
Valor LwTry(LowerCtx *c, const Expr *e);        /* usa LwExpandirDefers(c, 0) antes do retorno do erro */
Valor LwOrElse(LowerCtx *c, const Expr *e);
/* Dobra a expressao constante em *saida; false se nao e constante. E0420 (estouro do tipo) via sem->diag e retorna false com
 * c->sem->diag.tem = true. */
bool LwDobrar(LowerCtx *c, const Expr *e, Valor *saida);
/* Texto do literal EX_TEXTO ja decodificado e internado (o lexer interna CRU, com escapes). */
int32_t LwTextoDecodificado(LowerCtx *c, const Expr *e);

/* ---- lower_decl.c: structs, enums, constantes (agente A) ---- */
void LwDeclaracoes(LowerCtx *c);                /* preenche tabelas por indice e prog->structs/enums/consts */

/* ---- lower_stmt.c / lower_fluxo.c: comandos (agente B) ---- */
void LwStmt(LowerCtx *c, const Stmt *s);
/* Bloco { } de comandos ligados por 'prox': empilha um escopo de defer, faz o lowering de cada comando e, no fim normal
 * (bloco que nao termina em return/break/continue), expande os defers do escopo em ordem inversa e desempilha. */
void LwBloco(LowerCtx *c, const Stmt *lista, bool e_laco);
/* Faz o lowering dos defers de todos os escopos de 'escopo_alvo' ate o topo (do mais interno para fora, ordem inversa dentro de
 * cada um), SEM desempilhar. return usa 0; break/continue usam LwEscopoDoLaco(c). */
void LwExpandirDefers(LowerCtx *c, uint32_t escopo_alvo);
uint32_t LwEscopoDoLaco(LowerCtx *c);           /* indice do escopo e_laco mais interno */
/* Comandos de fluxo (lower_fluxo.c), chamados pela tabela de lower_stmt.c. */
void LwStmtIf(LowerCtx *c, const Stmt *s);
void LwStmtWhile(LowerCtx *c, const Stmt *s);
void LwStmtFor(LowerCtx *c, const Stmt *s);
void LwStmtBreak(LowerCtx *c, const Stmt *s);
void LwStmtContinue(LowerCtx *c, const Stmt *s);
void LwStmtDefer(LowerCtx *c, const Stmt *s);
void LwStmtSwitch(LowerCtx *c, const Stmt *s);
/* Anotacao faltando na AST: FalhaInterna com arquivo:linha:coluna (lower_stmt.c). */
_Noreturn void LwFalhaStmt(LowerCtx *c, const Stmt *s, const char *motivo);

/* ---- lower_array.c: [N]T (Linguagem v1.5; fatia e laco de colecao entram no P7b e no P8) ---- */
Valor LwEndereco(LowerCtx *c, const Expr *e);       /* ^T do lvalor (cadeia); a semantica ja provou enderecavel */
Valor LwIndice(LowerCtx *c, const Expr *e);
Valor LwFatia(LowerCtx *c, const Expr *e);
Valor LwBaseDaColecao(LowerCtx *c, const Expr *e, Valor *tamanho); /* for de colecao: base (^[N]T, []T ou [N]T por valor) + n */
Valor LwLiteralArray(LowerCtx *c, const Expr *e);
Valor LwZeroComposto(LowerCtx *c, TipoId t, uint32_t pos); /* IR_ARRAY nargs 0 */
Valor LwTamanhoColecao(LowerCtx *c, const Expr *arg);      /* len de array/^array: V_INT N, sem avaliar o argumento; fatia: .len */

/* Envoltorios publicos dos static de lower_array.c (v1.9). */
Valor LwIndiceInt(LowerCtx *c, const Expr *e);
Valor LwBaseIndexada(LowerCtx *c, const Expr *base);
bool LwEnderecavel(const LowerCtx *c, const Expr *e);

/* ---- lower_v19.c: bit_set (Linguagem v1.9 P4; matrix, soa e map acrescentam nos passos deles) ---- */
Valor LwLiteralV19(LowerCtx *c, const Expr *e);              /* literal {..} de bit_set */
bool LwBinarioV19(LowerCtx *c, const Expr *e, Valor *saida); /* false = segue o caminho de hoje */
Valor LwTamanhoV19(LowerCtx *c, const Expr *arg);            /* len de bit_set */
void LwStmtForV19(LowerCtx *c, const Stmt *s);               /* for item in bit_set */
Valor LwEmbutidoV19(LowerCtx *c, const Expr *e);             /* insert, remove, has */
bool LwEhV19(const LowerCtx *c, TipoId t);                   /* classe >= TC_MAPA, ou ponteiro para ela */
Valor LwMatrizUnaria(LowerCtx *c, const Expr *e, uint8_t op_aux); /* -m (op_aux 6) */
Valor LwIndiceMatriz(LowerCtx *c, const Expr *e);            /* m[i, j] */
Valor LwEnderecoMatriz(LowerCtx *c, const Expr *e);          /* &m[i, j] e alvo de m[i, j] = v */

/* ---- lower_soa.c: soa[N]S e soa[dynamic]S (Linguagem v1.9 P8) ---- */
Valor LwIndiceSoa(LowerCtx *c, const Expr *e);               /* a[i] inteiro (gather) */
Valor LwEnderecoCampoSoa(LowerCtx *c, const Expr *e);        /* &a[i].campo e o ponteiro de a[i].campo = v */
Valor LwCampoSoa(LowerCtx *c, const Expr *e);                /* a[i].campo lido ou a.campo (coluna) */
bool LwCampoDeSoa(const LowerCtx *c, const Expr *e);         /* e e a[i].campo ou a.campo de uma soa */
void LwForSoa(LowerCtx *c, const Stmt *s);                   /* for item / for i, item sobre soa */

/* ---- lower_mapa.c: map[K]V (Linguagem v1.9 P11) ---- */
Valor LwEmbutidoMapa(LowerCtx *c, const Expr *e);            /* insert, get, get_ptr, has, remove sobre map */
void LwForMapa(LowerCtx *c, const Stmt *s);                  /* for k, v in m / for k, &v in m */

/* ---- lower_memoria.c / lower_expr.c / lower_array.c: memoria explicita (Linguagem v1.6 P7a) ---- */
Valor LwZeroDoTipo(LowerCtx *c, TipoId tipo, uint32_t pos);  /* valor zero de qualquer tipo com zero (lista = IR_ARRAY nargs 0) */
bool LwEhLista(const LowerCtx *c, TipoId t);                 /* [dynamic]T ou ^[dynamic]T */
Valor LwMemoria(LowerCtx *c, const Expr *e);                 /* new, make, append, delete, frame_reset */
Valor LwResize(LowerCtx *c, const Expr *e);                  /* IA1-L5b: resize(&l, len = n) */
Valor LwSort(LowerCtx *c, const Expr *e);                    /* IA1-L5b: sort(fatia) */

/* ---- lower_memoria.c / lower_formato.c: alocador e texto formatado (Linguagem v1.7 P8) ---- */
Valor LwAlocador(LowerCtx *c, const Expr *e);                /* alocador do embutido: V_INT u32 1 ou 2, ou o handle (u32) de Arena/Pool */
/* f"...": modo 0 imprime, 1 str, 2 cstr (op_aux do IR_FORMATA); *saida = a instrucao, para o chamador ler o destino. */
void LwFormata(LowerCtx *c, const Expr *ftexto, uint8_t modo, Valor alocador, IrInstr **saida);

/* ---- lower_vetor.c: conta elemento a elemento em [N]T (Linguagem v1.11 L12a) ---- */
/* op: OP_SOMA, OP_SUB, OP_MUL, OP_DIV ou OP_NEGAR (b = V_NENHUM); a e b ja calculados ([N]T ou escalar); tipo = [N]R do resultado; pos = IrInstr.pos. */
typedef struct { uint8_t op; TipoId tipo; uint32_t pos; Valor a, b; } ContaVetor;
Valor LwContaVetor(LowerCtx *c, const ContaVetor *conta);        /* N <= 16 desenrolada (IR_ARRAY), senao IR_PARA 0..<N; sem IrOp nova */
bool LwBinarioVetor(LowerCtx *c, const Expr *e, Valor *saida);   /* false = resultado nao e [N]T: segue o caminho de hoje */
bool LwUnarioVetor(LowerCtx *c, const Expr *e, Valor *saida);    /* -a em [N]T; false = segue o caminho de hoje */
/* v1.11 L13: o desenrolar/laco por elemento sem conhecer a conta. 'elemento' devolve o elemento 'indice' do resultado (calcula e emite no bloco atual);
 * tipo = [N]R do resultado; dados e repassado a 'elemento'. Mesmas regras de N do LwContaVetor (N <= 16 desenrolado, senao IR_PARA). */
typedef Valor (*ElementoFn)(LowerCtx *c, const void *dados, Valor indice);
typedef struct { TipoId tipo; uint32_t pos; ElementoFn elemento; const void *dados; } PorElemento;
Valor LwPorElemento(LowerCtx *c, const PorElemento *por);
Valor LwElementoDoOperando(LowerCtx *c, Valor v, Valor indice, uint32_t pos); /* IR_ELEMENTO (indice provado) se v e [N]T; o escalar vale como esta */
Valor LwSwizzle(LowerCtx *c, const Expr *e);                    /* v.xy..v.xyzw (EX_CAMPO REF_COMPONENTE, Linguagem v1.11 L12b): base lida uma vez + IR_ELEMENTO por letra + IR_ARRAY */

/* ---- lower_matematica.c: min, max, abs, clamp e sqrt (Linguagem v1.11 L13) ---- */
Valor LwMatematica(LowerCtx *c, const Expr *e);              /* e->ref_id = EMB_MIN..EMB_SQRT; sem IrOp nova (sqrt = IR_UNARIO OP_RAIZ) */

Valor LwRaiz(LowerCtx *c, uint32_t pos, TipoId tipo, Valor a); /* IR_UNARIO OP_RAIZ de a (f32 ou f64); tambem usado por LwAbsQuat */

/* ---- lower_quat.c: o tipo quat (Linguagem v1.11 L14a); sem IrOp nova: IR_CAMPO por componente + IR_BINARIO/IR_UNARIO f32 + IR_STRUCT ---- */
Valor LwBinarioQuat(LowerCtx *c, const Expr *e);             /* conta com algum operando quat: q + r, q - r, q * k, k * q por componente na ordem x y z w; q * r e q * v desviam */
Valor LwProdutoQuat(LowerCtx *c, const Expr *e);             /* q * r (QuaternionMultiply do raymath) */
Valor LwGirarQuat(LowerCtx *c, const Expr *e);               /* q * [3]f32 (Vector3RotateByQuaternion do raymath; e->tipo = o do vetor) */
Valor LwAbsQuat(LowerCtx *c, const Expr *e);                 /* abs(q) = sqrt(x*x + y*y + z*z + w*w) (EMB_ABS com argumento quat; f32) */
Valor LwNegarQuat(LowerCtx *c, const Expr *e);               /* -q (e->tipo = quat) */
Valor LwConjQuat(LowerCtx *c, const Expr *e);                /* conj(q) = {-x, -y, -z, w} (EMB_CONJ) */

/* ---- lower_depurar.c: {expr:?} (Linguagem v1.11 L11) ---- */
/* Empilha em 'pares' (Vec de Valor; 2 por parte: formato u32 0 e valor) o texto de depuracao de v (struct, [N]T, enum, str e escalares);
 * emite IR_CAMPO/IR_ELEMENTO/IR_CHAMADA no bloco atual e cria, quando preciso, o proc oculto 0depurar_<TipoId> do enum. */
void LwDepurar(LowerCtx *c, Vec *pares, Valor v, uint32_t pos);

/* ---- lower_lista_fixa.c: [dynamic, N]T = struct oculta {itens: [N]T, n: int} (Linguagem v1.11 L15a) ---- */
bool LwEhListaFixa(const LowerCtx *c, TipoId t);               /* [dynamic, N]T ou ^[dynamic, N]T */
Valor LwIndiceListaFixa(LowerCtx *c, const Expr *e);           /* l[i]: IR_ELEMENTO sobre itens[0 ..< n] (R0006 em dev com i fora de 0..<n) */
Valor LwEnderecoListaFixa(LowerCtx *c, const Expr *e);         /* &l[i] */
Valor LwFatiaListaFixa(LowerCtx *c, const Expr *e);            /* l[..], l[a..<b], l[a..=b] */
Valor LwTamanhoListaFixa(LowerCtx *c, const Expr *arg);        /* len(l) = campo n */
Valor LwBaseListaFixa(LowerCtx *c, const Expr *e, Valor *tamanho); /* for: a fatia itens[0 ..< n] e o n */
Valor LwAppendListaFixa(LowerCtx *c, const Expr *e, const Expr *alvo, const Expr *item); /* append(&l, item = x): cheia = R0007 */

/* ---- lower_layout.c: size_of, align_of, offset_of e static_assert (Linguagem v1.11 L9) ---- */
void LwLayoutCalcular(LowerCtx *c);                          /* LayoutCalcular dos tipos, se o programa usa layout; depois das structs, antes das const */
bool LwLayoutValor(LowerCtx *c, const Expr *e, int64_t *valor); /* valor de size_of/align_of/offset_of; false = E0328 em sem->diag */
void LwStaticAssert(LowerCtx *c, const Expr *e);             /* static_assert(cond, msg = ...): falso ou nao constante = E0329 */
void LwAssertsDeTopo(LowerCtx *c);                           /* os static_assert de topo (D_ASSERT), na ordem do fonte */
Valor LwLayoutChamada(LowerCtx *c, const Expr *e);           /* chamada de embutido de layout: int literal (ou V_NENHUM no static_assert) */

/* ---- lower.c: funcoes e programa (main) ---- */
void LwFuncaoProc(LowerCtx *c, const Decl *d);
void LwFuncaoTeste(LowerCtx *c, const Decl *d);

#endif
