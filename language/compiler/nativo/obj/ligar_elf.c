/* ligar_elf.c - o lado ELF do ligador do backend nativo (M5b P36): relocacoes dos membros de bibliotecas estaticas (ABS64, ABS32, ABS32S, REL32, GOTPCREL e GOTPCRELX),
 * celulas .got sinteticas (ADDR64, uma por simbolo), simbolos do ligador (__init_array_start e similares) e a coleta dos simbolos da Imagem.
 * Versao: 0.7.0
 * Changelog: 0.7.0 - M6 P68: LigarElfDebug vale para o PE tambem (ImageBase + RVA no ADDR64 de debug; nunca entra no .reloc, que so LigarAplicarReloc alimenta).
 *            0.6.0 - M6 P66: LigarElfDebug copia as secoes de debug do programa para Imagem.dbg e aplica os ADDR64 delas como enderecos absolutos (nunca RELATIVE).
 *            0.5.0 - M5c-b P61: bloco TLS. OS_TDATA e OS_TBSS vao para LIGAR_SAIDA_TLS (.tdata com bytes e .tbss na cauda); TPOFF32 = offset - bloco (variante II);
 *            GOTTPOFF em executavel relaxa IE -> LE (mov 8b -> c7 /0, add 03 -> 81 /0, imm32 no lugar do disp32); forma que nao relaxa = X0114; TLS sem musl = X0113;
 *            TLS de simbolo fora do bloco = X0111. Sem TLS, os bytes nao mudam.
 *            0.4.0 - M5b P41 (--pie): ADDR64 vivo (e cada celula .got com endereco) vira entrada RELATIVE em Imagem.relativos (base 0); ABS32/ABS32S e ADDR64 em .text
 *            (TEXTREL) = X0112 com a forma certa; _DYNAMIC (so no pie) = inicio do .dynamic (ElfInicioDinamico). Sem pie, os bytes nao mudam.
 *            0.3.1 - M5b P37d: OS_PDATA e OS_XDATA vao para a saida de sempre (LigarSaidaSoDoPe as descarta no ELF; sem isso o teste de ligacao ELF dava X0111).
 *            0.3.0 - M5b P37d: LigarSaidaElf (saida da secao pelo tipo OS_*, nao pelo nome) e LigarElfSemSaida (TLS = X0111).
 *            0.2.0 - M5b P36: _GLOBAL_OFFSET_TABLE_ e simbolo do ligador (inicio da .got; sem .got, X0101 como antes): o -fPIC do gcc o deixa indefinido no .o.
 *            0.1.0 - M5b P36: primeira versao. O que era so de ELF sai de ligar.c (LigarColetarSimbolos e LigarConferirFormato). GOTPCRELX relaxa para lea (mov), call ou jmp
 *                    (nop + e8/e9) quando o alvo e definido; senao cai na celula .got. TPOFF32 e GOTTPOFF = X0111. Mesmos bytes do ELF cru de antes (ADDR64 e REL32 iguais).
 * Verificacao: construir.bat && teste_nativo.exe; tests\nativo\linux\interop\ms (verificar_linux.py --pasta, no WSL).
 */
#include <stdint.h>
#include <string.h>

#include "nativo/obj/ligar_elf.h"
#include "nativo/fmt/elf.h"

