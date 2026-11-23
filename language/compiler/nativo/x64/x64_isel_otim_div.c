/* x64_isel_otim_div.c - selecao pos-alocador (M7b F1, passe `divconst`): DIV e REM de I8..I64 por imediato viram multiplicacao pela constante magica e deslocamentos, sem div/idiv.
 * O plano (qual sequencia, qual magic, qual shift) vem de DivMagicaPlanejar (x64_div_magica.h, que documenta a sequencia de cada estrategia); aqui so se emite o que o emulador
 * DivMagicaEmular reproduz.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M7b F1 rodada 2: o resto fecha em sub (produto em rcx, sem neg+add); SINAL_64 e SINAL_MUL com |d| e a correcao n - (n<0 ? |d| : 0) em paralelo ao imul; POT2_SINAL com
 *                    ((n + t) and mascara) - t. As formas seguem o DivMagicaEmular 1.1.0.
 *            0.1.0 - M7b F1: primeira versao (UM, POT2, POT2_SINAL, LEMIRE, SEM_SINAL_64, MAIOR_SEM_SINAL, SINAL_MUL, SINAL_64, SINAL_MIN; resto = a - q*d).
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_otim.py --forja <forja.exe> --contra <exe da F1> --sem-passe divconst
 *
 * Scratch: so rax, rcx, rdx e r11 (o alocador nunca os da a um vreg). O dividendo e lido de uma vez (do registrador do vreg ou carregado em r11) e o dst e escrito por ultimo,
 * entao dst pode ser o mesmo registrador do dividendo (TentarDestinoFundido). O resultado de I8/I16/I32 sai SEMPRE zero-estendido (movzx / mov r32): o
 * invariante que IselOtimCarregarEm presume. Aritmetica modular: os bits baixos da conta de 32 (ou 64) bits bastam para o resto de N bits, e a normalizacao final corta o excesso.
 * Divisor 0 e -1 (com sinal) nunca chegam aqui (o lowering os resolve); a divisao sem sinal por 2^k de 32 e 64 bits segue em IselOtimDeslocaDivide (shr/and).
 */
#include "nativo/x64/x64_div_magica.h"
#include "nativo/x64/x64_isel_otim.h"

#define RAX X64_RAX
#define RCX X64_RCX
#define RDX X64_RDX

/* Largura em bits da classe BC_I8..BC_I64. */
static const uint8_t BITS_DA_CLASSE[BC_I64 + 1] = {0, 8, 16, 32, 64};

typedef struct {
    X64Ctx *c;
    DivMagicaPlano plano;
    bool resto;
    uint8_t w;    /* largura da conta: 64 para I64, 32 para I8, I16 e I32 */
    X64Reg src;   /* dividendo zero-estendido a 64 bits: o registrador do vreg, ou r11 carregado */
    X64Reg n;     /* o dividendo com sinal (so SINAL_MUL e POT2_SINAL); pode ser r11 */
    X64Reg res;   /* registrador que guarda o quociente (ou o resto); acima de N bits so vale zero em I32 e I64 quando res e o alvo */
    X64Reg trab;  /* onde a conta sem rax/rdx fixos e feita: o proprio alvo quando ele e outro registrador que nao o dividendo, senao rax */
} DivCtx;

/* ---- instrucoes pequenas ---- */

static bool Mover(X64Ctx *c, X64Reg d, X64Reg s, uint8_t w)
{
    if (d == s)
        return true;
    return X64Emitir(c, MI2(w == 64 ? FX_MOV64_RR : FX_MOV32_RR, MReg(d, w), MReg(s, w)));
}

/* mov r32 (zera o alto) ate 2^32-1, mov r64 no resto. */
static bool CarregarConstante(X64Ctx *c, X64Reg d, uint64_t v)
{
    if (v <= UINT32_MAX)
        return X64Emitir(c, MI2(FX_MOV32_RI, MReg(d, 32), MImed((int32_t)(uint32_t)v)));
    return X64Emitir(c, MI2(FX_MOV64_RI, MReg(d, 64), MImed((int64_t)v)));
}

