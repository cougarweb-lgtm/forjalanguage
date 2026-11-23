/* lower_expr.c - lowering de expressoes: nomes, campos, unarios, binarios, e/ou, conversao, texto, literal de struct e faixa.
 * Versao: 0.8.8
 * Changelog: 0.8.8 - Linguagem v1.13 L4b: LwTextoDecodificado devolve o nome cru quando Expr.carregar (#load: bytes do arquivo sem desescape de barra invertida).
 *            0.8.7 - Linguagem v1.13 L1a-ir: IrTrampolim.arquivo (trecho da Fonte de Decl.pos) no lugar de IrTrampolim.modulo.
 *            0.8.6 - Ryu P5: LowerConversao marca IrPrograma.usa_real_curto em real para inteiro (o texto do R0002 do backend C sai pelo Display curto).
 *            0.8.5 - Linguagem v1.12 M3 P21v-b: EX_CHAMADA_IND baixa por LwChamadaInd (lower_call.c); LwChamadaIndSolta sai.
 *            0.8.4 -Linguagem v1.12 M3 P21v-a: LOWERINGS[EX_CHAMADA_IND + 1] (LwChamadaIndSolta: FalhaInterna; o programa com proc como valor nem chega aqui).
 *            0.8.3 -Linguagem v1.11 L14b: LowerBinario desvia toda conta com operando quat (q * r e q * v tem resultado quat ou [3]f32).
 *            0.8.2 - Linguagem v1.11 L14a: LowerUnario e LowerBinario desviam a conta com resultado quat (LwNegarQuat, LwBinarioQuat).
 *            0.8.1 -Linguagem v1.11 L12b: LowerCampo desvia v.xy (REF_COMPONENTE) para LwSwizzle.
 *            0.8.0 - Linguagem v1.11 L12a: LowerUnario e LowerBinario desviam a conta com resultado [N]T (LwUnarioVetor, LwBinarioVetor).
 *            0.7.0 -Linguagem v1.9 P11: LwZeroDoTipo de map = IR_ARRAY nargs 0.
 *                    Linguagem v1.9 P8: LowerCampo desvia a[i].campo e a.campo de soa (LwCampoSoa); LwZeroDoTipo de soa = IR_ARRAY nargs 0.
 *                    Linguagem v1.9 P6: LowerUnario desvia -m de matrix (LwMatrizUnaria); LwZeroDoTipo de matrix = IR_ARRAY nargs 0.
 *                    Linguagem v1.9 P4: LowerBinario desvia bit_set (LwBinarioV19); LwZeroDoTipo de bit_set = 0 (map, soa e matrix entram nos passos deles).
 *            0.6.0 - Linguagem v1.8b P5b: LowerNome aceita REF_PROC (V_PROC + RegistrarTrampolim).
 *            0.5.0 -Linguagem v1.8b P4: LwZeroDoTipo ganha TC_STRUCT (LwPadraoDaStruct: campo struct ausente no literal).
 *            0.4.0 - Linguagem v1.7 P8: LwEmbrulha, LwOpcTeste, LwTupla e LwFTextoSolto; LwZeroDoTipo de ?T = none; sai o provisorio do P1.
 *            0.3.1 - Linguagem v1.7 P1: LOWERINGS[EX_FTEXTO + 1]; as 4 expressoes novas = FalhaInterna "chega no P8" (provisorio).
 *            0.3.0 - Linguagem v1.6 P7a: ZeroDoTipo vira LwZeroDoTipo (publico) e aceita [dynamic]T (lista zero).
 *            0.2.0 - Linguagem v1.6 P1a: LOWERINGS[EX_TIPO + 1]; LOWERINGS[EX_TIPO] = LwTipoSolto (FalhaInterna, provisorio).
 *            0.1.4 - Linguagem v1.5 P1a: LOWERINGS[EX_LITERAL_ARRAY + 1] (entradas novas sem lowering ate o P7).
 *            0.1.3 - Linguagem v1.3 P6: literal de texto leva o tipo (str ou cstr); zero de str e cstr = "".
 *            0.1.2 - Linguagem v1.3: LowerNone (V_NULO); campo de 'p^' le pelo ponteiro sem copiar a struct; ZeroDoTipo de ?^T e rawptr = none;
 *                    OP_DEREF e IR_UNARIO comum.
 *            0.1.1 - Linguagem v1.3: LOWERINGS[EX_NONE + 1] (forma da AST; EX_NONE ainda sem lowering).
 *            0.1.0 - Fase 5d: primeira versao. Arvore flexivel dobra em 1 Valor (lower_const.c); e/ou com curto-circuito por IR_SE
 *                    e local sintetico; literal de struct com args na ordem dos campos; faixa constante conferida na compilacao.
 * Verificacao: construir.bat && python ..\tests\rodar_suite.py --compilador forja.exe --corpus ..\tests\dourado
 */