#define ELF_TAM_CELULA 8u
#define ELF_TAM_CAMPO_ABS64 8u
#define ELF_TAM_CAMPO_32 4u
#define ELF_BITS_BYTE 8u
/* O campo PC-relativo acaba 4 bytes depois do P: a convencao do Objeto neutro (terceiros.c soma 4 no adendo dos membros ELF). */
#define ELF_FIM_CAMPO_PC 4
#define ELF_OP_FF 0xffu
#define ELF_OP_MOV 0x8bu
#define ELF_OP_LEA 0x8du
#define ELF_OP_NOP 0x90u
#define ELF_OP_CALL 0xe8u
#define ELF_OP_JMP 0xe9u
#define ELF_MODRM_CALL 0x15u /* ff /2 com [rip+d] */
#define ELF_MODRM_JMP 0x25u  /* ff /4 com [rip+d] */
#define ELF_MODRM_RIP 0x05u  /* mod 00, rm 101: [rip+d] */
#define ELF_MODRM_MASCARA 0xc7u
/* Ordem dos simbolos do ligador em ALCANCE_SIMBOLOS_DO_LIGADOR (ligar_alcance.c). */
#define ELF_SIMB_INIT_INI 0
#define ELF_SIMB_INIT_FIM 1
#define ELF_SIMB_FINI_INI 2
#define ELF_SIMB_FINI_FIM 3
#define ELF_SIMB_EHDR 4
#define ELF_SIMB_GOT 5
#define ELF_SIMB_DYNAMIC 6 /* so no pie: o inicio do .dynamic (ElfInicioDinamico), que o rcrt1.o do musl le */
/* TLS (M5c-b P61): GOTTPOFF relaxado para LE. mov r,[rip+d] (8b) vira mov r,imm32 (c7 /0); add r,[rip+d] (03) vira add r,imm32 (81 /0). */
#define ELF_OP_ADD 0x03u
#define ELF_OP_MOV_IMM 0xc7u
#define ELF_OP_ADD_IMM 0x81u
#define ELF_MODRM_DIRETO 0xc0u   /* mod 11: o registrador e o rm */
#define ELF_MODRM_REG_SHIFT 3
#define ELF_MODRM_REG 0x07u
#define ELF_REX_R 0x44u          /* 0100 0100: prefixo REX com o bit R (reg 8..15) */
#define ELF_REX_R_MASCARA 0xf4u  /* o bit R e o 2 (0x04) do REX; 0x40..0x4f o pedem */

/* Como o campo e relaxado: nada, mov [rip+d] -> lea, ou call/jmp [rip+d] -> nop + e8/e9 (o disp32 fica no mesmo lugar e acaba no mesmo byte). */
typedef enum { ELF_FORMA_NENHUMA, ELF_FORMA_MOV, ELF_FORMA_CALL, ELF_FORMA_JMP } LigarElfForma;

/* O que LigarElfValor precisa de uma relocacao: o objeto, a relocacao, a VA do campo e o campo na saida (o opcode vem antes dele). */
typedef struct {
    int objeto;
    const ObjReloc *reloc;
    uint64_t p;
    uint8_t *campo;
} LigarElfAplicacao;

static const ObjSimbolo *LigarElfSimbolo(const LigacaoEstado *L, int o, uint32_t i)
{
    return VecEm(&L->obj[o]->simbolos, i);
}

static bool LigarElfDefinidoFora(const LigacaoEstado *L, int o, const char *nome)
{
    for (int outro = 0; outro < LIGAR_QTD_OBJETOS; outro++) {
        int64_t j = outro == o ? -1 : ObjAcharSimbolo(L->obj[outro], nome);
        if (j < 0)
            continue;
        const ObjSimbolo *d = VecEm(&L->obj[outro]->simbolos, (size_t)j);
        if (d->global && d->secao != OBJ_SECAO_INDEFINIDA)
            return true;
    }
    return false;
}

/* Definido antes de ligar: na propria secao, ou global definido em outro objeto. A relaxacao e a celula dependem so disto (o RVA sai depois). */
static bool LigarElfDefinido(const LigacaoEstado *L, int o, uint32_t i)
{
    const ObjSimbolo *s = LigarElfSimbolo(L, o, i);
    if (s->secao != OBJ_SECAO_INDEFINIDA)
        return true;
    return s->global && LigarElfDefinidoFora(L, o, s->nome);
}

/* A forma da instrucao que o GOTPCRELX corrige: mov r, [rip+d] (opcode 8b), ou ff /2 e ff /4 com [rip+d]; o disp32 fica em off. */
static LigarElfForma LigarElfFormaDe(const ObjSecao *s, uint32_t off)
{
    const uint8_t *b = (const uint8_t *)s->bytes.dados;

    if (off < 2 || off + ELF_TAM_CAMPO_32 > s->bytes.tam)
        return ELF_FORMA_NENHUMA;
    if (b[off - 2] == ELF_OP_MOV && (b[off - 1] & ELF_MODRM_MASCARA) == ELF_MODRM_RIP)
        return ELF_FORMA_MOV;
    if (b[off - 2] == ELF_OP_FF && b[off - 1] == ELF_MODRM_CALL)
        return ELF_FORMA_CALL;
    if (b[off - 2] == ELF_OP_FF && b[off - 1] == ELF_MODRM_JMP)
        return ELF_FORMA_JMP;
    return ELF_FORMA_NENHUMA;
}

static bool LigarElfRelaxavel(const LigacaoEstado *L, int o, const ObjReloc *r)
{
    const ObjSecao *s = VecEm(&L->obj[o]->secoes, r->alvo.secao);
    return LigarElfDefinido(L, o, r->correcao.simbolo) && LigarElfFormaDe(s, r->alvo.off) != ELF_FORMA_NENHUMA;
}

