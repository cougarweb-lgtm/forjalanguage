/* sem_expr.c - tipos e unidades das expressoes: literais flexiveis, nomes, campos, membros de enum, unarios, binarios, cast.
 * Versao: 0.18.8
 * Changelog: 0.18.8 - B01c: ExNome recusa ler o nome do parametro Allocator fora do rotulo 'alloc =' (SemAllocatorValorSolto, E0336).
 *            0.18.7 - B01a: ExCampo recusa a leitura de .id de um Allocator (SemAllocatorLerCampo, E0336).
 *            0.18.6 - IA1-L3c2: ExMembro acha o membro pela forma antiga da tabela de renomeados (SemAcharMembro) e o E0201 sai para SemMembroAusente (sem_tipo.c).
 *            0.18.5 - IA1-L2: ExNome e ErroCampoSemStruct (e o campo de lista fixa) consultam SemIdiomaDeNome/SemIdiomaDeCampo antes do E0201/E0308 (forma de outra linguagem = E0110).
 *            0.18.4 - IA1-b15: ExNome com nome sem declaracao ja relatado no corpo sai calado (SemNomeJaRelatado).
 *            0.18.3 - IA1-b08: o E0306 de conta com texto (str, cstr ou literal) leva a forma da ocorrencia com os tipos reais (SemFormaContaDeTexto).
 *            0.18.2 - IA1-b07: E0301 de conta com tipos diferentes (numerico, bits, comparacao) e de condicao nao bool levam a forma da ocorrencia (SemFormaOperandos, SemFormaDoValor).
 *            0.18.1 - IA1-b06: "a" + "b" (dois literais de texto) sai do E0306 enganoso para o E0307 (D-IA1-8); o E0307 da soma de texto leva a sugestao (SemSugerirConcatenar).
 *            0.18.0- Linguagem v1.12 M3 P21v-a: CHECAGENS[EX_CHAMADA_IND + 1] (ExChamadaInd); 'as proc(...)' (E0910, ConversaoParaProc) e 'f as rawptr'; ExNome passa o nome de proc por SemProcComoCallback em qualquer posicao.
 *            0.17.0 - Linguagem v1.11 L15a: ExCampo recusa campo em [dynamic, N]T e ^[dynamic, N]T (E0335).
 *            0.16.0 - Linguagem v1.11 L14a: quat (sem_quat.c) em SemAritmetica, UnarioNegar, ContextoDoLiteral (f32 ao lado de quat) e ComparacaoRara (E0333).
 *            0.15.0 -Linguagem v1.11 L12b: ExCampo manda v.x, v.xy, c.rgb em [N]T de numero para SemSwizzle (sem_vetor.c); texto do E0308 de campo em array.
 *            0.14.0 - Linguagem v1.11 L12a: conta com [N]T (SemContaVetor, sem_vetor.c) em SemAritmetica (publica); -a em [N]T (SemNegarVetor); ContextoDoLiteral devolve o elemento ao lado de [N]T; bits em colecao = E0331.
 *            0.13.0 -Linguagem v1.11 L10a: ExNome marca LOCAL_LIDO e UnarioEndereco marca LOCAL_ALTERADO no nome (higiene, sem_higiene.c).
 *            0.12.0 -Linguagem v1.11 L8: SemChecar liga o toque no veneno quando o no sai com TIPO_VENENO; ExNome e ExMembro dao TIPO_VENENO a nome/enum envenenado.
 *            0.11.0- Linguagem v1.11 L5: E0201 de nome, de membro de enum e de campo de struct com candidatos "parecido" (sem_parecido.c).
 *            0.10.0 - Linguagem v1.11 L1: ExLiteral e UnarioNegar entregam o literal inteiro em contexto real a SemLiteralInteiroEmReal (K2/A15, E0330).
 *            0.9.0 (P7) - UnarioEndereco: &a[i] de soa = E0320 (atras de Tipos.tem_soa).
 *            0.9.0 (P5) - UnarioNegar aceita matrix (-m) e ContextoDoLiteral devolve o elemento ao lado de uma matrix (atras de Tipos.tem_matriz).
 *            0.9.0 - Linguagem v1.9 P3: ExCampo (CampoForaDeStruct), AplicarAritmetica/AplicarBits (SemContaV19) e ComparacaoRara (SemComparacaoV19) despacham os tipos novos; textos que prometiam a v1.9 passam a "depois da v1.9".
 *            0.8.0 - Linguagem v1.8b P5a: ExNome devolve SemProcComoCallback para nome de proc (antes E0201).
 *            0.7.0 - Linguagem v1.8b P3a: E0908 no deref de ^Opaco e no campo de struct opaca.
 *            0.6.0 - Linguagem v1.8 P3: ExNome e ExMembro conferem a visibilidade entre modulos (SemVisivel); 'apelido.nome' nunca e local.
 *            0.5.5- Linguagem v1.7 P5b: '&x' de let, parametro, item de for ou ligacao = E0705 (D21, UnarioEndereco).
 *            0.5.4 - Linguagem v1.7 P5a: f"..." fora de print/format/format_c = E0315 (ExFTexto); saiu o provisorio.
 *            0.5.3 - Linguagem v1.7 P4: EX_TUPLA fora do return = E0314 (ExTupla); saiu o provisorio.
 *            0.5.2 - Linguagem v1.7 P3: union em campo, conta e == recebe E0313 (so caminhos de erro).
 *            0.5.1- Linguagem v1.7 P2: ?T (ExNone, literais/unario/binario desembrulham o contexto, E0312 em campo, conta e ==), ExNome grava a natureza em op (D21), EX_EMBRULHA/EX_OPC_TESTE na tabela, SemExprCom coage.
 *            0.5.0 - Linguagem v1.7 P1: CHECAGENS[EX_FTEXTO + 1]; EX_EMBRULHA..EX_FTEXTO recusados com E0101 "chega no P2/P4/P5a" (provisorio).
 *            0.4.0 - Linguagem v1.6 P4: causa propria de lista (E0308) em ==, conta, bits e campo; so caminhos de erro.
 *            0.3.1 - Linguagem v1.6 P3a: CHECAGENS[EX_TIPO] chama SemTipoSolto (E0310); saiu o provisorio E0101.
 *            0.3.0 - Linguagem v1.6 P1a: CHECAGENS[EX_TIPO + 1]; EX_TIPO recusado com E0101 "chega no P3a" (provisorio).
 *            0.2.5 - Linguagem v1.5 P3a: indexar e literal de array (SemIndice, SemLiteralArray); E0308 em ==, conta e campo de array/fatia.
 *            0.2.4 - Linguagem v1.5 P1a: CHECAGENS[EX_LITERAL_ARRAY + 1]; indice, fatia e literal de array recusados com E0101 "chega no P3" (provisorio).
 *            0.2.3 - Linguagem v1.3 P6: texto: literal str/cstr pelo contexto, == e != em str, E0307 (conta, ordem, cstr ==, str as cstr), cstr as str.
 *            0.2.2- Linguagem v1.3: ponteiros na semantica: none, '&' de qualquer variavel, 'p^', campo de ?^T/^^T, comparacao e 'as' de ponteiro, E0306 em conta/bits/ordem.
 *            0.2.1 - Linguagem v1.3: CHECAGENS[EX_NONE + 1]; 'as' resolve o TipoRef do alvo (Expr.alvo).
 *            0.2.0 - Linguagem v1.2: E0420 por largura no literal (caminho rapido por digitos), '-literal', '-' em sem sinal (E0305),
 *                    f64, 'as' entre todos os numericos (literal sob 'as' conferido), bits & | ~ << >> so em sem sinal (E0303).
 *            0.1.0 - Fase 5c: regras do prototipo (benchmark/c/check_expr.c) sobre TipoId; enum (== e !=), ordem so em numerico,
 *                    texto fora de comparacao; flexivel marcado uma vez por raiz (SemMarcarFlexivel).
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_interno.h"

typedef bool (*ChecaExpr)(Semantica *s, Expr *e, TipoId ctx);

/* ---------- flexivel ---------- */

