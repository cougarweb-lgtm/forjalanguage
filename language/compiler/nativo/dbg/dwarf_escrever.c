/* dwarf_escrever.c - DWARF 4 do programa (M6 P66-P67, D-M6-3): orquestra as 4 secoes de debug do Objeto e escreve .debug_abbrev e .debug_info
 * (1 CU DW_LANG_C99; um DW_TAG_subprogram por proc com low/high_pc e frame_base rbp, seus parametros e locais; os tipos vao como filhos da CU, em
 * dwarf_tipos.c). A linha e o CFI saem em dwarf_linhas.c e dwarf_frame.c.
 * Versao: 0.4.0
 * Changelog: 0.4.0 - M6b P76d: DwarfArquivoDe por dica de linha e cache do ultimo arquivo; DwarfRelocarProc (simbolo do proc achado uma vez para info e line);
 *                    localizacao sem Buf temporario; U16/U32 numa gravacao so. Saida igual.
 *            0.3.0 - M6 P68: .debug_frame so quando DwarfSemantica.com_frame (ELF); o PE leva abbrev, info e line.
 *            0.2.0 - M6 P67: DW_TAG_formal_parameter e DW_TAG_variable por proc (location DW_OP_breg6 com o rbp_off; indireto ganha DW_OP_deref);
 *                    tipos pelo registro de dwarf_tipos.c; nome de arquivo com barra normal (o gdb do Linux nao acha 'C:\...\geo.fj').
 *            0.1.0 - M6 P66: primeira versao (escritores de bytes, tabela de arquivos, relocacao ADDR64, abbrev e info).
 * Verificacao: python compiler\construir_rapido.py && python ..\tests\nativo\m6\verificar_gdb.py --forja .\forja.exe --alvo linux
 */
#include <string.h>

#include "nativo/dbg/dwarf_interno.h"

#define DWARF_VERSAO 4
#define DWARF_LANG_C99 0x0c
#define DWARF_TAG_CU 0x11
#define DWARF_TAG_SUB 0x2e
#define DWARF_TAG_PARAM 0x05
#define DWARF_TAG_VAR 0x34
#define DWARF_TAG_BASE 0x24
#define DWARF_TAG_PONTEIRO 0x0f
#define DWARF_TAG_ESTRUTURA 0x13
#define DWARF_TAG_MEMBRO 0x0d
#define DWARF_TAG_VETOR 0x01
#define DWARF_TAG_INTERVALO 0x21
#define DWARF_TAG_ENUM 0x04
#define DWARF_TAG_ENUMERADOR 0x28
#define DWARF_CHILDREN_NAO 0
#define DWARF_CHILDREN_SIM 1
#define DWARF_TAM_ENDERECO 8
#define DW_OP_REG6 0x56     /* DW_OP_reg6: rbp (frame_base) */
#define DW_OP_BREG6 0x76    /* DW_OP_breg6: rbp + deslocamento (sleb) */
#define DW_OP_DEREF 0x06

/* Pares (atributo, forma) de cada abreviacao, na ordem em que o DIE os escreve. Formas: 0x08 string, 0x13 ref4, 0x0b data1, 0x0f udata, 0x0d sdata,
 * 0x07 data8, 0x17 sec_offset, 0x01 addr, 0x18 exprloc, 0x19 flag_present. */
