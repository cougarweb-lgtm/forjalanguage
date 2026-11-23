/* pdb_modulo.c - fluxo de simbolos do modulo do PDB (M6b P76a/P76c, D-M6-9): assinatura C13, um S_GPROC32 + S_FRAMEPROC + S_END por proc do programa, o C13
 * (DEBUG_S_FILECHKSMS com um registro por arquivo .fj e DEBUG_S_LINES por proc) e o GlobalRefsSize.
 * Versao: 0.4.0
 * Changelog: 0.4.0 - M6b P77b: o S_REGREL32 cobre todas as variaveis nao indiretas (o tipo nao simples vem do TPI, PdbTipoIndice); ficam de fora so as indiretas e os tipos sem forma.
 *            0.3.0 - M6b P77a: S_REGREL32 (RBP) de cada variavel simples entre o S_FRAMEPROC e o S_END; o End do S_GPROC32 e remendado no S_END. Ficam de fora os tipos nao simples
 *                    (P77b) e as variaveis indiretas (o home guarda o endereco do dado).
 *            0.2.1 - M6b P76c: as linhas leem PdbPosLinha da Colher (sem nova busca na Fonte); a funcao ficou so de leitura de PdbArquivos.
 *            0.2.0 - M6b P76c: C13 no fluxo do modulo (F4 antes; F2 por proc com offsets relativos ao inicio e uma linha no offset 0; linha gravada so quando muda a
 *                    linha ou o arquivo; um bloco por arquivo). PdbFluxoModulo devolve SymByteSize e C13ByteSize (PdbTamanhosModulo); PdbPatchU32 mora aqui.
 *            0.1.0 - M6b P76a: primeira versao. CodeOffset = DbgProc.inicio (offset na .text; o programa comeca em 0).
 * Verificacao: python ..\tests\nativo\m6b\ler_pdb.py <arquivo.pdb> --exigir-modulo  e  python ..\tests\nativo\m6b\verificar_pdb.py --modo linhas --exe <exe> --json <exe>.debug.jsonl
 */
#include "nativo/dbg/pdb.h"

#define CV_ASSINATURA_C13 4u
#define CV_S_END 0x0006u
#define CV_S_FRAMEPROC 0x1012u
#define CV_S_GPROC32 0x1110u
#define CV_C13_LINHAS 0x00F2u
#define CV_C13_ARQUIVOS 0x00F4u
#define CV_TAM_CABECALHO 4u               /* u16 tamanho (sem ele mesmo) + u16 tipo */
#define CV_TAM_GPROC_FIXO 35u             /* 8 u32 + segmento u16 + flags u8; o nome vem depois */
#define CV_TAM_FRAMEPROC_CORPO 26u        /* 5 u32 + u16 + u32 de flags; o registro e arredondado para 32 */
#define CV_TAM_FRAMEPROC 32u
#define CV_TAM_SUBSTREAM 8u               /* u32 tipo + u32 tamanho dos dados (alinhados a 4) */
#define CV_TAM_LINHAS_CABECALHO 12u       /* offCon u32, segCon u16, flags u16 0, cbCon u32 */
#define CV_TAM_BLOCO 12u                  /* FileId u32, nLines u32, cbBlock u32 = 12 + 8 * nLines */
#define CV_TAM_LINHA 8u                   /* offset u32 relativo ao proc + linha u32 (24 bits, delta 0, fStatement no bit 31) */
#define CV_TAM_ARQUIVO 8u                 /* NameIndex u32, cbChecksum u8 0, tipo u8 0, pad u16 */
#define CV_LINHA_STATEMENT 0x80000000u
#define CV_LINHA_MAX 0xFFFFFFu
#define CV_S_REGREL32 0x1111u
#define CV_REG_RBP 334u                   /* CV_AMD64_RBP: a variavel mora em [rbp + offset] (x64_quadro.c) */
#define CV_TAM_REGREL_FIXO 10u            /* i32 offset + u32 TypeIndex + u16 registrador; o nome C-string vem depois */

