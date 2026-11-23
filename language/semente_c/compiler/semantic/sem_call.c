/* sem_call.c - chamadas (regras de argumento), embutidos print/assert/panic, literal de struct e faixa constante.
 * Versao: 0.42.0
 * Changelog: 0.42.0 - M6e P93g: ChamarEmbutido liga corpo_muta_colecao (gancho do E0913).
 *            0.41.0 - M6c P87: SemChamada registra o handle de 'creates p' (SemHandleDaSaida) e passa o proc a SemPosseDosArgumentos (emprestimo x entrega).
 *            0.40.0 - K15: EXATOS ganha EMB_SET_UNION, EMB_SET_INTERSECT e EMB_SET_DIFF (SemChamarSet*, sem_lista.c).
 *            0.39.0 - K09: EXATOS ganha EMB_INDEX_BY, EMB_FIND_BY, EMB_ANY_BY, EMB_ALL_BY e EMB_COUNT_BY (SemChamar*By, sem_lista.c).
 *            0.38.0 - S3 P4: SemChamada chama SemLibmNotarChamada (chamada de foreign de libm que migra para math.<fn>; so com o forja migrar ligado).
 *            0.37.0 - S3 P3: SemChamada passa por SemMathChamada (sem_math_std.c) antes de ligar: math.<fn> confere os tipos e f32 vai para a gemea <fn>_f32.
 *            0.36.0 - K17: EXATOS ganha EMB_HEAP_PUSH, EMB_HEAP_POP, EMB_HEAP_PUSH_BY e EMB_HEAP_POP_BY (SemChamarHeap*, sem_lista.c).
 *            0.35.0 - K08c: EXATOS ganha EMB_DEDUP (SemChamarDedup, sem_lista.c).
 *            0.34.0 - S2b: EXATOS ganha EMB_SHUFFLE e EMB_CHOICE (SemChamarShuffle e SemChamarChoice, sem_sorteio.c); inclui sem_lista.h.
 *            0.33.0 - K08b: EXATOS ganha EMB_SORT_BY_KEY e EMB_BINARY_SEARCH (sem_memoria.c).
 *            0.32.0 - K08a: EXATOS ganha EMB_SORT_BY (SemChamarSortBy, sem_memoria.c).
 *            0.31.0 - B01d: ConferirFaltantes aceita a chamada sem 'alloc' no forja migrar (SemAllocatorInserirNoMigrar); o modo normal segue com o E0503.
 *            0.30.0 - B01c: ConferirTipos passa o argumento do parametro Allocator pelo conversor (SemAllocatorConverterArg); SemChamada grava a origem do resultado (SemAllocatorOrigemDaChamada).
 *            0.29.0 - B01b: transmute(x, para = Allocator) = E0336 (ChamarTransmute).
 *            0.28.0 - B01a: SemLiteralStruct recusa o literal Allocator{...} (SemAllocatorLiteral, E0336).
 *            0.27.0 - IA1-L9e: ChamarPorFaixa despacha EMB_MIN_OF em diante para SemChamarEstat (sem_estat.c).
 *            0.26.0 - IA1-L9c/L9d: ChamarPorFaixa despacha EMB_CAP em diante para SemChamarLista (sem_lista.c).
 *            0.25.0 - IA1-L9a/L9b: ChamarPorFaixa despacha EMB_WRAPPING_ADD em diante para SemChamarAritmetica (sem_aritmetica.c).
 *            0.24.0 - IA1-L5b: EXATOS ganha EMB_RESIZE e EMB_SORT (sem_memoria.c).
 *            0.23.0 - IA1-L5a: ChamarSaida (print aceita f32/f64/unidade; eprint = print para stderr, EMB_EPRINT); real e eprint viram f"{x}" (SemFTextoDoArgumento).
 *            0.22.0 - IA1-L3d1: ConferirRotuloAntigo registra o rotulo antigo em Diag.trocas (RenomeadoNotarTabela) para o forja migrar.
 *            0.21.0 - IA1-L3c2: rotulo de proc de modulo (REN_PARAM, escopo = modulo inteiro): o rotulo antigo (caminho) acha o parametro declarado com o nome novo (path) e vice-versa (EntradaDoRotulo).
 *            0.20.0 - IA1-L3c: rotulos novos da tabela de renomeados (alloc, to) valem junto dos antigos (em, para): AcharParametroRenomeado; flag recusar -> E0110 (ConferirRotuloAntigo).
 *            0.19.0 - IA1-L3b (V-06 = B): o argumento e posicional (na ordem dos parametros) OU nomeado; posicional depois de nomeado = E0502; argumento a mais = E0502 (sobra); transmute aceita o tipo posicional.
 *            0.18.4- IA1-L2: proc nao declarado 'range', 'println', 'puts' ou 'printf' = E0110 (SemIdiomaDeChamada) antes do E0201.
 *            0.18.3 - IA1-b15: proc nao declarado, ja relatado no corpo, sai calado (SemNomeJaRelatado); chamar por local envenenado (let que falhou) e cascata calada.
 *            0.18.2 - IA1-b07: argumento e campo de literal marcam s->ctx_tipo antes da coercao; o print de tipo errado leva a forma da ocorrencia (SemFormaDoPrint) (E0301).
 *            0.18.1 - IA1-b06: E0502 leva a sugestao aplicavel (SemSugerirTirarNome, SemSugerirNomearArg com o nome REAL do parametro, SemSugerirTirarArg).
 *            0.18.0- Linguagem v1.12 M3 P21v-a: chamada indireta (SemChamarComParams; SemChamada converte 'f(x)' de local proc em EX_CHAMADA_IND), valor_para_foreign no argumento de foreign, E0911 no literal sem o campo proc.
 *            0.17.0 - Linguagem v1.11 L15a: len de [dynamic, N]T avalia o argumento (le o n; nao e constante como o do array).
 *            0.16.0 - Linguagem v1.11 L14a: SemLiteralStruct registra o quat no 1o uso (SemQuatGarantir); ChamarEmbutido despacha conj (SemChamarConj).
 *            0.15.0 -Linguagem v1.11 L13: ChamarEmbutido despacha min/max/abs/clamp/sqrt (EMB_MIN em diante) para SemChamarMatematica (sem_matematica.c).
 *            0.14.1 -Linguagem v1.11 L12b: SemInteiroConstante le o indice de v.x (EX_INT com REF_COMPONENTE) de ref_id.
 *            0.14.0 - Linguagem v1.11 L9: ChamarStaticAssert; ChamarEmbutido despacha size_of/align_of/offset_of para SemChamarLayout (sem_layout.c).
 *            0.13.0 - Linguagem v1.11 L8: chamada de proc de assinatura invalida e literal de struct envenenada valem TIPO_VENENO (argumentos e campos nao sao conferidos).
 *            0.12.0- Linguagem v1.11 L5: E0504 (parametro nomeado e campo de literal de struct) e E0201 (proc nao declarado) com candidatos "parecido" (sem_parecido.c).
 *            0.11.0 (P10) - ChamarLenV19: len de map e ^map devolve int (sai o provisorio).
 *            0.11.0 (P7) - ChamarLenV19: len de soa devolve int (soa[N]S com chamada no argumento = E0320).
 *            0.11.0 (P5) - ChamarLenV19: len de matrix = E0321.
 *            0.11.0 - Linguagem v1.9 P3: ChamarEmbutido despacha embutido >= EMB_INSERT para SemChamarV19; ChamarLenV19 (bit_set conta os membros; matrix/soa/map provisorios).
 *            0.10.0 - Linguagem v1.8b P6: MarcarOrigemC (chamada a proc foreign com retorno cstr/?cstr grava ORIGEM_C).
 *            0.9.0 - Linguagem v1.8b P4: CampoSemZero (campo struct cuja struct exige literal = E0503 no literal).
 *            0.8.0 - Linguagem v1.8b P3a: E0908 no literal de struct opaca (SemLiteralStruct).
 *            0.7.1 - Linguagem v1.8 P7: campo de literal de struct (guarda) e argumento de proc do usuario (entrega) conferem a posse.
 *            0.7.0 - Linguagem v1.8 P3: SemChamada e SemLiteralStruct conferem a visibilidade entre modulos (SemVisivel).
 *            0.6.0- Linguagem v1.7 P10: versao de fechamento da v1.7 (sem mudanca de logica).
 *            0.5.3 - Linguagem v1.7 P5a: print(f"...") e despacho de format/format_c para sem_formato.c.
 *            0.5.2 - Linguagem v1.7 P3: campo union sem padrao e sem valor no literal = E0503 (ConferirCamposPonteiro).
 *            0.5.1- Linguagem v1.7 P2: argumento e campo de literal de struct coagem o valor (SemCoagir: T -> ?T).
 *            0.5.0 - Linguagem v1.6 P4: campo de literal de struct confere movimento (E0311); len aceita listas ([dynamic]T e ^[dynamic]T).
 *            0.4.0 - Linguagem v1.6 P3a: SemLigarEmbutido (nao-static) e despacho de new/make/append/delete/frame_reset para SemChamarMemoria.
 *            0.2.3 - Linguagem v1.5 P3a: len de [N]T, ^[N]T e []T; SemInteiroConstante (aceita const); campo array sem zero = E0503.
 *            0.2.2 - Linguagem v1.3 P6: print de str e cstr; embutido len; assert/panic com cstr.
 *            0.2.1 - Linguagem v1.3: literal de struct com campo ^T sem padrao e sem valor = E0503.
 *            0.2.0 - Linguagem v1.2: print de qualquer inteiro, SemFaixaLiteral por sinal+magnitude, embutido transmute.
 *            0.1.0 - Fase 5c: regras do prototipo (benchmark/c/check_call.c); assert e panic usam o mesmo ligador de argumentos
 *                    com assinaturas ficticias; texto literal so em print/assert/panic.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_interno.h"

#include "common/renomeados.h"
#include "semantic/sem_lista.h"

#include <string.h>

typedef struct { const Param *params; uint32_t nparams; bool externo; } Assinatura; /* externo (v1.12): proc foreign, o nome de proc como argumento e callback */