static const uint8_t PARES_CU[] = {0x25, 0x08, 0x13, 0x0b, 0x03, 0x08, 0x10, 0x17, 0x11, 0x01, 0x12, 0x07};
static const uint8_t PARES_SUB[] = {0x03, 0x08, 0x6e, 0x08, 0x3a, 0x0b, 0x3b, 0x05, 0x11, 0x01, 0x12, 0x07, 0x40, 0x18, 0x3f, 0x19};
static const uint8_t PARES_VARIAVEL[] = {0x03, 0x08, 0x49, 0x13, 0x02, 0x18};
static const uint8_t PARES_BASE[] = {0x03, 0x08, 0x0b, 0x0f, 0x3e, 0x0b};
static const uint8_t PARES_PONTEIRO[] = {0x0b, 0x0f, 0x49, 0x13};
static const uint8_t PARES_PONTEIRO_VOID[] = {0x0b, 0x0f};
static const uint8_t PARES_ESTRUTURA[] = {0x03, 0x08, 0x0b, 0x0f};
static const uint8_t PARES_MEMBRO[] = {0x03, 0x08, 0x49, 0x13, 0x38, 0x0f};
static const uint8_t PARES_VETOR[] = {0x49, 0x13};
static const uint8_t PARES_INTERVALO[] = {0x37, 0x0f};
static const uint8_t PARES_ENUM[] = {0x03, 0x08, 0x0b, 0x0f};
static const uint8_t PARES_ENUMERADOR[] = {0x03, 0x08, 0x1c, 0x0d};

#define PARES(v) (v), sizeof(v)

static const DwarfAbrev ABREVIACOES[DWARF_ABREV_QTD - 1] = {
    {DWARF_ABREV_CU, DWARF_TAG_CU, DWARF_CHILDREN_SIM, PARES(PARES_CU)},
    {DWARF_ABREV_SUB, DWARF_TAG_SUB, DWARF_CHILDREN_SIM, PARES(PARES_SUB)},
    {DWARF_ABREV_PARAM, DWARF_TAG_PARAM, DWARF_CHILDREN_NAO, PARES(PARES_VARIAVEL)},
    {DWARF_ABREV_VAR, DWARF_TAG_VAR, DWARF_CHILDREN_NAO, PARES(PARES_VARIAVEL)},
    {DWARF_ABREV_BASE, DWARF_TAG_BASE, DWARF_CHILDREN_NAO, PARES(PARES_BASE)},
    {DWARF_ABREV_PONTEIRO, DWARF_TAG_PONTEIRO, DWARF_CHILDREN_NAO, PARES(PARES_PONTEIRO)},
    {DWARF_ABREV_PONTEIRO_VOID, DWARF_TAG_PONTEIRO, DWARF_CHILDREN_NAO, PARES(PARES_PONTEIRO_VOID)},
    {DWARF_ABREV_ESTRUTURA, DWARF_TAG_ESTRUTURA, DWARF_CHILDREN_SIM, PARES(PARES_ESTRUTURA)},
    {DWARF_ABREV_MEMBRO, DWARF_TAG_MEMBRO, DWARF_CHILDREN_NAO, PARES(PARES_MEMBRO)},
    {DWARF_ABREV_VETOR, DWARF_TAG_VETOR, DWARF_CHILDREN_SIM, PARES(PARES_VETOR)},
    {DWARF_ABREV_INTERVALO, DWARF_TAG_INTERVALO, DWARF_CHILDREN_NAO, PARES(PARES_INTERVALO)},
    {DWARF_ABREV_ENUM, DWARF_TAG_ENUM, DWARF_CHILDREN_SIM, PARES(PARES_ENUM)},
    {DWARF_ABREV_ENUMERADOR, DWARF_TAG_ENUMERADOR, DWARF_CHILDREN_NAO, PARES(PARES_ENUMERADOR)},
};

void DwarfBytesU8(Buf *b, uint8_t v)
{
    BufAnexarN(b, (const char *)&v, 1);
}

void DwarfBytesU16(Buf *b, uint16_t v)
{
    const char bytes[2] = {(char)(v & 0xff), (char)((v >> 8) & 0xff)};

    BufAnexarN(b, bytes, sizeof bytes);
}

void DwarfBytesU32(Buf *b, uint32_t v)
{
    const char bytes[4] = {(char)(v & 0xff), (char)((v >> 8) & 0xff), (char)((v >> 16) & 0xff), (char)((v >> 24) & 0xff)};

    BufAnexarN(b, bytes, sizeof bytes);
}

void DwarfBytesU64(Buf *b, uint64_t v)
{
    DwarfBytesU32(b, (uint32_t)(v & 0xffffffffu));
    DwarfBytesU32(b, (uint32_t)(v >> 32));
}

