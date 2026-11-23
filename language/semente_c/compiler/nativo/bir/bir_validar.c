/* bir_validar.c - BirValidar: confere a forma da BIR (blocos terminados, alvos, vregs, classes, simbolos). Falha = X0904.
 * Versao: 0.10.1
 * Changelog: 0.10.1 - 0.21.0 F5-a: LIMITE de ArestaUnica (niveis 4); codigo igual.
 *            0.10.0 - S2 (D0703): BIR_SPLAT e ADD SUB MUL DIV de V128 nas formas 4XF32 e 2XF64 (ContaVetorialPermitida, SplatPermitido); V128 inteiro segue recusado.
 *            0.9.0 - S1b (D0703): BC_V128 so em COPIA, CARREGA e GRAVA (forma em aux < BVEC_QTD); recusa em operacao escalar, chamada, parametro e retorno.
 *            0.8.0 - R5 (SIMD): SelecionaValida (BIR_SELECIONA).
 *            0.7.2 - S1a (D0703): comparacao de classe real trocada por BirClasseEhReal (mesmo codigo).
 *            0.7.1 - M3 F9b-2: decisoes extraidas de OperandoValido, OpTambemEmReal, ContaUnariaValida, ContaBinariaValida, ConversaoRealValida, ArestaPara, FatoDaAresta, GuardaValida,
 *                    ChamadaValida, TerminadorValido, FormaValida (switch por operacao), FuncaoValida e SimbolosValidos.
 *            0.7.0 - M3 P21v-c: BIR_CHAMA_IND valida como CHAMA (AlvoDaChamadaValido: b = vreg I64 sem tam no lugar do simbolo; ArgumentosValidos extraida).
 *            0.6.0 - Linguagem v1.11 L13: RaizValida (BIR_RAIZ: dst e a reais da mesma classe, sem b).
 *            0.5.0 - M2 P11b: agregado por valor (BirOperando.tam so em vreg I64; CHAMA.a com tam = destino do retorno agregado; RETORNA.a.tam = retorno_tam; param_tam e retorno_tam
 *                    na classe BC_I64 por BirAgregadosValidos).
 *            0.4.0 - M2 P11a: as 5 operacoes de memoria e os slots da funcao vao para bir_validar_mem.c (X0904 "forma invalida" / slot invalido).
 *            0.3.0 - M2 P10b: real (ADD SUB MUL DIV NEG CONST COPIA CMP com BCOND_F*; as demais operacoes recusam classe F*), I_PARA_F e F_PARA_F, a mascara de sinal do NEG real
 *                    (BS_DADO de 16 bytes alinhado em 16), DIV de real sem guarda e BS_DADO nos simbolos. CMP com BCOND inteira em real (ou o inverso) e X0904.
 *            0.2.0 - M2 P9: DIV, REM, SHL, SHR e NOT; a pre-condicao de DIV/REM/SHL/SHR e conferida pela forma do grafo (GuardaValida): divisor ou
 *                    contador imediato seguro, ou unico predecessor que testa o vreg (sem sinal: != 0; deslocamento: < largura; com sinal: != -1 e,
 *                    antes dele, != 0). Sem a guarda o X0904 sai como "forma invalida" da operacao.
 *            0.1.1 - M2 P9a: Reprova recebe a causa ja formatada (sem varargs; regra 5.4 a).
 *            0.1.0 - backend x64 P6: primeira versao.
 * Verificacao: construir.bat && teste_nativo.exe (verificacoes de unidade da BIR) e teste_nativo.exe bir <arq.fj>
 */
#include <stdio.h>

#include "nativo/bir/bir_interno.h"

#define REPROVA_CAUSA_MAX 300

typedef struct {
    const BirPrograma *p;
    const BirFuncao *f;
    const BirInstr *i;
    Diag *erro;
    uint32_t bloco, indice; /* bloco e posicao da instrucao em validacao */
} Validador;

/* O que se sabe de um vreg ao entrar num bloco pela unica aresta que chega nele: "vreg cond imed" vale. */
typedef struct { uint32_t vreg, bloco; uint8_t cond; int64_t imed; } FatoGuarda;

