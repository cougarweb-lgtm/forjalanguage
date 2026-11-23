/* nativo_debug_json.c - formatador JSONL do --debug-code (<saida>.debug.jsonl): uma linha JSON por registro de DebugPercorrer.
 * Versao: 0.5.6
 * Changelog: 0.5.6 - G0d: REG_PASSES nao gera linha (item so de texto).
 *            0.5.5 - M8 P2: REG_PERFIL nao gera linha (item so de texto).
 *            0.5.4 - M8 H1: REG_INTERFACE nao gera linha (item so de texto).
 *            0.5.3 - M7a F0: REG_OTIM nao gera linha (item so de texto).
 *            0.5.2 - M6 P65b: registro "linhas" (proc do DbgPrograma: linhas, parametros e locais).
 *            0.5.1 - M6 P65a: registro "locais" (funcao + texto, como o bir).
 *            0.5.0 - M5 P27b: "formato" do ELF ("ELF64", machine 0x3e, sem linker_*) e o registro "segmento" (esquema 0.7.0).
 *            0.4.0 - M4 P24p: registro "poda" {procs_total, procs_gerados, procs_podados} (esquema 0.6.0).
 *            0.3.0 - M4 P24a: o registro "tempos" sai da tabela NATIVO_TEMPOS_CAMPOS (13 campos, esquema 0.5.0).
 *            0.2.2 - M3 P22a: registro "export" (esquema 0.4.0).
 *            0.2.1 - M3 P19a: "import" ganha descritor (esquema 0.3.0).
 *            0.2.0 - M2 P11a: "funcao" ganha slots_agregado (esquema 0.2.0).
 *            0.1.0 - backend x64 P8b: cabecalho, funcao, instr, bir, reloc, secao, import, formato, faixa e tempos (esquema 0.1.0).
 * Verificacao: python -I ..\tests\cli\verificar_cli.py --forja .\forja.exe  (valida cada linha em tests\nativo\esquema_debug.json)
 *
 * Determinismo: sem hora, sem caminho de saida, sem endereco de maquina; so o registro "tempos" muda de uma execucao para outra.
 */
#include <stdio.h>
#include <string.h>

#include "nativo/nativo_debug.h"
#include "nativo/nativo_json.h"

static void Chave(Buf *b, const char *nome)
{
    BufFormatar(b, ", \"%s\": ", nome);
}

static void CampoTexto(Buf *b, const char *nome, const char *valor)
{
    Chave(b, nome);
    JsonTexto(b, valor, strlen(valor));
}

static void CampoNumero(Buf *b, const char *nome, uint64_t valor)
{
    BufFormatar(b, ", \"%s\": %llu", nome, (unsigned long long)valor);
}

static void CampoHex(Buf *b, const char *nome, const uint8_t *bytes, uint32_t tam)
{
    Chave(b, nome);
    BufAnexarN(b, "\"", 1);
    for (uint32_t k = 0; k < tam; k++)
        BufFormatar(b, "%02x", (unsigned)bytes[k]);
    BufAnexarN(b, "\"", 1);
}

/* Abre a linha: {"tipo": "<tipo>" */
static void Abrir(Buf *b, const char *tipo)
{
    BufFormatar(b, "{\"tipo\": \"%s\"", tipo);
}

static void Fechar(Buf *b)
{
    BufAnexar(b, "}\n");
}

static void CampoPosicao(Buf *b, const char *arquivo, uint32_t linha, uint32_t coluna)
{
    if (arquivo == NULL)
        return;
    Chave(b, "arquivo");
    JsonCaminho(b, arquivo);
    CampoNumero(b, "linha", linha);
    if (coluna != 0)
        CampoNumero(b, "coluna", coluna);
}