static int32_t AcharParametro(const Assinatura *a, int32_t nome)
{
    for (uint32_t i = 0; i < a->nparams; i++) {
        if (a->params[i].nome == nome) return (int32_t)i;
    }
    return -1;
}

/* Parametro ligado pelo argumento anterior (-1 antes do 1o): os argumentos so avancam na ordem declarada. */
static int32_t UltimoLigado(const Expr *e, uint32_t i)
{
    return i > 0 ? e->args[i - 1].indice : -1;
}

/* V-06 = B: o argumento posicional ocupa o parametro da propria posicao; depois de um nomeado, posicional e E0502. */
static bool LigarPosicional(Semantica *s, Expr *e, const Assinatura *a, uint32_t i)
{
    Arg *g = &e->args[i];
    int32_t ultimo = UltimoLigado(e, i);
    if (i > 0 && e->args[i - 1].nome >= 0) {
        SemErro(s, E0502, g->valor->pos, "argumento posicional depois de um nomeado; escreva nome = valor");
        if ((uint32_t)(ultimo + 1) < a->nparams) SemSugerirNomearArg(s, g, SemNomeTexto(s, a->params[ultimo + 1].nome));
        return false;
    }
    if (i >= a->nparams) {
        if (a->nparams == 0) SemErro(s, E0502, g->valor->pos, "o proc nao tem parametros");
        else SemErro(s, E0502, g->valor->pos, "sobra argumento: o proc tem so %u parametro(s)", (unsigned)a->nparams);
        SemSugerirTirarArg(s, g);
        return false;
    }
    g->indice = (int32_t)i;
    return true;
}

