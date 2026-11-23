/* nativo_debug_texto.c - formatador de texto do --debug-code (<saida>.debug.txt): le os registros de DebugPercorrer.
 * Versao: 0.2.8
 * Changelog: 0.2.8 - G0d: secao passes (a BIR depois de cada passe, depois de perfil).
 *            0.2.7 - M8 P2: secao perfil (as procs instrumentadas, depois de interface).
 *            0.2.6 - M8 H1: secao interface (texto canonico da interface da DLL, depois de otim).
 *            0.2.5 - M7a F0: secao otim.
 *            0.2.4 - M6 P65b: secao linhas (proc, par offset -> linha, parametros e locais do DbgPrograma).
 *            0.2.3 - M6 P65a: secao locais (depois de tempos).
 *            0.2.2 - M5 P27b: ELF: a linha exe diz ELF64 (sem linker), uma linha "segmento" por cabecalho de programa e a nota do .reloc some.
 *            0.2.1 - M4 P24p: o registro "poda" so existe no JSONL (o texto o ignora).
 *            0.2.0 - M4 P24a: a secao tempos sai da tabela NATIVO_TEMPOS_CAMPOS (nome=valor dos 13 campos).
 *            0.1.1 - portao M1: a linha de tempos diz que regs e codificar nao sao medidos (isel cobre selecao+alocacao+codificacao).
 *            0.1.0 - backend x64 P8b: texto do P8 (nativo_debug.c 0.1.0) passado para os registros. O conteudo e o mesmo, salvo os bytes de salto e de
 *                    call, que agora saem resolvidos (os do exe ligado) em vez de zerados.
 * Verificacao: construir.bat && python -I ..\tests\nativo\unwind\conferir_unwind.py --forja .\forja.exe
 *
 * Formato (lido por tests\nativo\unwind\conferir_unwind.py; mudou aqui, mude la):
 *   funcao <nome> texto+0x<inicio>                         (cabecalho do asm de uma funcao)
 *     +<offset na funcao, 4 hex>  <bytes em hex>  <asm>[  ; @<simbolo>][  ; <linha>:<coluna>]
 *   funcao <nome> texto+0x<inicio> tam=0x<tam> quadro=<N> slots=<n> maxargs=<n> sondagem=<sim|nao> push=<n> fpreg=<n> alocacao=<n> unwind=<hex>
 */
#include <stdio.h>
#include <string.h>

#include "nativo/nativo_debug.h"

#define TEXTO_LARGURA_BYTES 44 /* 15 bytes em hex com espaco entre eles: 15 * 3 - 1 */
#define TEXTO_TAM_ENTRADA_PDATA 12u
#define TEXTO_TAM_MAX_INSTR 15u

typedef enum { SEC_BIR, SEC_ASM, SEC_QUADROS, SEC_RELOC, SEC_FORMATO, SEC_TEMPOS, SEC_LOCAIS, SEC_LINHAS, SEC_OTIM, SEC_INTERFACE, SEC_PERFIL, SEC_PASSES, SEC_QTD } SecaoTexto;

static const char *const NOMES_SECAO[SEC_QTD] = {"bir", "asm", "quadros", "reloc", "formato", "tempos", "locais", "linhas", "otim", "interface", "perfil", "passes"};
static const uint32_t BITS_SECAO[SEC_QTD] = {DEBUG_BIR, DEBUG_ASM, DEBUG_QUADROS, DEBUG_RELOC, DEBUG_FORMATO, DEBUG_TEMPOS, DEBUG_LOCAIS, DEBUG_LINHAS, DEBUG_OTIM, DEBUG_INTERFACE, DEBUG_PERFIL, DEBUG_PASSES};

typedef struct {
    uint32_t mascara;
    Buf secoes[SEC_QTD];
    bool tem_pdata;
    bool elf;    /* o formato do exe e ELF: sem a nota do .reloc */
    uint32_t pdata_rva, pdata_tam;
} TextoEstado;

static void BytesEmHex(const uint8_t *bytes, uint32_t tam, char *hex)
{
    hex[0] = '\0';
    for (uint32_t k = 0; k < tam; k++)
        snprintf(hex + k * 3, 4, k + 1 < tam ? "%02x " : "%02x", (unsigned)bytes[k]);
}