#include "ir/lower_interno.h"

typedef Valor (*LowerExpr)(LowerCtx *c, const Expr *e);
typedef struct { char escrito, real; } Escape;

/* Escapes que o lexer deixa como escritos no texto; os demais ficam literais (barra e caractere). */
static const Escape ESCAPES[] = {{'n', '\n'}, {'t', '\t'}, {'r', '\r'}, {'\\', '\\'}, {'"', '"'}};

/* ---------- apoio ---------- */

/* Expressao que LwDobrar resolve. Falhar sem diagnostico e bug do lowering; com diagnostico (E0420) segue com valor vazio. */
static Valor ValorConstante(LowerCtx *c, const Expr *e)
{
    Valor v;
    if (LwDobrar(c, e, &v)) return v;
    if (!c->sem->diag->tem) FalhaInterna("lowering: expressao constante nao dobrou (offset %u)", (unsigned)e->pos_op);
    return LwValorNenhum();
}

/* Instrucao com destino em temporario novo; chamar depois de calcular os operandos. */
static IrInstr *InstrComTemp(LowerCtx *c, IrOp op, const Expr *e, TipoId tipo)
{
    IrInstr *i = LwNovaInstr(c, op, e->pos_op);
    i->destino = LwNovoTemp(c);
    i->tipo = tipo;
    return i;
}

static Valor ResultadoTemp(const IrInstr *i)
{
    return LwValorTemp(i->destino, i->tipo);
}

static void AtribuirLocal(LowerCtx *c, int32_t local, Valor v, uint32_t pos)
{
    IrInstr *i = LwNovaInstr(c, IR_ATRIBUICAO, pos);
    i->destino = local;
    i->tipo = v.tipo;
    i->a = v;
}

/* ---------- faixa ---------- */

/* Valor constante: confere na compilacao (a semantica ja provou os literais; aqui pegam-se consts e padroes). */
static void ConferirFaixaConstante(LowerCtx *c, const Campo *campo, int64_t v, uint32_t pos)
{
    /* v guarda bits: campo sem sinal le (uint64_t); acima de INT64_MAX esta fora de qualquer faixa (limites cabem em int64) */
    bool sem_sinal = TipoEhSemSinal(campo->tipo.tipo);
    bool cabe64 = !sem_sinal || v >= 0;
    bool dentro = cabe64 && v >= campo->faixa_min && (campo->faixa_excl ? v < campo->faixa_max : v <= campo->faixa_max);
    if (dentro || c->sem->diag->tem) return;
    if (sem_sinal) {
        DiagRelatar(c->sem->diag, E0420, pos, "valor %llu fora da faixa %lld%s%lld do campo '%s'", (unsigned long long)v,
                    (long long)campo->faixa_min, campo->faixa_excl ? "..<" : "..=", (long long)campo->faixa_max,
                    InternerTexto(c->sem->nomes, campo->nome));
        return;
    }
    DiagRelatar(c->sem->diag, E0420, pos, "valor %lld fora da faixa %lld%s%lld do campo '%s'", (long long)v,
                (long long)campo->faixa_min, campo->faixa_excl ? "..<" : "..=", (long long)campo->faixa_max,
                InternerTexto(c->sem->nomes, campo->nome));
}

void LwVerificaFaixa(LowerCtx *c, const Decl *estrutura, int32_t indice_campo, Valor v, uint32_t pos)
{
    const Campo *campo = &estrutura->campos[indice_campo];
    if (!campo->tem_faixa || !TipoEhInteiro(campo->tipo.tipo)) return;
    if (v.tipo_no == V_INT) {
        ConferirFaixaConstante(c, campo, v.inteiro, pos);
        return;
    }
    IrInstr *i = LwNovaInstr(c, IR_VERIFICA_FAIXA, pos);
    i->a = v;
    i->nome = campo->nome;
    i->faixa_min = campo->faixa_min;
    i->faixa_max = campo->faixa_max;
    i->faixa_excl = campo->faixa_excl;
}