/* Inversa de cada BirCond (o ramo "falso" de um DESVIA vale a condicao negada). */
static const uint8_t COND_INVERSA[] = {
    BCOND_NE, BCOND_EQ, BCOND_GE_S, BCOND_GT_S, BCOND_LE_S, BCOND_LT_S, BCOND_GE_U, BCOND_GT_U, BCOND_LE_U, BCOND_LT_U,
};

static bool Reprova(const Validador *v, const char *causa);

static bool Reprova(const Validador *v, const char *causa)
{
    return DiagRelatar(v->erro, X0904, v->i != NULL && v->i->pos != BIR_SEM_POS ? v->i->pos : 0, "BIR invalida na funcao %s: %s",
                       v->p->simbolos[v->f->simbolo].nome, causa);
}

static bool EhTerminador(uint8_t op)
{
    return op == BIR_SALTA || op == BIR_DESVIA || op == BIR_RETORNA || op == BIR_ARMADILHA;
}

static bool EhContaOv(uint8_t op)
{
    return op >= BIR_ADD_OV && op <= BIR_NEG_OV;
}

/* Agregado: so o endereco, num vreg I64. */
static bool AgregadoInvalido(const BirOperando *o)
{
    return o->tam != 0 && (o->tipo != BO_VREG || o->classe != BC_I64);
}

static bool OperandoValido(const Validador *v, const BirOperando *o)
{
    if (AgregadoInvalido(o))
        return false;
    if (o->tipo == BO_VREG)
        return o->id < v->f->nvregs && v->f->classe_vreg[o->id] == o->classe;
    if (o->tipo == BO_IMED)
        return o->classe != BC_NADA;
    if (o->tipo == BO_SIMBOLO)
        return o->id < v->p->nsimbolos;
    return o->tipo == BO_NADA;
}

static bool DestinoValido(const Validador *v)
{
    const BirInstr *i = v->i;

    if (i->dst == BIR_SEM_VREG)
        return false;
    return i->dst < v->f->nvregs && v->f->classe_vreg[i->dst] == i->classe;
}

/* O NEG real leva em b o simbolo da mascara de sinal: um BS_DADO de 16 bytes alinhado em 16 (xorps xmm,m128). */
static bool MascaraDeSinalValida(const Validador *v)
{
    const BirInstr *i = v->i;
    const BirSimbolo *s;

    if (i->b.tipo != BO_SIMBOLO || i->b.id >= v->p->nsimbolos)
        return false;
    s = &v->p->simbolos[i->b.id];
    return s->tipo == BS_DADO && s->tam == 16 && s->alinhamento >= 16;
}

static bool ClasseReal(uint8_t classe)
{
    return BirClasseEhReal(classe);
}

/* Operacoes que existem em real: o resto (and or xor not shl shr rem, ext, trunc e *_ov) so em inteiro. */
static bool OpTambemEmReal(uint8_t op)
{
    switch (op) {
    case BIR_CONST: case BIR_COPIA: case BIR_NEG: case BIR_ADD: case BIR_SUB: case BIR_MUL: case BIR_DIV: case BIR_CMP: return true;
    default: return false;
    }
}

/* Classe real so passa nas operacoes que existem em real. */
static bool ClasseRealPermitida(const BirInstr *i)
{
    return !(ClasseReal(i->classe) || ClasseReal(i->a.classe)) || OpTambemEmReal(i->op);
}

/* A largura de dst e a por operacao: CONST traz imediato, EXT alarga, TRUNC estreita, o resto (NEG real com mascara de sinal) mantem. */
static bool LarguraUnariaValida(const Validador *v)
{
    const BirInstr *i = v->i;

    switch (i->op) {
    case BIR_CONST: return i->a.tipo == BO_IMED && i->a.classe == i->classe;
    case BIR_EXT_S:
    case BIR_EXT_Z: return i->a.classe < i->classe;
    case BIR_TRUNC: return i->a.classe > i->classe;
    case BIR_NEG:
        if (ClasseReal(i->classe) && !MascaraDeSinalValida(v))
            return false;
        break;
    default: break;
    }
    return i->a.classe == i->classe;
}

