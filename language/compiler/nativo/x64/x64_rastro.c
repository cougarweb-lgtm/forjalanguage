/* x64_rastro.c - tabela de posicoes do --rastreio (forja run|test --rastreio, backend x64): para cada "call" gerado em proc .fj, onde ele fica em .text e a posicao
 * (arquivo, linha, coluna) dele no fonte; mais o inicio e o nome de cada proc. Vai para o .rdata do exe com os simbolos forja_rastro_tabela e forja_rastro_texto, que o
 * runtime da variante -DFORJA_RASTREIO (runtime\nativo\rt_pilha.c, formato no cabecalho dele) le junto da cadeia de rbp. Sem --rastreio nada disto roda e nenhum byte muda.
 * Versao: 0.3.0
 * Changelog: 0.3.0 - M6e P93d: cabecalho de 12 palavras ([10] flags, bit0 = guarda; [11] off_linhas, 0 = sem tabela); com X64Ctx.guarda a subtabela de LINHAS {u32 nlinhas; nlinhas x {off, arquivo, linha, coluna}} sai do DbgPrograma (o .text nao muda).
 *            0.2.0 - M6 P65b: a tabela le funcoes, chamadas e a pos do main do DbgPrograma (X64Ctx.dbg); formato FRT1 inalterado; X64RastroIniciar/Funcao saem.
 *            0.1.0 - IA1-b12: primeira versao (X64RastroIniciar, X64RastroFuncao, X64RastroChamada, X64RastroFechar).
 * Verificacao: construir.bat && python -I ..\tests\ia_first\verificar_rastreio_b12.py --forja .\forja.exe
 */
#include <string.h>

#include "common/fonte.h"
#include "nativo/x64/x64_interno.h"

#define RASTRO_MAGIA 0x31545246u          /* "FRT1" */
#define RASTRO_CABECALHO_PALAVRAS 12u
#define RASTRO_PALAVRAS_FUNCAO 2u
#define RASTRO_PALAVRAS_CHAMADA 4u
#define RASTRO_PALAVRAS_LINHA 4u
#define RASTRO_FLAG_GUARDA 1u
#define RASTRO_ALINHAMENTO 8u
#define RASTRO_SIMBOLO_TABELA "forja_rastro_tabela"
#define RASTRO_SIMBOLO_TEXTO "forja_rastro_texto"

typedef struct { const char *caminho; uint32_t off; } RastroArquivo;

/* A tabela em montagem: palavras (cabecalho, funcoes, chamadas) e os textos (nomes e caminhos, com NUL); base = onde os textos comecam na tabela final. */
typedef struct {
    Buf palavras, textos;
    Vec arquivos;               /* RastroArquivo: cada caminho entra uma vez */
    uint32_t base;
} RastroTabela;

static bool CallDeProcFj(const X64Ctx *c, const MInstr *mi)
{
    bool chamada = mi->forma == FX_CALL_REL32 || mi->forma == FX_CALL_M64 || mi->forma == FX_CALL_R64;

    return chamada && c->instr != NULL && c->instr->pos != BIR_SEM_POS && !c->f->sintetica && c->p->fonte != NULL;
}

/* Depois de emitir o call: o endereco de retorno (offset logo apos a instrucao) e a posicao da BirInstr de origem. */
void X64RastroChamada(X64Ctx *c, const MInstr *mi)
{
    if (CallDeProcFj(c, mi))
        DbgChamadaVista(c->dbg, X64Offset(c), c->instr->pos);
}

static void Palavra(RastroTabela *t, uint32_t v)
{
    BufAnexarN(&t->palavras, (const char *)&v, sizeof v);
}

/* Offset do texto na tabela final (base + posicao em textos). */
static uint32_t TextoAdicionar(RastroTabela *t, const char *s)
{
    uint32_t off = t->base + (uint32_t)t->textos.tam;

    BufAnexarN(&t->textos, s, strlen(s) + 1);
    return off;
}

static uint32_t CaminhoOffset(RastroTabela *t, const char *caminho)
{
    RastroArquivo *novo;

    for (size_t k = 0; k < t->arquivos.n; k++) {
        const RastroArquivo *a = VecEm(&t->arquivos, k);

        if (strcmp(a->caminho, caminho) == 0)
            return a->off;
    }
    novo = VecEmpurrar(&t->arquivos);
    novo->caminho = caminho;
    novo->off = TextoAdicionar(t, caminho);
    return novo->off;
}

static void EscreverFuncoes(const X64Ctx *c, RastroTabela *t)
{
    for (size_t k = 0; k < c->dbg->procs.n; k++) {
        const DbgProc *f = VecEm(&c->dbg->procs, k);

        Palavra(t, f->inicio);
        Palavra(t, TextoAdicionar(t, f->nome));
    }
}

static void EscreverChamadas(const X64Ctx *c, RastroTabela *t)
{
    for (size_t k = 0; k < c->dbg->chamadas.n; k++) {
        const DbgChamada *ch = VecEm(&c->dbg->chamadas, k);
        FontePosicao p = FonteLocalizarNoArquivo(c->p->fonte, ch->pos);

        Palavra(t, ch->ra);
        Palavra(t, CaminhoOffset(t, p.caminho));
        Palavra(t, p.linha);
        Palavra(t, p.coluna);
    }
}

