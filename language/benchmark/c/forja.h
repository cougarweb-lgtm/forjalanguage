/* forja.h - tipos e prototipos compartilhados do compilador Forja (prototipo C11).
 * Versao: 1.0.0
 * Changelog: 1.0.0 - primeira versao para o benchmark do ADR-01.
 * Verificacao: construir.bat && python ../medir/rodar_suite.py --compilador "<pasta>/forja.exe" --corpus ../corpus/dourado --registro _tentativas.jsonl
 */
#ifndef FORJA_H
#define FORJA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* ---------- tokens ---------- */
typedef enum {
    TK_EOF, TK_NL, TK_IDENT, TK_INT, TK_REAL, TK_STR,
    KW_CONST, KW_LET, KW_VAR, KW_PROC, KW_STRUCT, KW_UNIT, KW_IF, KW_ELSE, KW_FOR,
    KW_IN, KW_WHILE, KW_RETURN, KW_AS, KW_AND, KW_OR, KW_NOT, KW_TRUE, KW_FALSE,
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE, TK_COMMA, TK_COLON, TK_ASSIGN,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT, TK_EQ, TK_NE, TK_LT, TK_LE,
    TK_GT, TK_GE, TK_PLUSEQ, TK_MINUSEQ, TK_ARROW, TK_DOTDOT, TK_DOTDOTLT,
    TK_DOTDOTEQ, TK_DOT, TK_CARET, TK_AMP
} TokKind;

/* 12 bytes: len satura em 65535 (so alimenta mensagens e literais curtos). */
typedef struct { uint32_t off; int32_t sym; uint16_t len; uint8_t kind; } Token;

/* Simbolos pre-internados: as 18 palavras-chave vem primeiro, na ordem de KW_CONST. */
enum { KEYWORD_COUNT = 18, SYM_I32 = 18, SYM_INT, SYM_F32, SYM_BOOL, SYM_PRINT, SYM_MAIN };

/* ---------- diagnosticos ---------- */
typedef enum {
    E0001, E0101, E0201, E0301, E0302, E0412, E0420, E0501, E0502, E0503, E0504, CODE_COUNT
} DiagCode;

typedef struct { bool has; DiagCode code; uint32_t off; char causa[320]; } Diag;

/* ---------- tipos da linguagem ---------- */
typedef enum { T_VOID, T_I32, T_I64, T_REAL, T_BOOL, T_STRUCT, T_PTR, T_STR } TypeKind;
typedef struct { uint8_t k; int32_t id; } Type; /* REAL: id = dimensao; STRUCT/PTR: id = struct */

#define MAX_DIMBASE 16
typedef struct { int8_t e[MAX_DIMBASE]; bool declared; } Dim;

typedef struct Decl Decl;
typedef struct Expr Expr;
typedef struct Stmt Stmt;
typedef struct Local { Type ty; } Local;

typedef struct { int32_t sym; uint32_t pos; bool ptr; Type ty; } TypeRef;

/* ---------- AST ---------- */
typedef enum { E_INT, E_REAL, E_BOOL, E_NAME, E_FIELD, E_CALL, E_STRUCTLIT, E_UNARY, E_BINARY, E_CAST, E_STR } ExprKind;
typedef enum {
    OP_OR, OP_AND, OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE, OP_ADD, OP_SUB, OP_MUL, OP_DIV,
    OP_MOD, OP_NEG, OP_NOT, OP_ADDR, OP_TRUE, OP_FALSE
} Op;
enum { NAME_LOCAL = 0, NAME_CONST = 1 };

typedef struct { uint32_t name_pos; int32_t name_sym; Expr *val; int32_t idx; } Arg;

struct Expr {
    uint8_t kind, op;
    bool flex; /* arvore so de literais numericos: o tipo vem do contexto */
    Type ty;
    uint32_t pos;   /* inicio da expressao */
    uint32_t oppos; /* operador, nome (variavel, campo, proc, struct), tipo do cast ou literal */
    int32_t sym;
    union { Expr *a; int64_t nargs; }; /* chamada e literal de struct nao usam 'a': 40 bytes por no */
    union { Expr *b; Arg *args; } u;
};

typedef enum { S_LET, S_VAR, S_ASSIGN, S_IF, S_WHILE, S_FOR, S_RETURN, S_CALL } StmtKind;

struct Stmt {
    uint8_t kind, op; /* op: TK_ASSIGN, TK_PLUSEQ ou TK_MINUSEQ */
    bool has_type;
    uint32_t pos, close_pos;
    int32_t sym;
    TypeRef type;
    Type ty; /* tipo do local (let/var/for) */
    Expr *e, *e2, *lhs;
    Stmt *body, *els, *next;
};