/* CONST COPIA NEG e as extensoes: dst e a; so a largura muda nas EXT e TRUNC. */
static bool ContaUnariaValida(const Validador *v)
{
    const BirInstr *i = v->i;

    if (!DestinoValido(v) || i->a.tipo == BO_NADA || i->a.tipo == BO_SIMBOLO)
        return false;
    if (!ClasseRealPermitida(i))
        return false;
    return LarguraUnariaValida(v);
}

/* CMP: inteiro com BCOND_* de inteiro, real com BCOND_F*; resultado sempre bool. */
static bool ComparacaoValida(const BirInstr *i)
{
    return i->classe == BC_I8 && i->aux <= BCOND_FGE && ClasseReal(i->a.classe) == (i->aux >= BCOND_FEQ);
}

static bool OperandosBinariosValidos(const Validador *v)
{
    const BirInstr *i = v->i;

    return DestinoValido(v) && i->a.tipo != BO_NADA && i->b.tipo != BO_NADA && i->a.classe == i->b.classe;
}

static bool ContaBinariaValida(const Validador *v)
{
    const BirInstr *i = v->i;

    if (!OperandosBinariosValidos(v))
        return false;
    if (ClasseReal(i->a.classe) && !OpTambemEmReal(i->op))
        return false;
    if (i->op == BIR_CMP)
        return ComparacaoValida(i);
    return i->a.classe == i->classe;
}

/* RAIZ (L13): dst e a sao reais da mesma classe (sqrtss/sqrtsd), sem b. */
static bool RaizValida(const Validador *v)
{
    const BirInstr *i = v->i;

    if (!DestinoValido(v) || !ClasseReal(i->classe) || i->a.classe != i->classe || i->b.tipo != BO_NADA)
        return false;
    return i->a.tipo == BO_VREG || i->a.tipo == BO_IMED;
}

/* SELECIONA (R5): dst, a e b da mesma classe (inteiro, ponteiro ou real; real so no padrao minss/maxss), args = os 2 operandos da comparacao (vreg ou imediato, mesma classe, aux
 * inteiro para inteiro e F* para real), todos sem agregado e sem extensao. */
static bool SelecionaOperandoSimples(const BirOperando *o)
{
    return (o->tipo == BO_VREG || o->tipo == BO_IMED) && o->tam == 0 && o->extensao == BE_NENHUMA;
}

static bool SelecionaValida(const Validador *v)
{
    const BirInstr *i = v->i;
    bool maximo;
    const BirOperando *primeiro, *segundo;

    if (!DestinoValido(v) || i->classe == BC_NADA || i->nargs != 2 || i->args == NULL || i->aux > BCOND_FGE)
        return false;
    if (!OperandoValido(v, &i->args[0]) || !OperandoValido(v, &i->args[1]) || i->args[0].classe != i->args[1].classe || i->args[0].classe == BC_NADA)
        return false;
    if (ClasseReal(i->args[0].classe) != (i->aux >= BCOND_FEQ) || ClasseReal(i->args[0].classe) != ClasseReal(i->classe))
        return false;
    if (!SelecionaOperandoSimples(&i->a) || !SelecionaOperandoSimples(&i->b) || !SelecionaOperandoSimples(&i->args[0]) || !SelecionaOperandoSimples(&i->args[1]))
        return false;
    if (i->a.classe != i->classe || i->b.classe != i->classe)
        return false;
    return !ClasseReal(i->classe) || BirSelecionaMinMax(i, &maximo, &primeiro, &segundo);
}

/* I_PARA_F: a e I32 (com ou sem sinal) ou I64 (so com sinal; o uint de 64 bits vira blocos no construtor). F_PARA_F: troca F32 <-> F64. */
static bool InteiroParaRealValido(const BirInstr *i)
{
    if (i->aux > BIR_SEM_SINAL || (i->a.classe != BC_I32 && i->a.classe != BC_I64))
        return false;
    return i->a.classe == BC_I32 || i->aux == BIR_SINAL;
}