static void TextoFuncao(TextoEstado *t, const RegFuncao *f)
{
    const ListaFuncao *q = f->quadro;

    if (t->mascara & DEBUG_ASM)
        BufFormatar(&t->secoes[SEC_ASM], "funcao %s texto+0x%x\n", f->nome, (unsigned)f->inicio);
    if ((t->mascara & DEBUG_QUADROS) == 0)
        return;
    BufFormatar(&t->secoes[SEC_QUADROS], "funcao %s texto+0x%x tam=0x%x quadro=%u slots=%u maxargs=%u sondagem=%s push=%u fpreg=%u alocacao=%u unwind=",
                f->nome, (unsigned)f->inicio, (unsigned)f->tamanho, (unsigned)q->quadro, (unsigned)q->slots, (unsigned)q->maxargs,
                q->sondagem ? "sim" : "nao", (unsigned)q->push, (unsigned)q->fpreg, (unsigned)q->alocacao);
    for (uint32_t k = 0; k < q->tam_unwind; k++)
        BufFormatar(&t->secoes[SEC_QUADROS], "%02x", (unsigned)q->unwind[k]);
    BufAnexar(&t->secoes[SEC_QUADROS], "\n");
}

static void TextoInstr(TextoEstado *t, const RegInstr *i)
{
    Buf *saida = &t->secoes[SEC_ASM];
    char hex[TEXTO_TAM_MAX_INSTR * 3 + 1];

    BytesEmHex(i->bytes, i->tam, hex);
    BufFormatar(saida, "  +%04x  %-*s  %s", (unsigned)i->deslocamento, TEXTO_LARGURA_BYTES, hex, i->texto_asm);
    if (i->simbolo != NULL)
        BufFormatar(saida, "  ; @%s", i->simbolo);
    if (i->linha != 0)
        BufFormatar(saida, "  ; %u:%u", (unsigned)i->linha, (unsigned)i->coluna);
    BufAnexar(saida, "\n");
}

static void TextoReloc(TextoEstado *t, const RegReloc *r)
{
    BufFormatar(&t->secoes[SEC_RELOC], "  %s+0x%04x  %-8s %s%+lld\n", r->secao, (unsigned)r->offset, r->kind, r->simbolo, (long long)r->adendo);
}

static void TextoFormato(TextoEstado *t, const RegFormato *f)
{
    Buf *saida = &t->secoes[SEC_FORMATO];

    t->elf = f->elf;
    if (f->elf)
        BufFormatar(saida, "exe: %zu bytes | gerado por Forja Native %s | ELF64 x86-64 ET_EXEC estatico\n", f->tam_arquivo, f->forja);
    else
        BufFormatar(saida, "exe: %zu bytes | gerado por Forja Native %s | linker %u.%u\n", f->tam_arquivo, f->forja, (unsigned)f->linker_maior,
                    (unsigned)f->linker_menor);
    BufFormatar(saida, "image_base: 0x%llx | entrada: rva=0x%x\n", (unsigned long long)f->imagem_base, (unsigned)f->entrada_rva);
}

static void TextoSegmento(TextoEstado *t, const RegSegmento *s)
{
    BufFormatar(&t->secoes[SEC_FORMATO], "segmento %-9s %-2s offset=0x%llx vaddr=0x%llx filesz=0x%llx memsz=0x%llx align=0x%llx\n", s->tipo, s->flags,
                (unsigned long long)s->offset, (unsigned long long)s->vaddr, (unsigned long long)s->filesz, (unsigned long long)s->memsz,
                (unsigned long long)s->alinhamento);
}

static void TextoSecao(TextoEstado *t, const RegSecao *s)
{
    BufFormatar(&t->secoes[SEC_FORMATO], "secao %-8.8s rva=0x%x virt=0x%x arq=0x%x carac=0x%08x\n", s->nome, (unsigned)s->rva, (unsigned)s->tam_virtual,
                (unsigned)s->tam_bruto, (unsigned)s->caracteristicas);
    if (strcmp(s->nome, ".pdata") == 0) {
        t->tem_pdata = true;
        t->pdata_rva = s->rva;
        t->pdata_tam = s->tam_virtual;
    }
}

static void TextoTempos(TextoEstado *t, const NativoTempos *n)
{
    Buf *saida = &t->secoes[SEC_TEMPOS];

    for (size_t k = 0; k < NATIVO_TEMPOS_QTD; k++)
        BufFormatar(saida, "%s%s=%llu", k == 0 ? "" : " ", NATIVO_TEMPOS_CAMPOS[k].nome, (unsigned long long)NativoTemposValor(n, k));
    BufAnexar(saida, " (ns; linhas = do programa inteiro; outros = total - soma das etapas)\n");
}