/* "slots_agregado": [{"slot": k, "tamanho": T, "alinhamento": A, "offset": O}, ...]; offset = bytes abaixo do rbp (endereco = rbp - offset). */
static void CampoSlotsAgregado(Buf *b, const ListaFuncao *q)
{
    Chave(b, "slots_agregado");
    BufAnexarN(b, "[", 1);
    for (uint32_t k = 0; k < q->nslots_agregado; k++) {
        const ListaSlot *s = &q->slots_agregado[k];

        BufFormatar(b, "%s{\"slot\": %u, \"tamanho\": %u, \"alinhamento\": %u, \"offset\": %u}", k == 0 ? "" : ", ", (unsigned)k, (unsigned)s->tam,
                    (unsigned)s->alinhamento, (unsigned)s->offset);
    }
    BufAnexarN(b, "]", 1);
}

static void JsonFuncao(Buf *b, const RegFuncao *f)
{
    const ListaFuncao *q = f->quadro;

    Abrir(b, "funcao");
    CampoTexto(b, "nome", f->nome);
    CampoNumero(b, "rva", f->rva);
    CampoNumero(b, "tamanho", f->tamanho);
    CampoNumero(b, "quadro", q->quadro);
    BufFormatar(b, ", \"sondagem\": %s", q->sondagem ? "true" : "false");
    CampoNumero(b, "slots", q->slots);
    CampoNumero(b, "maxargs", q->maxargs);
    CampoNumero(b, "push", q->push);
    CampoNumero(b, "fpreg", q->fpreg);
    CampoNumero(b, "alocacao", q->alocacao);
    CampoSlotsAgregado(b, q);
    CampoHex(b, "unwind", q->unwind, q->tam_unwind);
    CampoPosicao(b, f->arquivo, f->linha, 0);
    Fechar(b);
}

/* "b2.3" = 4o item do bloco b2 no dump da BIR da funcao (item bir); vazio no prologo e no epilogo. */
static void CampoBir(Buf *b, const RegInstr *i)
{
    Chave(b, "bir");
    if (i->bloco == LISTA_SEM_INDICE)
        BufAnexar(b, "\"\"");
    else
        BufFormatar(b, "\"b%u.%u\"", (unsigned)i->bloco, (unsigned)i->indice);
}

static void JsonInstr(Buf *b, const RegInstr *i)
{
    Abrir(b, "instr");
    CampoTexto(b, "funcao", i->funcao);
    CampoNumero(b, "rva", i->rva);
    CampoHex(b, "bytes", i->bytes, i->tam);
    CampoTexto(b, "asm", i->texto_asm);
    CampoBir(b, i);
    CampoPosicao(b, i->arquivo, i->linha, i->coluna);
    CampoTexto(b, "origem", DebugNomeOrigem(i->origem));
    if (i->simbolo != NULL)
        CampoTexto(b, "simbolo", i->simbolo);
    Fechar(b);
}

static void JsonBir(Buf *b, const RegBir *r, const char *tipo)
{
    Abrir(b, tipo);
    CampoTexto(b, "funcao", r->funcao);
    Chave(b, "texto");
    JsonTexto(b, r->texto, r->tam);
    Fechar(b);
}

static void JsonReloc(Buf *b, const RegReloc *r)
{
    Abrir(b, "reloc");
    CampoTexto(b, "secao", r->secao);
    CampoNumero(b, "offset", r->offset);
    CampoNumero(b, "rva", r->rva);
    CampoTexto(b, "kind", r->kind);
    CampoTexto(b, "simbolo", r->simbolo);
    BufFormatar(b, ", \"adendo\": %lld", (long long)r->adendo);
    Fechar(b);
}

static void JsonSecao(Buf *b, const RegSecao *s)
{
    Abrir(b, "secao");
    CampoTexto(b, "nome", s->nome);
    CampoNumero(b, "rva", s->rva);
    CampoNumero(b, "tamanho_virtual", s->tam_virtual);
    CampoNumero(b, "tamanho_bruto", s->tam_bruto);
    CampoNumero(b, "caracteristicas", s->caracteristicas);
    Fechar(b);
}