static bool EhAritmetico(uint8_t op) { return op >= OP_SOMA && op <= OP_RESTO; }
static bool EhBits(uint8_t op) { return op >= OP_BIT_E && op <= OP_DESLOCA_DIR; } /* binarios; o unario e OP_BIT_NAO */

static bool NoFlexivel(const Expr *e)
{
    if (e->tipo_no == EX_INT || e->tipo_no == EX_REAL) return true;
    if (e->tipo_no == EX_UNARIO) return (e->op == OP_NEGAR || e->op == OP_BIT_NAO) && e->a->flexivel;
    if (e->tipo_no != EX_BINARIO) return false;
    return (EhAritmetico(e->op) || EhBits(e->op)) && e->a->flexivel && e->b->flexivel;
}

void SemMarcarFlexivel(Expr *e)
{
    if (e->a != NULL) SemMarcarFlexivel(e->a);
    if (e->b != NULL) SemMarcarFlexivel(e->b);
    for (uint32_t i = 0; i < e->nargs; i++) SemMarcarFlexivel(e->args[i].valor);
    e->flexivel = NoFlexivel(e);
}

/* ---------- folhas ---------- */

/* Digitos decimais que SEMPRE cabem no tipo (99 em u8/i8, 9999 em 16 bits, ...): literal com tantos digitos ou menos nao le a fonte. */
static const uint8_t DIGITOS_SEGUROS[TIPO_FIXOS_QTD] = {
    [TIPO_I8] = 2, [TIPO_U8] = 2, [TIPO_I16] = 4, [TIPO_U16] = 4, [TIPO_I32] = 9, [TIPO_U32] = 9, [TIPO_INT] = 18, [TIPO_UINT] = 19,
};

/* E0420 por largura: o literal (negado se 'negativo') cabe no tipo? 'pos' = onde o erro aponta (o literal, ou o '-'). */
static bool ConferirLiteralInteiro(Semantica *s, const Expr *literal, TipoId tipo, bool negativo, uint32_t pos)
{
    uint64_t mag;
    if (literal->op <= DIGITOS_SEGUROS[tipo] && !(negativo && TipoEhSemSinal(tipo))) return true;
    if (!TipoLerLiteralU64(s->fonte->texto + literal->pos_op, &mag)) {
        return SemErro(s, E0420, pos, "o literal passa de 64 bits (maximo 18446744073709551615)");
    }
    if (TipoInteiroCabe(tipo, negativo, mag)) return true;
    return SemErro(s, E0420, pos, "o %s %s%llu nao cabe em %s (%lld..=%llu)", negativo ? "valor" : "literal", negativo ? "-" : "",
                   (unsigned long long)mag, TipoNomeFixo(tipo), (long long)TipoInteiroMin(tipo),
                   (unsigned long long)TipoInteiroMax(tipo));
}

static bool ExLiteral(Semantica *s, Expr *e, TipoId ctx)
{
    ctx = SemCtxSemOpcional(s, ctx);
    if (e->tipo_no == EX_INT) {
        if (TipoEhReal(&s->tipos, ctx)) return SemLiteralInteiroEmReal(s, e, ctx); /* v1.11: 1 vira 1.0 quando exato */
        e->tipo = TipoEhInteiro(ctx) ? ctx : TIPO_INT;
        return ConferirLiteralInteiro(s, e, e->tipo, false, e->pos);
    }
    e->tipo = ctx == TIPO_F64 || TipoClasseDe(&s->tipos, ctx) == TC_REAL ? ctx : TIPO_F32;
    return true;
}

static bool ExBool(Semantica *s, Expr *e, TipoId ctx)
{
    (void)s;
    (void)ctx;
    e->tipo = TIPO_BOOL;
    return true;
}

static bool ExTexto(Semantica *s, Expr *e, TipoId ctx)
{
    ctx = SemCtxSemOpcional(s, ctx);
    e->tipo = ctx == TIPO_CSTR ? TIPO_CSTR : TIPO_TEXTO; /* o literal vira o que o contexto pede */
    return true;
}

/* 'none' so existe com contexto ?T, ?^T ou rawptr. */
static bool ExNone(Semantica *s, Expr *e, TipoId ctx)
{
    TipoClasse c = TipoClasseDe(&s->tipos, ctx);
    if (c == TC_ANULAVEL || c == TC_RAWPTR || c == TC_OPCIONAL) {
        e->tipo = ctx;
        return true;
    }
    if (c == TC_PONTEIRO) return SemErro(s, E0306, e->pos, "'^T' nunca e none; declare '?^T' para aceitar none");
    return SemErro(s, E0306, e->pos, "none precisa de um tipo ?T, ?^T ou rawptr pelo contexto (ex.: let a: ?i32 = none)");
}