static bool DestinoRealComOrigemValida(const Validador *v)
{
    const BirInstr *i = v->i;

    return DestinoValido(v) && ClasseReal(i->classe) && (i->a.tipo == BO_VREG || i->a.tipo == BO_IMED);
}

static bool ConversaoRealValida(const Validador *v)
{
    const BirInstr *i = v->i;

    if (!DestinoRealComOrigemValida(v))
        return false;
    if (i->op == BIR_F_PARA_F)
        return ClasseReal(i->a.classe) && i->a.classe != i->classe;
    return InteiroParaRealValido(i);
}

static bool ContaOvValida(const Validador *v)
{
    const BirInstr *i = v->i;

    if (!ContaBinariaValida(v) || i->aux > BIR_SEM_SINAL || i->alvo[0] >= v->f->nblocos)
        return false;
    return v->f->blocos[i->alvo[0]].frio;
}

/* ---- guarda das pre-condicoes de DIV, REM, SHL e SHR ---- */

static unsigned BitsDaClasse(uint8_t classe)
{
    return classe == BC_I8 ? 8u : (classe == BC_I16 ? 16u : (classe == BC_I32 ? 32u : 64u));
}

/* O imediato como o numero sem sinal da largura da classe (-1 vira todos os bits ligados). */
static uint64_t SemSinalNaClasse(int64_t imed, uint8_t classe)
{
    unsigned bits = BitsDaClasse(classe);

    return bits == 64u ? (uint64_t)imed : (uint64_t)imed & ((1ull << bits) - 1u);
}

static bool EhDeslocamento(uint8_t op)
{
    return op == BIR_SHL || op == BIR_SHR;
}

static bool ImediatoSeguro(const BirInstr *i)
{
    uint64_t valor = SemSinalNaClasse(i->b.imed, i->classe);

    if (EhDeslocamento(i->op))
        return valor < BitsDaClasse(i->classe);
    if (i->aux == BIR_SEM_SINAL)
        return valor != 0;
    return valor != 0 && valor != SemSinalNaClasse(-1, i->classe);
}

static bool SemDefinicaoDe(const BirBloco *bloco, uint32_t ate, uint32_t vreg)
{
    for (uint32_t k = 0; k < ate; k++) {
        if (bloco->instrs[k].dst == vreg)
            return false;
    }
    return true;
}

/* A instrucao e uma aresta para alvo? ramo 0 = SALTA, DESVIA verdadeiro ou frio do *_OV; ramo 1 = DESVIA falso. */
static bool ArestaPara(const BirInstr *i, uint8_t ramo, uint32_t alvo)
{
    if (ramo == 1)
        return i->op == BIR_DESVIA && i->alvo[1] == alvo;
    return (i->op == BIR_SALTA || i->op == BIR_DESVIA || EhContaOv(i->op)) && i->alvo[0] == alvo;
}

/* O bloco alvo tem UMA so aresta de entrada (o bloco de entrada da funcao tem a da chamada, entao nunca serve)?
 * LIMITE: niveis 4 (bloco, instrucao, ramo e a conta de arestas; partir criaria helper de uso unico). */
static bool ArestaUnica(const BirFuncao *f, uint32_t alvo, uint32_t *origem, uint8_t *ramo)
{
    uint32_t arestas = 0;

    for (uint32_t b = 0; b < f->nblocos && alvo != 0; b++) {
        for (uint32_t k = 0; k < f->blocos[b].n; k++) {
            for (uint8_t r = 0; r < 2; r++) {
                if (!ArestaPara(&f->blocos[b].instrs[k], r, alvo))
                    continue;
                arestas++;
                *origem = b;
                *ramo = r;
            }
        }
    }
    return arestas == 1;
}

/* "cmp vN, #imed" seguido do "desvia" que testa o resultado dele. */
static bool CmpComImediatoDoDesvia(const BirInstr *cmp, const BirInstr *desvia)
{
    if (desvia->op != BIR_DESVIA || cmp->op != BIR_CMP || cmp->dst != desvia->a.id || cmp->aux >= sizeof COND_INVERSA)
        return false;
    return cmp->a.tipo == BO_VREG && cmp->b.tipo == BO_IMED;
}

