/* ast_json_ref.h - ids estruturais e ligacoes ("ref") da AST 1.0 em JSON, e o recorte de um no por id (forja ast --no).
 * Versao: 0.2.2
 * Changelog: 0.2.2 - G3 G0c: AstRefEntrada.ordem (indice de coleta) para a ordenacao total por (pos_op, ordem).
 *            0.2.1 - G3 G0b: quebra de ciclo frontend->semantic (movido de frontend/ para semantic/).
 *            0.2.0 - IA1-I2: AstRefEntrada.dono, AstRefs.tipos (AstRefTipo: tipo escrito, case e nome de tipo em expressao) e AstRefsDeclDoTipo; o indice de forja consultar le a mesma tabela.
 *            0.1.0 - IA1-I1: AstRefs (tabela posicao -> uso resolvido, montada de uma AST depois da semantica), ids "<modulo>:<decl>/<campo>[i]/...", "ref" nos usos resolvidos e AstNoExtrair.
 * Verificacao: construir.bat && python ..\tests\ast\verificar_ast.py --forja <forja.exe>
 *
 * Por que a tabela e separada da arvore que se imprime: a semantica REESCREVE a AST no lugar (embrulha, opc_teste, chamada de proc local vira chamada
 * indireta, swizzle vira indice), entao a AST impressa sem --tipos vem de um parse sem semantica e as ligacoes vem de OUTRO parse analisado; as duas
 * se casam pelo offset do nome (Expr.pos_op), unico por ocorrencia. Em --tipos as duas sao a mesma arvore.
 */
#ifndef FORJA_AST_JSON_REF_H
#define FORJA_AST_JSON_REF_H

#include "common/buf.h"
#include "common/intern.h"
#include "common/vec.h"
#include "frontend/ast.h"
#include "frontend/modulos.h"
#include "semantic/semantic.h"

/* Uso resolvido pela semantica. decl_pos: offset do no que declara o local (Param.pos, Stmt.pos ou Caso.pos); AST_REF_SEM_POS nos demais. dono: a declaracao que contem o uso. */
typedef struct { uint32_t pos_op, decl_pos; const Expr *e; const Decl *dono; uint32_t ordem; /* indice de coleta: desempata pos_op igual */ } AstRefEntrada;
enum { AST_REF_SEM_POS = UINT32_MAX };

/* Uso de nome que a semantica nao liga a um Expr: tipo escrito (TR_NOME), 'case Tipo[.membro]' e nome de tipo dentro de uma expressao (transmute(x, para = T)).
 * pos = offset do no impresso (TipoRef.pos, Caso.pos, Expr.pos); nome/membro = ids do Interner da semantica (membro -1 sem); modulo = qualificador ('apelido.'). */
typedef enum { AST_USO_TIPO_ESCRITO, AST_USO_CASO, AST_USO_NOME_DE_TIPO } AstUsoClasse;
typedef struct { uint32_t pos; int32_t nome, membro; uint8_t classe, modulo; const Decl *dono; } AstRefTipo;

typedef struct {
    const Semantica *sem; /* a semantica que analisou a arvore de onde as entradas saem */
    Vec entradas;         /* AstRefEntrada, ordenado por pos_op */
    Vec tipos;            /* AstRefTipo, na ordem da arvore */
} AstRefs;

/* Percorre sem->programa e guarda cada nome, chamada, campo e literal de struct que a semantica ligou. */
void AstRefsMontar(AstRefs *refs, const Semantica *sem);
void AstRefsLiberar(AstRefs *refs);

/* Texto do id de uma declaracao nomeada: "<caminho do modulo>:<nome>"; test, import e link levam "test:<titulo>", "import:<caminho>", "link:<lib>" depois do modulo. */
void AstIdBaseDaDecl(Buf *saida, const Modulos *modulos, const Interner *nomes, const Decl *d);

/* Declaracao global (unit, const, struct, enum, union, proc) do nome escrito no modulo 'modulo_do_uso' (com o qualificador 'apelido.' do modulo 'qualificador', 0 = nenhum), na
 * semantica de 'refs'; NULL se nao ha ou nao esta na AST impressa. IA1-L8: o mesmo nome pode ser de modulos diferentes. */
const Decl *AstRefsDeclDoNome(const AstRefs *refs, int32_t nome, uint8_t qualificador, uint8_t modulo_do_uso);

/* struct, enum ou union do tipo (atras de um '^'); NULL nos demais. */
const Decl *AstRefsDeclDoTipo(const AstRefs *refs, TipoId t);

#endif