/* ---------- texto ---------- */

static int EscapeDecodificado(char escrito)
{
    for (size_t k = 0; k < sizeof ESCAPES / sizeof ESCAPES[0]; k++) {
        if (ESCAPES[k].escrito == escrito) return (unsigned char)ESCAPES[k].real;
    }
    return -1;
}

int32_t LwTextoDecodificado(LowerCtx *c, const Expr *e)
{
    if (e->nome < 0) FalhaInterna("lowering: texto sem nome internado (offset %u)", (unsigned)e->pos_op);
    /* #load: o nome e o conteudo binario cru do arquivo; desescapar corromperia os bytes (0x5C seguido de n vira quebra de linha). */
    if (e->carregar) return e->nome;
    const char *cru = InternerTexto(c->sem->nomes, e->nome);
    uint32_t tam = InternerTam(c->sem->nomes, e->nome), n = 0;
    char *saida = LwAlocar(c, (size_t)tam + 1);
    for (uint32_t i = 0; i < tam; i++) {
        int real = (cru[i] == '\\' && i + 1 < tam) ? EscapeDecodificado(cru[i + 1]) : -1;
        if (real >= 0) i++;
        saida[n++] = real >= 0 ? (char)real : cru[i];
    }
    return Internar(c->sem->nomes, saida, n);
}

static Valor LowerTexto(LowerCtx *c, const Expr *e)
{
    return LwValorTexto(LwTextoDecodificado(c, e), e->tipo);
}

/* ---------- nome e campo ---------- */

/* Anota o proc da Forja usado como callback (sem repeticao por nome) para o trampolim fjt_<nome> do emissor. */
static void RegistrarTrampolim(LowerCtx *c, const Decl *d, TipoId tipo)
{
    IrPrograma *p = c->prog;

    if (p->trampolins == NULL) p->trampolins = LwAlocar(c, c->sem->qtd_proc * sizeof(IrTrampolim));
    for (uint32_t k = 0; k < p->ntrampolins; k++) {
        if (p->trampolins[k].nome == d->chave) return;
    }
    p->trampolins[p->ntrampolins++] = (IrTrampolim){.nome = d->chave, .tipo = tipo,
        .linha = FonteLocalizarNoArquivo(c->sem->fonte, d->pos).linha, .arquivo = FonteTrechoDoOffset(c->sem->fonte, d->pos)};
}

/* Nome de proc da Forja como argumento de callback: V_PROC (o emissor escreve fjt_<nome>). */
static Valor LwValorProc(LowerCtx *c, const Expr *e)
{
    const Decl *d = c->procs[e->ref_id];

    RegistrarTrampolim(c, d, e->tipo);
    return (Valor){.tipo_no = V_PROC, .tipo = e->tipo, .id = d->chave};
}

/* Const escalar vira o Valor dobrado; const de struct refaz o lowering do init a cada uso. */
static Valor LowerNome(LowerCtx *c, const Expr *e)
{
    if (e->ref == REF_LOCAL || e->ref == REF_PARAM) return LwValorLocal(e->ref_id, e->tipo);
    if (e->ref != REF_CONST) {
        if (e->ref == REF_PROC) return LwValorProc(c, e);
        FalhaInterna("lowering: nome sem ligacao da semantica (offset %u)", (unsigned)e->pos_op);
    }
    if (c->const_escalar[e->ref_id]) return ValorConstante(c, e);
    return LwExpr(c, c->consts[e->ref_id]->init);
}

/* Membro de enum e constante (V_INT do enum); campo de struct e IR_CAMPO sobre o valor da base. */
static Valor LowerCampo(LowerCtx *c, const Expr *e)
{
    if (e->ref == REF_MEMBRO) return ValorConstante(c, e);
    if (e->ref == REF_COMPONENTE) return LwSwizzle(c, e); /* v1.11 L12b: v.xy, c.bgr */
    if (__builtin_expect(c->sem->tipos.tem_soa, 0) && LwCampoDeSoa(c, e)) return LwCampoSoa(c, e);
    if (e->ref != REF_CAMPO) FalhaInterna("lowering: campo sem ligacao da semantica (offset %u)", (unsigned)e->pos_op);
    bool via_deref = e->a->tipo_no == EX_UNARIO && e->a->op == OP_DEREF; /* 'p^.campo': le pelo ponteiro, sem copiar a struct */
    Valor base = LwExpr(c, via_deref ? e->a->a : e->a);
    IrInstr *i = InstrComTemp(c, IR_CAMPO, e, e->tipo);
    i->nome = e->nome;
    i->a = base;
    return ResultadoTemp(i);
}