/* Estado de um DEBUG_S_LINES em curso: a ultima linha gravada decide se a proxima sai (offset novo e linha ou arquivo diferentes). */
typedef struct {
    Buf *b;
    const PdbParametros *p;
    const PdbArquivos *a;
    uint32_t segmento;
    uint32_t inicio, tam;             /* DbgProc.inicio e o tamanho do proc: os offsets gravados sao relativos a inicio */
    bool tem_ultima, bloco_aberto;
    uint32_t ultimo_off, ultima_linha, ultimo_arq;
    size_t bloco;                     /* posicao do cabecalho do bloco em aberto */
    uint32_t bloco_n;                 /* linhas do bloco em aberto */
} PdbLinhas;

void PdbPatchU32(Buf *b, size_t pos, uint32_t v)
{
    uint8_t *d = (uint8_t *)b->dados;

    for (int k = 0; k < 4; k++)
        d[pos + (size_t)k] = (uint8_t)((v >> (8 * k)) & 0xff);
}

static void PdbZeros(Buf *b, uint32_t n)
{
    static const char zeros[4] = {0};

    BufAnexarN(b, zeros, n);
}

static void PdbRegistroS_end(Buf *b)
{
    PdbU16(b, 2);
    PdbU16(b, CV_S_END);
}

/* S_FRAMEPROC: so o tamanho do quadro; o resto (padding, salvos, handler, flags) fica 0. */
static void PdbRegistroFrameProc(Buf *b, const DbgProc *p)
{
    PdbU16(b, CV_TAM_FRAMEPROC - 2);
    PdbU16(b, CV_S_FRAMEPROC);
    PdbU32(b, p->rbp_quadro);
    PdbZeros(b, 4);
    PdbZeros(b, 4);
    PdbZeros(b, 4);
    PdbZeros(b, 4);
    PdbZeros(b, 2);                                   /* SectionIdOfExceptionHandler */
    PdbZeros(b, 4);                                   /* flags */
    PdbZeros(b, CV_TAM_FRAMEPROC - CV_TAM_CABECALHO - CV_TAM_FRAMEPROC_CORPO);
}

/* Preenchimento do registro de simbolo ate o multiplo de 4: n bytes que terminam em F1 (F3 F2 F1 no maximo), nunca zeros. */
static void PdbPreenchimento(Buf *b, uint32_t n)
{
    static const char preenchimento[3] = {(char)0xF3, (char)0xF2, (char)0xF1};

    BufAnexarN(b, preenchimento + (3 - n), n);
}

/* S_REGREL32 de uma variavel: offset sobre RBP (o rbp_off do DbgVar, sem conversao), TypeIndex simples e o nome. */
static void PdbRegistroRegrel(Buf *b, const DbgVar *v, uint32_t tipo)
{
    uint32_t nome = (uint32_t)strlen(v->nome) + 1;
    uint32_t total = (CV_TAM_CABECALHO + CV_TAM_REGREL_FIXO + nome + 3u) & ~3u;

    PdbU16(b, total - 2);                             /* o tamanho nao conta a si mesmo */
    PdbU16(b, CV_S_REGREL32);
    PdbU32(b, (uint32_t)v->loc.rbp_off);
    PdbU32(b, tipo);
    PdbU16(b, CV_REG_RBP);
    BufAnexarN(b, v->nome, nome);
    PdbPreenchimento(b, total - CV_TAM_CABECALHO - CV_TAM_REGREL_FIXO - nome);
}

/* S_REGREL32 das variaveis do proc (parametros e locais), entre o S_FRAMEPROC e o S_END; o tipo nao simples pede o registro ao TPI. Indireta fica de fora: o home
 * guarda o endereco, nao o dado (LF_POINTER por referencia fica para depois). */
static void PdbVariaveisProc(Buf *b, const PdbParametros *p, const DbgProc *proc)
{
    for (uint32_t k = 0; k < proc->nvars; k++) {
        const DbgVar *v = VecEm(&p->dbg->vars, proc->var_ini + k);
        uint32_t tipo;

        if (v->loc.indireto)
            continue;
        tipo = PdbTipoIndice(p->tpi, v->tipo);
        if (tipo != 0)
            PdbRegistroRegrel(b, v, tipo);
    }
}