/* Reescreve o opcode antes do disp32 (campo aponta para o disp32). Mesmo tamanho: o disp32 continua acabando no mesmo byte. */
static void LigarElfRelaxar(uint8_t *campo, LigarElfForma forma)
{
    if (forma == ELF_FORMA_MOV) {
        campo[-2] = ELF_OP_LEA;
        return;
    }
    campo[-2] = ELF_OP_NOP;
    campo[-1] = forma == ELF_FORMA_CALL ? ELF_OP_CALL : ELF_OP_JMP;
}

static void LigarElfEscrever(uint8_t *p, uint32_t n, uint64_t v)
{
    for (uint32_t k = 0; k < n; k++)
        p[k] = (uint8_t)(v >> (k * ELF_BITS_BYTE));
}

/* ---------- pie (P41): entradas RELATIVE ---------- */

/* Fraco indefinido vale 0 (D-R6): o campo fica com o adendo e nao recebe RELATIVE (o ld faz igual). */
static bool LigarElfFracoZero(const LigacaoEstado *L, int o, uint32_t i)
{
    const ObjSimbolo *s = LigarElfSimbolo(L, o, i);
    return *(const uint32_t *)VecEm(&L->rva_sim[o], i) == LIGAR_SEM_RVA && s->fraco && s->secao == OBJ_SECAO_INDEFINIDA;
}

/* Um campo de 64 bits que o carregador reescreve com base + valor (valor = o endereco com base 0, o adendo do R_X86_64_RELATIVE). */
static bool LigarElfRelativa(LigacaoEstado *L, uint32_t rva, uint64_t valor)
{
    ImagemRelativa *r = VecEmpurrar(&L->im->relativos);

    r->rva = rva;
    r->valor = valor;
    return true;
}

/* ADDR64 vivo no pie vira RELATIVE; dentro de .text seria TEXTREL, que o pie nao aceita (X0112). Sem pie, nada. */
static bool LigarElfPieAddr64(LigacaoEstado *L, const LigarElfAplicacao *ap, int saida, uint64_t valor)
{
    const ObjReloc *r = ap->reloc;

    if (!L->pedido->pie || r->correcao.tipo != OR_ADDR64 || LigarElfFracoZero(L, ap->objeto, r->correcao.simbolo))
        return true;
    if (saida == LIGAR_SAIDA_TEXTO)
        return DiagRelatar(L->erro, X0112, 0, "ELF pie: ADDR64 do simbolo '%s' cai em .text (TEXTREL), que o static-pie nao aceita",
                           LigarElfSimbolo(L, ap->objeto, r->correcao.simbolo)->nome);
    return LigarElfRelativa(L, (uint32_t)(ap->p - PeBaseDaImagem(L->im)), valor);
}

/* ---------- celulas .got ---------- */

static bool LigarElfMesmaCelula(const LigacaoEstado *L, const LigarCelula *c, int o, uint32_t i)
{
    const ObjSimbolo *a = LigarElfSimbolo(L, c->objeto, c->simbolo);
    const ObjSimbolo *b = LigarElfSimbolo(L, o, i);

    if (a->global && b->global)
        return strcmp(a->nome, b->nome) == 0;
    return c->objeto == o && c->simbolo == i;
}

/* Posicao da celula do simbolo (o, i) em L->celulas, ou -1 se ainda nao ha celula para ele. */
static int64_t LigarElfIndiceCelula(const LigacaoEstado *L, int o, uint32_t i)
{
    for (size_t k = 0; k < L->celulas.n; k++) {
        if (LigarElfMesmaCelula(L, VecEm(&L->celulas, k), o, i))
            return (int64_t)k;
    }
    return -1;
}

/* GOTPCREL sempre pede celula; GOTPCRELX so quando nao relaxa. */
static bool LigarElfPrecisaCelula(const LigacaoEstado *L, int o, const ObjReloc *r)
{
    if (r->correcao.tipo == OR_GOTPCREL)
        return true;
    return r->correcao.tipo == OR_GOTPCRELX && !LigarElfRelaxavel(L, o, r);
}

static bool LigarElfViva(const LigacaoEstado *L, int o, const ObjReloc *r)
{
    return ((const LigarContrib *)VecEm(&L->contrib[o], r->alvo.secao))->saida != LIGAR_SEM_SAIDA;
}