/* O unico predecessor termina em "cmp vN, #imed" + "desvia": a aresta de entrada garante "vN cond imed". */
static bool FatoDaAresta(const BirFuncao *f, uint32_t origem, uint8_t ramo, FatoGuarda *fato)
{
    const BirBloco *bloco = &f->blocos[origem];
    const BirInstr *desvia, *cmp;

    if (bloco->n < 2)
        return false;
    desvia = &bloco->instrs[bloco->n - 1];
    cmp = &bloco->instrs[bloco->n - 2];
    if (!CmpComImediatoDoDesvia(cmp, desvia))
        return false;
    *fato = (FatoGuarda){.vreg = cmp->a.id, .bloco = origem, .cond = ramo == 0 ? cmp->aux : COND_INVERSA[cmp->aux], .imed = cmp->b.imed};
    return true;
}

static bool FatoDeEntrada(const BirFuncao *f, uint32_t bloco, FatoGuarda *fato)
{
    uint32_t origem = 0;
    uint8_t ramo = 0;

    return ArestaUnica(f, bloco, &origem, &ramo) && FatoDaAresta(f, origem, ramo, fato);
}

/* Contador: vN < k (k <= largura) ou vN <= k (k < largura), sem sinal. */
static bool FatoLimitaContador(const FatoGuarda *fato, uint8_t classe)
{
    uint64_t k = SemSinalNaClasse(fato->imed, classe);

    return (fato->cond == BCOND_LT_U && k <= BitsDaClasse(classe)) || (fato->cond == BCOND_LE_U && k < BitsDaClasse(classe));
}

static bool FatoNaoZero(const FatoGuarda *fato)
{
    return (fato->cond == BCOND_NE || fato->cond == BCOND_GT_U) && fato->imed == 0;
}

/* Com sinal: o bloco so e alcancado com b != -1 e o predecessor so com b != 0 (e o predecessor nao mexe em b entre os dois testes). */
static bool GuardaComSinal(const Validador *v, const FatoGuarda *menos_um)
{
    const BirBloco *teste = &v->f->blocos[menos_um->bloco];
    FatoGuarda zero;

    if (menos_um->cond != BCOND_NE || SemSinalNaClasse(menos_um->imed, v->i->classe) != SemSinalNaClasse(-1, v->i->classe))
        return false;
    if (!SemDefinicaoDe(teste, teste->n, menos_um->vreg))
        return false;
    return FatoDeEntrada(v->f, menos_um->bloco, &zero) && zero.vreg == menos_um->vreg && FatoNaoZero(&zero);
}

/* O divisor (ou contador) e um vreg ainda nao redefinido no bloco, que a aresta de entrada testa. */
static bool FatoDoDivisor(const Validador *v, FatoGuarda *fato)
{
    const BirInstr *i = v->i;

    if (i->b.tipo != BO_VREG || !SemDefinicaoDe(&v->f->blocos[v->bloco], v->indice, i->b.id))
        return false;
    return FatoDeEntrada(v->f, v->bloco, fato) && fato->vreg == i->b.id;
}

static bool GuardaValida(const Validador *v)
{
    const BirInstr *i = v->i;
    FatoGuarda fato;

    if (i->b.tipo == BO_IMED)
        return ImediatoSeguro(i);
    if (!FatoDoDivisor(v, &fato))
        return false;
    if (EhDeslocamento(i->op))
        return FatoLimitaContador(&fato, i->classe);
    return i->aux == BIR_SEM_SINAL ? FatoNaoZero(&fato) : GuardaComSinal(v, &fato);
}

static bool ContaGuardadaValida(const Validador *v)
{
    const BirInstr *i = v->i;

    if (!ContaBinariaValida(v))
        return false;
    if (ClasseReal(i->a.classe) || i->a.classe == BC_V128) /* a / b de real (e de vetor de real, S2) nao tem guarda: 1/0 e inf, 0/0 e NaN */
        return i->op == BIR_DIV;
    if ((i->op == BIR_DIV || i->op == BIR_REM) && i->aux > BIR_SEM_SINAL)
        return false;
    return GuardaValida(v);
}

