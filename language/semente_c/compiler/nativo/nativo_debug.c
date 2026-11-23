/* nativo_debug.c - --debug-code: a lista de itens e os REGISTROS (DebugPercorrer), a unica fonte do texto e do JSONL.
 * Versao: 0.7.0
 * Changelog: 0.7.0 - G0d: item passes (REG_PASSES com o texto colhido pelo OtimExecutar; depois do otim).
 *            0.6.0 - M8 P2: item perfil (PercorrerPerfil: um REG_PERFIL com "id nome" de cada proc que o passe `perfil` instrumentou).
 *            0.5.0 - M8 H1: item interface (PercorrerInterface: um REG_INTERFACE com o texto canonico da interface da DLL).
 *            0.4.0 - M7a F0: item otim (REG_OTIM logo depois dos locais e linhas).
 *            0.3.0 - M6 P65b: item linhas (PercorrerLinhas: um REG_LINHAS por proc do DbgPrograma).
 *            0.2.9 - M6 P65a: item locais (PercorrerTrechos serve ao bir e aos locais).
 *            0.2.8 - M5 P27b: no ELF, o registro formato leva elf = true e PercorrerSegmentos emite um "segmento" por PT_* (ElfPlanejar) depois das secoes; PE igual (esquema 0.7.0).
 *            0.2.7 - M4 P24p: PercorrerPoda (registro "poda" logo depois do cabecalho: procs do programa e os que entraram na BIR).
 *            0.2.6 - M3 F9b-1: decisoes extraidas de PercorrerFuncao e DebugPercorrer.
 *            0.2.5 - M3 F9a: PercorrerImports de 4 para 2 niveis (EmitirImport emite o registro); saida igual.
 *            0.2.4 - M3 P22a: PercorrerExports (registro "export" por simbolo da DLL, depois dos "import") e a imagem_base do formato vem de PeBaseDaImagem (0x180000000 na DLL).
 *            0.2.3 - M3 P19a: PercorrerImports percorre as DLLs da Imagem (ImagemImportacao.dlls) e grava o indice do descritor em cada registro "import"; LigarImportacoes saiu.
 *            0.2.2 -M2 P17c: PercorrerImports usa LigarImportacoes(im, ...): lista so os imports que o exe tem; a faixa "runtime" comeca em Imagem.runtime_texto_rva
 *                    (o ligador descarta as funcoes do runtime que o programa nao alcanca).
 *            0.2.1 - M2 P9a: PosicaoDe devolve PosicaoFonte (3 parametros em vez de 4 com ponteiros de saida).
 *            0.2.0 - backend x64 P8b: DebugPercorrer produz os registros (funcao, instr, bir, reloc, secao, import, formato, faixa, tempos); o texto saiu para
 *                    nativo_debug_texto.c e o JSON e novo (nativo_debug_json.c). Item "json". Os bytes de cada instrucao sao os do exe ligado.
 *            0.1.0 - backend x64 P8: DebugItens e DebugMontar. isel_ns fica com a selecao, a alocacao e a codificacao juntas (o degrau 0 nao as
 *                    separa; cronometrar cada instrucao custaria mais que o proprio gerador): regs_ns e codificar_ns saem 0.
 * Verificacao: construir.bat && python -I ..\tests\cli\verificar_cli.py --forja .\forja.exe
 *
 * Cobertura do .text: toda a secao e instrucao ou faixa. Cada funcao tem suas instrucoes e a faixa "preenchimento" (0xCC ate 16 bytes) que
 * vem depois; entre o fim do programa e o runtime pode haver mais preenchimento do ligador; o resto e o .text do runtime embutido.
 */
#include <stdio.h>
#include <string.h>

#include "backend/interface_dll.h"
#include "nativo/fmt/elf.h"
#include "nativo/nativo_debug.h"
#include "nativo/optimizer/perfil.h"

typedef struct {
    const char *nome;
    uint32_t bit;
} ItemDebug;