/* S_GPROC32 do proc: sem tipo (TypeIndex 0), DbgStart 0 e DbgEnd = CodeSize. Devolve a posicao do campo End, que PdbFluxoModulo remenda no S_END. */
static size_t PdbRegistroGproc(Buf *b, const DbgProc *p, uint32_t segmento)
{
    uint32_t nome = (uint32_t)strlen(p->nome) + 1;
    uint32_t total = (CV_TAM_CABECALHO + CV_TAM_GPROC_FIXO + nome + 3u) & ~3u;
    uint32_t tam_codigo = p->fim - p->inicio;
    size_t campo_fim;

    PdbU16(b, total - 2);
    PdbU16(b, CV_S_GPROC32);
    PdbU32(b, 0);                                     /* Parent */
    campo_fim = b->tam;
    PdbU32(b, 0);                                     /* End: o S_END do proc, remendado depois das variaveis */
    PdbU32(b, 0);                                     /* Next */
    PdbU32(b, tam_codigo);
    PdbU32(b, 0);                                     /* DbgStart */
    PdbU32(b, tam_codigo);                            /* DbgEnd */
    PdbU32(b, 0);                                     /* FunctionType */
    PdbU32(b, p->inicio);
    PdbU16(b, segmento);
    PdbZeros(b, 1);                                   /* flags do proc */
    BufAnexarN(b, p->nome, nome);
    PdbZeros(b, total - CV_TAM_CABECALHO - CV_TAM_GPROC_FIXO - nome);
    return campo_fim;
}

/* Abre um substream C13 (tipo e tamanho, preenchido em PdbSubstreamFechar). Devolve a posicao do campo tamanho. */
static size_t PdbSubstreamAbrir(Buf *b, uint32_t tipo)
{
    PdbU32(b, tipo);
    PdbU32(b, 0);
    return b->tam - 4;
}

static void PdbSubstreamFechar(Buf *b, size_t campo)
{
    PdbPatchU32(b, campo, (uint32_t)(b->tam - campo - 4));
}

/* FILECHKSMS: uma entrada de 8 B por arquivo; o FileId de cada bloco de linhas e o offset da entrada aqui (nao o indice). Sem checksum. */
static void PdbSubstreamArquivos(Buf *b, const PdbArquivos *a)
{
    size_t campo = PdbSubstreamAbrir(b, CV_C13_ARQUIVOS);

    for (size_t k = 0; k < a->lista.n; k++) {
        PdbU32(b, ((const PdbArquivo *)VecEm(&a->lista, k))->nome_off);
        PdbU32(b, 0);                                 /* cbChecksum 0, tipo 0, pad */
    }
    PdbSubstreamFechar(b, campo);
}

/* Fecha o bloco em aberto: nLines e cbBlock (= 12 + 8 * nLines) so se sabem no fim. */
static void PdbBlocoFechar(PdbLinhas *c)
{
    if (!c->bloco_aberto)
        return;
    PdbPatchU32(c->b, c->bloco + 4, c->bloco_n);
    PdbPatchU32(c->b, c->bloco + 8, CV_TAM_BLOCO + CV_TAM_LINHA * c->bloco_n);
    c->bloco_aberto = false;
}

static void PdbBlocoAbrir(PdbLinhas *c, uint32_t arquivo)
{
    PdbBlocoFechar(c);
    c->bloco = c->b->tam;
    PdbU32(c->b, CV_TAM_ARQUIVO * arquivo);           /* FileId: offset da entrada no FILECHKSMS */
    PdbU32(c->b, 0);
    PdbU32(c->b, 0);
    c->bloco_n = 0;
    c->bloco_aberto = true;
}