static void JsonFormato(Buf *b, const RegFormato *f)
{
    Abrir(b, "formato");
    CampoTexto(b, "formato", f->elf ? "ELF64" : "PE32+");
    CampoTexto(b, "machine", f->elf ? "0x3e" : "0x8664");
    CampoNumero(b, "imagem_base", f->imagem_base);
    CampoNumero(b, "entrada_rva", f->entrada_rva);
    CampoNumero(b, "tamanho_arquivo", f->tam_arquivo);
    if (!f->elf) {
        CampoNumero(b, "linker_maior", f->linker_maior);
        CampoNumero(b, "linker_menor", f->linker_menor);
    }
    Fechar(b);
}

static void JsonSegmento(Buf *b, const RegSegmento *s)
{
    Abrir(b, "segmento");
    CampoTexto(b, "tipo_ph", s->tipo);
    CampoTexto(b, "flags", s->flags);
    CampoNumero(b, "offset", s->offset);
    CampoNumero(b, "vaddr", s->vaddr);
    CampoNumero(b, "filesz", s->filesz);
    CampoNumero(b, "memsz", s->memsz);
    CampoNumero(b, "alinhamento", s->alinhamento);
    Fechar(b);
}

static void JsonTempos(Buf *b, const NativoTempos *t)
{
    Abrir(b, "tempos");
    NativoTemposAnexarJson(b, t);
    Fechar(b);
}

static void JsonCabecalho(Buf *b, const RegCabecalho *c)
{
    Abrir(b, "cabecalho");
    CampoTexto(b, "debug_versao", DEBUG_VERSAO);
    CampoTexto(b, "forja_versao", c->forja);
    CampoTexto(b, "alvo", c->alvo);
    CampoTexto(b, "modo", c->modo);
    Fechar(b);
}

static void JsonImport(Buf *b, const RegImport *i)
{
    Abrir(b, "import");
    CampoTexto(b, "dll", i->dll);
    CampoTexto(b, "nome", i->nome);
    CampoNumero(b, "iat_rva", i->iat_rva);
    CampoNumero(b, "descritor", i->descritor);
    Fechar(b);
}

static void JsonExport(Buf *b, const RegExport *x)
{
    Abrir(b, "export");
    CampoTexto(b, "nome", x->nome);
    CampoNumero(b, "rva", x->rva);
    CampoNumero(b, "ordinal", x->ordinal);
    Fechar(b);
}

static void JsonPoda(Buf *b, const RegPoda *r)
{
    Abrir(b, "poda");
    CampoNumero(b, "procs_total", r->procs_total);
    CampoNumero(b, "procs_gerados", r->procs_gerados);
    CampoNumero(b, "procs_podados", r->procs_total - r->procs_gerados);
    Fechar(b);
}

/* Arquivo, linha e coluna de pos (0 = sem posicao): "arquivo", "linha" e "coluna" dentro do objeto aberto. */
static void CampoPosDoFonte(Buf *b, const Fonte *fonte, uint32_t pos)
{
    FontePosicao p;

    if (pos == BIR_SEM_POS || fonte == NULL)
        return;
    p = FonteLocalizarNoArquivo(fonte, pos);
    CampoPosicao(b, p.caminho, p.linha, p.coluna);
}

/* "linhas": [{"off": O, "arquivo": A, "linha": L, "coluna": C}, ...] do proc, na ordem de offset. */
static void CampoLinhasDoProc(Buf *b, const RegLinhas *r)
{
    Chave(b, "linhas");
    BufAnexarN(b, "[", 1);
    for (uint32_t k = 0; k < r->proc->nlinhas; k++) {
        const DbgLinha *l = VecEm(&r->dbg->linhas, r->proc->linha_ini + k);

        BufFormatar(b, "%s{\"off\": %u", k == 0 ? "" : ", ", (unsigned)l->off);
        CampoPosDoFonte(b, r->fonte, l->pos);
        BufAnexarN(b, "}", 1);
    }
    BufAnexarN(b, "]", 1);
}