static const ItemDebug ITENS[] = {
    {"bir", DEBUG_BIR}, {"asm", DEBUG_ASM}, {"quadros", DEBUG_QUADROS},
    {"reloc", DEBUG_RELOC}, {"formato", DEBUG_FORMATO}, {"tempos", DEBUG_TEMPOS}, {"json", DEBUG_JSON},
    {"locais", DEBUG_LOCAIS}, {"linhas", DEBUG_LINHAS}, {"otim", DEBUG_OTIM}, {"interface", DEBUG_INTERFACE}, {"perfil", DEBUG_PERFIL},
    {"passes", DEBUG_PASSES},
};

#define DEBUG_QTD_ITENS (sizeof ITENS / sizeof ITENS[0])
#define DEBUG_TAM_NOME_BIR 256

static bool ItemPorNome(const char *nome, size_t n, uint32_t *bit)
{
    for (size_t k = 0; k < DEBUG_QTD_ITENS; k++) {
        if (strlen(ITENS[k].nome) == n && strncmp(ITENS[k].nome, nome, n) == 0) {
            *bit = ITENS[k].bit;
            return true;
        }
    }
    return false;
}

bool DebugItens(const char *lista, uint32_t *mascara, char *causa, size_t n)
{
    const char *p = lista;

    *mascara = 0;
    while (*p != '\0') {
        const char *fim = strchr(p, ',');
        size_t tam = fim != NULL ? (size_t)(fim - p) : strlen(p);
        uint32_t bit = 0;

        if (!ItemPorNome(p, tam, &bit)) {
            snprintf(causa, n, "item desconhecido em --debug-code: '%.*s' (validos: bir,asm,quadros,reloc,formato,tempos,json,locais,linhas,otim,interface,perfil,passes)", (int)tam, p);
            return false;
        }
        *mascara |= bit;
        p += tam + (fim != NULL ? 1 : 0);
    }
    if (*mascara == DEBUG_JSON)
        *mascara |= DEBUG_TODOS_ITENS;
    return true;
}

const char *DebugNomeOrigem(uint8_t origem)
{
    static const char *const NOMES[] = {"prologo", "sondagem", "codigo", "frio", "epilogo"};

    return origem < sizeof NOMES / sizeof NOMES[0] ? NOMES[origem] : "?";
}

/* ---- percurso ---- */

typedef struct {
    const DebugEntrada *e;
    DebugEscritor escrever;
    void *estado;
    const PeSecao *texto;       /* .text da imagem ligada */
} Percurso;

static void Emitir(const Percurso *p, const DebugReg *reg)
{
    p->escrever(p->estado, reg);
}

static const PeSecao *AcharSecaoDaImagem(const Imagem *im, const char *nome)
{
    for (uint32_t k = 0; k < im->qtd_secoes; k++) {
        if (strncmp(im->secoes[k].nome, nome, 8) == 0)
            return &im->secoes[k];
    }
    return NULL;
}

static uint32_t TamanhoDoTexto(const Objeto *o)
{
    for (size_t k = 0; k < o->secoes.n; k++) {
        const ObjSecao *s = VecEm(&o->secoes, k);

        if (strcmp(s->nome, ".text") == 0)
            return (uint32_t)s->bytes.tam;
    }
    return 0;
}

/* Posicao local ao .fj (arquivo, linha, coluna); arquivo NULL e linha 0 quando a instrucao nao tem posicao. */
typedef struct {
    const char *arquivo;
    uint32_t linha, coluna;
} PosicaoFonte;

static PosicaoFonte PosicaoDe(const Fonte *fonte, uint32_t pos)
{
    PosicaoFonte r = {NULL, 0, 0};
    FontePosicao p;

    if (pos == BIR_SEM_POS || fonte == NULL)
        return r;
    p = FonteLocalizarNoArquivo(fonte, pos);
    r.arquivo = p.caminho;
    r.linha = p.linha;
    r.coluna = p.coluna;
    return r;
}