static bool ExNome(Semantica *s, Expr *e, TipoId ctx)
{
    LocalInfo *local = e->modulo != 0 ? NULL : EscopoBuscarMut(&s->escopo, e->nome); /* v1.8: 'apelido.nome' nunca e local */
    if (local != NULL) {
        if (__builtin_expect(s->tipo_allocator != 0 && local->tipo == s->tipo_allocator && !s->alocador_lendo, 0)) return SemAllocatorValorSolto(s, e); /* B01c */
        local->usos |= LOCAL_LIDO; /* v1.11 L10a: a higiene (sem_higiene.c) desfaz esta marca na atribuicao simples */
        e->ref = local->natureza == LOC_PARAM ? REF_PARAM : REF_LOCAL;
        e->op = local->natureza; /* D21: a natureza do local (EX_NOME nao usa 'op') */
        e->ref_id = local->id;
        e->tipo = local->tipo;
        return true;
    }
    const Simbolo *y = SemAcharSimbolo(s, e->nome, e->modulo);
    NomeUsado uso = {e->nome, e->modulo != 0 ? e->pos_op : e->pos, e->modulo};
    if (!SemVisivel(s, y, &uso)) return false;
    if (__builtin_expect(SemSimboloVeneno(y), 0)) { /* L8: const (ou proc) com erro; o erro ja foi relatado la */
        e->tipo = TIPO_VENENO;
        return true;
    }
    if (y->categoria == SIM_CONST) {
        e->ref = REF_CONST;
        e->ref_id = y->indice;
        e->tipo = y->tipo;
        return true;
    }
    if (y->categoria == SIM_PROC) return SemProcComoCallback(s, e, y, ctx); /* v1.8b: caminho que era erro */
    if (y->categoria == SIM_NENHUM && e->modulo == 0 && SemNomeJaRelatado(s, e->nome)) return SemErroCalado(s, e->pos_op); /* b15: cascata do mesmo nome */
    if (y->categoria == SIM_NENHUM && e->modulo == 0 && SemIdiomaDeNome(s, e)) return false; /* IA1-L2: nil, True, console.log... = E0110 */
    SemParecido sp;
    SemParecidoIniciar(s, E0201, e->pos_op, e->nome, &sp);
    SemParecidosDeNome(s, e->modulo, &sp);
    return SemErroParecido(s, &sp, "nome '%s' nao declarado como variavel ou constante", SemNomeTexto(s, e->nome));
}

/* ---------- campo e membro de enum ---------- */

static bool NomeEhEnum(const Semantica *s, const Expr *nome)
{
    return nome->tipo_no == EX_NOME && (nome->modulo != 0 || EscopoBuscar(&s->escopo, nome->nome) == NULL) &&
           SemAcharSimbolo(s, nome->nome, nome->modulo)->categoria == SIM_ENUM;
}

static bool ExMembro(Semantica *s, Expr *e)
{
    Expr *base = e->a;
    const Simbolo *y = SemAcharSimbolo(s, base->nome, base->modulo);
    NomeUsado uso = {base->nome, base->modulo != 0 ? base->pos_op : base->pos, base->modulo};
    if (!SemVisivel(s, y, &uso)) return false;
    if (__builtin_expect(SemSimboloVeneno(y), 0)) { /* L8: enum com erro: os membros dele nao sao conferidos */
        base->tipo = e->tipo = TIPO_VENENO;
        return true;
    }
    int32_t idx = SemAcharMembro(s, y->decl, e->nome, e->pos_op);
    if (idx < 0) return SemMembroAusente(s, e, y->decl, idx);
    base->ref = REF_ENUM;
    base->ref_id = y->indice;
    base->tipo = y->tipo;
    e->ref = REF_MEMBRO;
    e->ref_id = idx;
    e->tipo = y->tipo;
    return true;
}

/* [N]T, []T ou ^[N]T: so indexa e fatia (nao tem campo, nem ==, nem conta). */
static bool EhColecao(const Semantica *s, TipoId t)
{
    ColecaoForma forma;
    SemElementoDe(s, t, &forma);
    return forma != FORMA_NENHUMA;
}

/* [dynamic]T ou ^[dynamic]T: so no caminho de erro, para dar a causa propria da lista. */
static bool EhLista(const Semantica *s, TipoId t)
{
    ColecaoForma forma;
    SemElementoDe(s, t, &forma);
    return forma == FORMA_DINAMICO || forma == FORMA_PONTEIRO_DINAMICO;
}

/* Campo pedido onde nao ha struct: '?^T' (pode ser none) e '^^T' (o '.' desreferencia um nivel so) ganham causa propria. */
static bool ErroCampoSemStruct(Semantica *s, const Expr *e)
{
    const TipoInfo *k = TipoDe(&s->tipos, e->a->tipo);
    char apontado[96];
    if (EhLista(s, e->a->tipo)) return SemErro(s, E0308, e->pos_op, "lista nao tem campo: use l[0] e len(l)");
    if (EhColecao(s, e->a->tipo)) {
        return SemErro(s, E0308, e->pos_op, "array nao tem campo: use a[0] (.x .y .z .w so em [2]T, [3]T e [4]T de numero; matrix[R, C]T usa m[i, j])");
    }
    if (k->classe == TC_ANULAVEL) {
        SemDescrever(s, k->a, apontado, sizeof apontado);
        return SemErro(s, E0306, e->pos_op, "campo de '?^%s' pode estar em none: converta com 'p as ^%s' antes", apontado, apontado);
    }
    if (k->classe == TC_OPCIONAL) {
        SemDescrever(s, e->a->tipo, apontado, sizeof apontado);
        return SemErro(s, E0312, e->pos_op, "campo de '%s' pode estar em none: desembrulhe com switch v in x { case T: ... case none: ... } ou x or_else padrao", apontado);
    }
    if (k->classe == TC_UNIAO) {
        return SemErro(s, E0313, e->pos_op, "union nao tem campo: leia com switch v in x { case Variante: v.%s }", SemNomeTexto(s, e->nome));
    }
    if (k->classe == TC_PONTEIRO && TipoClasseDe(&s->tipos, k->a) == TC_PONTEIRO) {
        return SemErro(s, E0306, e->pos_op, "'.' desreferencia um nivel so; escreva 'pp^.%s'", SemNomeTexto(s, e->nome));
    }
    return SemErro(s, E0201, e->pos_op, "campo '%s' pedido em valor que nao e struct", SemNomeTexto(s, e->nome));
}

/* Campo pedido onde nao ha struct: tipo da v1.9 (soa, matrix, map, bit_set) tem causa propria; o resto cai no erro de sempre. */
static bool CampoForaDeStruct(Semantica *s, Expr *e)
{
    if (__builtin_expect(TiposTemV19(&s->tipos), 0) && SemEhTipoV19(s, e->a->tipo)) return SemCampoV19(s, e);
    return SemIdiomaDeCampo(s, e) ? false : ErroCampoSemStruct(s, e); /* IA1-L2: x.len(), x.push(v) = E0110 */
}