/* ---------- unario, binario, conversao ---------- */

static Valor LowerUnario(LowerCtx *c, const Expr *e)
{
    if (e->op == OP_ENDERECO && (e->a->tipo_no == EX_CAMPO || e->a->tipo_no == EX_INDICE)) return LwEndereco(c, e->a);
    if (__builtin_expect(c->sem->tipos.tem_matriz, 0) && e->op == OP_NEGAR && TipoClasseDe(&c->sem->tipos, e->tipo) == TC_MATRIZ)
        return LwMatrizUnaria(c, e, 6);
    if (__builtin_expect(c->sem->tipo_quat != 0, 0) && e->op == OP_NEGAR && e->tipo == c->sem->tipo_quat) return LwNegarQuat(c, e); /* v1.11 L14a: -q */
    Valor vetor;
    if (e->op == OP_NEGAR && LwUnarioVetor(c, e, &vetor)) return vetor; /* v1.11 L12a: -a em [N]T */
    Valor a = LwExpr(c, e->a);
    if (e->op == OP_ENDERECO && a.tipo_no != V_LOCAL) {
        FalhaInterna("lowering: '&' sobre algo que nao e variavel (offset %u)", (unsigned)e->pos_op);
    }
    IrInstr *i = InstrComTemp(c, IR_UNARIO, e, e->tipo);
    i->op_aux = e->op;
    i->a = a;
    return ResultadoTemp(i);
}

static Valor LowerNone(LowerCtx *c, const Expr *e)
{
    (void)c;
    return LwValorNulo(e->tipo);
}

/* e/ou: local bool = a; se a (ou 'not a', no 'or') entao local = b. O b so e calculado dentro do SE. */
static Valor LowerLogico(LowerCtx *c, const Expr *e)
{
    Valor a = LwExpr(c, e->a);
    int32_t local = LwNovoLocal(c);
    IrInstr *def = LwNovaInstr(c, IR_LOCAL, e->pos_op);
    def->destino = local;
    def->tipo = TIPO_BOOL;
    def->mutavel = true;
    def->a = a;
    Valor teste = a;
    if (e->op == OP_OU) {
        IrInstr *nega = InstrComTemp(c, IR_UNARIO, e, TIPO_BOOL);
        nega->op_aux = OP_NAO;
        nega->a = a;
        teste = ResultadoTemp(nega);
    }
    IrInstr *se = LwNovaInstr(c, IR_SE, e->pos_op);
    se->a = teste;
    IrBloco *anterior = LwTrocarBloco(c, &se->corpo);
    AtribuirLocal(c, local, LwExpr(c, e->b), e->pos_op);
    LwTrocarBloco(c, anterior);
    return LwValorLocal(local, TIPO_BOOL);
}

static Valor LowerBinario(LowerCtx *c, const Expr *e)
{
    if (e->op == OP_E || e->op == OP_OU) return LowerLogico(c, e);
    if (__builtin_expect(c->sem->tipo_quat != 0, 0) && (e->a->tipo == c->sem->tipo_quat || e->b->tipo == c->sem->tipo_quat)) return LwBinarioQuat(c, e); /* v1.11 L14a/L14b: + - * com quat */
    if (__builtin_expect(c->sem->tipos.tem_matriz | c->sem->tipos.tem_bit_set, 0)) {
        Valor especial;
        if (LwBinarioV19(c, e, &especial)) return especial;
    }
    Valor vetor;
    if (LwBinarioVetor(c, e, &vetor)) return vetor; /* v1.11 L12a: conta com resultado [N]T */
    Valor a = LwExpr(c, e->a);
    Valor b = LwExpr(c, e->b);
    IrInstr *i = InstrComTemp(c, IR_BINARIO, e, e->tipo);
    i->op_aux = e->op;
    i->a = a;
    i->b = b;
    return ResultadoTemp(i);
}