static void PercorrerCabecalho(const Percurso *p)
{
    DebugReg reg = {.tipo = REG_CABECALHO, .cabecalho = {p->e->versao, p->e->alvo, p->e->modo}};

    Emitir(p, &reg);
}

static void PercorrerPoda(const Percurso *p)
{
    DebugReg reg = {.tipo = REG_PODA, .poda = {p->e->procs_total, p->e->procs_gerados}};

    Emitir(p, &reg);
}

static const char *NomeNaListagem(const AlvoListagem *l, uint32_t offset)
{
    return l->textos.dados + offset;
}

static void EmitirFuncao(const Percurso *p, const ListaFuncao *f)
{
    const AlvoListagem *l = p->e->lista;
    RegFuncao r = {.nome = NomeNaListagem(l, f->nome), .rva = p->texto->rva + f->inicio, .inicio = f->inicio, .tamanho = f->fim - f->inicio,
                   .quadro = f};
    DebugReg reg = {.tipo = REG_FUNCAO};

    PosicaoFonte pos = PosicaoDe(p->e->fonte, f->pos);

    r.arquivo = pos.arquivo;
    r.linha = pos.linha;
    reg.funcao = r;
    Emitir(p, &reg);
}

static void EmitirInstr(const Percurso *p, const ListaFuncao *f, const ListaInstr *i)
{
    const AlvoListagem *l = p->e->lista;
    RegInstr r = {.funcao = NomeNaListagem(l, f->nome), .texto_asm = NomeNaListagem(l, i->texto), .rva = p->texto->rva + i->off,
                  .deslocamento = i->off - f->inicio, .tam = i->tam, .bytes = (const uint8_t *)p->texto->bytes.dados + i->off,
                  .bloco = i->bloco, .indice = i->indice, .origem = i->origem};
    DebugReg reg = {.tipo = REG_INSTR};

    r.simbolo = i->simbolo != 0 ? NomeNaListagem(l, i->simbolo - 1) : NULL;
    PosicaoFonte pos = PosicaoDe(p->e->fonte, i->pos);

    r.arquivo = pos.arquivo;
    r.linha = pos.linha;
    r.coluna = pos.coluna;
    reg.instr = r;
    Emitir(p, &reg);
}

static void EmitirFaixa(const Percurso *p, uint32_t off, uint32_t tamanho, const char *origem)
{
    DebugReg reg = {.tipo = REG_FAIXA, .faixa = {p->texto->rva + off, tamanho, origem}};

    if (tamanho > 0)
        Emitir(p, &reg);
}

/* As instrucoes da funcao k a partir de proxima_instr (emitidas so com o item asm); devolve a primeira que e de outra funcao. */
static size_t PercorrerInstrs(const Percurso *p, size_t k, const ListaFuncao *f, size_t proxima_instr)
{
    const AlvoListagem *l = p->e->lista;

    for (; proxima_instr < l->instrs.n; proxima_instr++) {
        const ListaInstr *i = VecEm(&l->instrs, proxima_instr);

        if (i->funcao != k)
            break;
        if (p->e->mascara & DEBUG_ASM)
            EmitirInstr(p, f, i);
    }
    return proxima_instr;
}

/* Funcao k: o registro, as instrucoes dela (item asm) e o preenchimento ate a proxima funcao (ou ate o fim do programa). */
static size_t PercorrerFuncao(const Percurso *p, size_t k, size_t proxima_instr, uint32_t fim_programa)
{
    const AlvoListagem *l = p->e->lista;
    const ListaFuncao *f = VecEm(&l->funcoes, k);
    const ListaFuncao *seguinte = k + 1 < l->funcoes.n ? VecEm(&l->funcoes, k + 1) : NULL;
    uint32_t limite = seguinte != NULL ? seguinte->inicio : fim_programa;

    EmitirFuncao(p, f);
    proxima_instr = PercorrerInstrs(p, k, f, proxima_instr);
    if (p->e->mascara & DEBUG_ASM)
        EmitirFaixa(p, f->fim, limite - f->fim, "preenchimento");
    return proxima_instr;
}