static bool ExCampo(Semantica *s, Expr *e, TipoId ctx)
{
    (void)ctx;
    if (NomeEhEnum(s, e->a)) return ExMembro(s, e);
    s->alocador_lendo = e->a->tipo_no == EX_NOME; /* B01c: alloc.id diz a causa propria (SemAllocatorLerCampo), nao a de valor solto */
    bool base_ok = SemChecar(s, e->a, TIPO_VAZIO);
    s->alocador_lendo = false;
    if (!base_ok) return false;
    if (SemEhListaFixaOuPonteiro(s, e->a->tipo)) {
        return SemIdiomaDeCampo(s, e) ? false : SemErroDeListaFixa(s, e->pos_op, "[dynamic, N]T nao tem campo: use l[i], len(l), l[..] e append(&l, item = x)"); /* IA1-L2 */
    }
    if (__builtin_expect(SemEhAllocator(s, e->a->tipo), 0)) return SemAllocatorLerCampo(s, e); /* B01a: alloc.id */
    const Decl *estrutura = SemDeclDaStruct(s, e->a->tipo);
    if (estrutura == NULL) return SemEhSwizzle(s, e) ? SemSwizzle(s, e) : CampoForaDeStruct(s, e); /* v1.11 L12b: v.x, v.xy, c.rgb */
    int32_t idx = SemAcharCampo(estrutura, e->nome);
    if (idx < 0) {
        if (estrutura->externo) {
            return SemErro(s, E0908, e->pos_op, "o tipo opaco '%s' nao tem campos visiveis: use as funcoes da biblioteca C",
                           SemNomeTexto(s, estrutura->nome));
        }
        SemParecido sp;
        SemParecidoIniciar(s, E0201, e->pos_op, e->nome, &sp);
        SemParecidosDeCampos(s, estrutura, &sp);
        return SemErroParecido(s, &sp, "a struct '%s' nao tem o campo '%s'", SemNomeTexto(s, estrutura->nome), SemNomeTexto(s, e->nome));
    }
    e->ref = REF_CAMPO;
    e->ref_id = idx;
    e->tipo = estrutura->campos[idx].tipo.tipo;
    return true;
}

/* ---------- unarios ---------- */

static bool UnarioNao(Semantica *s, Expr *e)
{
    if (!SemChecar(s, e->a, TIPO_BOOL)) return false;
    if (e->a->tipo != TIPO_BOOL) return SemErro(s, E0301, e->pos, "'not' pede bool");
    e->tipo = TIPO_BOOL;
    return true;
}

static bool UnarioEndereco(Semantica *s, Expr *e)
{
    Expr *a = e->a;
    if (a->tipo_no == EX_CAMPO || a->tipo_no == EX_INDICE) { /* cadeia: o elemento ou campo precisa ser alteravel (D7) */
        if (!SemChecar(s, a, TIPO_VAZIO)) return false;
        if (__builtin_expect(s->tipos.tem_soa, 0) && a->tipo_no == EX_INDICE && TipoClasseDe(&s->tipos, a->a->tipo) == TC_SOA) {
            return SemErro(s, E0320, e->pos, "&a[i] nao existe em soa: a struct nao fica inteira na memoria; use &a[i].campo");
        }
        if (!SemLvalorMutavelDe(s, a, "'&' de campo ou elemento permite altera-lo: declare com var")) {
            return false;
        }
        e->tipo = TipoPonteiro(&s->tipos, a->tipo);
        return true;
    }
    if (a->tipo_no != EX_NOME) return SemErro(s, E0301, e->pos, "'&' vale sobre variavel, campo ou elemento (a[i])");
    if (!SemChecar(s, a, TIPO_VAZIO)) return false;
    if (a->ref != REF_LOCAL && a->ref != REF_PARAM) return SemErro(s, E0301, e->pos, "'&' so vale sobre o nome de uma variavel");
    if (a->op != LOC_VAR) { /* D21: so var tem endereco alteravel (a natureza vem de ExNome) */
        return SemErro(s, E0705, a->pos, "'%s' e imutavel: '&' permitiria alterar o valor; declare com var", SemNomeTexto(s, a->nome));
    }
    SemHigieneAlterar(s, a);
    e->tipo = TipoPonteiro(&s->tipos, a->tipo);
    return true;
}

/* 'p^': so ^T se desreferencia; as outras formas de ponteiro dizem o que fazer. */
static bool UnarioDeref(Semantica *s, Expr *e)
{
    char apontado[96], tipo[96];
    if (!SemChecar(s, e->a, TIPO_VAZIO)) return false;
    const TipoInfo *k = TipoDe(&s->tipos, e->a->tipo);
    switch (k->classe) {
    case TC_PONTEIRO:
        if (__builtin_expect(s->tem_opaco, 0) && SemTipoEhOpaco(s, k->a)) {
            SemDescrever(s, k->a, apontado, sizeof apontado);
            return SemErro(s, E0908, e->pos_op, "nao da para ler o valor de um tipo opaco ('%s'): passe o ponteiro ^%s para as funcoes da biblioteca C",
                           apontado, apontado);
        }
        e->tipo = k->a;
        return true;
    case TC_ANULAVEL:
        SemDescrever(s, k->a, apontado, sizeof apontado);
        return SemErro(s, E0306, e->pos_op, "'?^%s' pode ser none: teste 'p != none' e converta com 'p as ^%s' antes de ler", apontado, apontado);
    case TC_RAWPTR:
        return SemErro(s, E0306, e->pos_op, "rawptr nao tem tipo apontado: converta com 'p as ^T' antes de ler");
    case TC_CSTR:
        return SemErro(s, E0306, e->pos_op, "cstr nao se desreferencia; converta para str com 'c as str'");
    default:
        SemDescrever(s, e->a->tipo, tipo, sizeof tipo);
        return SemErro(s, E0306, e->pos_op, "'^' depois de um valor que nao e ponteiro (tipo %s)", tipo);
    }
}

static bool UnarioNegar(Semantica *s, Expr *e, TipoId ctx)
{
    Expr *a = e->a;
    if (a->tipo_no == EX_INT && TipoEhReal(&s->tipos, ctx)) { /* v1.11: '-1' em contexto real; o sinal nao muda a exatidao */
        e->tipo = ctx;
        return SemLiteralInteiroEmReal(s, a, ctx);
    }
    if (a->tipo_no == EX_INT) { /* '-literal' e conferido inteiro: -128 cabe em i8 mesmo que 128 nao caiba */
        TipoId tipo = TipoEhInteiro(ctx) ? ctx : TIPO_INT;
        a->tipo = e->tipo = tipo;
        return ConferirLiteralInteiro(s, a, tipo, true, e->pos);
    }
    if (!SemChecar(s, a, ctx)) return false;
    if (!TipoEhNumerico(&s->tipos, a->tipo)) {
        if (TipoClasseDe(&s->tipos, a->tipo) == TC_MATRIZ) { /* -m (v1.9) */
            e->tipo = a->tipo;
            return true;
        }
        if (TipoClasseDe(&s->tipos, a->tipo) == TC_ARRAY) return SemNegarVetor(s, e); /* -a em [N]T (v1.11) */
        if (SemEhQuat(s, a->tipo)) { /* -q (v1.11 L14a) */
            e->tipo = a->tipo;
            return true;
        }
        return SemErro(s, E0301, e->pos, "'-' pede numero");
    }
    if (TipoEhSemSinal(a->tipo)) return SemErro(s, E0305, e->pos, "'-' em inteiro sem sinal (%s)", TipoNomeFixo(a->tipo));
    e->tipo = a->tipo;
    return true;
}