static Valor LowerConversao(LowerCtx *c, const Expr *e)
{
    Valor a = LwExpr(c, e->a);
    IrInstr *i = InstrComTemp(c, IR_CONVERSAO, e, e->tipo);
    i->a = a;
    /* Ryu P5: o R0002 de real para inteiro escreve o valor pelo Display curto (como o nativo). */
    if (TipoEhReal(&c->sem->tipos, e->a->tipo) && !TipoEhReal(&c->sem->tipos, e->tipo)) c->prog->usa_real_curto = true;
    return ResultadoTemp(i);
}

/* ---------- literal de struct ---------- */

static Valor ValorDoCampoAusente(LowerCtx *c, const Decl *estrutura, uint32_t k, uint32_t pos);

/* v1.8b: campo struct ausente no literal = a struct de dentro com os padroes dela (ciclo por valor ja e E0301). */
static Valor LwPadraoDaStruct(LowerCtx *c, TipoId tipo, uint32_t pos)
{
    const Decl *estrutura = c->structs[c->sem->globais[TipoDe(&c->sem->tipos, tipo)->a].indice];
    Valor *slots = LwArgs(c, estrutura->ncampos);
    for (uint32_t k = 0; k < estrutura->ncampos; k++) slots[k] = ValorDoCampoAusente(c, estrutura, k, pos);
    IrInstr *i = LwNovaInstr(c, IR_STRUCT, pos);
    i->destino = LwNovoTemp(c);
    i->tipo = tipo;
    i->args = slots;
    i->nargs = estrutura->ncampos;
    return LwValorTemp(i->destino, tipo);
}

Valor LwZeroDoTipo(LowerCtx *c, TipoId tipo, uint32_t pos)
{
    switch (TipoClasseDe(&c->sem->tipos, tipo)) {
    case TC_BOOL: return LwValorBool(false);
    case TC_REAL:
    case TC_F64: return LwValorReal(0.0, tipo);
    case TC_I32:
    case TC_INT:
    case TC_I8:
    case TC_I16:
    case TC_U8:
    case TC_U16:
    case TC_U32:
    case TC_UINT:
    case TC_ENUM: return LwValorInt(0, tipo);
    case TC_TEXTO: return LwValorTexto(Internar(c->sem->nomes, "", 0), TIPO_TEXTO);
    case TC_CSTR: return LwValorTexto(Internar(c->sem->nomes, "", 0), TIPO_CSTR);
    case TC_ANULAVEL:
    case TC_OPCIONAL:
    case TC_RAWPTR: return LwValorNulo(tipo);
    case TC_ARRAY:
    case TC_FATIA:
    case TC_DINAMICO: return LwZeroComposto(c, tipo, pos);
    case TC_STRUCT: return LwPadraoDaStruct(c, tipo, pos);
    case TC_BIT_SET: return LwValorInt(0, tipo);
    case TC_MAPA:
    case TC_MATRIZ:
    case TC_SOA: return LwZeroComposto(c, tipo, pos);
    default: FalhaInterna("lowering: campo sem padrao de tipo %u nao tem zero (offset %u)", (unsigned)tipo, (unsigned)pos);
    }
}

/* Campo que o literal nao escreveu: o padrao avaliado no ponto do literal, ou zero tipado. */
static Valor ValorDoCampoAusente(LowerCtx *c, const Decl *estrutura, uint32_t k, uint32_t pos)
{
    const Campo *campo = &estrutura->campos[k];
    if (campo->padrao == NULL) return LwZeroDoTipo(c, campo->tipo.tipo, pos);
    Valor v = LwExpr(c, campo->padrao);
    LwVerificaFaixa(c, estrutura, (int32_t)k, v, pos);
    return v;
}