static void LigarElfPlanejarObjeto(LigacaoEstado *L, int o)
{
    const Vec *rels = &L->obj[o]->relocs;

    for (size_t i = 0; i < rels->n; i++) {
        const ObjReloc *r = VecEm(rels, i);
        if (!LigarElfViva(L, o, r) || !LigarElfPrecisaCelula(L, o, r) || LigarElfIndiceCelula(L, o, r->correcao.simbolo) >= 0)
            continue;
        LigarCelula *nova = VecEmpurrar(&L->celulas);
        nova->objeto = o;
        nova->simbolo = r->correcao.simbolo;
    }
}

void LigarElfPlanejarGot(LigacaoEstado *L)
{
    if (L->pedido->formato != IMAGEM_ELF)
        return;
    for (int o = 0; o < LIGAR_QTD_OBJETOS; o++)
        LigarElfPlanejarObjeto(L, o);
    L->tam[LIGAR_SAIDA_GOT] = (uint32_t)(L->celulas.n * ELF_TAM_CELULA);
}

void LigarElfPreencherGot(LigacaoEstado *L)
{
    if (L->tam[LIGAR_SAIDA_GOT] == 0)
        return;
    PeSecao *ps = &L->im->secoes[L->idx_im[LIGAR_SAIDA_GOT]];
    for (size_t k = 0; k < L->celulas.n; k++) {
        const LigarCelula *c = VecEm(&L->celulas, k);
        uint32_t rva = *(const uint32_t *)VecEm(&L->rva_sim[c->objeto], c->simbolo);
        uint64_t endereco = rva == LIGAR_SEM_RVA ? 0 : PeBaseDaImagem(L->im) + rva; /* fraco indefinido = 0 (D-R6) */
        LigarElfEscrever((uint8_t *)ps->bytes.dados + k * ELF_TAM_CELULA, ELF_TAM_CELULA, endereco);
        if (L->pedido->pie && endereco != 0) /* a celula guarda um endereco: no pie o carregador o soma a base */
            LigarElfRelativa(L, L->rva[LIGAR_SAIDA_GOT] + (uint32_t)(k * ELF_TAM_CELULA), endereco);
    }
}

/* ---------- valor das relocacoes ---------- */

/* VA do simbolo: a da definicao, ou 0 para o fraco indefinido (D-R6). Simbolo indefinido nao fraco nunca chega aqui (o alcance recusa antes). */
static bool LigarElfAlvoAbs(const LigacaoEstado *L, int o, uint32_t i, uint64_t *s)
{
    uint32_t rva = *(const uint32_t *)VecEm(&L->rva_sim[o], i);
    const ObjSimbolo *sim = LigarElfSimbolo(L, o, i);

    if (rva == LIGAR_SEM_RVA && sim->fraco && sim->secao == OBJ_SECAO_INDEFINIDA) {
        *s = 0;
        return true;
    }
    if (rva == LIGAR_SEM_RVA)
        return DiagRelatar(L->erro, X0903, 0, "ELF: relocacao aponta para o simbolo '%s', que cai numa secao vazia", sim->nome);
    *s = PeBaseDaImagem(L->im) + rva;
    return true;
}

static bool LigarElfAbs32(const LigacaoEstado *L, const LigarElfAplicacao *ap, int64_t v, uint64_t *valor)
{
    bool sem_sinal = ap->reloc->correcao.tipo == OR_ABS32;
    bool cabe = sem_sinal ? (v >= 0 && v <= (int64_t)UINT32_MAX) : (v >= INT32_MIN && v <= INT32_MAX);

    if (L->pedido->pie)
        return DiagRelatar(L->erro, X0112, 0, "ELF pie: relocacao absoluta de 32 bits do simbolo '%s' nao cabe em PIE (o campo nao recebe a base)",
                           LigarElfSimbolo(L, ap->objeto, ap->reloc->correcao.simbolo)->nome);
    if (!cabe)
        return DiagRelatar(L->erro, X0903, 0, "ELF: relocacao absoluta de 32 bits do simbolo '%s' nao cabe no endereco 0x%llx",
                           LigarElfSimbolo(L, ap->objeto, ap->reloc->correcao.simbolo)->nome, (unsigned long long)v);
    *valor = (uint64_t)v;
    return true;
}