static bool ArgumentosValidos(const Validador *v)
{
    const BirInstr *i = v->i;

    if (i->nargs > 0 && i->args == NULL)
        return false;
    for (uint32_t k = 0; k < i->nargs; k++) {
        if (!OperandoValido(v, &i->args[k]) || i->args[k].tipo == BO_NADA)
            return false;
    }
    return true;
}

/* CHAMA_IND: o endereco da funcao e um vreg I64 sem tam (b); CHAMA e ARMADILHA: um simbolo que nao e texto nem dado. */
static bool AlvoDaChamadaValido(const Validador *v)
{
    const BirInstr *i = v->i;
    const BirSimbolo *s;

    if (i->op == BIR_CHAMA_IND)
        return i->b.tipo == BO_VREG && i->b.classe == BC_I64 && i->b.tam == 0;
    if (i->simbolo >= v->p->nsimbolos)
        return false;
    s = &v->p->simbolos[i->simbolo];
    return s->tipo != BS_TEXTO && s->tipo != BS_DADO;
}

static bool EhChamada(uint8_t op)
{
    return op == BIR_CHAMA || op == BIR_CHAMA_IND;
}

/* Retorno agregado: o endereco de destino em a; so o CHAMA e o CHAMA_IND. */
static bool RetornoAgregadoValido(const BirInstr *i)
{
    return EhChamada(i->op) && i->a.tam != 0 && i->dst == BIR_SEM_VREG && i->classe == BC_NADA;
}

static bool ChamadaValida(const Validador *v)
{
    const BirInstr *i = v->i;

    if (!AlvoDaChamadaValido(v) || !ArgumentosValidos(v))
        return false;
    if (i->a.tipo != BO_NADA)
        return RetornoAgregadoValido(i);
    if (EhChamada(i->op) && i->dst != BIR_SEM_VREG)
        return DestinoValido(v);
    return true;
}

static bool DesviaValido(const Validador *v)
{
    const BirInstr *i = v->i;

    return i->alvo[0] < v->f->nblocos && i->alvo[1] < v->f->nblocos && i->a.tipo == BO_VREG && i->a.classe == BC_I8;
}

static bool RetornaValido(const Validador *v)
{
    const BirInstr *i = v->i;

    return v->f->retorno == BC_NADA ? i->a.tipo == BO_NADA
                                    : (i->a.tipo != BO_NADA && i->a.classe == v->f->retorno && i->a.tam == v->f->retorno_tam);
}

static bool TerminadorValido(const Validador *v)
{
    const BirInstr *i = v->i;

    switch (i->op) {
    case BIR_SALTA: return i->alvo[0] < v->f->nblocos;
    case BIR_DESVIA: return DesviaValido(v);
    case BIR_RETORNA: return RetornaValido(v);
    default: return ChamadaValida(v);
    }
}

static bool EndSimboloValido(const Validador *v)
{
    const BirInstr *i = v->i;

    return DestinoValido(v) && i->classe == BC_I64 && i->a.tipo == BO_SIMBOLO && i->a.id < v->p->nsimbolos;
}

/* Forma das operacoes que nao sao de memoria (op < BIR_END_SLOT); o que nao tem regra propria e conta binaria. */
static bool FormaDeOperacao(const Validador *v, uint8_t op)
{
    switch (op) {
    case BIR_RAIZ: return RaizValida(v);
    case BIR_SELECIONA: return SelecionaValida(v);
    case BIR_SPLAT: return DestinoValido(v) && v->i->b.tipo == BO_NADA; /* classe e origem: SplatPermitido (UsaVetor) */
    case BIR_END_SIMBOLO: return EndSimboloValido(v);
    case BIR_CHAMA:
    case BIR_CHAMA_IND: return ChamadaValida(v);
    case BIR_ADD_OV: case BIR_SUB_OV: case BIR_MUL_OV: case BIR_NEG_OV: return ContaOvValida(v);
    case BIR_SALTA: case BIR_DESVIA: case BIR_RETORNA: case BIR_ARMADILHA: return TerminadorValido(v);
    case BIR_CONST:
    case BIR_COPIA:
    case BIR_NEG:
    case BIR_EXT_S:
    case BIR_EXT_Z:
    case BIR_TRUNC:
    case BIR_NOT: return ContaUnariaValida(v);
    case BIR_I_PARA_F:
    case BIR_F_PARA_F: return ConversaoRealValida(v);
    case BIR_DIV:
    case BIR_REM:
    case BIR_SHL:
    case BIR_SHR: return ContaGuardadaValida(v);
    default: return ContaBinariaValida(v);
    }
}