/* L3c/L3c2: a entrada do rotulo na tabela de renomeados: REN_ROTULO pelo nome do embutido (make, transmute...) ou, na chamada 'apelido.proc(...)' (e->modulo != 0), REN_PARAM pelo caminho
 * canonico do modulo do proc (o escopo e o modulo inteiro, nao o proc). *era_antigo diz qual das duas formas foi escrita. */
static const Renomeado *EntradaDoRotulo(const Semantica *s, const Expr *e, int32_t rotulo, bool *era_antigo)
{
    const char *nome = SemNomeTexto(s, rotulo);

    if (e->modulo == 0 || s->modulos == NULL) return RenomeadoAchar(REN_ROTULO, SemNomeTexto(s, e->nome), nome, era_antigo);
    return RenomeadoAchar(REN_PARAM, InternerTexto(s->nomes, s->modulos->itens[e->modulo].caminho), nome, era_antigo);
}

/* L3c: o rotulo escrito na OUTRA forma da tabela de renomeados acha o parametro pelo nome da forma declarada: o embutido foi declarado com o nome antigo (alloc -> em, to -> para) e o proc
 * de modulo com o novo (caminho -> path, indice -> index). -1 = nao e rotulo renomeado. */
static int32_t AcharParametroRenomeado(const Semantica *s, const Expr *e, const Assinatura *a, int32_t nome)
{
    bool era_antigo = false;
    const Renomeado *r = EntradaDoRotulo(s, e, nome, &era_antigo);
    const char *declarado = r == NULL ? NULL : era_antigo ? r->novo : r->antigo;

    for (uint32_t i = 0; declarado != NULL && i < a->nparams; i++) {
        if (strcmp(SemNomeTexto(s, a->params[i].nome), declarado) == 0) return (int32_t)i;
    }
    return -1;
}

/* L3f: o rotulo escrito na forma ANTIGA da tabela e E0110 quando a entrada tem 'recusar' (hoje false: a forma antiga vale em silencio). */
static bool ConferirRotuloAntigo(Semantica *s, const Expr *e, const Arg *g)
{
    bool era_antigo = false;
    const Renomeado *r = EntradaDoRotulo(s, e, g->nome, &era_antigo);
    if (r == NULL || !era_antigo) return true;
    RenomeadoNotarTabela(s->diag, g->pos_nome, InternerTam(s->nomes, g->nome), r);
    if (!RenomeadoRecusa(r)) return true;
    return RenomeadoRecusar(s->diag, g->pos_nome, InternerTam(s->nomes, g->nome), r);
}

static bool LigarNomeado(Semantica *s, Expr *e, const Assinatura *a, uint32_t i)
{
    Arg *g = &e->args[i];
    int32_t idx = AcharParametro(a, g->nome);
    if (idx < 0) idx = AcharParametroRenomeado(s, e, a, g->nome);
    if (idx >= 0 && !ConferirRotuloAntigo(s, e, g)) return false;
    if (idx < 0) {
        SemParecido sp;
        SemParecidoIniciar(s, E0504, g->pos_nome, g->nome, &sp);
        SemParecidosDeParams(s, a->params, a->nparams, &sp);
        return SemErroParecido(s, &sp, "o proc nao tem o parametro '%s'", SemNomeTexto(s, g->nome));
    }
    if (idx <= UltimoLigado(e, i)) return SemErro(s, E0501, g->pos_nome, "argumento fora da ordem declarada ou repetido");
    g->indice = idx;
    return true;
}

/* Passo 1: cada argumento e posicional (na ordem declarada) ou nomeado (em ordem crescente); posicional depois de nomeado e erro. */
static bool LigarArgumentos(Semantica *s, Expr *e, const Assinatura *a)
{
    for (uint32_t i = 0; i < e->nargs; i++) {
        bool ligou = e->args[i].nome < 0 ? LigarPosicional(s, e, a, i) : LigarNomeado(s, e, a, i);
        if (!ligou) return false;
    }
    return true;
}

/* Passo 2: tipo de cada argumento ja ligado, da esquerda para a direita. */
static bool ConferirTipos(Semantica *s, Expr *e, const Assinatura *a)
{
    for (uint32_t i = 0; i < e->nargs; i++) {
        Arg *g = &e->args[i];
        TipoId querido = a->params[g->indice].tipo.tipo;
        s->valor_para_foreign = a->externo && g->valor->tipo_no == EX_NOME; /* v1.12: so o nome solto no argumento de foreign e o callback de sempre */
        bool ok = SemEhAllocator(s, querido) ? SemAllocatorConverterArg(s, e, g) : SemChecar(s, g->valor, querido); /* B01c: alloc = heap|frame|Arena|alloc */
        s->valor_para_foreign = false;
        s->ctx_tipo = (ContextoDoTipo){CTX_ARGUMENTO, g->valor, a->params[g->indice].nome, e->nome};
        if (!ok || !SemCoagir(s, g->valor, querido, g->valor->pos)) return false;
    }
    return true;
}