/* GOTPCREL/GOTPCRELX: relaxa (lea/call/jmp direto ao alvo) quando o alvo e definido; senao o campo aponta para a celula .got do simbolo. */
static bool LigarElfValorGot(const LigacaoEstado *L, const LigarElfAplicacao *ap, uint64_t s, int64_t *v)
{
    int64_t a = ap->reloc->correcao.adendo;

    if (LigarElfRelaxavel(L, ap->objeto, ap->reloc)) {
        const ObjSecao *sec = VecEm(&L->obj[ap->objeto]->secoes, ap->reloc->alvo.secao);
        LigarElfRelaxar(ap->campo, LigarElfFormaDe(sec, ap->reloc->alvo.off));
    } else {
        int64_t k = LigarElfIndiceCelula(L, ap->objeto, ap->reloc->correcao.simbolo);
        if (k < 0)
            FalhaInterna("ligar: celula .got de um simbolo do objeto %d nao foi planejada", ap->objeto);
        s = PeBaseDaImagem(L->im) + L->rva[LIGAR_SAIDA_GOT] + (uint64_t)k * ELF_TAM_CELULA;
    }
    *v = (int64_t)s + a - (int64_t)(ap->p + ELF_FIM_CAMPO_PC);
    return true;
}

/* ---------- TLS (M5c-b P61): TPOFF32 e GOTTPOFF (variante II do x86-64: o TP fica logo depois do bloco) ---------- */

/* Offset do simbolo dentro do bloco TLS (o RVA dele menos o da secao TLS). Fora do bloco (indefinido ou nao __thread) = X0111. */
static bool LigarElfOffsetTls(const LigacaoEstado *L, const LigarElfAplicacao *ap, int64_t *s)
{
    uint32_t rva = *(const uint32_t *)VecEm(&L->rva_sim[ap->objeto], ap->reloc->correcao.simbolo);
    uint32_t ini = L->rva[LIGAR_SAIDA_TLS];
    const ObjSimbolo *sim = LigarElfSimbolo(L, ap->objeto, ap->reloc->correcao.simbolo);

    if (rva == LIGAR_SEM_RVA || rva < ini || rva - ini >= L->tam[LIGAR_SAIDA_TLS])
        return DiagRelatar(L->erro, X0111, 0, "ELF: relocacao de TLS do simbolo '%s' fora do bloco TLS (indefinido ou sem __thread)", sim->nome);
    *s = (int64_t)(rva - ini);
    return true;
}

/* Opcode da forma que o LE relaxa: 8b (mov) ou 03 (add) com modrm [rip+d] (mod 00, rm 101) e o disp32 em off. Prefixo REX.R (reg 8..15) = 0: X0114. */
int LigarElfFormaTls(const ObjSecao *s, uint32_t off)
{
    const uint8_t *b = (const uint8_t *)s->bytes.dados;

    if (off < 2 || off + ELF_TAM_CAMPO_32 > s->bytes.tam)
        return 0;
    if (off >= 3 && (b[off - 3] & ELF_REX_R_MASCARA) == ELF_REX_R)
        return 0;
    if ((b[off - 1] & ELF_MODRM_MASCARA) != ELF_MODRM_RIP)
        return 0;
    if (b[off - 2] == ELF_OP_MOV || b[off - 2] == ELF_OP_ADD)
        return b[off - 2];
    return 0;
}

/* GOTTPOFF em executavel (IE -> LE, especificacao TLS de Drepper): o opcode muda e o modrm vira direto (mod 11) no mesmo registrador; o imm32 cai onde estava o disp32. */
static bool LigarElfRelaxarTls(const LigacaoEstado *L, const LigarElfAplicacao *ap)
{
    const ObjSecao *sec = VecEm(&L->obj[ap->objeto]->secoes, ap->reloc->alvo.secao);
    int op = LigarElfFormaTls(sec, ap->reloc->alvo.off);

    if (op == 0)
        return DiagRelatar(L->erro, X0114, 0, "ELF: GOTTPOFF do simbolo '%s' com forma que o LE nao relaxa (so mov ou add com [rip+d], sem REX.R)",
                           LigarElfSimbolo(L, ap->objeto, ap->reloc->correcao.simbolo)->nome);
    ap->campo[-2] = op == ELF_OP_MOV ? ELF_OP_MOV_IMM : ELF_OP_ADD_IMM;
    ap->campo[-1] = (uint8_t)(ELF_MODRM_DIRETO | ((ap->campo[-1] >> ELF_MODRM_REG_SHIFT) & ELF_MODRM_REG));
    return true;
}