void DwarfBytesUleb(Buf *b, uint64_t v)
{
    do {
        uint8_t byte = (uint8_t)(v & 0x7f);
        v >>= 7;
        DwarfBytesU8(b, v != 0 ? (uint8_t)(byte | 0x80) : byte);
    } while (v != 0);
}

void DwarfBytesSleb(Buf *b, int64_t v)
{
    int mais = 1;

    while (mais) {
        uint8_t byte = (uint8_t)(v & 0x7f);
        v >>= 7; /* aritmetico: o sinal continua no topo (gcc) */
        if ((v == 0 && (byte & 0x40) == 0) || (v == -1 && (byte & 0x40) != 0))
            mais = 0;
        else
            byte |= 0x80;
        DwarfBytesU8(b, byte);
    }
}

void DwarfBytesTexto(Buf *b, const char *s)
{
    BufAnexarN(b, s, strlen(s) + 1);
}

void DwarfPatchU32(Buf *b, size_t pos, uint32_t v)
{
    uint8_t *d = (uint8_t *)b->dados;

    for (int k = 0; k < 4; k++)
        d[pos + (size_t)k] = (uint8_t)((v >> (8 * k)) & 0xff);
}

Buf *DwarfBufSecao(const DwarfCtx *x, uint32_t idx)
{
    return &((ObjSecao *)VecEm(&x->o->secoes, idx))->bytes;
}

/* Caminho com barra normal: o gdb do Linux so casa o nome de arquivo com '/'. Copia na arena (a tabela guarda o ponteiro). */
static const char *DwarfCaminhoNormal(DwarfCtx *x, const char *caminho)
{
    size_t tam = strlen(caminho);
    char *copia = ArenaCopiarTexto(&x->arena, caminho, tam);

    copia[tam] = '\0'; /* ArenaCopiarTexto nao termina a copia */
    for (size_t i = 0; i < tam; i++) {
        if (copia[i] == '\\')
            copia[i] = '/';
    }
    return copia;
}

/* Compara o caminho normal da tabela com o caminho cru da fonte, tratando '\' como '/' (sem copiar a cada linha da tabela de linhas). */
static bool DwarfCaminhoIgual(const char *normal, const char *cru)
{
    for (; *normal != '\0' && *cru != '\0'; normal++, cru++) {
        char c = *cru == '\\' ? '/' : *cru;

        if (*normal != c)
            return false;
    }
    return *normal == *cru;
}

uint32_t DwarfArquivoDe(DwarfCtx *x, uint32_t pos, uint32_t *linha)
{
    FontePosicao fp = {x->f->caminho, 0, 0};
    const char *caminho;

    if (pos != BIR_SEM_POS)
        fp = DbgPosicaoComDica(x->f, pos, &x->dica);
    caminho = fp.caminho != NULL ? fp.caminho : "";
    *linha = fp.linha;
    if (caminho == x->ult_cru)
        return x->ult_arquivo;
    x->ult_cru = caminho;
    for (size_t i = 0; i < x->arquivos.n; i++) {
        if (DwarfCaminhoIgual(*(const char **)VecEm(&x->arquivos, i), caminho)) {
            x->ult_arquivo = (uint32_t)i + 1;
            return x->ult_arquivo;
        }
    }
    *(const char **)VecEmpurrar(&x->arquivos) = DwarfCaminhoNormal(x, caminho);
    x->ult_arquivo = (uint32_t)x->arquivos.n;
    return x->ult_arquivo;
}

void DwarfRelocar(DwarfCtx *x, uint32_t secao, size_t off, const char *simbolo)
{
    int64_t idx = ObjAcharSimbolo(x->o, simbolo);

    if (idx < 0)
        FalhaInterna("dwarf: simbolo '%s' ausente no objeto do programa", simbolo);
    ObjRelocar(x->o, (ObjAlvoReloc){secao, (uint32_t)off}, (ObjCorrecao){OR_ADDR64, (uint32_t)idx, 0});
}