/* Passo 3: parametro sem padrao e sem argumento. */
static bool ConferirFaltantes(Semantica *s, Expr *e, const Assinatura *a)
{
    for (uint32_t i = 0; i < a->nparams; i++) {
        bool ligado = false;
        for (uint32_t k = 0; k < e->nargs; k++) ligado = ligado || e->args[k].indice == (int32_t)i;
        if (!ligado && a->params[i].padrao == NULL) {
            if (SemAllocatorInserirNoMigrar(s, e, &a->params[i])) continue; /* B01d */
            return SemErro(s, E0503, e->pos_op, "falta o argumento '%s'", SemNomeTexto(s, a->params[i].nome));
        }
    }
    return true;
}

/* Embutidos de memoria: so liga os argumentos e confere faltantes; os tipos sao conferidos por sem_memoria.c. */
bool SemLigarEmbutido(Semantica *s, Expr *e, const Param *params, uint32_t nparams)
{
    Assinatura a = {params, nparams, false};
    return LigarArgumentos(s, e, &a) && ConferirFaltantes(s, e, &a);
}

static bool ChamarCom(Semantica *s, Expr *e, const Assinatura *a, TipoId retorno)
{
    if (!LigarArgumentos(s, e, a) || !ConferirTipos(s, e, a) || !ConferirFaltantes(s, e, a)) return false;
    e->tipo = retorno;
    return true;
}

/* v1.12 P21v-a: chamada indireta (sem_callback.c): os "parametros" vem do tipo proc do chamado. */
bool SemChamarComParams(Semantica *s, Expr *e, const Param *params, uint32_t nparams)
{
    Assinatura a = {params, nparams, false};
    return LigarArgumentos(s, e, &a) && ConferirTipos(s, e, &a) && ConferirFaltantes(s, e, &a);
}

/* print(x) e eprint(x) (stderr): 1 argumento posicional, inteiro de qualquer largura, real (f32, f64, unidade), bool, str ou cstr (o literal adota cstr pelo contexto).
 * print de real e todo eprint viram f"{x}" (SemFTextoDoArgumento); print de inteiro, bool, str e cstr segue direto. */
static bool ChamarSaida(Semantica *s, Expr *e, const char *nome, bool para_erro)
{
    if (e->nargs != 1 || e->args[0].nome >= 0) {
        return SemErro(s, E0301, e->pos_op, "%s recebe exatamente 1 argumento posicional", nome);
    }
    Expr *v = e->args[0].valor;
    e->args[0].indice = 0;
    e->tipo = TIPO_VAZIO;
    if (v->tipo_no == EX_FTEXTO) return SemFTexto(s, v);
    if (!SemChecar(s, v, TIPO_CSTR)) return false;
    bool real = TipoEhReal(&s->tipos, v->tipo);
    if (!real && !TipoEhInteiro(v->tipo) && v->tipo != TIPO_BOOL && v->tipo != TIPO_TEXTO && v->tipo != TIPO_CSTR) {
        SemErro(s, E0301, v->pos, "%s so aceita inteiro (i8..u64, int, uint), real (f32, f64, unidade), bool, str ou cstr", nome);
        SemFormaDoPrint(s, v);
        return false;
    }
    return !(real || para_erro) || SemFTextoDoArgumento(s, e);
}

static bool ChamarPrint(Semantica *s, Expr *e)
{
    return ChamarSaida(s, e, "print", false);
}

static bool ChamarEprint(Semantica *s, Expr *e)
{
    return ChamarSaida(s, e, "eprint", true);
}

/* O argumento de len(array) nao e avaliado (N e constante): uma chamada dentro dele nunca executaria. */
static bool ContemChamada(const Expr *e)
{
    if (e->tipo_no == EX_CHAMADA) return true;
    if (e->a != NULL && ContemChamada(e->a)) return true;
    if (e->b != NULL && ContemChamada(e->b)) return true;
    for (uint32_t i = 0; i < e->nargs; i++) {
        if (ContemChamada(e->args[i].valor)) return true;
    }
    return false;
}

/* len de tipo da v1.9 (3.9 (f)): bit_set conta os membros; matrix e erro; soa e map contam (map e ^map valem). */
static bool ChamarLenV19(Semantica *s, Expr *e, const Expr *v)
{
    TipoClasse c = TipoClasseDe(&s->tipos, v->tipo);
    bool por_ponteiro = c == TC_PONTEIRO;
    if (por_ponteiro) c = TipoClasseDe(&s->tipos, TipoDe(&s->tipos, v->tipo)->a);
    if (c == TC_BIT_SET) {
        if (por_ponteiro) return SemErro(s, E0319, v->pos, "len recebe o bit_set: len(s)");
        e->args[0].indice = 0;
        e->tipo = TIPO_INT;
        return true;
    }
    if (c == TC_MATRIZ) {
        return SemErro(s, E0321, v->pos, "len de matrix nao existe: o tamanho e fixo no tipo matrix[R, C]T");
    }
    if (c == TC_SOA) {
        TipoId soa = por_ponteiro ? (TipoId)TipoDe(&s->tipos, v->tipo)->a : v->tipo;
        if (!TipoSoaDinamica(&s->tipos, soa) && ContemChamada(v)) {
            return SemErro(s, E0320, v->pos,
                           "len de soa[N]S e constante e nao avalia o argumento: guarde o valor numa variavel antes");
        }
        e->args[0].indice = 0;
        e->tipo = TIPO_INT;
        return true;
    }
    e->args[0].indice = 0; /* map e ^map */
    e->tipo = TIPO_INT;
    return true;
}