/* LIMITE: parametros 5 (contexto, registrador, largura, sentido e contador de um deslocamento so; agrupar criaria um tipo de uso unico). */
static bool Deslocar(X64Ctx *c, X64Reg r, uint8_t w, bool aritmetico, uint32_t n)
{
    static const uint16_t FORMA[2][2] = {{FX_SHR32_RI, FX_SHR64_RI}, {FX_SAR32_RI, FX_SAR64_RI}};

    return n == 0 || X64Emitir(c, MI2(FORMA[aritmetico][w == 64], MReg(r, w), MImed(n)));
}

static bool Somar(X64Ctx *c, X64Reg d, X64Reg s, uint8_t w)
{
    return X64Emitir(c, MI2(w == 64 ? FX_ADD64_RR : FX_ADD32_RR, MReg(d, w), MReg(s, w)));
}

static bool Negar(X64Ctx *c, X64Reg r, uint8_t w)
{
    return X64Emitir(c, MI1(w == 64 ? FX_NEG64_R : FX_NEG32_R, MReg(r, w)));
}

/* LIMITE: parametros 5 (destino, fonte, largura e imediato de um imul so; agrupar criaria um tipo de uso unico).
 * d = s * imm (modulo 2^w): imul r,r,imm32 quando cabe (fora de 0x204..0x207, que colidem); senao a constante vai para um registrador (o proprio d, ou rcx quando d == s) e sai imul rr. */
static bool ImulConstante(X64Ctx *c, X64Reg d, X64Reg s, uint8_t w, int64_t imm)
{
    uint16_t rri = w == 64 ? FX_IMUL64_RRI : FX_IMUL32_RRI, rr = w == 64 ? FX_IMUL64_RR : FX_IMUL32_RR;
    X64Reg constante = d == s ? RCX : d;
    bool cabe = imm >= INT32_MIN && imm <= INT32_MAX && !(imm >= IMED_COLIDE_MIN && imm <= IMED_COLIDE_MAX);

    if (cabe)
        return X64Emitir(c, MI3(rri, MReg(d, w), MReg(s, w), MImed(imm)));
    if (!CarregarConstante(c, constante, w == 64 ? (uint64_t)imm : (uint64_t)(uint32_t)imm))
        return false;
    return X64Emitir(c, MI2(rr, MReg(d, w), MReg(d == s ? constante : s, w)));
}

/* ---- dividendo ---- */

static bool CarregarDividendo(DivCtx *k, const BirInstr *i)
{
    int reg = IselOtimRegInt(k->c, &i->a);

    if (reg >= 0) {
        k->src = (X64Reg)reg;
        return true;
    }
    k->src = X64_R11;
    return IselCarregar(k->c, X64_R11, &i->a, BE_NENHUMA);
}

/* O dividendo com sinal em k->n: o proprio registrador quando a conta de 32 (I32) ou 64 bits ja o enxerga; senao movsx/movsxd para r11 (os bits baixos de r11 seguem os de src). */
static bool PrepararDividendoComSinal(DivCtx *k)
{
    X64Ctx *c = k->c;
    uint32_t bits = k->plano.bits;

    if (bits == 64 || (bits == 32 && !k->plano.produto_64)) {
        k->n = k->src;
        return true;
    }
    k->n = X64_R11;
    if (bits == 32)
        return X64Emitir(c, MI2(FX_MOVSXD64_RR32, MReg(X64_R11, 64), MReg(k->src, 32)));
    return X64Emitir(c, MI2(bits == 16 ? FX_MOVSX32_RR16 : FX_MOVSX32_RR8, MReg(X64_R11, 32), MReg(k->src, (uint8_t)bits)));
}

/* ---- quociente de cada estrategia ---- */

static bool QuocientePot2SemSinal(DivCtx *k)
{
    k->res = k->trab;
    return Mover(k->c, k->trab, k->src, k->w) && Deslocar(k->c, k->trab, k->w, false, k->plano.shift);
}