/* TPOFF32 = S + A - bloco. GOTTPOFF relaxado = S - bloco (o adendo -4 e do campo PC-relativo que sumiu; a entrada da GOT nao existe mais). */
static bool LigarElfValorTls(const LigacaoEstado *L, const LigarElfAplicacao *ap, uint64_t *valor)
{
    int64_t s = 0;
    int64_t tpoff = 0;

    if (!LigarElfOffsetTls(L, ap, &s))
        return false;
    tpoff = s - (int64_t)L->tls_bloco;
    if (ap->reloc->correcao.tipo == OR_TPOFF32)
        tpoff += ap->reloc->correcao.adendo;
    else if (!LigarElfRelaxarTls(L, ap))
        return false;
    *valor = (uint64_t)tpoff;
    return true;
}

static bool LigarElfValor(const LigacaoEstado *L, const LigarElfAplicacao *ap, uint64_t *valor)
{
    const ObjReloc *r = ap->reloc;
    uint64_t s = 0;
    int64_t v = 0;

    if (r->correcao.tipo == OR_TPOFF32 || r->correcao.tipo == OR_GOTTPOFF)
        return LigarElfValorTls(L, ap, valor);
    if (!LigarElfAlvoAbs(L, ap->objeto, r->correcao.simbolo, &s))
        return false;
    switch (r->correcao.tipo) {
    case OR_ADDR64:
        *valor = (uint64_t)((int64_t)s + r->correcao.adendo);
        return true;
    case OR_ABS32:
    case OR_ABS32S:
        return LigarElfAbs32(L, ap, (int64_t)s + r->correcao.adendo, valor);
    case OR_REL32:
        v = (int64_t)s + r->correcao.adendo - (int64_t)(ap->p + ELF_FIM_CAMPO_PC);
        *valor = (uint64_t)v;
        return true;
    case OR_GOTPCREL:
    case OR_GOTPCRELX:
        if (!LigarElfValorGot(L, ap, s, &v))
            return false;
        *valor = (uint64_t)v;
        return true;
    default:
        FalhaInterna("ligar: relocacao ELF de tipo %d sem tratamento", (int)r->correcao.tipo);
    }
}

bool LigarElfRelocar(LigacaoEstado *L, int o, const ObjReloc *r)
{
    const LigarContrib *c = VecEm(&L->contrib[o], r->alvo.secao);
    uint32_t tam = r->correcao.tipo == OR_ADDR64 ? ELF_TAM_CAMPO_ABS64 : ELF_TAM_CAMPO_32;
    uint32_t sec_off = c->off + r->alvo.off;
    PeSecao *ps = &L->im->secoes[L->idx_im[c->saida]];
    uint64_t valor = 0;

    if (L->tam[c->saida] == 0 || sec_off + tam > ps->bytes.tam)
        FalhaInterna("ligar: relocacao ELF em 0x%x fora da secao de saida %d", sec_off, c->saida);
    LigarElfAplicacao ap = {o, r, PeBaseDaImagem(L->im) + L->rva[c->saida] + sec_off, (uint8_t *)ps->bytes.dados + sec_off};
    if (!LigarElfValor(L, &ap, &valor))
        return false;
    LigarElfEscrever(ap.campo, tam, valor);
    return LigarElfPieAddr64(L, &ap, c->saida, valor);
}

/* M6 P66 (D-M6-4): secao de debug (OS_DEBUG_*) -> indice em Imagem.dbg; -1 para as demais. */
static int LigarElfIndiceDebug(ObjTipoSecao tipo)
{
    return tipo >= OS_DEBUG_ABBREV ? (int)tipo - (int)OS_DEBUG_ABBREV : -1;
}

/* M6 P66: ADDR64 de debug = endereco absoluto do simbolo do .text (base do ELF + RVA + adendo), gravado little-endian. Nunca RELATIVE:
 * a entrada do .rela.dyn (pie) so sai de LigarElfPieAddr64, que as secoes de debug nao chamam. */
static void LigarElfDebugReloc(LigacaoEstado *L, uint8_t *campo, const ObjReloc *r)
{
    uint32_t rva = *(const uint32_t *)VecEm(&L->rva_sim[LIGAR_PROGRAMA], r->correcao.simbolo);
    uint64_t valor = PeBaseDaImagem(L->im) + rva + (uint64_t)r->correcao.adendo;

    if (r->correcao.tipo != OR_ADDR64 || rva == LIGAR_SEM_RVA)
        FalhaInterna("ligar: relocacao de debug fora do ADDR64 ou simbolo sem RVA");
    for (int k = 0; k < 8; k++)
        campo[k] = (uint8_t)((valor >> (8 * k)) & 0xff);
}