/* Regra de forma da instrucao pela operacao; a mensagem diz qual op reprovou. */
static bool FormaValida(const Validador *v)
{
    uint8_t op = v->i->op;

    if (op >= BIR_QTD)
        return false;
    if (op >= BIR_END_SLOT)
        return BirMemoriaValida(v->f, v->i);
    return FormaDeOperacao(v, op);
}

/* BC_V128 aparece na instrucao (resultado, operando ou argumento)? */
static bool UsaVetor(const BirInstr *i)
{
    if (i->classe == BC_V128 || i->a.classe == BC_V128 || i->b.classe == BC_V128)
        return true;
    for (uint32_t k = 0; k < i->nargs && i->args != NULL; k++) {
        if (i->args[k].classe == BC_V128)
            return true;
    }
    return false;
}

/* S2: ADD SUB MUL DIV de V128 so nas formas de real (4XF32, 2XF64), com os dois operandos em vreg V128; o vetor inteiro (4XI32, 2XI64) segue recusado. */
static bool ContaVetorialPermitida(const BirInstr *i)
{
    bool forma_real = i->aux == BVEC_4XF32 || i->aux == BVEC_2XF64;

    return forma_real && i->classe == BC_V128 && i->a.tipo == BO_VREG && i->b.tipo == BO_VREG && i->a.classe == BC_V128 && i->b.classe == BC_V128;
}

/* S2: SPLAT difunde um vreg escalar da classe da forma (4XF32 com F32, 2XF64 com F64) num V128. */
static bool SplatPermitido(const BirInstr *i)
{
    uint8_t origem = i->aux == BVEC_4XF32 ? BC_F32 : BC_F64;

    if (i->aux != BVEC_4XF32 && i->aux != BVEC_2XF64)
        return false;
    return i->classe == BC_V128 && i->a.tipo == BO_VREG && i->a.classe == origem && i->b.tipo == BO_NADA;
}

/* Vetor so em COPIA, CARREGA, GRAVA, SPLAT e ADD SUB MUL DIV de forma real, com a forma em aux e valor em vreg (sem imediato V128); chamada e retorno ainda nao suportam. */
static bool VetorPermitido(const BirInstr *i)
{
    if (i->aux >= BVEC_QTD)
        return false;
    switch (i->op) {
    case BIR_COPIA: return i->a.tipo == BO_VREG;
    case BIR_CARREGA: return true;
    case BIR_GRAVA: return i->b.tipo == BO_VREG;
    case BIR_SPLAT: return SplatPermitido(i);
    case BIR_ADD: case BIR_SUB: case BIR_MUL: case BIR_DIV: return ContaVetorialPermitida(i);
    default: return false;
    }
}

static bool InstrValida(Validador *v, const BirInstr *i, bool ultima)
{
    char causa[REPROVA_CAUSA_MAX];

    v->i = i;
    if (!OperandoValido(v, &i->a) || !OperandoValido(v, &i->b)) {
        snprintf(causa, sizeof causa, "operando invalido em op %u", (unsigned)i->op);
        return Reprova(v, causa);
    }
    if (UsaVetor(i) && !VetorPermitido(i)) {
        snprintf(causa, sizeof causa, "classe v128 so em copia, carrega, grava, splat e add/sub/mul/div de f32/f64 com forma valida (op %u; chamada e retorno de v128 ainda nao suportados)", (unsigned)i->op);
        return Reprova(v, causa);
    }
    if (!FormaValida(v)) {
        snprintf(causa, sizeof causa, "forma invalida em op %u", (unsigned)i->op);
        return Reprova(v, causa);
    }
    if (EhTerminador(i->op) != ultima) {
        snprintf(causa, sizeof causa, ultima ? "bloco termina em op %u, que nao e terminador" : "terminador op %u no meio do bloco", (unsigned)i->op);
        return Reprova(v, causa);
    }
    return true;
}

