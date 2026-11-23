/* sem_parecido.c - "voce quis dizer" (B02, v1.11 L5): candidatos do nome escrito errado e a causa com a sugestao.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - IA1-b14: cada candidato leva a posicao da declaracao (ParecidosVerEm) e SemErroParecido chama SemRelacionarParecidos e SemRelacionar (relacionados do pacote do erro).
 *            0.1.1 - IA1-b06: SemErroParecido chama SemSugerirParecido (E0201 e E1003 com um parecido unico levam a sugestao aplicavel).
 *            0.1.0- Linguagem v1.11 L5: SemParecidoIniciar, SemParecidosDeNome (locais e globais visiveis, ou pub do modulo), SemParecidosDeCampos, SemParecidosDeMembros, SemParecidosDeParams e SemErroParecido.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py && teste_common.exe
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "semantic/sem_interno.h"

/* pos = onde o candidato foi declarado (PARECIDO_SEM_POS = embutido, sem declaracao no fonte). */
static void VerNome(const Semantica *s, int32_t nome, Parecidos *busca, uint32_t pos)
{
    const char *texto = SemNomeTexto(s, nome); /* IA1-L8: o nome puro (a chave de modulo perde o prefixo), logo o tamanho vem do texto */
    ParecidosVerEm(busca, texto, strlen(texto), pos);
}

void SemParecidoIniciar(const Semantica *s, DiagCodigo codigo, uint32_t pos, int32_t nome, SemParecido *sp)
{
    sp->codigo = codigo;
    sp->pos = pos;
    const char *texto = SemNomeTexto(s, nome);
    ParecidosIniciar(&sp->busca, texto, strlen(texto));
}

static void VerLocais(const Semantica *s, Parecidos *busca)
{
    for (size_t i = 0; i < s->escopo.pilha.n; i++) {
        const LocalInfo *local = VecEm(&s->escopo.pilha, i);
        VerNome(s, local->nome, busca, local->pos);
    }
}

/* Mesmo criterio de SemVisivel (caminho rapido): nome de outro modulo nao e candidato sem apelido. */
static void VerGlobaisVisiveis(const Semantica *s, Parecidos *busca)
{
    for (int32_t nome = 0; nome < (int32_t)s->qtd_nomes; nome++) {
        const Simbolo *y = &s->globais[nome];
        if (y->categoria == SIM_NENHUM) continue;
        if (y->decl != NULL && y->decl->modulo != s->modulo_atual) continue;
        VerNome(s, nome, busca, y->decl != NULL ? y->decl->pos : PARECIDO_SEM_POS);
    }
}

static void VerPubDoModulo(const Semantica *s, uint8_t modulo, Parecidos *busca)
{
    for (int32_t nome = 0; nome < (int32_t)s->qtd_nomes; nome++) {
        const Decl *d = s->globais[nome].decl;
        if (d != NULL && d->modulo == modulo && d->publico) VerNome(s, nome, busca, d->pos);
    }
}

void SemParecidosDeNome(const Semantica *s, uint8_t modulo, SemParecido *sp)
{
    if (modulo != 0) {
        VerPubDoModulo(s, modulo, &sp->busca);
        return;
    }
    VerLocais(s, &sp->busca);
    VerGlobaisVisiveis(s, &sp->busca);
}

void SemParecidosDeCampos(const Semantica *s, const Decl *estrutura, SemParecido *sp)
{
    for (uint32_t i = 0; i < estrutura->ncampos; i++) VerNome(s, estrutura->campos[i].nome, &sp->busca, estrutura->campos[i].pos);
}

void SemParecidosDeMembros(const Semantica *s, const Decl *enumeracao, SemParecido *sp)
{
    for (uint32_t i = 0; i < enumeracao->nmembros; i++) VerNome(s, enumeracao->membros[i].nome, &sp->busca, enumeracao->membros[i].pos);
}

void SemParecidosDeParams(const Semantica *s, const Param *params, uint32_t nparams, SemParecido *sp)
{
    for (uint32_t i = 0; i < nparams; i++) VerNome(s, params[i].nome, &sp->busca, params[i].pos);
}

/* Acrescenta " (parecido: 'a', 'b')" ao fim da causa. */
static void AnexarParecidos(const Parecidos *busca, char *causa, size_t cap)
{
    for (uint32_t i = 0; i < busca->qtd; i++) {
        size_t usado = strlen(causa);
        snprintf(causa + usado, cap - usado, "%s'%.*s'", i == 0 ? " (parecido: " : ", ", (int)busca->nomes[i].tam,
                 busca->nomes[i].texto);
    }
    size_t usado = strlen(causa);
    snprintf(causa + usado, cap - usado, ")");
}

/* Valor JSON do extra "parecidos": ["vida", "vidas"]. Nomes sao identificadores, sem aspas nem barra. */
static void MontarJsonParecidos(const Parecidos *busca, char *json, size_t cap)
{
    snprintf(json, cap, "[");
    for (uint32_t i = 0; i < busca->qtd; i++) {
        size_t usado = strlen(json);
        snprintf(json + usado, cap - usado, "%s\"%.*s\"", i == 0 ? "" : ", ", (int)busca->nomes[i].tam, busca->nomes[i].texto);
    }
    size_t usado = strlen(json);
    snprintf(json + usado, cap - usado, "]");
}

bool SemErroParecido(Semantica *s, const SemParecido *sp, const char *fmt, ...)
{
    char causa[512], json[384];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(causa, sizeof causa, fmt, ap);
    va_end(ap);
    if (sp->busca.qtd == 0) {
        DiagRelatar(s->diag, sp->codigo, sp->pos, "%s", causa);
        SemRelacionar(s);
        return false;
    }
    AnexarParecidos(&sp->busca, causa, sizeof causa);
    MontarJsonParecidos(&sp->busca, json, sizeof json);
    DiagRelatar(s->diag, sp->codigo, sp->pos, "%s", causa);
    DiagAcrescentarExtra(s->diag, "parecidos", json);
    SemSugerirParecido(s, sp);
    SemRelacionarParecidos(s, &sp->busca);
    SemRelacionar(s);
    return false;
}