typedef enum { D_UNIT_BASE, D_UNIT_DERIV, D_CONST, D_STRUCT, D_PROC } DeclKind;

typedef struct {
    int32_t sym; uint32_t pos; TypeRef type; Expr *def;
    bool has_range, excl; int64_t lo, hi;
} Field;

typedef struct { int32_t sym; uint32_t pos; TypeRef type; Expr *def; } Param;

struct Decl {
    uint8_t kind, op;
    int32_t sym, index;
    uint32_t pos, close_pos;
    int32_t sym_a, sym_b; uint32_t pos_a, pos_b;
    TypeRef type;  /* const */
    Expr *init;    /* const */
    Field *fields; int32_t nfields;
    Param *params; int32_t nparams;
    TypeRef ret; bool has_ret;
    Stmt *body;
    Decl *next;
};

/* ---------- contexto do compilador ---------- */
typedef struct { const char *s; uint32_t len, hash; Decl *proc, *cnst; Local *local; int32_t unit_dim, struct_idx; } Sym;
typedef struct { int32_t sym; Local *prev; } ScopeEntry;
typedef struct { int32_t idx; uint32_t hash; } HashSlot; /* idx -1 = vazio; hash evita tocar na Sym */
typedef struct ArenaChunk ArenaChunk;
typedef struct { char *data; size_t len, cap; } Buf;

typedef struct {
    const char *src; size_t src_len; const char *path;
    Token *toks; uint32_t ntoks, captoks, cur;
    Sym *syms; int32_t nsyms, capsyms; HashSlot *htab; uint32_t hcap;
    ArenaChunk *chunk; size_t used;
    Decl *first_decl, *last_decl;
    Decl **structs; int32_t nstructs, capstructs;
    Dim *dims; int32_t ndims, capdims, nbase;
    ScopeEntry *scope; int32_t nscope, capscope;
    Decl *cur_proc; int no_struct;
    Diag diag;
    Buf out; int32_t for_counter;
} Ctx;

typedef struct { uint64_t lex, parse, check, emit, total; } Tempos;

/* ---------- util.c ---------- */
void *ArenaAlloc(Ctx *c, size_t n);
bool BufGrowCat(Buf *b, const char *s, size_t n);
bool BufPrepare(Buf *b, size_t cap);
bool BufCatU64(Buf *b, uint64_t v);
bool BufPrintf(Buf *b, const char *fmt, ...);

/* Caminho rapido inline: com literal, o strlen e o memcpy viram constantes. */
static inline bool BufCatN(Buf *b, const char *s, size_t n)
{
    if (b->len + n + 1 > b->cap) return BufGrowCat(b, s, n);
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
    return true;
}

static inline bool BufCat(Buf *b, const char *s)
{
    return BufCatN(b, s, strlen(s));
}
uint64_t NowNs(void);
bool InitSymbols(Ctx *c);
int32_t InternSym(Ctx *c, const char *s, uint32_t len);
bool Report(Ctx *c, DiagCode code, uint32_t off, const char *fmt, ...);
void PrintDiag(const Ctx *c);
bool ReadWholeFile(const char *path, char **data, size_t *len);
bool WriteWholeFile(const char *path, const char *data, size_t len);
uint64_t CountLines(const char *s, size_t n);

/* ---------- lexer.c / parser.c / check*.c / emit.c ---------- */
bool LexFile(Ctx *c);
bool ParseFile(Ctx *c);
bool CheckFile(Ctx *c);
bool EmitFile(Ctx *c);
uint64_t LiteralValue(const Ctx *c, const Expr *e);

/* ---------- tipos (check_types.c) ---------- */
Type MakeType(TypeKind k, int32_t id);
bool TypeEq(Type a, Type b);
bool IsNumeric(Type t);
bool IsIntType(Type t);
int32_t DimCombine(Ctx *c, int32_t a, int32_t b, int sign);
int32_t DimNewBase(Ctx *c);
bool ResolveTypeRef(Ctx *c, TypeRef *t);
bool CompatTypes(Ctx *c, Type want, Type got, uint32_t pos);
bool CheckExpr(Ctx *c, Expr *e, Type ctx);
bool CheckBlock(Ctx *c, Stmt *s);
bool CheckConstExprAgainst(Ctx *c, Expr *e, Type want);
void ScopePush(Ctx *c, int32_t sym, Type ty);
void ScopePop(Ctx *c, int32_t height);
const char *SymText(const Ctx *c, int32_t sym);

#endif