static bool BlocoValido(Validador *v, uint32_t indice)
{
    const BirBloco *b = &v->f->blocos[indice];

    char causa[REPROVA_CAUSA_MAX];

    v->i = NULL;
    if (b->n == 0) {
        snprintf(causa, sizeof causa, "bloco b%u vazio", (unsigned)indice);
        return Reprova(v, causa);
    }
    v->bloco = indice;
    for (uint32_t k = 0; k < b->n; k++) {
        v->indice = k;
        if (!InstrValida(v, &b->instrs[k], k + 1 == b->n))
            return false;
    }
    return true;
}

static bool SimboloDeFuncaoValido(const Validador *v)
{
    const BirFuncao *f = v->f;

    return f->simbolo < v->p->nsimbolos && v->p->simbolos[f->simbolo].tipo == BS_FUNCAO;
}

static bool EntradaDaFuncaoValida(const BirFuncao *f)
{
    return f->nblocos != 0 && !f->blocos[0].frio && f->nparams <= f->nvregs;
}

static bool BlocosDaFuncaoValidos(Validador *v)
{
    for (uint32_t b = 0; b < v->f->nblocos; b++) {
        if (!BlocoValido(v, b))
            return false;
    }
    return true;
}

static bool FuncaoValida(Validador *v)
{
    const BirFuncao *f = v->f;

    v->i = NULL;
    if (!SimboloDeFuncaoValido(v))
        return DiagRelatar(v->erro, X0904, 0, "BIR invalida: simbolo %u nao e uma funcao", (unsigned)f->simbolo);
    if (!EntradaDaFuncaoValida(f))
        return Reprova(v, "funcao sem bloco de entrada valido");
    if (!BirSlotsValidos(f))
        return Reprova(v, "slot com tamanho zero ou alinhamento que nao e potencia de 2 ate 16");
    if (!BirAgregadosValidos(f))
        return Reprova(v, "parametro ou retorno agregado fora da classe BC_I64");
    if (f->retorno == BC_V128)
        return Reprova(v, "retorno v128 ainda nao suportado");
    for (uint32_t k = 0; k < f->nparams; k++) {
        if (f->classe_vreg[k] == BC_V128)
            return Reprova(v, "parametro v128 ainda nao suportado");
    }
    return BlocosDaFuncaoValidos(v);
}

static bool TextoDoSimboloValido(const BirSimbolo *s)
{
    return s->tipo != BS_TEXTO || (s->bytes != NULL && s->bytes[s->tam] == '\0');
}

static bool DadoDoSimboloValido(const BirSimbolo *s)
{
    return s->tipo != BS_DADO || (s->bytes != NULL && s->alinhamento != 0 && (s->alinhamento & (s->alinhamento - 1)) == 0);
}

static bool NomeDoSimboloValido(const BirSimbolo *s)
{
    return s->tipo == BS_TEXTO || s->tipo == BS_DADO || s->nome != NULL;
}

static bool SimbolosValidos(const BirPrograma *p, Diag *erro)
{
    for (uint32_t k = 0; k < p->nsimbolos; k++) {
        const BirSimbolo *s = &p->simbolos[k];

        if (!TextoDoSimboloValido(s) || !DadoDoSimboloValido(s) || !NomeDoSimboloValido(s))
            return DiagRelatar(erro, X0904, 0, "BIR invalida: simbolo s%u sem nome, sem NUL final ou com alinhamento invalido", (unsigned)k);
    }
    return true;
}

bool BirValidar(const BirPrograma *programa, Diag *erro)
{
    Validador v = {.p = programa, .erro = erro};

    if (!SimbolosValidos(programa, erro))
        return false;
    for (uint32_t k = 0; k < programa->nfuncoes; k++) {
        v.f = &programa->funcoes[k];
        if (!FuncaoValida(&v))
            return false;
    }
    return true;
}