/* '~a': complemento de bits, so em inteiro sem sinal. */
static bool UnarioBitNao(Semantica *s, Expr *e, TipoId ctx)
{
    if (!SemChecar(s, e->a, ctx)) return false;
    if (!TipoEhSemSinal(e->a->tipo)) return SemErro(s, E0303, e->pos, "'~' em tipo que nao e inteiro sem sinal");
    e->tipo = e->a->tipo;
    return true;
}

static bool ExUnario(Semantica *s, Expr *e, TipoId ctx)
{
    ctx = SemCtxSemOpcional(s, ctx);
    if (e->op == OP_NAO) return UnarioNao(s, e);
    if (e->op == OP_ENDERECO) return UnarioEndereco(s, e);
    if (e->op == OP_DEREF) return UnarioDeref(s, e);
    if (e->op == OP_BIT_NAO) return UnarioBitNao(s, e, ctx);
    return UnarioNegar(s, e, ctx);
}

/* 'literal as inteiro' (ou '-literal as inteiro'): o literal precisa caber no destino, senao E0420 na compilacao. */
static bool ConferirLiteralSobAs(Semantica *s, const Expr *e, TipoId alvo)
{
    const Expr *literal = e->a;
    bool negativo = literal->tipo_no == EX_UNARIO && literal->op == OP_NEGAR && literal->a->tipo_no == EX_INT;
    if (negativo) literal = literal->a;
    if (literal->tipo_no != EX_INT || !TipoEhInteiro(alvo)) return true;
    return ConferirLiteralInteiro(s, literal, alvo, negativo, negativo ? e->a->pos : literal->pos);
}

static bool EhPonteiroOuAnulavel(TipoClasse c) { return c == TC_PONTEIRO || c == TC_ANULAVEL; }

/* v1.12: 'x as proc(...) -> R' so de rawptr, ^T, ?^T (endereco que veio do C) ou de proc da mesma assinatura (E0910). */
static bool ConversaoParaProc(Semantica *s, const Expr *e, TipoId origem, TipoId alvo)
{
    TipoClasse co = TipoClasseDe(&s->tipos, origem);
    if (co == TC_RAWPTR || EhPonteiroOuAnulavel(co) || TipoProcCompativel(&s->tipos, origem, alvo)) return true;
    return SemErro(s, E0910, e->pos_op, "'as proc(...)' so converte de rawptr, ^T, ?^T (endereco de funcao vindo do C) ou de proc com a mesma assinatura");
}

/* 'as' fora do numerico (tabela 3.5, parte de ponteiro): mesmo tipo, ^T/?^T/cstr -> rawptr, rawptr -> ^T/?^T/cstr,
 * ^T <-> ?^T do mesmo apontado. Erros em pos_op (o 1o token do tipo alvo). */
static bool ConversaoNaoNumerica(Semantica *s, const Expr *e, TipoId origem, TipoId alvo)
{
    const Tipos *t = &s->tipos;
    TipoClasse co = TipoClasseDe(t, origem), ca = TipoClasseDe(t, alvo);
    if (origem == alvo) return true;
    if (co == TC_CSTR && alvo == TIPO_TEXTO) return true;
    if (origem == TIPO_TEXTO && ca == TC_CSTR) {
        return SemErro(s, E0307, e->pos_op,
                       "str nao termina em NUL: um cstr a partir de str precisa alocar e copiar (format_c, v1.6); para literal escreva let c: cstr = \"...\"");
    }
    if ((TipoEhFamiliaPonteiro(t, origem) && TipoEhNumerico(t, alvo)) ||
        (TipoEhFamiliaPonteiro(t, alvo) && TipoEhNumerico(t, origem))) {
        return SemErro(s, E0306, e->pos_op, "ponteiro nao vira numero nem numero vira ponteiro");
    }
    if (ca == TC_PROC) return ConversaoParaProc(s, e, origem, alvo);
    if (co == TC_PROC && ca == TC_RAWPTR) return true; /* v1.12: 'f as rawptr' */
    if ((co == TC_PONTEIRO || co == TC_ANULAVEL || co == TC_CSTR) && ca == TC_RAWPTR) return true;
    if (co == TC_RAWPTR && (ca == TC_PONTEIRO || ca == TC_ANULAVEL || ca == TC_CSTR)) return true;
    if (EhPonteiroOuAnulavel(co) && EhPonteiroOuAnulavel(ca)) {
        if (TipoDe(t, origem)->a == TipoDe(t, alvo)->a) return true;
        return SemErro(s, E0306, e->pos_op,
                       "'as' nao troca o tipo apontado direto; passe por rawptr: (p as rawptr) as ^B");
    }
    return SemErro(s, E0301, e->pos_op,
                   "'as' converte entre numericos, entre ponteiros (^T, ?^T, rawptr, cstr) e de cstr para str");
}

static bool ExConversao(Semantica *s, Expr *e, TipoId ctx)
{
    TipoId alvo;
    (void)ctx;
    if (!SemChecar(s, e->a, TIPO_VAZIO) || !SemResolverTipoRef(s, e->alvo)) return false;
    alvo = e->alvo->tipo;
    if (!TipoEhNumerico(&s->tipos, e->a->tipo) || !TipoEhNumerico(&s->tipos, alvo)) {
        if (!ConversaoNaoNumerica(s, e, e->a->tipo, alvo)) return false;
        e->tipo = alvo;
        return true;
    }
    if (!ConferirLiteralSobAs(s, e, alvo)) return false;
    e->tipo = alvo;
    return true;
}

/* ---------- binarios ---------- */

static bool EhLogico(uint8_t op) { return op == OP_E || op == OP_OU; }
static bool EhComparacao(uint8_t op) { return op >= OP_IGUAL && op <= OP_MAIOR_IGUAL; }