/* Parametros (parametro = true) ou locais (false) do proc: [{"id", "nome", "tipo", "tid", "rbp", "indireto"}, ...]; rbp = deslocamento com sinal em relacao ao rbp. */
static void CampoVariaveis(Buf *b, const RegLinhas *r, const char *chave, bool parametros)
{
    uint32_t n = 0;

    Chave(b, chave);
    BufAnexarN(b, "[", 1);
    for (uint32_t k = 0; k < r->proc->nvars; k++) {
        const DbgVar *v = VecEm(&r->dbg->vars, r->proc->var_ini + k);

        if (v->parametro != parametros)
            continue;
        BufFormatar(b, "%s{\"id\": %u", n++ == 0 ? "" : ", ", (unsigned)v->local_id);
        CampoTexto(b, "nome", v->nome);
        CampoTexto(b, "tipo", v->tipo_nome);
        CampoNumero(b, "tid", v->tipo);
        BufFormatar(b, ", \"rbp\": %d, \"indireto\": %s}", (int)v->loc.rbp_off, v->loc.indireto ? "true" : "false");
    }
    BufAnexarN(b, "]", 1);
}

static void JsonLinhas(Buf *b, const RegLinhas *r)
{
    Abrir(b, "linhas");
    CampoTexto(b, "funcao", r->proc->nome);
    CampoTexto(b, "simbolo", r->proc->simbolo);
    CampoPosDoFonte(b, r->fonte, r->proc->pos);
    CampoNumero(b, "inicio", r->proc->inicio);
    CampoNumero(b, "fim", r->proc->fim);
    CampoNumero(b, "quadro", r->proc->rbp_quadro);
    CampoLinhasDoProc(b, r);
    CampoVariaveis(b, r, "parametros", true);
    CampoVariaveis(b, r, "locais", false);
    Fechar(b);
}

static void JsonFaixa(Buf *b, const RegFaixa *f)
{
    Abrir(b, "faixa");
    CampoNumero(b, "rva", f->rva);
    CampoNumero(b, "tamanho", f->tamanho);
    CampoTexto(b, "origem", f->origem);
    Fechar(b);
}

static void EscreverJson(void *estado, const DebugReg *reg)
{
    Buf *b = estado;

    switch (reg->tipo) {
    case REG_CABECALHO: JsonCabecalho(b, &reg->cabecalho); break;
    case REG_FUNCAO: JsonFuncao(b, &reg->funcao); break;
    case REG_INSTR: JsonInstr(b, &reg->instr); break;
    case REG_BIR: JsonBir(b, &reg->bir, "bir"); break;
    case REG_LOCAIS: JsonBir(b, &reg->bir, "locais"); break;
    case REG_LINHAS: JsonLinhas(b, &reg->linhas); break;
    case REG_OTIM: break; /* o item otim so existe no texto */
    case REG_INTERFACE: break; /* o item interface so existe no texto */
    case REG_PERFIL: break; /* o item perfil so existe no texto */
    case REG_PASSES: break; /* o item passes so existe no texto */
    case REG_RELOC: JsonReloc(b, &reg->reloc); break;
    case REG_SECAO: JsonSecao(b, &reg->secao); break;
    case REG_IMPORT: JsonImport(b, &reg->import); break;
    case REG_FORMATO: JsonFormato(b, &reg->formato); break;
    case REG_FAIXA: JsonFaixa(b, &reg->faixa); break;
    case REG_EXPORT: JsonExport(b, &reg->exporta); break;
    case REG_PODA: JsonPoda(b, &reg->poda); break;
    case REG_SEGMENTO: JsonSegmento(b, &reg->segmento); break;
    default: JsonTempos(b, reg->tempos.tempos); break;
    }
}

void DebugMontarJson(const DebugEntrada *e, Buf *saida)
{
    DebugPercorrer(e, EscreverJson, saida);
}