/* Depois da ultima funcao: o alinhamento que o ligador poe antes do runtime e o que o runtime pos no .text (so as funcoes alcancadas; vai ate o fim da secao). */
static void PercorrerFaixasDoRuntime(const Percurso *p, uint32_t fim_programa)
{
    uint32_t inicio_runtime = p->e->imagem->runtime_texto_rva - p->texto->rva;
    uint32_t tam_runtime = p->texto->tam_virtual - inicio_runtime;

    EmitirFaixa(p, fim_programa, inicio_runtime - fim_programa, "preenchimento");
    EmitirFaixa(p, inicio_runtime, tam_runtime, "runtime");
}

static void PercorrerFuncoes(const Percurso *p)
{
    uint32_t fim_programa = TamanhoDoTexto(p->e->codigo);
    size_t proxima_instr = 0;

    for (size_t k = 0; k < p->e->lista->funcoes.n; k++)
        proxima_instr = PercorrerFuncao(p, k, proxima_instr, fim_programa);
    if (p->e->mascara & DEBUG_ASM)
        PercorrerFaixasDoRuntime(p, fim_programa);
}

/* Tamanho do trecho que comeca em s: a linha de abertura mais as seguintes ate a proxima "funcao " ou "simbolos:". */
static size_t TamanhoDoTrecho(const char *s, size_t resto)
{
    size_t n = 0;

    do {
        const char *fim = memchr(s + n, '\n', resto - n);

        n = fim != NULL ? (size_t)(fim - s) + 1 : resto;
    } while (n < resto && strncmp(s + n, "funcao ", 7) != 0 && strncmp(s + n, "simbolos:", 9) != 0);
    return n;
}

/* "funcao fj_main(v0:i64) -> vazio" -> "fj_main"; o bloco "simbolos:" fica sem nome. */
static void NomeDoTrecho(const char *s, char *nome)
{
    size_t n = 0;

    nome[0] = '\0';
    if (strncmp(s, "funcao ", 7) != 0)
        return;
    while (s[7 + n] != '(' && s[7 + n] != '\n' && s[7 + n] != '\0' && n + 1 < DEBUG_TAM_NOME_BIR)
        n++;
    memcpy(nome, s + 7, n);
    nome[n] = '\0';
}

/* Um registro por trecho "funcao ..." do texto colhido: tipo REG_BIR (item bir) ou REG_LOCAIS (item locais). */
static void PercorrerTrechos(const Percurso *p, const Buf *texto, uint8_t tipo)
{
    const char *s = texto->dados;
    size_t resto = texto->tam;
    char nome[DEBUG_TAM_NOME_BIR];

    while (s != NULL && resto > 0) {
        size_t n = TamanhoDoTrecho(s, resto);
        DebugReg reg = {.tipo = tipo};

        NomeDoTrecho(s, nome);
        reg.bir = (RegBir){nome, s, n};
        Emitir(p, &reg);
        s += n;
        resto -= n;
    }
}

static void PercorrerBir(const Percurso *p)
{
    PercorrerTrechos(p, p->e->bir, REG_BIR);
}

/* Um registro por proc do DbgPrograma (item linhas); sem coleta nao emite nada. */
static void PercorrerLinhas(const Percurso *p)
{
    const DbgPrograma *d = p->e->dbg;

    for (size_t k = 0; d != NULL && k < d->procs.n; k++) {
        DebugReg reg = {.tipo = REG_LINHAS};

        reg.linhas = (RegLinhas){d, VecEm(&d->procs, k), p->e->fonte};
        Emitir(p, &reg);
    }
}