/* Uma linha do DbgPrograma (k = indice global): sai so se o offset e novo e a linha ou o arquivo mudou; a 1a do proc vai para o offset 0. */
static void PdbLinhaVista(PdbLinhas *c, uint32_t k)
{
    const DbgLinha *l = VecEm(&c->p->dbg->linhas, k);
    const PdbPosLinha *pos = VecEm(&c->a->linhas, k);
    uint32_t rel;

    if (l->off < c->inicio || l->off - c->inicio >= c->tam)
        FalhaInterna("pdb: linha no offset %u fora do proc (%u..%u)", l->off, c->inicio, c->inicio + c->tam);
    if (pos->linha > CV_LINHA_MAX)
        FalhaInterna("pdb: linha %u passa de 24 bits", pos->linha);
    rel = c->tem_ultima ? l->off - c->inicio : 0;
    if (c->tem_ultima && (rel <= c->ultimo_off || (pos->linha == c->ultima_linha && pos->arquivo == c->ultimo_arq)))
        return;
    if (!c->bloco_aberto || pos->arquivo != c->ultimo_arq)
        PdbBlocoAbrir(c, pos->arquivo);
    PdbU32(c->b, rel);
    PdbU32(c->b, pos->linha | CV_LINHA_STATEMENT);
    c->bloco_n++;
    c->tem_ultima = true;
    c->ultimo_off = rel;
    c->ultima_linha = pos->linha;
    c->ultimo_arq = pos->arquivo;
}

/* DEBUG_S_LINES do proc: cabecalho (offCon = inicio, segCon = .text, cbCon = tamanho do proc) e os blocos por arquivo. Proc sem linha nao tem registro. */
static void PdbProcLinhas(PdbLinhas *c, const DbgProc *proc)
{
    size_t campo;

    if (proc->nlinhas == 0)
        return;
    campo = PdbSubstreamAbrir(c->b, CV_C13_LINHAS);
    PdbU32(c->b, proc->inicio);
    PdbU16(c->b, c->segmento);
    PdbU16(c->b, 0);
    PdbU32(c->b, proc->fim - proc->inicio);
    c->inicio = proc->inicio;
    c->tam = proc->fim - proc->inicio;
    c->tem_ultima = false;
    c->bloco_aberto = false;
    for (uint32_t k = 0; k < proc->nlinhas; k++)
        PdbLinhaVista(c, proc->linha_ini + k);
    PdbBlocoFechar(c);
    PdbSubstreamFechar(c->b, campo);
}

/* O C13 do modulo: FILECHKSMS (se ha arquivo) antes e depois um DEBUG_S_LINES por proc com linha. */
static void PdbC13(Buf *b, const PdbParametros *p, const PdbArquivos *a, uint32_t segmento)
{
    PdbLinhas c = {.b = b, .p = p, .a = a, .segmento = segmento};

    if (a->lista.n > 0)
        PdbSubstreamArquivos(b, a);
    for (size_t k = 0; k < p->dbg->procs.n; k++)
        PdbProcLinhas(&c, VecEm(&p->dbg->procs, k));
}

PdbTamanhosModulo PdbFluxoModulo(Buf *b, const PdbParametros *p, const PdbArquivos *a, uint32_t segmento)
{
    PdbTamanhosModulo t;
    size_t inicio_c13;

    t.arquivos = a;
    PdbU32(b, CV_ASSINATURA_C13);
    for (size_t k = 0; k < p->dbg->procs.n; k++) {
        const DbgProc *proc = VecEm(&p->dbg->procs, k);
        size_t campo_fim;

        if (proc->fim < proc->inicio || proc->fim > p->codigo_tam)
            FalhaInterna("pdb: o proc '%s' (%u..%u) passa do .text do programa (%u bytes)", proc->nome, proc->inicio, proc->fim, p->codigo_tam);
        campo_fim = PdbRegistroGproc(b, proc, segmento);
        PdbRegistroFrameProc(b, proc);
        PdbVariaveisProc(b, p, proc);
        PdbPatchU32(b, campo_fim, (uint32_t)b->tam);  /* End: o S_END comeca aqui */
        PdbRegistroS_end(b);
    }
    t.simbolos = (uint32_t)b->tam;                    /* SymByteSize: assinatura + registros */
    inicio_c13 = b->tam;
    PdbC13(b, p, a, segmento);
    t.c13 = (uint32_t)(b->tam - inicio_c13);
    PdbU32(b, 0);                                     /* GlobalRefsSize */
    return t;
}