void DwarfRelocarProc(DwarfCtx *x, uint32_t secao, size_t off, size_t proc)
{
    uint32_t *memo = VecEm(&x->simb_proc, proc);

    if (*memo == 0) {
        const char *simbolo = ((const DbgProc *)VecEm(&x->d->procs, proc))->simbolo;
        int64_t idx = ObjAcharSimbolo(x->o, simbolo);

        if (idx < 0)
            FalhaInterna("dwarf: simbolo '%s' ausente no objeto do programa", simbolo);
        *memo = (uint32_t)idx + 1;
    }
    ObjRelocar(x->o, (ObjAlvoReloc){secao, (uint32_t)off}, (ObjCorrecao){OR_ADDR64, *memo - 1, 0});
}

static void DwarfAbreviacao(Buf *b, const DwarfAbrev *a)
{
    DwarfBytesUleb(b, a->codigo);
    DwarfBytesUleb(b, a->tag);
    DwarfBytesU8(b, a->filhos);
    for (size_t i = 0; i < a->qtd; i += 2) {
        DwarfBytesUleb(b, a->pares[i]);
        DwarfBytesUleb(b, a->pares[i + 1]);
    }
    DwarfBytesU8(b, 0);
    DwarfBytesU8(b, 0);
}

static void DwarfEscreverAbreviacoes(Buf *b)
{
    for (size_t i = 0; i < sizeof ABREVIACOES / sizeof ABREVIACOES[0]; i++)
        DwarfAbreviacao(b, &ABREVIACOES[i]);
    DwarfBytesU8(b, 0); /* fim da tabela de abreviacoes */
}

/* Menor inicio e maior fim entre os procs: a faixa de .text que a CU cobre. */
static void DwarfFaixa(const DbgPrograma *d, const DbgProc **menor, uint32_t *fim)
{
    *menor = VecEm(&d->procs, 0);
    *fim = 0;
    for (size_t i = 0; i < d->procs.n; i++) {
        const DbgProc *p = VecEm(&d->procs, i);

        if (p->inicio < (*menor)->inicio)
            *menor = p;
        if (p->fim > *fim)
            *fim = p->fim;
    }
}

/* Localizacao do dado: rbp + rbp_off (DW_OP_breg6). No indireto o home guarda o ponteiro do agregado: o dado esta no endereco que ele aponta. */
static void DwarfEscreverLocalizacao(Buf *b, const DbgVar *v)
{
    size_t campo = b->tam;

    DwarfBytesU8(b, 0); /* tamanho da expressao: sempre < 128 (opcode + sleb de 32 bits + deref), 1 byte de ULEB; preenchido ao fim */
    DwarfBytesU8(b, DW_OP_BREG6);
    DwarfBytesSleb(b, v->loc.rbp_off);
    if (v->loc.indireto)
        DwarfBytesU8(b, DW_OP_DEREF);
    b->dados[campo] = (char)(b->tam - campo - 1);
}

static void DwarfEscreverVariavel(DwarfCtx *x, Buf *b, const DbgVar *v)
{
    DwarfBytesUleb(b, v->parametro ? DWARF_ABREV_PARAM : DWARF_ABREV_VAR);
    DwarfBytesTexto(b, v->nome);
    DwarfRefTipo(x, b, v->tipo);
    DwarfEscreverLocalizacao(b, v);
}

static void DwarfEscreverVariaveis(DwarfCtx *x, Buf *b, const DbgProc *p)
{
    for (uint32_t k = 0; k < p->nvars; k++)
        DwarfEscreverVariavel(x, b, VecEm(&x->d->vars, p->var_ini + k));
}