/* len(x): 1 argumento posicional, str (bytes UTF-8), [N]T, ^[N]T (N constante) ou []T; devolve int. */
static bool ChamarLen(Semantica *s, Expr *e)
{
    ColecaoForma forma;
    if (e->nargs == 0) return SemErro(s, E0503, e->pos_op, "len recebe o texto");
    if (e->args[0].nome >= 0) {
        SemErro(s, E0502, e->args[0].pos_nome, "o 1o argumento deve ser posicional");
        SemSugerirTirarNome(s, &e->args[0], -1);
        return false;
    }
    if (e->nargs > 1) return SemErro(s, E0504, e->args[1].valor->pos, "len recebe so 1 argumento");
    Expr *v = e->args[0].valor;
    if (!SemChecar(s, v, TIPO_VAZIO)) return false;
    if (v->tipo == TIPO_CSTR) {
        return SemErro(s, E0307, v->pos, "len de cstr precisaria contar ate o NUL: use len(c as str)");
    }
    SemElementoDe(s, v->tipo, &forma);
    if (v->tipo != TIPO_TEXTO && forma == FORMA_NENHUMA) {
        if (SemEhTipoV19(s, v->tipo)) return ChamarLenV19(s, e, v);
        return SemErro(s, E0301, v->pos, "len pede str, array ou fatia");
    }
    if ((forma == FORMA_ARRAY || forma == FORMA_PONTEIRO_ARRAY) && !SemEhListaFixaOuPonteiro(s, v->tipo) && ContemChamada(v)) { /* v1.11 L15a: len(lista fixa) le o n */
        return SemErro(s, E0308, v->pos, "len de array e constante e nao avalia o argumento: guarde o valor numa variavel antes");
    }
    e->args[0].indice = 0;
    e->tipo = TIPO_INT;
    return true;
}

static bool ChamarAssert(Semantica *s, Expr *e)
{
    Assinatura a = {s->params_assert, 2, false};
    return ChamarCom(s, e, &a, TIPO_VAZIO);
}

static bool ChamarPanic(Semantica *s, Expr *e)
{
    Assinatura a = {s->params_panic, 1, false};
    return ChamarCom(s, e, &a, TIPO_VAZIO);
}

/* transmute(x, to = T): forma da chamada (1o posicional, 2o 'to = Tipo' ou 'para = Tipo' (forma antiga, L3c), nada mais). */
static bool ConferirFormaTransmute(Semantica *s, const Expr *e)
{
    if (e->nargs == 0) return SemErro(s, E0503, e->pos_op, "falta o valor a converter");
    if (e->nargs == 1) return SemErro(s, E0503, e->pos_op, "falta o argumento 'to' (tipo de destino)");
    if (e->args[0].nome >= 0) {
        SemErro(s, E0502, e->args[0].pos_nome, "o 1o argumento deve ser posicional");
        SemSugerirTirarNome(s, &e->args[0], -1);
        return false;
    }
    if (e->args[1].nome >= 0 && e->args[1].nome != s->nome_to && e->args[1].nome != s->nome_para) { /* V-06 = B: o 2o argumento tambem pode ser posicional (o tipo) */
        return SemErro(s, E0504, e->args[1].pos_nome, "transmute nao tem o parametro '%s'", SemNomeTexto(s, e->args[1].nome));
    }
    if (e->nargs > 2) return SemErro(s, E0504, e->args[2].valor->pos, "transmute so recebe o valor e 'to'");
    if (e->args[1].valor->tipo_no != EX_NOME) {
        return SemErro(s, E0201, e->args[1].valor->pos, "'to' recebe o nome de um tipo");
    }
    return e->args[1].nome < 0 || ConferirRotuloAntigo(s, e, &e->args[1]);
}

/* Origem e alvo numericos (inteiros, f32, f64, unidades) e do mesmo tamanho em bytes. */
static bool ConferirTiposTransmute(Semantica *s, const Expr *e, TipoId origem, TipoId alvo)
{
    if (!TipoEhNumerico(&s->tipos, origem) || !TipoEhNumerico(&s->tipos, alvo)) {
        return SemErro(s, E0304, e->pos_op, "transmute so troca bits entre tipos numericos (inteiros, f32, f64, unidades)");
    }
    uint8_t bytes_origem = TipoLarguraBits(&s->tipos, origem) / 8, bytes_alvo = TipoLarguraBits(&s->tipos, alvo) / 8;
    if (bytes_origem == bytes_alvo) return true;
    char nome_origem[64], nome_alvo[64];
    SemDescrever(s, origem, nome_origem, sizeof nome_origem);
    SemDescrever(s, alvo, nome_alvo, sizeof nome_alvo);
    return SemErro(s, E0304, e->pos_op, "tamanhos diferentes: %s tem %u bytes, %s tem %u", nome_origem, (unsigned)bytes_origem,
                   nome_alvo, (unsigned)bytes_alvo);
}

static bool ChamarTransmute(Semantica *s, Expr *e)
{
    TipoId alvo;
    if (!ConferirFormaTransmute(s, e)) return false;
    Arg *para = &e->args[1];
    if (!SemResolverNomeTipo(s, para->valor->nome, para->valor->pos, 0, &alvo)) return false;
    if (SemEhAllocator(s, alvo)) return SemAllocatorNomeForaDeParametro(s, para->valor->pos); /* B01b: nao se forja um Allocator */
    if (!SemChecar(s, e->args[0].valor, TIPO_VAZIO)) return false;
    if (!ConferirTiposTransmute(s, e, e->args[0].valor->tipo, alvo)) return false;
    e->args[0].indice = 0;
    para->indice = 1;
    para->valor->tipo = alvo;
    e->tipo = alvo;
    return true;
}

