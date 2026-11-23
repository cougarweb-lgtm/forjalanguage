/* sem_formato.c - f"..." (partes, tipos e formato de {expr:FMT}) e os embutidos format/format_c (D16..D18 da v1.7).
 * Versao: 0.4.1
 * Changelog: 0.4.1 - Ryu P2: ConferirChave grava FMT_CURTO em todo real sem precisao (f"{x}", format, print); SemFTextoDoArgumento deixa de grava-lo.
 *            0.4.0 - IA1-L9f: SemChamarFormato desvia format(f"...", into = &buf) para SemChamarFormatoInto (sem_texto_builder.c); inclui sem_lista.h.
 *            0.3.0 - IA1-L5a: ConferirChave (a parte de ChaveDoFTexto depois da expressao) e SemFTextoDoArgumento (print de real e eprint viram f"{x}", real com FMT_CURTO).
 *            0.2.0 - Linguagem v1.11 L11: {expr:?} (ChaveDepurar, TipoDepuravel): FMT_DEPURAR em Arg.indice; E0315 para tipo que o ':?' nao mostra e para '?' com largura/precisao.
 *            0.1.1 -Linguagem v1.8 P6a: SemChamarFormato passa a chamada a SemConferirAlocador (alocacao).
 *            0.1.0 - Linguagem v1.7 P5a: SemFTexto, SemChamarFormato, LerFormato e ConferirFormatoDoTipo.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_lista.h"

static bool EhDigito(char c) { return c >= '0' && c <= '9'; }

/* 1 ou mais digitos decimais, valor ate 'maximo'; avanca *p. */
static bool LerDigitos(const char **p, uint32_t maximo, uint32_t *valor)
{
    uint32_t v = 0;
    if (!EhDigito(**p)) return false;
    while (EhDigito(**p)) {
        v = v * 10 + (uint32_t)(**p - '0');
        if (v > maximo) return false;
        (*p)++;
    }
    *valor = v;
    return true;
}

/* ['0'][largura 1..255]: o '0' pede largura logo depois. */
static bool LerLargura(const char **p, uint32_t *fmt)
{
    uint32_t largura = 0;
    if (**p == '0') {
        *fmt |= FMT_ZERO;
        (*p)++;
        if (!LerDigitos(p, 255, &largura) || largura == 0) return false;
    } else if (EhDigito(**p) && !LerDigitos(p, 255, &largura)) {
        return false;
    }
    *fmt |= largura;
    return true;
}

/* ['.'precisao 0..17]: grava precisao + 1 (0 = sem precisao). */
static bool LerPrecisao(const char **p, uint32_t *fmt)
{
    uint32_t precisao;
    if (**p != '.') return true;
    (*p)++;
    if (!EhDigito(**p) || !LerDigitos(p, 17, &precisao)) return false;
    *fmt |= (precisao + 1) << FMT_PRECISAO_DESLOC;
    return true;
}

/* [0][largura 1..255][.precisao 0..17][x]; qualquer sobra ou texto vazio e invalido. */
static bool LerFormato(const char *texto, uint32_t *fmt)
{
    const char *p = texto;
    *fmt = 0;
    if (*p == '\0') return false;
    if (!LerLargura(&p, fmt) || !LerPrecisao(&p, fmt)) return false;
    if (*p == 'x') {
        *fmt |= FMT_HEX;
        p++;
    }
    return *p == '\0';
}

static bool TipoDeFormatoAceito(const Semantica *s, TipoId t)
{
    return TipoEhInteiro(t) || t == TIPO_BOOL || t == TIPO_STR || t == TIPO_CSTR || TipoEhReal(&s->tipos, t);
}

/* Cada letra do formato so vale para a sua familia de tipo. */
static bool ConferirFormatoDoTipo(Semantica *s, uint32_t fmt, TipoId t, uint32_t pos)
{
    if (((fmt >> FMT_PRECISAO_DESLOC) & FMT_PRECISAO_MASCARA) != 0 && !TipoEhReal(&s->tipos, t)) {
        return SemErro(s, E0315, pos, "'.precisao' so vale para real (f32, f64, unidade)");
    }
    if ((fmt & FMT_HEX) != 0 && !TipoEhSemSinal(t)) {
        return SemErro(s, E0315, pos, "'x' so vale para inteiro sem sinal: converta com 'as u64'");
    }
    if ((fmt & FMT_ZERO) != 0 && !TipoEhNumerico(&s->tipos, t)) {
        return SemErro(s, E0315, pos, "'0' so vale para numero");
    }
    return true;
}

static bool TipoDepuravel(Semantica *s, TipoId t, TipoId *culpado);

/* struct da Forja (nao opaca) cujos campos todos o ':?' mostra; *culpado = o tipo que barrou. */
static bool CamposDepuraveis(Semantica *s, TipoId t, TipoId *culpado)
{
    const Decl *estrutura = SemDeclDaStruct(s, t);
    if (estrutura == NULL || estrutura->externo) {
        *culpado = t;
        return false;
    }
    for (uint32_t k = 0; k < estrutura->ncampos; k++) {
        if (!TipoDepuravel(s, estrutura->campos[k].tipo.tipo, culpado)) return false;
    }
    return true;
}