/* Contexto de um literal ao lado de 'outro': em REAL x REAL (* e /) o literal e adimensional. */
static TipoId ContextoDoLiteral(const Semantica *s, TipoId outro, uint8_t op)
{
    if (__builtin_expect(s->tipos.tem_matriz, 0) && TipoClasseDe(&s->tipos, outro) == TC_MATRIZ) {
        return (TipoId)TipoDe(&s->tipos, outro)->a; /* v1.9: o literal ao lado de uma matriz e escalar do elemento */
    }
    if (SemEhQuat(s, outro)) return TIPO_F32; /* v1.11 L14a: ao lado de quat o literal e f32 (q * 2.0) */
    if (TipoClasseDe(&s->tipos, outro) == TC_ARRAY) return ContextoDoLiteral(s, (TipoId)TipoDe(&s->tipos, outro)->a, op); /* v1.11: ao lado de [N]T o literal e escalar do elemento */
    bool produto = op == OP_MUL || op == OP_DIV;
    return TipoClasseDe(&s->tipos, outro) == TC_REAL && produto ? TIPO_F32 : outro;
}

static bool OperandosDoBinario(Semantica *s, Expr *e, TipoId ctx)
{
    Expr *a = e->a, *b = e->b;
    TipoId ctx_operando = EhAritmetico(e->op) || EhBits(e->op) ? ctx : TIPO_VAZIO;
    if (EhLogico(e->op)) return SemChecar(s, a, TIPO_BOOL) && SemChecar(s, b, TIPO_BOOL);
    if (a->tipo_no == EX_NONE && b->tipo_no != EX_NONE) { /* none toma o tipo do outro lado como contexto */
        return SemChecar(s, b, ctx_operando) && SemChecar(s, a, b->tipo);
    }
    if (a->flexivel && !b->flexivel) {
        return SemChecar(s, b, ctx_operando) && SemChecar(s, a, ContextoDoLiteral(s, b->tipo, e->op));
    }
    if (!SemChecar(s, a, ctx_operando)) return false;
    if (b->tipo_no == EX_NONE) return SemChecar(s, b, a->tipo);
    return SemChecar(s, b, b->flexivel ? ContextoDoLiteral(s, a->tipo, e->op) : ctx_operando);
}

static bool AplicarLogica(Semantica *s, Expr *e)
{
    if (e->a->tipo != TIPO_BOOL || e->b->tipo != TIPO_BOOL) {
        return SemErro(s, E0301, e->pos_op, "'and' e 'or' pedem operandos bool");
    }
    e->tipo = TIPO_BOOL;
    return true;
}

/* Classes que aceitam == e !=; a ordem (< <= > >=) exige numerico. */
static bool ClasseComparavel(TipoClasse c)
{
    return c == TC_BOOL || c == TC_I32 || c == TC_INT || c == TC_REAL || c == TC_ENUM || (c >= TC_I8 && c <= TC_F64) ||
           c == TC_PONTEIRO || c == TC_ANULAVEL || c == TC_RAWPTR || c == TC_TEXTO;
}

static bool EhOrdem(uint8_t op) { return op >= OP_MENOR && op <= OP_MAIOR_IGUAL; }

/* Mesma classe e comparavel; enum, ^T e ?^T so com o mesmo tipo e so em == e !=; ^T x ?^T do mesmo apontado vale (D7). */
static bool ComparacaoValida(const Semantica *s, const Expr *e)
{
    TipoClasse ca = TipoClasseDe(&s->tipos, e->a->tipo), cb = TipoClasseDe(&s->tipos, e->b->tipo);
    if (ca != cb) {
        return EhPonteiroOuAnulavel(ca) && EhPonteiroOuAnulavel(cb) && !EhOrdem(e->op) &&
               TipoDe(&s->tipos, e->a->tipo)->a == TipoDe(&s->tipos, e->b->tipo)->a;
    }
    if (!ClasseComparavel(ca)) return false;
    if (ca == TC_ENUM || ca == TC_PONTEIRO || ca == TC_ANULAVEL) return e->a->tipo == e->b->tipo && !EhOrdem(e->op);
    return (ca != TC_RAWPTR && ca != TC_TEXTO) || !EhOrdem(e->op);
}

/* IA1-b07: E0301 de conta com dois lados de tipos que nao se misturam; a forma da ocorrencia diz a conta, os tipos e a linha corrigida. */
static bool ErroDeOperandos(Semantica *s, const Expr *e, const char *causa)
{
    SemErro(s, E0301, e->pos_op, "%s", causa);
    SemFormaOperandos(s, e);
    return false;
}

static bool ErroDeComparacao(Semantica *s, const Expr *e)
{
    if (EhLista(s, e->a->tipo) || EhLista(s, e->b->tipo)) {
        return SemErro(s, E0308, e->pos_op, "lista nao tem ==, != nem ordem nem conta: compare elemento a elemento num for");
    }
    if (EhColecao(s, e->a->tipo) || EhColecao(s, e->b->tipo)) {
        return SemErro(s, E0308, e->pos_op, "array e fatia nao tem ==, != nem ordem: compare elemento a elemento num for");
    }
    if (EhOrdem(e->op) && (TipoEhFamiliaPonteiro(&s->tipos, e->a->tipo) || TipoEhFamiliaPonteiro(&s->tipos, e->b->tipo))) {
        return SemErro(s, E0306, e->pos_op, "ponteiro nao tem ordem; so == e !=");
    }
    if (EhOrdem(e->op) && (e->a->tipo == TIPO_TEXTO || e->b->tipo == TIPO_TEXTO)) {
        return SemErro(s, E0307, e->pos_op, "str nao tem ordem nesta versao; so == e !=");
    }
    if (e->a->tipo == TIPO_CSTR || e->b->tipo == TIPO_CSTR) {
        return SemErro(s, E0307, e->pos_op,
                       "== em cstr compararia enderecos: compare como str, (a as str) == (b as str)");
    }
    return ErroDeOperandos(s, e, "comparacao entre tipos incompativeis");
}

/* ?T so se compara com none (D5): reescreve o binario para EX_OPC_TESTE (op 1 = '== none', 0 = '!= none'). Caminho de erro da comparacao. */
static bool ComparacaoComNone(Semantica *s, Expr *e)
{
    bool com_none = e->b->tipo_no == EX_NONE || e->a->tipo_no == EX_NONE;
    if (!com_none || (e->op != OP_IGUAL && e->op != OP_DIFERENTE)) {
        return SemErro(s, E0312, e->pos_op, "?T so se compara com none (x == none, x != none); para o valor use switch ou or_else");
    }
    Expr *valor = e->b->tipo_no == EX_NONE ? e->a : e->b;
    e->op = e->op == OP_IGUAL ? 1 : 0;
    e->tipo_no = EX_OPC_TESTE;
    e->a = valor;
    e->b = NULL;
    e->tipo = TIPO_BOOL;
    return true;
}