static void PercorrerReloc(const Percurso *p)
{
    static const char *const TIPOS[] = {"REL32", "ADDR64", "ADDR32NB"};
    const Objeto *o = p->e->codigo;

    for (size_t k = 0; k < o->relocs.n; k++) {
        const ObjReloc *r = VecEm(&o->relocs, k);
        const ObjSecao *s = VecEm(&o->secoes, r->alvo.secao);
        const PeSecao *na_imagem = AcharSecaoDaImagem(p->e->imagem, s->nome);
        DebugReg reg = {.tipo = REG_RELOC};

        reg.reloc = (RegReloc){s->nome, TIPOS[r->correcao.tipo], ((const ObjSimbolo *)VecEm(&o->simbolos, r->correcao.simbolo))->nome,
                               r->alvo.off, (na_imagem != NULL ? na_imagem->rva : 0) + r->alvo.off, r->correcao.adendo};
        Emitir(p, &reg);
    }
}

static void PercorrerSecoes(const Percurso *p)
{
    const Imagem *im = p->e->imagem;

    for (uint32_t k = 0; k < im->qtd_secoes; k++) {
        const PeSecao *s = &im->secoes[k];
        DebugReg reg = {.tipo = REG_SECAO};

        memcpy(reg.secao.nome, s->nome, 8);
        reg.secao.nome[8] = '\0';
        reg.secao.rva = s->rva;
        reg.secao.tam_virtual = s->tam_virtual;
        reg.secao.tam_bruto = (uint32_t)s->bytes.tam;
        reg.secao.caracteristicas = s->caracteristicas;
        Emitir(p, &reg);
    }
}

static void EmitirImport(const Percurso *p, const ImagemImportacao *imp, uint32_t d, uint32_t k)
{
    DebugReg reg = {.tipo = REG_IMPORT, .import = {imp->dlls[d].nome, imp->nomes[k], imp->iat_rva[k], d}};

    Emitir(p, &reg);
}

static void PercorrerImports(const Percurso *p)
{
    const ImagemImportacao *imp = &p->e->imagem->importacao;

    for (uint32_t d = 0; d < imp->qtd_dlls; d++) {
        const ImagemDll *dll = &imp->dlls[d];

        for (uint32_t k = dll->primeiro; k < dll->primeiro + dll->qtd; k++) EmitirImport(p, imp, d, k);
    }
}

/* DLL: um registro por simbolo do export directory, na ordem da tabela (ASCII); o ordinal e a posicao + 1 (ordinal base 1). */
static void PercorrerExports(const Percurso *p)
{
    const Imagem *im = p->e->imagem;

    for (uint32_t k = 0; k < im->exportacoes.n; k++) {
        const PeExporta *x = VecEm(&im->exportacoes, k);
        DebugReg reg = {.tipo = REG_EXPORT, .exporta = {x->nome, x->rva, k + 1}};

        Emitir(p, &reg);
    }
}

/* ELF: um registro por cabecalho de programa (o plano e o mesmo que o escritor usou). PE: nada. */
static void PercorrerSegmentos(const Percurso *p)
{
    static const char *const FLAGS[8] = {"", "X", "W", "WX", "R", "RX", "RW", "RWX"};
    ElfPlano plano;

    if (p->e->imagem->formato != IMAGEM_ELF || ElfPlanejar(p->e->imagem, &plano) != NULL)
        return;
    for (uint32_t k = 0; k < plano.qtd; k++) {
        const ElfSegmento *s = &plano.seg[k];
        DebugReg reg = {.tipo = REG_SEGMENTO, .segmento = {s->tipo == ELF_PT_LOAD ? "LOAD" : "GNU_STACK", FLAGS[s->flags & 7u], s->offset, s->vaddr, s->filesz, s->memsz,
                                                           s->alinhamento}};

        Emitir(p, &reg);
    }
}

static void PercorrerFormato(const Percurso *p)
{
    const Imagem *im = p->e->imagem;
    DebugReg reg = {.tipo = REG_FORMATO,
                    .formato = {p->e->versao, PeBaseDaImagem(im), im->entrada_rva, p->e->tam_exe, im->linker_maior, im->linker_menor,
                                im->formato == IMAGEM_ELF}};

    Emitir(p, &reg);
    PercorrerSecoes(p);
    PercorrerSegmentos(p);
    PercorrerImports(p);
    PercorrerExports(p);
}