/* v1.11 L11: o que o ':?' mostra: o que o {x} ja mostra, enum, struct e [N]T desses. ?T, union, lista, fatia, map, soa, matrix,
 * bit_set e ponteiro ficam de fora. */
static bool TipoDepuravel(Semantica *s, TipoId t, TipoId *culpado)
{
    TipoClasse classe = TipoClasseDe(&s->tipos, t);
    if (TipoDeFormatoAceito(s, t) || classe == TC_ENUM) return true;
    if (classe == TC_ARRAY) return TipoDepuravel(s, TipoDe(&s->tipos, t)->a, culpado);
    if (classe == TC_STRUCT) return CamposDepuraveis(s, t, culpado);
    *culpado = t;
    return false;
}

/* {expr:?}: o formato '?' vale sozinho (E0315 se vier com largura, precisao, zeros ou hex) e so para tipo depuravel. */
static bool ChaveDepurar(Semantica *s, Arg *a, const char *texto)
{
    TipoId culpado = a->valor->tipo;
    char tipo[96];
    if (strcmp(texto, "?") != 0) {
        return SemErro(s, E0315, a->pos_nome, "':?' nao combina com largura, precisao, zeros nem hex: escreva so {expr:?}");
    }
    if (!TipoDepuravel(s, a->valor->tipo, &culpado)) {
        SemDescrever(s, culpado, tipo, sizeof tipo);
        return SemErro(s, E0315, a->valor->pos, "':?' ainda nao mostra %s", tipo);
    }
    a->indice = FMT_DEPURAR;
    return true;
}

/* {expr[:formato]} com a expressao ja conferida: tipo aceito, formato valido e coerente com o tipo; o formato empacotado vai em Arg.indice. */
static bool ConferirChave(Semantica *s, Arg *a)
{
    uint32_t fmt = 0;
    char tipo[96];
    if (a->nome >= 0 && strchr(SemNomeTexto(s, a->nome), '?') != NULL) return ChaveDepurar(s, a, SemNomeTexto(s, a->nome));
    if (!TipoDeFormatoAceito(s, a->valor->tipo)) {
        SemDescrever(s, a->valor->tipo, tipo, sizeof tipo);
        return SemErro(s, E0315, a->valor->pos, "{...} de tipo %s nao se formata: aceitos inteiros, bool, str, cstr, f32, f64 e unidade", tipo);
    }
    if (a->nome >= 0) {
        const char *texto = SemNomeTexto(s, a->nome);
        if (!LerFormato(texto, &fmt)) {
            return SemErro(s, E0315, a->pos_nome, "formato '%s' invalido: use [0][largura 1..255][.precisao 0..17][x]", texto);
        }
        if (!ConferirFormatoDoTipo(s, fmt, a->valor->tipo, a->pos_nome)) return false;
    }
    if (TipoEhReal(&s->tipos, a->valor->tipo) && ((fmt >> FMT_PRECISAO_DESLOC) & FMT_PRECISAO_MASCARA) == 0) fmt |= FMT_CURTO; /* Ryu P2: real sem precisao = Display curto */
    a->indice = (int32_t)fmt;
    return true;
}

static bool ChaveDoFTexto(Semantica *s, Arg *a)
{
    return SemExpr(s, a->valor, TIPO_VAZIO) && ConferirChave(s, a);
}

/* L-03/L-04: o 1o argumento de print/eprint (ja conferido) vira o f"{x}" de uma parte so; real sai no formato curto que volta igual (FMT_CURTO). */
bool SemFTextoDoArgumento(Semantica *s, Expr *chamada)
{
    Expr *valor = chamada->args[0].valor;
    Expr *texto = ArenaAlocar(s->arena, sizeof *texto);
    Arg *parte = ArenaAlocar(s->arena, sizeof *parte);
    *parte = (Arg){.nome = -1, .pos_nome = valor->pos, .valor = valor, .indice = 0};
    texto->tipo_no = EX_FTEXTO;
    texto->pos = texto->pos_op = valor->pos;
    texto->nargs = 1;
    texto->args = parte;
    texto->tipo = TIPO_STR;
    if (!ConferirChave(s, parte)) return false;
    chamada->args[0].valor = texto;
    return true;
}

bool SemFTexto(Semantica *s, Expr *e)
{
    e->tipo = TIPO_STR;
    for (uint32_t i = 0; i < e->nargs; i++) {
        Arg *a = &e->args[i];
        bool ok = a->indice < 0 ? SemChecar(s, a->valor, TIPO_STR) : ChaveDoFTexto(s, a);
        if (!ok) return false;
    }
    return true;
}

/* format(f"...", em = A) -> str; format_c(f"...", em = A) -> cstr com NUL. */
bool SemChamarFormato(Semantica *s, Expr *e)
{
    if (SemFormatoTemInto(s, e)) return SemChamarFormatoInto(s, e); /* L9f: format(f"...", into = &buf) */
    if (!SemLigarEmbutido(s, e, s->params_format, 2)) return false;
    Expr *texto = e->args[0].valor;
    if (texto->tipo_no != EX_FTEXTO) {
        return SemErro(s, E0315, texto->pos, "format pede f\"...\" como 1o argumento");
    }
    if (!SemFTexto(s, texto) || !SemConferirAlocador(s, e, &e->args[1])) return false;
    e->tipo = e->ref_id == EMB_FORMAT ? TIPO_STR : TIPO_CSTR;
    return true;
}