void LigarElfDebug(LigacaoEstado *L)
{
    const Objeto *o = L->obj[LIGAR_PROGRAMA];

    for (size_t s = 0; s < o->secoes.n; s++) {
        const ObjSecao *sec = VecEm(&o->secoes, s);
        int k = LigarElfIndiceDebug(sec->tipo);
        Buf *dst;

        if (k < 0)
            continue;
        dst = &L->im->dbg[k];
        BufAnexarN(dst, sec->bytes.dados, sec->bytes.tam);
        for (size_t i = 0; i < o->relocs.n; i++) {
            const ObjReloc *r = VecEm(&o->relocs, i);

            if (r->alvo.secao == s)
                LigarElfDebugReloc(L, (uint8_t *)dst->dados + r->alvo.off, r);
        }
    }
}

/* ---------- simbolos do ligador ---------- */

bool LigarElfSimboloDoLigador(const LigacaoEstado *L, const char *nome, uint32_t *rva)
{
    uint32_t ini = L->tam[LIGAR_SAIDA_INIT_ARRAY] > 0 ? L->rva[LIGAR_SAIDA_INIT_ARRAY] : 0;
    uint32_t fim = ini + L->tam[LIGAR_SAIDA_INIT_ARRAY];
    int k = AlcanceIndiceSimboloDoLigador(nome);

    if (k < 0 || L->pedido->formato != IMAGEM_ELF)
        return false;
    if (k == ELF_SIMB_DYNAMIC) { /* so o pie tem .dynamic; sem ele _DYNAMIC fica indefinido (fraco = 0) */
        if (!L->pedido->pie)
            return false;
        *rva = (uint32_t)ElfInicioDinamico(L->im);
        return true;
    }
    if (k == ELF_SIMB_INIT_INI)
        *rva = ini;
    else if (k == ELF_SIMB_EHDR)
        *rva = 0;
    else if (k == ELF_SIMB_GOT) {
        if (L->tam[LIGAR_SAIDA_GOT] == 0)
            return false;
        *rva = L->rva[LIGAR_SAIDA_GOT];
    } else
        *rva = fim; /* __init_array_end e os dois __fini_array_* (vazios por ora: o fim do .init_array) */
    return true;
}

/* ---------- coleta da Imagem e formato ---------- */

static bool LigarElfSimboloNaImagem(const LigacaoEstado *L, int o, const ObjSimbolo *s, int *k)
{
    if (s->secao < 0 || s->nome[0] == '.')
        return false;
    *k = ((const LigarContrib *)VecEm(&L->contrib[o], (size_t)s->secao))->saida;
    return *k != LIGAR_SEM_SAIDA && L->tam[*k] > 0;
}

void LigarElfColetarSimbolos(const LigacaoEstado *L)
{
    for (int o = 0; o < LIGAR_QTD_OBJETOS; o++) {
        for (size_t j = 0; j < L->obj[o]->simbolos.n; j++) {
            const ObjSimbolo *s = VecEm(&L->obj[o]->simbolos, j);
            int k = 0;
            if (LigarElfSimboloNaImagem(L, o, s, &k))
                *(ImagemSimbolo *)VecEmpurrar(&L->im->simbolos) = (ImagemSimbolo){s->nome, *(const uint32_t *)VecEm(&L->rva_sim[o], j), L->idx_im[k]};
        }
    }
}

/* O ELF estatico nao carrega DLL nem importa nada (foreign com biblioteca de DLL segue sem caminho): o ligador recusa em vez de montar uma imagem sem sentido. */
bool LigarElfConferirFormato(const LigarPedido *pedido, Diag *erro)
{
    if (pedido->formato != IMAGEM_ELF)
        return true;
    if (pedido->dll || pedido->qtd_tabelas > 0)
        return DiagRelatar(erro, X0903, 0, "o alvo ELF nao liga DLL nem tabelas de imports (%u pedidas)", pedido->qtd_tabelas);
    return true;
}

/* M5b P37d: no ELF a saida da secao sai do TIPO (OS_*), nao do nome: .rodata, .rodata.cst8, .data.rel.ro e .init nao batem com os nomes do PE.
 * TLS (M5c-b P61): OS_TDATA e OS_TBSS vao para LIGAR_SAIDA_TLS (o bloco unico). */