static bool QuocientePot2ComSinal(DivCtx *k)
{
    X64Ctx *c = k->c;
    uint8_t w = k->w;
    uint32_t s = k->plano.shift;
    X64Reg t = k->trab;

    k->res = t;
    if (!PrepararDividendoComSinal(k) || !Mover(c, t, k->n, w))
        return false;
    return Deslocar(c, t, w, true, w - 1u) && Deslocar(c, t, w, false, w - s) && Somar(c, t, k->n, w) && Deslocar(c, t, w, true, s);
}

static bool QuocienteLemire(DivCtx *k)
{
    k->res = RDX;
    return CarregarConstante(k->c, RAX, k->plano.magic) && X64Emitir(k->c, MI1(FX_MUL64_R, MReg(k->src, 64)));
}

static bool QuocienteSemSinal64(DivCtx *k)
{
    X64Ctx *c = k->c;
    X64Reg t = k->trab;

    k->res = t;
    if (!CarregarConstante(c, RAX, k->plano.magic) || !X64Emitir(c, MI1(FX_MUL64_R, MReg(k->src, 64))))
        return false;
    if (!Mover(c, t, k->src, 64) || !X64Emitir(c, MI2(FX_SUB64_RR, MReg(t, 64), MReg(RDX, 64))))
        return false;
    return Deslocar(c, t, 64, false, 1) && Somar(c, t, RDX, 64) && Deslocar(c, t, 64, false, k->plano.shift - 1u);
}

/* res = (src cond divisor) como 0/1: MAIOR_SEM_SINAL (ae, d de 64 bits em rcx) e SINAL_MIN (e). */
static bool QuocienteComparacao(DivCtx *k, X64Cond cond)
{
    X64Ctx *c = k->c;
    uint64_t d = k->plano.divisor;

    k->res = k->trab;
    if (!X64Emitir(c, MI2(FX_XOR32_RR, MReg(k->trab, 32), MReg(k->trab, 32))))
        return false;
    if (k->plano.bits == 64) {
        if (!CarregarConstante(c, RCX, d) || !X64Emitir(c, MI2(FX_CMP64_RR, MReg(k->src, 64), MReg(RCX, 64))))
            return false;
    } else if (!X64Emitir(c, MI2(FX_CMP32_RI, MReg(k->src, 32), MImed((int32_t)(uint32_t)d)))) {
        return false;
    }
    return X64Emitir(c, MICc(FX_SETCC8_R, cond, MReg(k->trab, 8)));
}

/* I8, I16 (imul de 32 bits) e I32 (movsxd + imul de 64 bits): p = n * M, q = (p sar s) + (n < 0). */
static bool QuocienteSinalMul(DivCtx *k)
{
    X64Ctx *c = k->c;
    const DivMagicaPlano *p = &k->plano;
    uint8_t wm = p->produto_64 ? 64 : 32;
    int64_t magic = p->produto_64 ? (int64_t)p->magic : (int64_t)(int32_t)p->magic;

    X64Reg t = k->trab;

    k->res = t;
    if (!PrepararDividendoComSinal(k) || !ImulConstante(c, t, k->n, wm, magic) || !Deslocar(c, t, wm, true, p->shift))
        return false;
    if (!p->soma_bit_sinal)
        return X64Emitir(c, MI2(FX_MOV32_RR, MReg(t, 32), MReg(t, 32))); /* zera o alto do produto de 64 bits */
    /* o bit de sinal do dividendo e o bit 31 de src (I32) ou de n (I8, I16, ja estendido a 32 bits) */
    return Mover(c, RCX, p->bits == 32 ? k->src : k->n, 32) && Deslocar(c, RCX, 32, false, 31) && Somar(c, t, RCX, 32);
}

/* I64: hi64 com sinal de n * M em rdx (imul de um operando), mais n quando soma_a, sar s e mais o bit de sinal. */
static bool QuocienteSinal64(DivCtx *k)
{
    X64Ctx *c = k->c;
    const DivMagicaPlano *p = &k->plano;

    k->res = RDX;
    if (!CarregarConstante(c, RAX, p->magic) || !X64Emitir(c, MI1(FX_IMUL64_R, MReg(k->src, 64))))
        return false;
    if ((p->soma_a && !Somar(c, RDX, k->src, 64)) || !Deslocar(c, RDX, 64, true, p->shift))
        return false;
    if (!p->soma_bit_sinal)
        return true;
    k->res = k->trab;
    return Mover(c, k->trab, k->src, 64) && Deslocar(c, k->trab, 64, false, 63) && Somar(c, k->trab, RDX, 64);
}

