/* ast_json.h - imprime a AST em JSON (comando 'forja ast': AST 1.0).
 * Versao: 1.1.1
 * Changelog: 1.1.1 - G3 G0b: quebra de ciclo frontend->semantic (movido de frontend/ para semantic/).
 *            1.1.0 - IA1-I2: AstNomeDaExpr (o nome do no de uma expressao, para 'forja consultar tipo').
 *            1.0.0 - IA1-I1: AST 1.0 (ast_versao 1.0.0). AstImprimir (um pedido so, no lugar de AstImprimirJson/AstImprimirJsonTipos): ids estruturais, "fim", "ref", --sem-pos, --no e projeto.
 *            0.2.0 - Fase 5c: AstImprimirJsonTipos (depois da semantica) acrescenta "tipo":"<texto>" e renomeia a chave "tipo" do tipo
 *                    escrito para "tipo_escrito". 'forja ast' sem --tipos nao muda.
 *            0.1.0 - Fase 5b: formato {"arquivo", "ast_versao", "declaracoes": [...]}; cada no tem "no" (tipo) e "pos" [linha, coluna].
 * Verificacao: construir.bat && forja.exe ast <arq.fj> | python -c "import json,sys; json.load(sys.stdin)"; python ..\tests\ast\verificar_ast.py --forja <forja.exe>
 */
#ifndef FORJA_AST_JSON_H
#define FORJA_AST_JSON_H

#include "common/buf.h"
#include "common/diag.h"
#include "common/fonte.h"
#include "common/intern.h"
#include "frontend/ast.h"
#include "semantic/ast_json_ref.h"
#include "frontend/modulos.h"
#include "semantic/semantic.h"

/* O que imprimir. programa/fonte/nomes/modulos/diag: a compilacao cuja AST sai. sem: so em --tipos (a propria arvore ja analisada: cada no ganha "tipo").
 * refs: a tabela de usos resolvidos (AstRefsMontar) de uma arvore analisada, ou NULL (sem "ref"). projeto: um documento com todos os modulos, na ordem de imports.
 * no: so a subarvore do no com esse id. sem_pos: sem "pos", "fim", "op_pos", "pos_nome" nem "arquivo" (comparacao estrutural). */
typedef struct {
    const Programa *programa;
    const Fonte *fonte;
    Interner *nomes;
    const Modulos *modulos;
    Diag *diag;
    const Semantica *sem;
    const AstRefs *refs;
    const char *no;
    bool projeto, sem_pos;
} AstPedido;

/* Anexa o documento JSON de uma linha, terminado em LF, a saida. false = *erro (texto fixo): id de --no inexistente ou erro lexico ao reler o fonte. */
bool AstImprimir(const AstPedido *pedido, Buf *saida, const char **erro);

/* O "no" que o 'forja ast' escreve para uma expressao ("nome", "chamada", "campo"...); tipo_no = Expr.tipo_no. */
const char *AstNomeDaExpr(uint8_t tipo_no);

#endif