/* Args avaliados na ordem do fonte e gravados no slot do campo; IR_STRUCT recebe os slots na ordem dos campos. */
static Valor LowerLiteralStruct(LowerCtx *c, const Expr *e)
{
    if (e->ref != REF_STRUCT) FalhaInterna("lowering: literal de struct sem ligacao (offset %u)", (unsigned)e->pos_op);
    const Decl *estrutura = c->structs[e->ref_id];
    Valor *slots = LwArgs(c, estrutura->ncampos);
    for (uint32_t i = 0; i < e->nargs; i++) {
        const Arg *g = &e->args[i];
        slots[g->indice] = LwExpr(c, g->valor);
        LwVerificaFaixa(c, estrutura, g->indice, slots[g->indice], g->valor->pos_op);
    }
    for (uint32_t k = 0; k < estrutura->ncampos; k++) {
        if (slots[k].tipo_no == V_NENHUM) slots[k] = ValorDoCampoAusente(c, estrutura, k, e->pos_op);
    }
    IrInstr *i = InstrComTemp(c, IR_STRUCT, e, e->tipo);
    i->args = slots;
    i->nargs = estrutura->ncampos;
    return ResultadoTemp(i);
}

/* EX_TIPO so existe como 1o argumento de new/make, que o lowering trata sem avaliar o argumento (provisorio ate o P7a). */
static Valor LwTipoSolto(LowerCtx *c, const Expr *e)
{
    (void)c;
    FalhaInterna("lowering: tipo escrito fora de new/make (offset %u)", (unsigned)e->pos_op);
}

/* ---------- v1.7: ?T, union, tupla e f"..." ---------- */

/* T -> ?T ou Variante -> union (criado pela semantica): union leva a tag da variante em b; ?T nao leva. */
static Valor LwEmbrulha(LowerCtx *c, const Expr *e)
{
    Valor v = LwExpr(c, e->a);
    IrInstr *i = InstrComTemp(c, IR_EMBRULHA, e, e->tipo);
    i->a = v;
    if (TipoClasseDe(&c->sem->tipos, e->tipo) == TC_UNIAO) i->b = LwValorInt(e->op, TIPO_U32);
    return ResultadoTemp(i);
}

/* x == none / x != none (op 1 = e none, 0 = tem valor). */
static Valor LwOpcTeste(LowerCtx *c, const Expr *e)
{
    Valor v = LwExpr(c, e->a);
    IrInstr *i = InstrComTemp(c, IR_OPC_TEM, e, TIPO_BOOL);
    i->op_aux = (uint8_t)e->op;
    i->a = v;
    return ResultadoTemp(i);
}

/* 'return a, b': valores na ordem do fonte. */
static Valor LwTupla(LowerCtx *c, const Expr *e)
{
    Valor *valores = LwArgs(c, e->nargs);
    for (uint32_t k = 0; k < e->nargs; k++) valores[k] = LwExpr(c, e->args[k].valor);
    IrInstr *i = InstrComTemp(c, IR_TUPLA, e, e->tipo);
    i->args = valores;
    i->nargs = e->nargs;
    return ResultadoTemp(i);
}

/* f"..." so existe dentro de print, format e format_c, que o lowering trata antes de avaliar o argumento. */
static Valor LwFTextoSolto(LowerCtx *c, const Expr *e)
{
    (void)c;
    FalhaInterna("lowering: f\"...\" fora de print/format (offset %u)", (unsigned)e->pos_op);
}

/* ---------- despacho ---------- */

static const LowerExpr LOWERINGS[EX_CHAMADA_IND + 1] = {
    [EX_CHAMADA_IND] = LwChamadaInd, [EX_EMBRULHA] = LwEmbrulha, [EX_OPC_TESTE] = LwOpcTeste, [EX_TUPLA] = LwTupla, [EX_FTEXTO] = LwFTextoSolto,
    [EX_TIPO] = LwTipoSolto,
    [EX_INT] = ValorConstante, [EX_REAL] = ValorConstante, [EX_BOOL] = ValorConstante, [EX_TEXTO] = LowerTexto,
    [EX_NOME] = LowerNome, [EX_CAMPO] = LowerCampo, [EX_CHAMADA] = LwChamada, [EX_LITERAL_STRUCT] = LowerLiteralStruct,
    [EX_UNARIO] = LowerUnario, [EX_BINARIO] = LowerBinario, [EX_CONVERSAO] = LowerConversao, [EX_TRY] = LwTry,
    [EX_OR_ELSE] = LwOrElse, [EX_NONE] = LowerNone, [EX_INDICE] = LwIndice, [EX_FATIA] = LwFatia,[EX_LITERAL_ARRAY] = LwLiteralArray,
};

Valor LwExpr(LowerCtx *c, const Expr *e)
{
    if (e->flexivel) return ValorConstante(c, e);
    return LOWERINGS[e->tipo_no](c, e);
}