/* static_assert(cond, msg = "..."): a assinatura do assert; a condicao tem de ser constante (v1.11 L9). */
static bool ChamarStaticAssert(Semantica *s, Expr *e)
{
    Assinatura a = {s->params_assert, 2, false};
    return ChamarCom(s, e, &a, TIPO_VAZIO) && SemConferirStaticAssert(s, e);
}

typedef bool (*ChamarExato)(Semantica *s, Expr *e);

/* Embutidos com tratador proprio, por id exato (vence as faixas de ChamarPorFaixa; EMB_CONJ, p.ex., esta dentro da faixa de EMB_MIN). */
static const ChamarExato EXATOS[] = {
    [EMB_PRINT] = ChamarPrint,         [EMB_ASSERT] = ChamarAssert,
    [EMB_PANIC] = ChamarPanic,         [EMB_TRANSMUTE] = ChamarTransmute,
    [EMB_LEN] = ChamarLen,             [EMB_FORMAT] = SemChamarFormato,
    [EMB_FORMAT_C] = SemChamarFormato, [EMB_SLICE_FROM] = SemChamarSliceFrom,
    [EMB_STATIC_ASSERT] = ChamarStaticAssert, [EMB_CONJ] = SemChamarConj,
    [EMB_EPRINT] = ChamarEprint,
    [EMB_RESIZE] = SemChamarResize,    [EMB_SORT] = SemChamarSort,
    [EMB_SORT_BY] = SemChamarSortBy,  [EMB_SORT_BY_KEY] = SemChamarSortByKey,
    [EMB_BINARY_SEARCH] = SemChamarBinarySearch,
    [EMB_SHUFFLE] = SemChamarShuffle,  [EMB_CHOICE] = SemChamarChoice,
    [EMB_DEDUP] = SemChamarDedup,
    [EMB_HEAP_PUSH] = SemChamarHeapPush, [EMB_HEAP_PUSH_BY] = SemChamarHeapPushBy,
    [EMB_HEAP_POP] = SemChamarHeapPop,   [EMB_HEAP_POP_BY] = SemChamarHeapPopBy,
    [EMB_INDEX_BY] = SemChamarIndexBy,   [EMB_FIND_BY] = SemChamarFindBy,
    [EMB_ANY_BY] = SemChamarAnyBy,       [EMB_ALL_BY] = SemChamarAllBy,
    [EMB_COUNT_BY] = SemChamarCountBy,
    [EMB_SET_UNION] = SemChamarSetUnion, [EMB_SET_INTERSECT] = SemChamarSetIntersect,
    [EMB_SET_DIFF] = SemChamarSetDiff,
};

/* Os demais embutidos: cada grupo e um intervalo de ids (do maior grupo para o menor); fora deles, o nome nao e chamavel assim. */
static bool ChamarPorFaixa(Semantica *s, Expr *e, int32_t embutido)
{
    if (embutido >= EMB_MIN_OF) return SemChamarEstat(s, e, embutido);
    if (embutido >= EMB_CAP) return SemChamarLista(s, e, embutido);
    if (embutido >= EMB_WRAPPING_ADD) return SemChamarAritmetica(s, e, embutido);
    if (embutido >= EMB_MIN) return SemChamarMatematica(s, e, embutido);
    if (embutido >= EMB_SIZE_OF) return SemChamarLayout(s, e, embutido);
    if (embutido >= EMB_INSERT) return SemChamarV19(s, e, embutido);
    if (embutido >= EMB_NEW) return SemChamarMemoria(s, e, embutido);
    return SemErro(s, E0201, e->pos_op, "'%s' nao e um proc que se chame assim", SemNomeTexto(s, e->nome));
}

static bool ChamarEmbutido(Semantica *s, Expr *e, int32_t embutido)
{
    e->ref = REF_EMBUTIDO;
    e->ref_id = embutido;
    if (SemEmbutidoMutaColecao(embutido)) s->corpo_muta_colecao = true;
    bool tem_exato = embutido >= 0 && (size_t)embutido < sizeof EXATOS / sizeof EXATOS[0];
    if (tem_exato && EXATOS[embutido] != NULL) return EXATOS[embutido](s, e);
    return ChamarPorFaixa(s, e, embutido);
}

/* v1.8b: chamada a proc foreign que devolve cstr/?cstr: o texto e emprestado da biblioteca C (origem ORIGEM_C; delete = E0310). */
static void MarcarOrigemC(const Semantica *s, Expr *e, const Decl *decl)
{
    TipoId retorno = decl->retorno.tipo;
    if (!decl->externo || !decl->tem_retorno) return;
    TipoClasse classe = TipoClasseDe(&s->tipos, retorno);
    /* v1.10: ^T e ?^T devolvidos por foreign tambem sao da biblioteca C (antes so o texto: delete(p) passava no check e quebrava em execucao) */
    if (retorno == TIPO_CSTR || TipoOpcionalNicho(&s->tipos, retorno) || classe == TC_PONTEIRO || classe == TC_ANULAVEL) e->alocacao = ORIGEM_C;
}

