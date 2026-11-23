/* sem_texto_builder.c - builder de texto (IA1-L9f, T11): gerar texto em laco sem alocar a cada pedaco. O builder e uma lista [dynamic]u8 no heap; nao existe tipo novo.
 * Sintaxe:
 *   append(&buf, item = "texto") -> acrescenta os BYTES do str (len(s)) ao builder. Hoje append(&l, item = x) acrescenta UM elemento; aqui o tipo do item (str, nao u8) desambigua
 *                                   e o lowering e o do append(&buf, items = "texto") (IR_ANEXA_FATIA). Item str em lista que nao e de u8 continua E0301 (tipo diferente).
 *   to_str(&buf) -> str              MOVE os bytes do builder para uma str dona, sem copia, com NUL depois do ultimo byte; buf fica vazio (len 0, cap 0) e reutilizavel.
 *                                   O bloco fica no alocador do builder (heap, frame ou Arena) e a str herda essa origem: o delete so vale no heap; no frame e na Arena a memoria sai com frame_reset ou arena_destroy.
 *                                   O resultado precisa ser guardado (descarte = E0310) e liberado com delete(s) quando for de heap.
 *   format(f"...", into = &buf)      formata como o format e ANEXA ao builder em vez de alocar uma str: e instrucao (sem valor). into e alloc juntos = E0501; format_c nao tem into.
 * &buf exige buf var (E0705 se let) e ^[dynamic]u8 (E0310 sem o &, E0301 se o elemento nao e u8).
 * Versao: 0.2.1
 * Changelog: 0.2.1 - B01 Assess: to_str de builder com alloc = scope = E0316 (o scope nao libera o bloco movido: vazava R0009 em dev e calado em release).
 *            0.2.0 - to_str aceita builder de qualquer alocador: SemChamarToStr grava em e->alocacao a origem do builder local (ALOC_PARAM, ALOC_ARENA, ALOC_FRAME); heap e builder indireto seguem ALOC_NENHUM.
 *            0.1.0 - IA1-L9f: primeira versao.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_lista.h"

#include <stdio.h>
#include <string.h>

/* O endereco do builder (^[dynamic]u8) de um argumento ja checado: '&x' pede x var; 'quem' diz o embutido e 'exemplo' a forma certa, para a mensagem. */
static bool BuilderPorEndereco(Semantica *s, Expr *lista, const char *quem, const char *exemplo)
{
    char texto[96], motivo[96];
    const TipoInfo *k = TipoDe(&s->tipos, lista->tipo);
    if (TipoClasseDe(&s->tipos, lista->tipo) == TC_DINAMICO) {
        return SemErro(s, E0310, lista->pos, "%s recebe o endereco do builder: %s", quem, exemplo);
    }
    if (k->classe != TC_PONTEIRO || TipoClasseDe(&s->tipos, (TipoId)k->a) != TC_DINAMICO) {
        SemDescrever(s, lista->tipo, texto, sizeof texto);
        return SemErro(s, E0310, lista->pos, "%s pede ^[dynamic]u8, o endereco do builder (tipo %s)", quem, texto);
    }
    TipoId elemento = TipoDe(&s->tipos, (TipoId)k->a)->a;
    if (elemento != TIPO_U8) {
        SemDescrever(s, elemento, texto, sizeof texto);
        return SemErro(s, E0301, lista->pos, "%s pede lista de u8, o builder de texto (elemento %s)", quem, texto);
    }
    snprintf(motivo, sizeof motivo, "%s altera o builder: declare com var", quem);
    return lista->tipo_no != EX_UNARIO || lista->op != OP_ENDERECO || SemLvalorMutavelDe(s, lista->a, motivo);
}

/* Origem que a str herda do builder: so '&x' de um local com alocador de alloc, Arena, frame ou scope. Heap e builder indireto (campo, ^[dynamic]u8 de parametro)
 * ficam sem origem: a str nao entra na posse (E0317) como antes. ALOC_ESCOPO e devolvido para o chamador recusar (E0316): o to_str move o bloco para a str
 * e o defer do scope, que libera o builder no fim do bloco, ja nao o encontra (vazamento). */
