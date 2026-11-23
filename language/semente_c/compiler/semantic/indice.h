/* indice.h - indice semantico do projeto: os usos resolvidos de nome (chamadas, referencias, tipos escritos, cases) de uma compilacao ja analisada.
 * Versao: 0.1.2
 * Changelog: 0.1.2 - G3 G0c: IndUso.ordem (indice de coleta) para a ordenacao total dos usos.
 *            0.1.1 - G3 G0b: quebra de ciclo frontend->semantic (movido de frontend/ para semantic/).
 *            0.1.0 - IA1-I2: primeira versao. As declaracoes (proc, struct, enum, union, const, unit, foreign, test, import, link; main e pub export) sao a propria lista
 *            Programa da semantica; os usos saem da tabela que a AST 1.0 ja monta (AstRefs, ast_json_ref.c), entao 'forja ast' e 'forja consultar refs' leem a mesma ligacao.
 * Verificacao: construir.bat && python tests\consultar\verificar_consultar.py --forja <forja.exe>
 */
#ifndef FORJA_INDICE_H
#define FORJA_INDICE_H

#include "common/vec.h"
#include "semantic/ast_json_ref.h"

/* O que um uso aponta (o mesmo "ref_tipo" do 'forja ast', mais PROC/CONST/... por categoria da declaracao). */
typedef enum { IND_PROC, IND_STRUCT, IND_ENUM, IND_UNIAO, IND_CONST, IND_UNIDADE, IND_MEMBRO, IND_CAMPO, IND_PARAM, IND_LOCAL, IND_EMBUTIDO } IndAlvo;

/* pos = offset do no impresso na AST (inicio da expressao, do tipo ou do case); pos_nome = offset do token do nome. alvo = a declaracao global apontada (dono do campo/membro;
 * em PARAM e LOCAL, o proc/test que os declara); NULL em EMBUTIDO. indice = campo, membro ou parametro dentro do alvo (-1 nos demais). decl_pos = offset do no que declara o
 * local/parametro (AST_REF_SEM_POS nos demais). dono = a declaracao que contem o uso. */
typedef struct {
    uint32_t pos, pos_nome, decl_pos, ordem; /* ordem: indice de coleta (desempate da ordenacao; nao entra na chave de repetido) */
    const Decl *alvo, *dono;
    int32_t nome, indice;
    uint8_t alvo_tipo; /* IndAlvo */
    bool chamada;      /* EX_CHAMADA (proc, foreign ou embutido) */
} IndUso;

typedef struct {
    const Semantica *sem;
    AstRefs refs;
    Vec usos; /* IndUso, ordenado por (pos, pos_nome) e sem repetidos */
} Indice;

/* Monta o indice de uma semantica que analisou o programa sem erro. */
void IndiceMontar(Indice *ind, const Semantica *sem);
void IndiceLiberar(Indice *ind);

/* Categoria textual de uma declaracao: "proc", "foreign", "struct", "enum", "union", "const", "unit", "teste", "import", "link", "static_assert". */
const char *IndiceCategoria(const Decl *d);

/* Texto do alvo de um uso ("proc", "campo", "param", "local", "embutido"...): o "ref_tipo" do AST. */
const char *IndiceNomeDoAlvo(IndAlvo alvo);

/* O IndAlvo de uma declaracao global (const, struct, enum, union, proc; as unidades dao IND_UNIDADE). */
IndAlvo IndiceAlvoDaDecl(const Decl *d);

/* A declaracao global (nao embutida) que o nome interno designa num uso do modulo 'modulo' (o proprio modulo vence; IA1-L8); NULL se nao ha. */
const Decl *IndiceDeclDoNome(const Indice *ind, int32_t nome, uint8_t modulo);

/* Posicao do membro de enum de nome 'membro' em d, ou -1. */
int32_t IndiceIndiceDoMembro(const Decl *d, int32_t membro);

#endif