static bool Quociente(DivCtx *k)
{
    switch (k->plano.estrategia) {
    case DIV_MAGICA_UM: k->res = k->src; return true;
    case DIV_MAGICA_POT2: return QuocientePot2SemSinal(k);
    case DIV_MAGICA_POT2_SINAL: return QuocientePot2ComSinal(k);
    case DIV_MAGICA_LEMIRE: return QuocienteLemire(k);
    case DIV_MAGICA_SEM_SINAL_64: return QuocienteSemSinal64(k);
    case DIV_MAGICA_MAIOR_SEM_SINAL: return QuocienteComparacao(k, X64_CC_AE);
    case DIV_MAGICA_SINAL_MUL: return QuocienteSinalMul(k);
    case DIV_MAGICA_SINAL_64: return QuocienteSinal64(k);
    case DIV_MAGICA_SINAL_MIN: return QuocienteComparacao(k, X64_CC_E);
    default: return false;
    }
}

/* ---- negacao, resto e fim ---- */

/* d < 0: o quociente sai negado. O registrador do dividendo nunca e alterado: UM copia para rax antes. */
static bool NegarQuociente(DivCtx *k)
{
    if (!k->plano.negar)
        return true;
    if (k->res == k->src) {
        if (!Mover(k->c, k->trab, k->src, k->w))
            return false;
        k->res = k->trab;
    }
    return Negar(k->c, k->res, k->w);
}

/* O divisor como a conta de largura w o enxerga: com sinal estendido de N bits, sem sinal zero-estendido. */
static int64_t ValorDoDivisor(const DivMagicaPlano *p, uint8_t w)
{
    uint32_t sobra = 64u - p->bits;
    int64_t v = p->com_sinal ? (int64_t)(p->divisor << sobra) >> sobra : (int64_t)p->divisor;

    return w == 32 ? (int64_t)(int32_t)v : v;
}

/* r = a - q * d: o produto vai para rcx e o resto fecha em sub (sem neg + add). Em d = 1 o resto e 0. */
static bool RestoDoQuociente(DivCtx *k)
{
    X64Ctx *c = k->c;
    X64Reg r = k->trab;

    if (k->plano.estrategia == DIV_MAGICA_UM) {
        k->res = r;
        return X64Emitir(c, MI2(FX_XOR32_RR, MReg(r, 32), MReg(r, 32)));
    }
    if (!ImulConstante(c, RCX, k->res, k->w, ValorDoDivisor(&k->plano, k->w)) || !Mover(c, r, k->src, k->w))
        return false;
    k->res = r;
    return X64Emitir(c, MI2(k->w == 64 ? FX_SUB64_RR : FX_SUB32_RR, MReg(r, k->w), MReg(RCX, k->w)));
}

/* |d| de um divisor com sinal (a conta de 64 bits o enxerga como positivo: d = MIN nunca chega aqui). */
static int64_t ModuloDoDivisor(const DivMagicaPlano *p)
{
    int64_t d = ValorDoDivisor(p, 64);

    return d < 0 ? -d : d;
}

/* and r, imm (cabe em imm32 com sinal) ou, fora disso, mov rcx, imm + and r, rcx. */
static bool AndConstante(X64Ctx *c, X64Reg r, uint8_t w, int64_t imm)
{
    if (imm >= INT32_MIN && imm <= INT32_MAX)
        return X64Emitir(c, MI2(w == 64 ? FX_AND64_RI : FX_AND32_RI, MReg(r, w), MImed(imm)));
    return CarregarConstante(c, RCX, (uint64_t)imm) && X64Emitir(c, MI2(FX_AND64_RR, MReg(r, 64), MReg(RCX, 64)));
}