/* Comparacao que ComparacaoValida recusou: ?T (D5) tem causa propria; o resto cai no erro de sempre. */
static bool ComparacaoRara(Semantica *s, Expr *e)
{
    TipoClasse ca = TipoClasseDe(&s->tipos, e->a->tipo), cb = TipoClasseDe(&s->tipos, e->b->tipo);
    if (SemEhQuat(s, e->a->tipo) || SemEhQuat(s, e->b->tipo)) return SemComparacaoQuat(s, e); /* v1.11 L14a */
    if (ca >= TC_MAPA || cb >= TC_MAPA) return SemComparacaoV19(s, e);
    if (ca == TC_OPCIONAL || cb == TC_OPCIONAL) return ComparacaoComNone(s, e);
    if (ca == TC_UNIAO || cb == TC_UNIAO) {
        return SemErro(s, E0313, e->pos_op, "union nao tem ==: compare a variante com switch");
    }
    return ErroDeComparacao(s, e);
}

static bool AplicarComparacao(Semantica *s, Expr *e)
{
    TipoId ta = e->a->tipo, tb = e->b->tipo;
    if (!ComparacaoValida(s, e)) return ComparacaoRara(s, e);
    if (TipoClasseDe(&s->tipos, ta) == TC_REAL && ta != tb) {
        return SemErro(s, E0412, e->pos_op, "comparacao entre dimensoes diferentes");
    }
    if (EhOrdem(e->op) && !TipoEhNumerico(&s->tipos, ta)) {
        return SemErro(s, E0301, e->pos_op, "a ordem (<, <=, >, >=) pede numero");
    }
    e->tipo = TIPO_BOOL;
    return true;
}

static bool AritmeticaReal(Semantica *s, Expr *e, TipoId ta, TipoId tb)
{
    if (e->op == OP_RESTO) return SemErro(s, E0301, e->pos_op, "'%%' so vale para inteiros");
    if (e->op == OP_SOMA || e->op == OP_SUB) {
        if (ta != tb) return SemErro(s, E0412, e->pos_op, "soma ou subtracao entre dimensoes diferentes");
        e->tipo = ta;
        return true;
    }
    DimId dim = DimCombinar(&s->dims, TipoDe(&s->tipos, ta)->a, TipoDe(&s->tipos, tb)->a, e->op == OP_MUL ? 1 : -1);
    if (!DimDeclarada(&s->dims, dim)) {
        return SemErro(s, E0412, e->pos_op, "a dimensao do resultado nao tem unidade declarada");
    }
    e->tipo = TipoReal(&s->tipos, dim);
    return true;
}

/* Caminho de erro dos bits (& | ~ << >>) com array, fatia ou lista: a conta elemento a elemento (v1.11) so tem + - * /. */
static bool ErroContaDeColecao(Semantica *s, const Expr *e)
{
    return SemErro(s, E0331, e->pos_op, "bits (& | ~ << >>) nao existem em array, fatia nem lista: a conta elemento a elemento tem + - * /; use for");
}

/* Caminho de erro de + - * / % e bits: algum operando e ?T ou union (v1.7). */
static bool ContaDeOpcional(const Semantica *s, TipoId ta, TipoId tb)
{
    TipoClasse ca = TipoClasseDe(&s->tipos, ta), cb = TipoClasseDe(&s->tipos, tb);
    return ca == TC_OPCIONAL || cb == TC_OPCIONAL || ca == TC_UNIAO || cb == TC_UNIAO;
}

static bool ErroContaDeOpcional(Semantica *s, const Expr *e)
{
    if (TipoClasseDe(&s->tipos, e->a->tipo) == TC_UNIAO || TipoClasseDe(&s->tipos, e->b->tipo) == TC_UNIAO) {
        return SemErro(s, E0313, e->pos_op, "union nao faz conta: leia a variante com switch");
    }
    return SemErro(s, E0312, e->pos_op, "?T nao faz conta: desembrulhe antes (x or_else 0)");
}

/* Caminho de erro de + - * / % e bits: algum operando e ponteiro (^T, ?^T, rawptr, cstr) ou str. */
static bool EhContaDePonteiro(const Semantica *s, TipoId ta, TipoId tb)
{
    return TipoEhFamiliaPonteiro(&s->tipos, ta) || TipoEhFamiliaPonteiro(&s->tipos, tb) || ta == TIPO_TEXTO || tb == TIPO_TEXTO;
}

static bool ErroContaDePonteiro(Semantica *s, const Expr *e)
{
    bool dois_textos = e->op == OP_SOMA && (e->a->tipo_no == EX_TEXTO || e->a->tipo == TIPO_TEXTO) &&
                       (e->b->tipo_no == EX_TEXTO || e->b->tipo == TIPO_TEXTO); /* D-IA1-8: "a" + "b" (literal ou str dos dois lados) e concatenacao, nao conta de ponteiro */
    if (dois_textos || (!TipoEhFamiliaPonteiro(&s->tipos, e->a->tipo) && !TipoEhFamiliaPonteiro(&s->tipos, e->b->tipo))) {
        SemErro(s, E0307, e->pos_op, "str nao se soma nem faz conta; concatenar precisa alocar (format, v1.6/v1.7)");
        SemSugerirConcatenar(s, e);
        return false;
    }
    SemErro(s, E0306, e->pos_op, "ponteiro nao tem conta (sem aritmetica de ponteiro); para andar numa sequencia use fatia e indice (v1.5)");
    SemFormaContaDeTexto(s, e); /* b08: "a" + 1 e c + 1 sao texto em conta, nao aritmetica de ponteiro: a forma diz os tipos reais */
    return false;
}

bool SemAritmetica(Semantica *s, Expr *e)
{
    TipoId ta = e->a->tipo, tb = e->b->tipo;
    bool real_a = TipoClasseDe(&s->tipos, ta) == TC_REAL, real_b = TipoClasseDe(&s->tipos, tb) == TC_REAL;
    if (real_a && real_b) return AritmeticaReal(s, e, ta, tb);
    if (ta == TIPO_F64 && tb == TIPO_F64) {
        if (e->op == OP_RESTO) return SemErro(s, E0301, e->pos_op, "'%%' so vale para inteiros");
        e->tipo = TIPO_F64;
        return true;
    }
    if (ta != tb || !TipoEhInteiro(ta)) {
        if (__builtin_expect(TiposTemV19(&s->tipos), 0) && (TipoClasseDe(&s->tipos, ta) >= TC_MAPA || TipoClasseDe(&s->tipos, tb) >= TC_MAPA)) {
            return SemContaV19(s, e);
        }
        if (SemEhQuat(s, ta) || SemEhQuat(s, tb)) return SemContaQuat(s, e); /* v1.11 L14a */
        if (ContaDeOpcional(s, ta, tb)) return ErroContaDeOpcional(s, e);
        if (EhColecao(s, ta) || EhColecao(s, tb)) return SemContaVetor(s, e);
        if (EhContaDePonteiro(s, ta, tb)) return ErroContaDePonteiro(s, e);
        return ErroDeOperandos(s, e, "operandos numericos de tipos diferentes");
    }
    e->tipo = ta;
    return true;
}