/* Subtabela de linhas (so com --guarda-memoria): quantidade e um {off, arquivo, linha, coluna} por par do DbgPrograma, em ordem de off. */
static void EscreverLinhas(const X64Ctx *c, RastroTabela *t)
{
    uint32_t dica = 0;

    Palavra(t, (uint32_t)c->dbg->linhas.n);
    for (size_t k = 0; k < c->dbg->linhas.n; k++) {
        const DbgLinha *l = VecEm(&c->dbg->linhas, k);
        FontePosicao p = DbgPosicaoComDica(c->p->fonte, l->pos, &dica);

        Palavra(t, l->off);
        Palavra(t, CaminhoOffset(t, p.caminho));
        Palavra(t, p.linha);
        Palavra(t, p.coluna);
    }
}

/* Nome, arquivo, linha e coluna do corpo do main (zeros se nao ha main com posicao). */
static void CabecalhoDoMain(const X64Ctx *c, RastroTabela *t, uint32_t destino[4])
{
    FontePosicao p;

    if (c->dbg->main_pos == BIR_SEM_POS)
        return;
    p = FonteLocalizarNoArquivo(c->p->fonte, c->dbg->main_pos);
    destino[0] = TextoAdicionar(t, "main");
    destino[1] = CaminhoOffset(t, p.caminho);
    destino[2] = p.linha;
    destino[3] = p.coluna;
}

/* Junta palavras e textos num bloco so e o acrescenta ao .rdata do programa (abre a secao se nao ha), alinhado a 8 bytes; devolve o offset da tabela. */
static uint32_t AnexarEmRdata(X64Ctx *c, const RastroTabela *t, int32_t *secao)
{
    uint32_t rdata = UINT32_MAX;
    ObjSecao *s;

    for (uint32_t k = 0; k < c->o->secoes.n && rdata == UINT32_MAX; k++) {
        if (strcmp(((ObjSecao *)VecEm(&c->o->secoes, k))->nome, ".rdata") == 0)
            rdata = k;
    }
    if (rdata == UINT32_MAX)
        rdata = ObjSecaoAbrir(c->o, ".rdata", OS_RDATA, RASTRO_ALINHAMENTO);
    s = VecEm(&c->o->secoes, rdata);
    if (s->alinhamento < RASTRO_ALINHAMENTO)
        s->alinhamento = RASTRO_ALINHAMENTO;
    while (s->bytes.tam % RASTRO_ALINHAMENTO != 0)
        BufAnexarN(&s->bytes, "", 1);
    *secao = (int32_t)rdata;
    BufAnexarN(&s->bytes, t->palavras.dados, t->palavras.tam);
    BufAnexarN(&s->bytes, t->textos.dados, t->textos.tam);
    return (uint32_t)(s->bytes.tam - t->palavras.tam - t->textos.tam);
}

/* Fim do X64Gerar com --rastreio: monta a tabela, grava no .rdata e define forja_rastro_tabela (.rdata) e forja_rastro_texto (inicio do .text do programa). */
void X64RastroFechar(X64Ctx *c)
{
    RastroTabela t = {0};
    uint32_t nf = (uint32_t)c->dbg->procs.n, nc = (uint32_t)c->dbg->chamadas.n;
    uint32_t cab_main[4] = {0, 0, 0, 0};
    uint32_t off_funcoes = RASTRO_CABECALHO_PALAVRAS * 4u;
    uint32_t off_chamadas = off_funcoes + nf * RASTRO_PALAVRAS_FUNCAO * 4u;
    uint32_t off_linhas = c->guarda ? off_chamadas + nc * RASTRO_PALAVRAS_CHAMADA * 4u : 0;
    uint32_t nl = c->guarda ? (uint32_t)c->dbg->linhas.n : 0;
    int32_t secao = 0;
    uint32_t off;

    t.base = off_chamadas + nc * RASTRO_PALAVRAS_CHAMADA * 4u;
    if (c->guarda)
        t.base += 4u + nl * RASTRO_PALAVRAS_LINHA * 4u;
    VecIniciar(&t.arquivos, sizeof(RastroArquivo));
    CabecalhoDoMain(c, &t, cab_main);
    Palavra(&t, RASTRO_MAGIA);
    Palavra(&t, X64Offset(c));
    Palavra(&t, nf);
    Palavra(&t, nc);
    Palavra(&t, off_funcoes);
    Palavra(&t, off_chamadas);
    for (int k = 0; k < 4; k++)
        Palavra(&t, cab_main[k]);
    Palavra(&t, c->guarda ? RASTRO_FLAG_GUARDA : 0);
    Palavra(&t, off_linhas);
    EscreverFuncoes(c, &t);
    EscreverChamadas(c, &t);
    if (c->guarda)
        EscreverLinhas(c, &t);
    off = AnexarEmRdata(c, &t, &secao);
    ObjSimboloDefinir(c->o, RASTRO_SIMBOLO_TABELA, secao, off);
    ObjSimboloDefinir(c->o, RASTRO_SIMBOLO_TEXTO, (int32_t)c->texto, 0);
    BufLiberar(&t.palavras);
    BufLiberar(&t.textos);
    VecLiberar(&t.arquivos);
}