/* u = n - (n < 0 ? mag : 0) em t, pela conta de largura w (n em sg): mov, sar, and mag, neg, add. Corre em paralelo ao imul do quociente. */
static bool CorrigirDividendoNegativo(DivCtx *k, X64Reg sg, uint8_t w, int64_t mag)
{
    X64Ctx *c = k->c;
    X64Reg t = k->trab;

    return Mover(c, t, sg, w) && Deslocar(c, t, w, true, w - 1u) && AndConstante(c, t, w, mag) && Negar(c, t, w) && Somar(c, t, sg, w);
}

/* r = u - q0 * |d| com q0 em rdx (u em trab): o produto sai em rdx e o resto fecha em sub. */
static bool FecharRestoComSinal(DivCtx *k, uint8_t w, int64_t mag)
{
    k->res = k->trab;
    return ImulConstante(k->c, RDX, RDX, w, mag) && X64Emitir(k->c, MI2(w == 64 ? FX_SUB64_RR : FX_SUB32_RR, MReg(k->trab, w), MReg(RDX, w)));
}

/* SINAL_64: r = (n - (n < 0 ? |d| : 0)) - q0 * |d|, q0 = quociente sem a correcao do bit de sinal; |d| serve ao divisor negativo (o resto leva o sinal do dividendo). */
static bool RestoSinal64(DivCtx *k)
{
    X64Ctx *c = k->c;
    const DivMagicaPlano *p = &k->plano;
    int64_t mag = ModuloDoDivisor(p);

    if (!CarregarConstante(c, RAX, p->magic) || !X64Emitir(c, MI1(FX_IMUL64_R, MReg(k->src, 64))))
        return false;
    if ((p->soma_a && !Somar(c, RDX, k->src, 64)) || !Deslocar(c, RDX, 64, true, p->shift))
        return false;
    return CorrigirDividendoNegativo(k, k->src, 64, mag) && FecharRestoComSinal(k, 64, mag);
}

/* SINAL_MUL (I8, I16, I32): o mesmo resto, com q0 = (n * M) sar s em rdx e a correcao de sinal em 32 bits. */
static bool RestoSinalMul(DivCtx *k)
{
    const DivMagicaPlano *p = &k->plano;
    uint8_t wm = p->produto_64 ? 64 : 32;
    int64_t magic = p->produto_64 ? (int64_t)p->magic : (int64_t)(int32_t)p->magic;
    int64_t mag = ModuloDoDivisor(p);

    if (!PrepararDividendoComSinal(k) || !ImulConstante(k->c, RDX, k->n, wm, magic) || !Deslocar(k->c, RDX, wm, true, p->shift))
        return false;
    /* o bit de sinal do dividendo e o bit 31 de src (I32) ou de n (I8, I16, ja estendido a 32 bits) */
    return CorrigirDividendoNegativo(k, p->bits == 32 ? k->src : k->n, 32, mag) && FecharRestoComSinal(k, 32, mag);
}

/* POT2_SINAL: t = (n sar (w-1)) shr (w-s) (2^s - 1 se n < 0); r = ((n + t) and (2^s - 1)) - t. */
static bool RestoPot2ComSinal(DivCtx *k)
{
    X64Ctx *c = k->c;
    uint8_t w = k->w;
    uint32_t s = k->plano.shift;
    X64Reg r = k->trab;

    if (!PrepararDividendoComSinal(k) || !Mover(c, RDX, k->n, w) || !Deslocar(c, RDX, w, true, w - 1u) || !Deslocar(c, RDX, w, false, w - s))
        return false;
    if (!Mover(c, r, k->n, w) || !Somar(c, r, RDX, w) || !AndConstante(c, r, w, (int64_t)((1ull << s) - 1u)))
        return false;
    k->res = r;
    return X64Emitir(c, MI2(w == 64 ? FX_SUB64_RR : FX_SUB32_RR, MReg(r, w), MReg(RDX, w)));
}

/* a % 2^s sem sinal de I8/I16 = a & (2^s - 1). */
static bool RestoPot2SemSinal(DivCtx *k)
{
    int64_t mascara = (int64_t)((1ull << k->plano.shift) - 1u);

    k->res = k->trab;
    return Mover(k->c, k->trab, k->src, 32) && X64Emitir(k->c, MI2(FX_AND32_RI, MReg(k->trab, 32), MImed(mascara)));
}

