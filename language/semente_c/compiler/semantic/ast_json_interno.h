/* ast_json_interno.h - estado e funcoes compartilhadas entre ast_json.c, ast_json_decl.c e ast_json_ref.c (AST 1.0 em JSON).
 * Versao: 0.1.2
 * Changelog: 0.1.2 - Linguagem v1.13 L1b: Json.arquivo_decl (o arquivo da declaracao num modulo de pasta); linha_base passa a ser do arquivo da declaracao.
 *            0.1.1 - G3 G0b: quebra de ciclo frontend->semantic (movido de frontend/ para semantic/).
 *            0.1.0 - IA1-I1: Json (antes interno do ast_json.c) ganha fim (tokens), id (caminho estrutural), refs, locais e imports; funcoes de escrita visiveis aos tres arquivos.
 * Verificacao: construir.bat && python ..\tests\ast\verificar_ast.py --forja <forja.exe>
 */
#ifndef FORJA_AST_JSON_INTERNO_H
#define FORJA_AST_JSON_INTERNO_H

#include "common/buf.h"
#include "common/fonte.h"
#include "common/intern.h"
#include "frontend/ast.h"
#include "frontend/ast_fim.h"
#include "semantic/ast_json_ref.h"
#include "semantic/semantic.h"

/* Nome declarado (local, parametro) ou import: chave -> texto do id do no que o declara. locais: chave = offset do no; imports: chave = id do apelido. */
typedef struct { uint32_t chave, off, tam; } AstGuardado;

typedef struct {
    Buf *b;
    const Fonte *fonte;
    Interner *nomes;
    Vec linhas;
    const Semantica *sem;   /* so em --tipos: a semantica que anotou a arvore impressa ("tipo" textual) */
    const Modulos *modulos;
    const AstRefs *refs;    /* ligacoes dos usos (NULL = sem "ref") */
    AstFim fim;
    Buf id;                 /* caminho estrutural do no em escrita: "<modulo>:<decl>/corpo[2]/valor" */
    Vec locais, imports;    /* AstGuardado */
    Buf guardados;          /* texto dos ids guardados */
    uint32_t linha_base;    /* linha global do inicio do arquivo da declaracao em escrita, menos 1 */
    const char *arquivo_decl; /* v1.13: caminho do arquivo da declaracao em escrita, so num modulo de pasta com 2+ arquivos; NULL = nao sai "arquivo" por declaracao */
    uint8_t modulo;         /* modulo em escrita */
    bool tipos, sem_pos;
} Json;

/* ---- ast_json.c: escrita ---- */
void TextoJson(Json *j, const char *s, size_t n, bool caminho);
void Chave(Json *j, const char *chave);
void NomeJson(Json *j, int32_t id);
void ChaveNome(Json *j, const char *chave, int32_t id);
void ChaveBool(Json *j, const char *chave, bool valor);
void ChavePos(Json *j, const char *chave, uint32_t off); /* ", "<chave>": [l, c]" (nada em --sem-pos) */
void MapearLinhas(Json *j);
void AbreNo(Json *j, const char *tipo, uint32_t off, uint32_t fim);
void AbreObjeto(Json *j, uint32_t off, uint32_t fim);
void ChaveExpr(Json *j, const char *chave, const Expr *e);
void ChaveCorpo(Json *j, const char *chave, const Stmt *s);
void ChaveTipo(Json *j, const TipoRef *t);
void ChaveTipoTexto(Json *j, const char *chave, TipoId tipo);
void TipoRefJson(Json *j, const TipoRef *t);
void CampoJson(Json *j, const Campo *c);
void ParamJson(Json *j, const Param *p);
void DeclJson(Json *j, const Decl *d);

/* ---- ast_json_ref.c: ids, ligacoes e recorte ---- */
size_t IdEmpurrar(Json *j, const char *chave, int indice); /* acrescenta "/chave" ou "/chave[i]" ao id; devolve o tamanho de antes */
void IdRestaurar(Json *j, size_t tam);
void LocalRegistrar(Json *j, uint32_t pos);                 /* o id corrente declara o nome do no que comeca em pos */
void ImportRegistrar(Json *j, int32_t apelido);             /* o id corrente e o do import de apelido */
void RefDaExpr(Json *j, const Expr *e);                     /* , "ref", "ref_modulo", "ref_tipo" e "ref_import" do uso resolvido */
void RefDoTipo(Json *j, const TipoRef *t);
void RefDoCaso(Json *j, const Caso *c);
bool AstNoExtrair(const char *json, size_t tam, const char *id, size_t *ini, size_t *fim);

#endif