bool SemChamada(Semantica *s, Expr *e)
{
    if (__builtin_expect(s->usa_proc_valor, 0) && e->modulo == 0 && SemConverterEmChamadaInd(s, e)) return SemChamadaInd(s, e); /* v1.12: local/parametro de tipo proc */
    const Simbolo *y = SemAcharSimbolo(s, e->nome, e->modulo);
    NomeUsado uso = {e->nome, e->pos_op, e->modulo};
    if (!SemVisivel(s, y, &uso)) return false;
    if (y->categoria == SIM_PROC_EMBUTIDO) return ChamarEmbutido(s, e, y->indice);
    if (__builtin_expect(SemSimboloVeneno(y), 0)) { /* L8: o erro esta na assinatura do proc; os argumentos nao seriam conferidos contra nada */
        e->tipo = SemUsoDeVeneno(s);
        return true;
    }
    if (y->categoria != SIM_PROC) {
        const LocalInfo *local = e->modulo == 0 ? EscopoBuscar(&s->escopo, e->nome) : NULL;
        if (local != NULL && local->tipo == TIPO_VENENO) { /* b15: local que o let com erro deixou envenenado: chamar por ele e cascata */
            e->tipo = SemUsoDeVeneno(s);
            return true;
        }
        if (y->categoria == SIM_NENHUM && e->modulo == 0 && SemNomeJaRelatado(s, e->nome)) return SemErroCalado(s, e->pos_op); /* b15: cascata do mesmo nome */
        if (y->categoria == SIM_NENHUM && e->modulo == 0 && SemIdiomaDeChamada(s, e)) return false; /* IA1-L2: range(n), println(x), printf = E0110 */
        SemParecido sp;
        SemParecidoIniciar(s, E0201, e->pos_op, e->nome, &sp);
        SemParecidosDeNome(s, e->modulo, &sp);
        return SemErroParecido(s, &sp, y->categoria == SIM_NENHUM ? "proc '%s' nao declarado" : "'%s' nao e um proc",
                               SemNomeTexto(s, e->nome));
    }
    if (!SemMathChamada(s, e, &y)) return false; /* S3 P3: std:math confere o tipo e troca f32 pela gemea */
    SemLibmNotarChamada(s, e, y->decl); /* S3 P4: foreign de libm que migra (so com o forja migrar ligado; SemLibmNotarChamada confere) */
    Assinatura a = {y->decl->params, y->decl->nparams, y->decl->externo};
    e->ref = REF_PROC;
    e->ref_id = y->indice;
    if (!ChamarCom(s, e, &a, y->decl->retorno.tipo)) return false;
    if (__builtin_expect(s->tipo_allocator != 0, 0)) SemAllocatorOrigemDaChamada(s, e); /* B01c: origem do alloc = ... no resultado */
    if (__builtin_expect(s->tem_externo, 0)) MarcarOrigemC(s, e, y->decl);
    if (y->decl->posse_c == POSSE_C_CRIA_SAIDA && !SemHandleDaSaida(s, e, y->decl)) return false;
    return SemPosseDosArgumentos(s, e, y->decl);
}

/* ---------- faixa constante ---------- */

/* Literal inteiro (ou '-' literal, ou nome de const cujo init e literal inteiro) como sinal + magnitude de 64 bits sem sinal,
 * lida do fonte (uint e u64 passam de INT64_MAX). */
bool SemInteiroConstante(const Semantica *s, const Expr *e, bool *negativo, uint64_t *magnitude)
{
    const Expr *numero = e;
    *negativo = e->tipo_no == EX_UNARIO && e->op == OP_NEGAR;
    if (*negativo) numero = e->a;
    if (numero->tipo_no == EX_NOME && numero->ref == REF_CONST) numero = SemAcharSimbolo(s, numero->nome, numero->modulo)->decl->init;
    if (numero->tipo_no != EX_INT) return false;
    if (numero->ref == REF_COMPONENTE) { /* v1.11 L12b: indice criado por v.x; o valor mora em ref_id, nao no fonte */
        *magnitude = (uint64_t)numero->ref_id;
        return true;
    }
    return TipoLerLiteralU64(s->fonte->texto + numero->pos_op, magnitude);
}

bool SemFaixaLiteral(Semantica *s, const Campo *campo, const Expr *valor)
{
    bool negativo;
    uint64_t mag;
    if (!campo->tem_faixa || !TipoEhInteiro(campo->tipo.tipo) || !SemInteiroConstante(s, valor, &negativo, &mag)) return true;
    /* os limites cabem em int64 (o parser garante); magnitude acima disso esta fora de qualquer faixa */
    bool cabe64 = mag <= (uint64_t)INT64_MAX + (negativo ? 1u : 0u);
    int64_t v = negativo ? (int64_t)(0u - mag) : (int64_t)mag;
    bool dentro = cabe64 && v >= campo->faixa_min && (campo->faixa_excl ? v < campo->faixa_max : v <= campo->faixa_max);
    if (dentro) return true;
    return SemErro(s, E0420, valor->pos, "valor %s%llu fora da faixa %lld%s%lld do campo '%s'", negativo ? "-" : "",
                   (unsigned long long)mag, (long long)campo->faixa_min, campo->faixa_excl ? "..<" : "..=",
                   (long long)campo->faixa_max, SemNomeTexto(s, campo->nome));
}

/* ---------- literal de struct ---------- */

