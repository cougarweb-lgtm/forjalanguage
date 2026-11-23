/* sem_veneno.c - declaracao envenenada e cascata calada (B01b, O6): o erro de uma declaracao nao gera erro nas que a usam.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - IA1-b15: SemNomeJaRelatado (nome sem declaracao relatado uma vez por corpo; os usos seguintes sao cascata calada).
 *            0.1.0 - Linguagem v1.11 L8: SemEnvenenar, SemErroCalado e SemVenenarCampos sobre o gancho TIPO_VENENO (tipos.h) e Simbolo.envenenado.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 *
 * REGRA: declaracao que teve erro fica ENVENENADA. O nome dela passa a valer TIPO_VENENO nos usos (struct, enum, unit, union, const; proc com
 * assinatura invalida). Proc com erro so no corpo continua com a assinatura (os chamadores conferem normalmente). Quem toca o veneno (um tipo,
 * um nome ou uma expressao de tipo TIPO_VENENO) liga Semantica.tocou_veneno; o 1o erro da declaracao em analise, se saiu depois do toque, e
 * CALADO (SemGuardarErro marca Diag.calado e DiagListaGuardar o descarta): a declaracao para ali, sem linha nova. Efeito colateral aceito: um
 * erro independente da mesma declaracao, depois do toque, tambem sai calado (aparece depois que o erro de origem for consertado).
 */
#include "semantic/sem_interno.h"

static SimboloCategoria CategoriaDaDecl(DeclTipo tipo)
{
    switch (tipo) {
    case D_UNIDADE_BASE:
    case D_UNIDADE_DERIV: return SIM_UNIDADE;
    case D_CONST: return SIM_CONST;
    case D_STRUCT: return SIM_STRUCT;
    case D_ENUM: return SIM_ENUM;
    case D_PROC: return SIM_PROC;
    case D_UNIAO: return SIM_UNIAO;
    default: return SIM_NENHUM; /* test, link, import: sem nome global */
    }
}

/* Marca a declaracao como envenenada. Nome ainda nao registrado (o erro veio antes de SemDeclararGlobal) entra na tabela com TIPO_VENENO;
 * nome registrado por outra declaracao (E0202) nao e tocado e o passe B deixa de rodar (pula_passe_b). O simbolo que entra aqui e um reservado
 * de nome: a proxima declaracao do mesmo nome que dar certo o substitui (SemDeclararGlobal), como era antes do veneno. */
void SemEnvenenar(Semantica *s, Decl *d)
{
    SimboloCategoria categoria = CategoriaDaDecl(d->tipo_no);
    Simbolo *y;
    if (categoria == SIM_NENHUM || d->nome < 0) return;
    y = &s->globais[d->chave];
    if (d->chave != d->nome && s->globais[d->nome].categoria == SIM_PROC_EMBUTIDO) { /* IA1-L8: nome reservado pela linguagem num modulo (E0202): o uso cairia no embutido */
        s->pula_passe_b = true;
        return;
    }
    if (y->categoria == SIM_NENHUM) {
        *y = (Simbolo){(uint8_t)categoria, -1, d,TIPO_VENENO, 1};
        return;
    }
    if (y->decl == d) y->envenenado = 1;
    else s->pula_passe_b = true; /* o nome e de outro simbolo (E0202: embutido, outro modulo, outra declaracao): o uso cairia nele, nao no veneno */
}

/* Erro que sera calado: a causa e um simbolo envenenado e o chamador nao tem como seguir (devolve false para a declaracao parar). */
bool SemErroCalado(Semantica *s, uint32_t pos)
{
    s->tocou_veneno = true;
    return SemErro(s, E0201, pos, "declaracao com erro (cascata calada)");
}

bool SemNomeJaRelatado(Semantica *s, int32_t nome)
{
    if (s->proc == NULL || s->tocou_veneno) return false; /* fora de corpo (passe A) cada declaracao relata o seu erro */
    for (size_t k = 0; k < s->nomes_relatados.n; k++) {
        if (*(const int32_t *)VecEm(&s->nomes_relatados, k) == nome) return true;
    }
    *(int32_t *)VecEmpurrar(&s->nomes_relatados) = nome;
    return false;
}

/* Campos (ou variantes) a partir de 'de' que a analise nao chegou a resolver passam a valer TIPO_VENENO: quem alcancar a struct por um tipo
 * resolvido antes dela ser envenenada nao le campo sem tipo. */
void SemVenenarCampos(Decl *d, uint32_t de)
{
    for (uint32_t k = de; k < d->ncampos; k++) {
        if (d->campos[k].tipo.tipo == 0) d->campos[k].tipo.tipo = TIPO_VENENO;
    }
}