/* Item interface: o texto canonico que o hash do forja_dll_info cobre (o mesmo EmitirInterfaceDll da struct da DLL); so o texto o escreve. */
static void PercorrerInterface(const Percurso *p)
{
    Buf texto = {0};
    DebugReg reg = {.tipo = REG_INTERFACE};

    EmitirInterfaceDll(p->e->pedido->ir, p->e->pedido->tipos, p->e->pedido->nomes, &texto);
    reg.texto_interface = (RegBir){"", texto.dados, texto.tam};
    Emitir(p, &reg);
    BufLiberar(&texto);
}

/* Item perfil: "perfil: N procs instrumentadas" e uma linha "id nome" por proc de usuario (a ordem da tabela FPF1); sem --perfil, so a frase "desligado". So o texto o escreve. */
static void PercorrerPerfil(const Percurso *p)
{
    const BirPrograma *bir = p->e->programa;
    Buf texto = {0};
    DebugReg reg = {.tipo = REG_PERFIL};
    char nome[PERFIL_NOME_MAX];
    uint32_t id = 0;

    for (uint32_t i = 0; i < bir->nfuncoes && bir->perfil; i++) {
        if (!OtimPerfilProc(bir, &bir->funcoes[i]))
            continue;
        OtimPerfilNome(bir, &bir->funcoes[i], nome, sizeof nome);
        BufFormatar(&texto, "%u %s\n", (unsigned)id++, nome);
    }
    if (bir->perfil)
        BufFormatar(&texto, "perfil: %u procs instrumentadas\n", (unsigned)id);
    else
        BufFormatar(&texto, "perfil: desligado (use --perfil)\n");
    reg.texto_perfil = (RegBir){"", texto.dados, texto.tam};
    Emitir(p, &reg);
    BufLiberar(&texto);
}

/* Itens asm ou quadros pedidos e a imagem tem .text: as funcoes. */
static void PercorrerFuncoesPedidas(const Percurso *p)
{
    if ((p->e->mascara & (DEBUG_ASM | DEBUG_QUADROS)) != 0 && p->texto != NULL)
        PercorrerFuncoes(p);
}

void DebugPercorrer(const DebugEntrada *e, DebugEscritor escrever, void *estado)
{
    Percurso p = {.e = e, .escrever = escrever, .estado = estado};

    p.texto = e->imagem != NULL ? AcharSecaoDaImagem(e->imagem, ".text") : NULL;
    PercorrerCabecalho(&p);
    PercorrerPoda(&p);
    PercorrerFuncoesPedidas(&p);
    if (e->mascara & DEBUG_BIR)
        PercorrerBir(&p);
    if (e->mascara & DEBUG_LOCAIS)
        PercorrerTrechos(&p, e->locais, REG_LOCAIS);
    if (e->mascara & DEBUG_LINHAS)
        PercorrerLinhas(&p);
    if ((e->mascara & DEBUG_OTIM) != 0 && e->otim != NULL) {
        DebugReg reg = {.tipo = REG_OTIM};

        reg.otim = (RegBir){"", e->otim->dados, e->otim->tam};
        Emitir(&p, &reg);
    }
    if ((e->mascara & DEBUG_PASSES) != 0 && e->passes != NULL) {
        DebugReg reg = {.tipo = REG_PASSES};

        reg.texto_passes = (RegBir){"", e->passes->dados, e->passes->tam};
        Emitir(&p, &reg);
    }
    if ((e->mascara & DEBUG_INTERFACE) != 0 && e->pedido != NULL)
        PercorrerInterface(&p);
    if ((e->mascara & DEBUG_PERFIL) != 0 && e->programa != NULL)
        PercorrerPerfil(&p);
    if (e->mascara & DEBUG_RELOC)
        PercorrerReloc(&p);
    if (e->mascara & DEBUG_FORMATO)
        PercorrerFormato(&p);
    if (e->mascara & DEBUG_TEMPOS) {
        DebugReg reg = {.tipo = REG_TEMPOS, .tempos = {e->tempos}};

        Emitir(&p, &reg);
    }
}