/* & | ~ << >>: dois operandos do MESMO tipo inteiro sem sinal (a contagem do deslocamento tem o tipo do valor). */
static bool AplicarBits(Semantica *s, Expr *e)
{
    TipoId ta = e->a->tipo, tb = e->b->tipo;
    if (ta != tb || !TipoEhSemSinal(ta)) {
        if (__builtin_expect(TiposTemV19(&s->tipos), 0) && (TipoClasseDe(&s->tipos, ta) >= TC_MAPA || TipoClasseDe(&s->tipos, tb) >= TC_MAPA)) {
            return SemContaV19(s, e);
        }
        if (ContaDeOpcional(s, ta, tb)) return ErroContaDeOpcional(s, e);
        if (EhColecao(s, ta) || EhColecao(s, tb)) return ErroContaDeColecao(s, e);
        if (EhContaDePonteiro(s, ta, tb)) return ErroContaDePonteiro(s, e);
        if (ta != tb) return ErroDeOperandos(s, e, "operandos de bits de tipos diferentes");
    }
    if (!TipoEhSemSinal(ta)) return SemErro(s, E0303, e->pos_op, "operador de bits em tipo que nao e inteiro sem sinal");
    e->tipo = ta;
    return true;
}

static bool ExBinario(Semantica *s, Expr *e, TipoId ctx)
{
    if (!OperandosDoBinario(s, e, SemCtxSemOpcional(s, ctx))) return false;
    if (EhLogico(e->op)) return AplicarLogica(s, e);
    if (EhBits(e->op)) return AplicarBits(s, e);
    if (EhComparacao(e->op)) return AplicarComparacao(s, e);
    return SemAritmetica(s, e);
}

/* ---------- despacho ---------- */

static bool ExChamada(Semantica *s, Expr *e, TipoId ctx)
{
    (void)ctx;
    return SemChamada(s, e);
}

static bool ExLiteralStruct(Semantica *s, Expr *e, TipoId ctx)
{
    (void)ctx;
    return SemLiteralStruct(s, e);
}

static bool ExTry(Semantica *s, Expr *e, TipoId ctx)
{
    (void)ctx;
    return SemTry(s, e);
}

static bool ExOrElse(Semantica *s, Expr *e, TipoId ctx)
{
    (void)ctx;
    return SemOrElse(s, e);
}

static bool ExIndice(Semantica *s, Expr *e, TipoId ctx)
{
    (void)ctx;
    return SemIndice(s, e);
}

static bool ExFatia(Semantica *s, Expr *e, TipoId ctx)
{
    (void)ctx;
    return SemFatia(s, e);
}

static bool ExTipo(Semantica *s, Expr *e, TipoId ctx)
{
    (void)ctx;
    return SemTipoSolto(s, e);
}

/* EX_EMBRULHA e EX_OPC_TESTE nascem da semantica ja tipados e ela nunca os revisita; a entrada na tabela so a completa. */
static bool ExJaTipado(Semantica *s, Expr *e, TipoId ctx)
{
    (void)s;
    (void)e;
    (void)ctx;
    return true;
}

/* 'a, b' so e conferido pelo return de proc com retorno multiplo (SemReturn); qualquer outra entrada aqui e erro (D12). */
static bool ExTupla(Semantica *s, Expr *e, TipoId ctx)
{
    (void)ctx;
    return SemErro(s, E0314, e->pos, "'a, b' so existe em 'return a, b' de proc com retorno multiplo");
}

/* f"..." so e conferido como 1o argumento de print, format e format_c (SemFTexto); em qualquer outro lugar e erro (D16). */
static bool ExFTexto(Semantica *s, Expr *e, TipoId ctx)
{
    (void)ctx;
    return SemErro(s, E0315, e->pos, "f\"...\" so vale como 1o argumento de print, format ou format_c");
}

static bool ExChamadaInd(Semantica *s, Expr *e, TipoId ctx)
{
    (void)ctx;
    return SemChamadaInd(s, e);
}

static const ChecaExpr CHECAGENS[EX_CHAMADA_IND + 1] = {
    [EX_CHAMADA_IND] = ExChamadaInd, [EX_EMBRULHA] = ExJaTipado, [EX_OPC_TESTE] = ExJaTipado, [EX_TUPLA] = ExTupla,
    [EX_FTEXTO] = ExFTexto,
    [EX_TIPO] = ExTipo,
    [EX_INDICE] = ExIndice, [EX_FATIA] = ExFatia, [EX_LITERAL_ARRAY] = SemLiteralArray,
    [EX_INT] = ExLiteral, [EX_REAL] = ExLiteral, [EX_BOOL] = ExBool, [EX_TEXTO] = ExTexto, [EX_NOME] = ExNome,
    [EX_CAMPO] = ExCampo, [EX_CHAMADA] = ExChamada, [EX_LITERAL_STRUCT] = ExLiteralStruct, [EX_UNARIO] = ExUnario,
    [EX_BINARIO] = ExBinario, [EX_CONVERSAO] = ExConversao, [EX_TRY] = ExTry, [EX_OR_ELSE] = ExOrElse, [EX_NONE] = ExNone,
};

bool SemChecar(Semantica *s, Expr *e, TipoId ctx)
{
    bool ok = CHECAGENS[e->tipo_no](s, e, ctx);
    if (__builtin_expect(e->tipo == TIPO_VENENO, 0) && ok) s->tocou_veneno = true; /* L8: o erro que vier depois e cascata */
    return ok;
}

bool SemExpr(Semantica *s, Expr *e, TipoId ctx)
{
    SemMarcarFlexivel(e);
    return SemChecar(s, e, ctx);
}

bool SemExprCom(Semantica *s, Expr *e, TipoId esperado)
{
    return SemExpr(s, e, esperado) && SemCoagir(s, e, esperado, e->pos);
}

bool SemCondicao(Semantica *s, Expr *cond)
{
    if (!SemExpr(s, cond, TIPO_BOOL)) return false;
    if (cond->tipo == TIPO_BOOL) return true;
    SemErro(s, E0301, cond->pos, "a condicao precisa ser bool");
    SemFormaDoValor(s, cond, TIPO_BOOL, "a condicao");
    return false;
}