/* O resultado sai zero-estendido a N bits no alvo, por ultimo (alvo pode ser o registrador do dividendo). */
static bool Finalizar(DivCtx *k, X64Reg alvo)
{
    X64Ctx *c = k->c;
    X64Reg r = k->res;

    if (r == alvo && (r == k->src || k->plano.bits >= 32))
        return true; /* as contas de 32 bits terminam em instrucao de 32 bits (alto zerado); a de 64 usa o registrador inteiro */
    switch (k->plano.bits) {
    case 64: return Mover(c, alvo, r, 64);
    case 32: return X64Emitir(c, MI2(FX_MOV32_RR, MReg(alvo, 32), MReg(r, 32)));
    case 16: return X64Emitir(c, MI2(FX_MOVZX32_RR16, MReg(alvo, 32), MReg(r, 16)));
    default: return X64Emitir(c, MI2(FX_MOVZX32_RR8, MReg(alvo, 32), MReg(r, 8)));
    }
}

/* Resto: as estrategias com sinal que tem forma propria (|d| e correcao do dividendo, sem negar o quociente); o resto sem sinal por 2^k e um and; o demais e a - q * d. */
static bool EmitirResto(DivCtx *k)
{
    DivMagicaEstrategia e = k->plano.estrategia;

    if (!k->plano.com_sinal && e == DIV_MAGICA_POT2)
        return RestoPot2SemSinal(k);
    if (e == DIV_MAGICA_SINAL_64)
        return RestoSinal64(k);
    if (e == DIV_MAGICA_SINAL_MUL)
        return RestoSinalMul(k);
    if (e == DIV_MAGICA_POT2_SINAL)
        return RestoPot2ComSinal(k);
    return Quociente(k) && NegarQuociente(k) && RestoDoQuociente(k);
}

static bool EmitirConta(DivCtx *k, X64Reg alvo)
{
    if (k->resto)
        return EmitirResto(k) && Finalizar(k, alvo);
    return Quociente(k) && NegarQuociente(k) && Finalizar(k, alvo);
}

/* O caso e deste arquivo? Preenche o plano. Sem sinal por 2^k de 32 e 64 bits fica com o shr/and de IselOtimDeslocaDivide. */
static bool PlanejarConta(DivCtx *k, const BirInstr *i)
{
    DivMagicaEstrategia e;

    if (i->classe < BC_I8 || i->classe > BC_I64 || i->a.classe != i->classe || i->b.classe != i->classe || i->a.tipo != BO_VREG)
        return false;
    k->w = i->classe == BC_I64 ? 64 : 32;
    /* o imediato pode vir com ou sem sinal: o planejador o corta aos N bits */
    if (!DivMagicaPlanejar(BITS_DA_CLASSE[i->classe], i->aux == BIR_SINAL, (uint64_t)i->b.imed, &k->plano))
        return false;
    e = k->plano.estrategia;
    return !(!k->plano.com_sinal && (e == DIV_MAGICA_POT2 || e == DIV_MAGICA_UM) && k->plano.bits >= 32);
}

int32_t IselOtimDivConst(X64Ctx *c, const BirInstr *i)
{
    DivCtx k = {.c = c, .resto = i->op == BIR_REM};
    int destino = IselOtimRegDestino(c, i->dst);
    X64Reg alvo = destino >= 0 ? (X64Reg)destino : RAX;

    if (!PlanejarConta(&k, i))
        return 0;
    if (!CarregarDividendo(&k, i))
        return -1;
    k.trab = alvo != k.src ? alvo : RAX;
    if (!EmitirConta(&k, alvo))
        return -1;
    if (destino < 0 && !IselGuardar(c, i->dst, RAX))
        return -1;
    return 1;
}

/* Passe `divconst` (tabela de passes): liga a troca na funcao; nao muda a BIR. So age com o `isel` ligado, que e quem chama o isel pos-alocador. */
void X64DivConstMarcar(const BirPrograma *programa, BirFuncao *funcao)
{
    (void)programa;
    funcao->div_const = true;
}