static bool ConferirCampo(Semantica *s, Expr *e, const Decl *estrutura, uint32_t i)
{
    Arg *g = &e->args[i];
    int32_t idx = SemAcharCampo(estrutura, g->nome);
    if (idx < 0) {
        SemParecido sp;
        SemParecidoIniciar(s, E0504, g->pos_nome, g->nome, &sp);
        SemParecidosDeCampos(s, estrutura, &sp);
        return SemErroParecido(s, &sp, "a struct nao tem o campo '%s'", SemNomeTexto(s, g->nome));
    }
    for (uint32_t k = 0; k < i; k++) {
        if (e->args[k].indice == idx) return SemErro(s, E0501, g->pos_nome, "campo '%s' repetido", SemNomeTexto(s, g->nome));
    }
    g->indice = idx;
    const Campo *campo = &estrutura->campos[idx];
    if (!SemChecar(s, g->valor, campo->tipo.tipo)) return false;
    s->ctx_tipo = (ContextoDoTipo){CTX_CAMPO, g->valor, campo->nome, e->nome};
    return SemCoagir(s, g->valor, campo->tipo.tipo, g->valor->pos) && SemConferirMovimento(s, g->valor) &&
           SemConferirPosse(s, g->valor, PONTO_GUARDA) && SemFaixaLiteral(s, campo, g->valor);
}

/* v1.8b: campo struct sem padrao cuja struct exige literal tambem nao tem valor zero. */
static bool CampoSemZero(Semantica *s, const Campo *campo)
{
    if (SemTipoSemZero(s, campo->tipo.tipo)) return true;
    return s->tem_struct_aninhada && TipoClasseDe(&s->tipos, campo->tipo.tipo) == TC_STRUCT &&
           SemStructExigeLiteral(s, SemDeclDaStruct(s, campo->tipo.tipo));
}

/* '^T' nunca e nulo e nao tem valor zero: campo ^T sem padrao exige valor no literal (E0503). */
static bool ConferirCamposPonteiro(Semantica *s, const Expr *e, const Decl *estrutura)
{
    for (uint32_t i = 0; i < estrutura->ncampos; i++) {
        const Campo *campo = &estrutura->campos[i];
        bool dado = false;
        if (campo->padrao != NULL || !CampoSemZero(s, campo)) continue;
        for (uint32_t k = 0; k < e->nargs; k++) dado = dado || e->args[k].indice == (int32_t)i;
        if (dado) continue;
        char apontado[96];
        if (SemProcSemZero(s, campo->tipo.tipo)) { /* v1.12: o campo (ou a struct de dentro) tem proc */
            return SemErro(s, E0911, e->pos_op, "falta o campo '%s': proc nao tem valor zero (nao ha proc nulo); de o proc no literal",
                           SemNomeTexto(s, campo->nome));
        }
        if (TipoClasseDe(&s->tipos, campo->tipo.tipo) == TC_STRUCT) {
            SemDescrever(s, campo->tipo.tipo, apontado, sizeof apontado);
            return SemErro(s, E0503, e->pos_op, "falta o campo '%s': a struct %s nao tem valor zero (tem campo ^T ou union sem padrao); de o valor no literal",
                           SemNomeTexto(s, campo->nome), apontado);
        }
        if (TipoClasseDe(&s->tipos, campo->tipo.tipo) == TC_ARRAY || TipoClasseDe(&s->tipos, campo->tipo.tipo) == TC_UNIAO) {
            SemDescrever(s, campo->tipo.tipo, apontado, sizeof apontado);
            return SemErro(s, E0503, e->pos_op, "falta o campo '%s': %s%s nao tem valor zero; de o valor no literal",
                           SemNomeTexto(s, campo->nome), TipoClasseDe(&s->tipos, campo->tipo.tipo) == TC_UNIAO ? "union " : "",
                           apontado);
        }
        SemDescrever(s, TipoDe(&s->tipos, campo->tipo.tipo)->a, apontado, sizeof apontado);
        return SemErro(s, E0503, e->pos_op, "falta o campo '%s': '^%s' nunca e nulo e nao tem valor zero; de o valor no literal ou use '?^%s'",
                       SemNomeTexto(s, campo->nome), apontado, apontado);
    }
    return true;
}

bool SemLiteralStruct(Semantica *s, Expr *e)
{
    if (__builtin_expect(e->nome == SYM_QUAT, 0)) SemQuatGarantir(s); /* v1.11 L14a */
    if (__builtin_expect(e->nome == SYM_ALLOCATOR, 0)) return SemAllocatorLiteral(s, e); /* B01a: o Allocator vem do chamador, nao de literal */
    const Simbolo *y = SemAcharSimbolo(s, e->nome, e->modulo);
    NomeUsado uso = {e->nome, e->pos_op, e->modulo};
    if (!SemVisivel(s, y, &uso)) return false;
    if (__builtin_expect(y->categoria == SIM_STRUCT && SemSimboloVeneno(y), 0)) { /* L8: struct com erro: os campos do literal nao sao conferidos */
        e->tipo = SemUsoDeVeneno(s);
        return true;
    }
    if (y->categoria != SIM_STRUCT) {
        return SemErro(s, E0201, e->pos_op, "struct '%s' nao declarada", SemNomeTexto(s, e->nome));
    }
    if (s->tem_opaco && y->decl->externo) {
        return SemErro(s, E0908, e->pos_op, "o tipo opaco '%s' (de foreign) nao se cria na Forja: quem cria e uma funcao da biblioteca C (ela devolve ^%s)",
                       SemNomeTexto(s, e->nome), SemNomeTexto(s, e->nome));
    }
    for (uint32_t i = 0; i < e->nargs; i++) {
        if (!ConferirCampo(s, e, y->decl, i)) return false;
    }
    if (!ConferirCamposPonteiro(s, e, y->decl)) return false;
    e->ref = REF_STRUCT;
    e->ref_id = y->indice;
    e->tipo = y->tipo;
    return true;
}