int LigarSaidaElf(const ObjSecao *s)
{
    switch (s->tipo) {
    case OS_TEXTO: return LIGAR_SAIDA_TEXTO;
    case OS_RDATA: return LIGAR_SAIDA_RDATA;
    case OS_DATA: return LIGAR_SAIDA_DATA;
    case OS_BSS: return LIGAR_SAIDA_BSS;
    case OS_INIT_ARRAY: return LIGAR_SAIDA_INIT_ARRAY;
    case OS_PDATA: return LIGAR_SAIDA_PDATA; /* so chega em teste de ligacao: LigarSaidaSoDoPe a descarta no ELF */
    case OS_XDATA: return LIGAR_SAIDA_XDATA;
    case OS_TDATA:
    case OS_TBSS: return LIGAR_SAIDA_TLS;
    default: return LIGAR_SEM_SAIDA;
    }
}

/* Secao viva de tipo sem saida conhecida (hoje nenhuma): recusa com X0903 em L->erro. Sempre false. */
bool LigarElfSemSaida(const LigacaoEstado *L, const ObjSecao *s)
{
    return DiagRelatar(L->erro, X0903, 0, "ELF: secao '%s' (tipo %d) sem saida no ligador", s->nome, (int)s->tipo);
}

/* ---------- bloco TLS (M5c-b P61): .tdata e .tbss numa saida so ---------- */

bool LigarElfTlsPermitido(const LigacaoEstado *L, const ObjSecao *s)
{
    if (L->pedido->semente != NULL)
        return true;
    return DiagRelatar(L->erro, X0113, 0, "ELF: a secao TLS '%s' liga so com a libc (musl): a ligacao crua nao monta o bloco TLS", s->nome);
}

typedef struct {
    uint32_t fim;        /* fim do bloco ate aqui, em bytes desde o inicio dele */
    uint32_t alinhamento; /* o maior alinhamento visto: p_align do PT_TLS */
} LigarElfCursorTls;

/* Uma contribuicao TLS entra no alinhamento dela, a partir do fim do bloco (o offset e relativo ao inicio do bloco, alinhado ao maximo). */
static void LigarElfTlsPor(LigarContrib *c, const ObjSecao *s, LigarElfCursorTls *cur)
{
    uint32_t tam = s->tipo == OS_TBSS ? s->tam_bss : (uint32_t)s->bytes.tam;

    c->off = (cur->fim + s->alinhamento - 1) / s->alinhamento * s->alinhamento;
    cur->fim = c->off + tam;
    if (s->alinhamento > cur->alinhamento)
        cur->alinhamento = s->alinhamento;
}

/* As contribuicoes TLS de um tipo (OS_TDATA ou OS_TBSS), na ordem dos objetos (a mesma de LigarJuntar, que copia os bytes do .tdata). */
static void LigarElfTlsPassar(LigacaoEstado *L, ObjTipoSecao tipo, LigarElfCursorTls *cur)
{
    for (int o = 0; o < LIGAR_QTD_OBJETOS; o++) {
        for (uint32_t sec = 0; sec < L->obj[o]->secoes.n; sec++) {
            const ObjSecao *s = VecEm(&L->obj[o]->secoes, sec);
            LigarContrib *c = VecEm(&L->contrib[o], sec);
            if (s->tipo == tipo && c->saida == LIGAR_SAIDA_TLS)
                LigarElfTlsPor(c, s, cur);
        }
    }
}

/* Os .tdata (bytes) primeiro, depois os .tbss (so memoria) no mesmo bloco: p_filesz e o fim dos bytes e p_memsz o fim do .tbss. */
void LigarElfPlanejarTls(LigacaoEstado *L)
{
    LigarElfCursorTls cur = {0, 1};

    if (L->pedido->formato != IMAGEM_ELF)
        return;
    LigarElfTlsPassar(L, OS_TDATA, &cur);
    LigarElfTlsPassar(L, OS_TBSS, &cur);
    L->tam[LIGAR_SAIDA_TLS] = cur.fim;
    L->tls_alinhamento = cur.alinhamento;
    L->tls_bloco = (cur.fim + cur.alinhamento - 1) / cur.alinhamento * cur.alinhamento;
}

/* Sem bloco TLS nao ha secao TLS na Imagem (o ELF sai como antes); com bloco, a Imagem leva a secao e o p_align do PT_TLS. */
void LigarElfMarcarTls(LigacaoEstado *L)
{
    if (L->tam[LIGAR_SAIDA_TLS] == 0)
        return;
    L->im->tls = true;
    L->im->tls_secao = L->idx_im[LIGAR_SAIDA_TLS];
    L->im->tls_alinhamento = L->tls_alinhamento;
}