/* Secao linhas: "proc <nome> [inicio,fim)", uma linha por par (off -> linha do fonte) e uma por variavel (param|local nome tipo rbp+-off [indireto]). */
static void TextoLinhas(TextoEstado *t, const RegLinhas *r)
{
    Buf *saida = &t->secoes[SEC_LINHAS];

    BufFormatar(saida, "proc %s [0x%x,0x%x)\n", r->proc->nome, (unsigned)r->proc->inicio, (unsigned)r->proc->fim);
    for (uint32_t k = 0; k < r->proc->nlinhas; k++) {
        const DbgLinha *l = VecEm(&r->dbg->linhas, r->proc->linha_ini + k);
        FontePosicao p = FonteLocalizarNoArquivo(r->fonte, l->pos);

        BufFormatar(saida, "  linha +0x%x %s:%u:%u\n", (unsigned)(l->off - r->proc->inicio), p.caminho, (unsigned)p.linha, (unsigned)p.coluna);
    }
    for (uint32_t k = 0; k < r->proc->nvars; k++) {
        const DbgVar *v = VecEm(&r->dbg->vars, r->proc->var_ini + k);

        BufFormatar(saida, "  %s %s tipo=%s rbp%+d%s\n", v->parametro ? "param" : "local", v->nome, v->tipo_nome, (int)v->loc.rbp_off,
                    v->loc.indireto ? " indireto" : "");
    }
}

static void EscreverTexto(void *estado, const DebugReg *reg)
{
    TextoEstado *t = estado;

    switch (reg->tipo) {
    case REG_FUNCAO: TextoFuncao(t, &reg->funcao); break;
    case REG_INSTR: TextoInstr(t, &reg->instr); break;
    case REG_BIR: BufAnexarN(&t->secoes[SEC_BIR], reg->bir.texto, reg->bir.tam); break;
    case REG_LOCAIS: BufAnexarN(&t->secoes[SEC_LOCAIS], reg->bir.texto, reg->bir.tam); break;
    case REG_LINHAS: TextoLinhas(t, &reg->linhas); break;
    case REG_OTIM: BufAnexarN(&t->secoes[SEC_OTIM], reg->otim.texto, reg->otim.tam); break;
    case REG_INTERFACE: BufAnexarN(&t->secoes[SEC_INTERFACE], reg->texto_interface.texto, reg->texto_interface.tam); break;
    case REG_PERFIL: BufAnexarN(&t->secoes[SEC_PERFIL], reg->texto_perfil.texto, reg->texto_perfil.tam); break;
    case REG_PASSES: BufAnexarN(&t->secoes[SEC_PASSES], reg->texto_passes.texto, reg->texto_passes.tam); break;
    case REG_RELOC: TextoReloc(t, &reg->reloc); break;
    case REG_FORMATO: TextoFormato(t, &reg->formato); break;
    case REG_SECAO: TextoSecao(t, &reg->secao); break;
    case REG_SEGMENTO: TextoSegmento(t, &reg->segmento); break;
    case REG_TEMPOS: TextoTempos(t, reg->tempos.tempos); break;
    default: break; /* cabecalho, poda, import, export e faixa so existem no JSONL */
    }
}

/* O que fecha o item formato: o diretorio de excecoes (.pdata) e a nota do .reloc. */
static void TextoFecharFormato(TextoEstado *t)
{
    Buf *saida = &t->secoes[SEC_FORMATO];

    if (t->tem_pdata)
        BufFormatar(saida, "excecoes: rva=0x%x tam=0x%x entradas=%u (programa e runtime)\n", (unsigned)t->pdata_rva, (unsigned)t->pdata_tam,
                    (unsigned)(t->pdata_tam / TEXTO_TAM_ENTRADA_PDATA));
    if (!t->elf)
        BufAnexar(saida, "reloc: tabela DIR64 gerada na escrita do PE\n");
}

void DebugMontar(const DebugEntrada *e, Buf *saida)
{
    TextoEstado t = {.mascara = e->mascara};

    DebugPercorrer(e, EscreverTexto, &t);
    if (e->mascara & DEBUG_FORMATO)
        TextoFecharFormato(&t);
    BufFormatar(saida, "# Forja Native %s --debug-code\n", e->versao);
    for (int k = 0; k < SEC_QTD; k++) {
        if ((e->mascara & BITS_SECAO[k]) == 0)
            continue;
        BufFormatar(saida, "## %s\n", NOMES_SECAO[k]);
        if (t.secoes[k].dados != NULL)
            BufAnexarN(saida, t.secoes[k].dados, t.secoes[k].tam);
    }
    for (int k = 0; k < SEC_QTD; k++)
        BufLiberar(&t.secoes[k]);
}