static uint8_t OrigemDoBuilderLocal(Semantica *s, const Expr *buf)
{
    if (buf->tipo_no != EX_UNARIO || buf->op != OP_ENDERECO || buf->a->tipo_no != EX_NOME || buf->a->modulo != 0) return ALOC_NENHUM;
    const LocalInfo *local = EscopoBuscar(&s->escopo, buf->a->nome);
    if (local == NULL) return ALOC_NENHUM;
    if (local->origem == ALOC_PARAM || local->origem == ALOC_ARENA || local->origem == ALOC_FRAME || local->origem == ALOC_ESCOPO) return local->origem;
    return ALOC_NENHUM;
}

/* to_str(&buf) -> str; a str herda a origem do builder local (OrigemDoBuilderLocal). */
bool SemChamarToStr(Semantica *s, Expr *e)
{
    if (!SemLigarEmbutido(s, e, &s->params_lista[ASS_LISTA], 1)) return false;
    Expr *buf = ArgumentoDoParametro(e, 0)->valor;
    if (!SemChecar(s, buf, TIPO_VAZIO) || !BuilderPorEndereco(s, buf, "to_str", "to_str(&buf)")) return false;
    uint8_t origem = OrigemDoBuilderLocal(s, buf);
    if (origem == ALOC_ESCOPO) {
        return SemErro(s, E0316, buf->pos, "to_str move o bloco do builder para a str e o alloc = scope o libera no fim do bloco: faca o builder com alloc = heap (e delete(s)), frame ou uma Arena");
    }
    e->tipo = TIPO_STR;
    e->alocacao = origem;
    return true;
}

/* format com um argumento chamado into? (o rotulo nao esta em params_format, entao a ligacao normal nao o conhece) */
bool SemFormatoTemInto(Semantica *s, const Expr *e)
{
    for (uint32_t k = 0; k < e->nargs; k++) {
        if (e->args[k].nome >= 0 && strcmp(SemNomeTexto(s, e->args[k].nome), "into") == 0) return true;
    }
    return false;
}

/* format(f"...", into = &buf): so o texto (posicional) e o into, nessa ordem. */
bool SemChamarFormatoInto(Semantica *s, Expr *e)
{
    if (e->ref_id != EMB_FORMAT) return SemErro(s, E0315, e->pos_op, "into so existe no format: format_c devolve um cstr novo");
    if (e->nargs != 2 || e->args[0].nome >= 0 || e->args[1].nome < 0 || strcmp(SemNomeTexto(s, e->args[1].nome), "into") != 0) {
        return SemErro(s, E0501, e->pos_op, "format(f\"...\", into = &buf) leva so o texto e o into, nessa ordem: into e alloc nao andam juntos");
    }
    Expr *texto = e->args[0].valor, *buf = e->args[1].valor;
    if (texto->tipo_no != EX_FTEXTO) return SemErro(s, E0315, texto->pos, "format pede f\"...\" como 1o argumento");
    e->args[0].indice = 0;
    e->args[1].indice = 1;
    if (!SemFTexto(s, texto) || !SemChecar(s, buf, TIPO_VAZIO) || !BuilderPorEndereco(s, buf, "format(into =)", "format(f\"...\", into = &buf)")) return false;
    e->tipo = TIPO_VAZIO;
    return true;
}

/* append(&buf, item = <str>) com o item ja checado e a lista de u8: so lista dinamica; o lowering acrescenta os bytes como no items. */
bool SemTextoNoBuilder(Semantica *s, const Expr *lista, Arg *item)
{
    if (TipoClasseDe(&s->tipos, (TipoId)TipoDe(&s->tipos, lista->tipo)->a) != TC_DINAMICO) {
        return SemErro(s, E0310, item->valor->pos, "item de texto so vale em lista dinamica ([dynamic]u8): em lista fixa, um append por byte");
    }
    item->indice = APPEND_PARAM_ITEMS;
    return true;
}