static void DwarfEscreverSubprograma(DwarfCtx *x, Buf *b, size_t indice)
{
    const DbgProc *p = VecEm(&x->d->procs, indice);
    uint32_t linha;
    uint32_t arquivo = DwarfArquivoDe(x, p->pos, &linha);

    DwarfBytesUleb(b, DWARF_ABREV_SUB);
    DwarfBytesTexto(b, p->nome);
    DwarfBytesTexto(b, p->simbolo);
    DwarfBytesU8(b, (uint8_t)arquivo);
    DwarfBytesU16(b, (uint16_t)linha);
    DwarfRelocarProc(x, x->sec_info, b->tam, indice); /* low_pc */
    DwarfBytesU64(b, 0);
    DwarfBytesU64(b, p->fim - p->inicio);                /* high_pc: DW_FORM_data8 = tamanho */
    DwarfBytesUleb(b, 1);                                /* frame_base: exprloc de 1 byte */
    DwarfBytesU8(b, DW_OP_REG6);                         /* frame_base: DW_OP_reg6 = rbp */
    DwarfEscreverVariaveis(x, b, p);
    DwarfBytesU8(b, 0); /* fim dos filhos do subprograma */
}

static void DwarfEscreverInfo(DwarfCtx *x)
{
    Buf *b = DwarfBufSecao(x, x->sec_info);
    const DbgProc *menor;
    uint32_t fim;
    size_t tam_pos = b->tam;

    DwarfFaixa(x->d, &menor, &fim);
    DwarfBytesU32(b, 0); /* unit_length: preenchido no fim */
    DwarfBytesU16(b, DWARF_VERSAO);
    DwarfBytesU32(b, 0); /* debug_abbrev_offset: a unica tabela, no inicio de .debug_abbrev */
    DwarfBytesU8(b, DWARF_TAM_ENDERECO);
    DwarfBytesUleb(b, DWARF_ABREV_CU);
    DwarfBytesTexto(b, "Forja");
    DwarfBytesU8(b, DWARF_LANG_C99);
    DwarfBytesTexto(b, x->f->caminho != NULL ? x->f->caminho : "programa");
    DwarfBytesU32(b, 0); /* stmt_list: a tabela de linhas da CU comeca no inicio de .debug_line */
    DwarfRelocar(x, x->sec_info, b->tam, menor->simbolo); /* low_pc da CU */
    DwarfBytesU64(b, 0);
    DwarfBytesU64(b, fim - menor->inicio);
    for (size_t i = 0; i < x->d->procs.n; i++)
        DwarfEscreverSubprograma(x, b, i);
    DwarfEscreverTipos(x); /* os tipos das variaveis, filhos da CU depois dos procs (1 DIE por TipoId) */
    DwarfBytesU8(b, 0); /* fim dos filhos da CU */
    DwarfPatchRefs(x);
    DwarfPatchU32(b, tam_pos, (uint32_t)(b->tam - tam_pos - 4));
}

void DwarfEscrever(Objeto *o, const DbgPrograma *d, const DwarfSemantica *s)
{
    DwarfCtx x = {.o = o, .d = d, .s = s, .f = s->fonte};

    if (d->procs.n == 0)
        return;
    x.sec_abrev = ObjSecaoAbrir(o, ".debug_abbrev", OS_DEBUG_ABBREV, 1);
    x.sec_info = ObjSecaoAbrir(o, ".debug_info", OS_DEBUG_INFO, 1);
    x.sec_linha = ObjSecaoAbrir(o, ".debug_line", OS_DEBUG_LINE, 1);
    if (s->com_frame)
        x.sec_frame = ObjSecaoAbrir(o, ".debug_frame", OS_DEBUG_FRAME, 1);
    VecIniciar(&x.arquivos, sizeof(const char *));
    VecIniciar(&x.simb_proc, sizeof(uint32_t));
    for (size_t i = 0; i < d->procs.n; i++)
        VecEmpurrar(&x.simb_proc);
    LayoutCalcular(s->tipos, s->ir, &x.arena, &x.lay);
    DwarfTiposIniciar(&x);
    DwarfEscreverAbreviacoes(DwarfBufSecao(&x, x.sec_abrev));
    DwarfEscreverInfo(&x);
    DwarfEscreverLinhas(&x);
    if (s->com_frame)
        DwarfEscreverFrames(&x);
    VecLiberar(&x.tipos.fila);
    VecLiberar(&x.tipos.refs);
    VecLiberar(&x.arquivos);
    VecLiberar(&x.simb_proc);
    ArenaLiberar(&x.arena);
}
