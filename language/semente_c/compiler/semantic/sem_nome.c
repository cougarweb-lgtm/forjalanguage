/* sem_nome.c - regra de caixa dos nomes (ADR-33), nomes globais, locais e posicoes de nomes que o parser nao guarda.
 * Versao: 0.3.10
 * Changelog: 0.3.10 - Linguagem v1.13 L1a: E0202 de nome repetido em outro arquivo do mesmo pacote diz "ja declarado em <arquivo>:<linha> (mesmo pacote)"; ConflitaComApelido olha os apelidos do pacote inteiro (ModulosApelidoNoPacote).
 *            0.3.9 - M6 P65a: SemGravarNomeLocal; SemDeclararLocal grava {nome, tipo, pos} na tabela por local_id.
 *            0.3.8 - B01a: SemVisivelLento deixa o Allocator (struct oculta) visivel em todo modulo; SemDeclararGlobal reserva o nome Allocator (E0202).
 *            0.3.7 - IA1-b06: E0102 (SemSugerirCaixa) e E1003 sem qualificar (SemSugerirQualificar) levam a sugestao aplicavel.
 *            0.3.6- Linguagem v1.11 L14a: SemVisivelLento deixa o quat (struct oculta, modulo 0) visivel em todo modulo.
 *            0.3.5 -Linguagem v1.11 L10a: SemDeclararLocal grava a posicao do nome no LocalInfo; SemVisivelLento marca o import usado quando o nome vem qualificado.
 *            0.3.4 -Linguagem v1.11 L8: SemDeclararGlobal substitui o simbolo reservado de uma declaracao com erro (SemEnvenenar) pela declaracao seguinte do mesmo nome.
 *            0.3.3- Linguagem v1.11 L5: E1003 de nome inexistente ou nao pub no modulo com candidatos "parecido" (pub do modulo, sem_parecido.c).
 *            0.3.2 - Linguagem v1.11 L3: SemDeclararLocal marca corpo_tem_ponteiro (a fuga so confere corpo com ponteiro).
 *            0.3.1 -Linguagem v1.8 P6a: SemDeclararLocal copia a origem para o LocalInfo.
 *            0.3.0 - Linguagem v1.8 P3: SemVisivelLento (E1003), E0202 entre modulos e contra apelido de import.
 *            0.2.0- Linguagem v1.7 P10: versao de fechamento da v1.7 (sem mudanca de logica).
 *            0.1.1 - Linguagem v1.7 P4: SemDeclararLocal recusa '_' (E0102): ele so descarta em let a, _ = f().
 *            0.1.0 - Fase 5c: caixa (E0102) com sugestao na causa; nome repetido (E0202) sem sombra nem sobrecarga.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_interno.h"

static bool EhMaiuscula(char c) { return c >= 'A' && c <= 'Z'; }
static bool EhMinuscula(char c) { return c >= 'a' && c <= 'z'; }
static bool EhDigito(char c) { return c >= '0' && c <= '9'; }
static bool EhEspaco(char c) { return c == ' ' || c == '\t'; }
static bool EhLetraOuDigito(char c) { return EhMaiuscula(c) || EhMinuscula(c) || EhDigito(c); }

static bool PrimeiroValido(char c, CaixaNome caixa)
{
    return caixa == CAIXA_SNAKE ? (EhMinuscula(c) || c == '_') : EhMaiuscula(c);
}

static bool RestoValido(char c, CaixaNome caixa)
{
    if (caixa == CAIXA_PASCAL) return EhLetraOuDigito(c);
    if (caixa == CAIXA_MAIUSCULA) return EhMaiuscula(c) || EhDigito(c) || c == '_';
    return EhMinuscula(c) || EhDigito(c) || c == '_';
}

static bool CaixaConfere(const char *nome, uint32_t tam, CaixaNome caixa)
{
    if (tam == 0 || !PrimeiroValido(nome[0], caixa)) return false;
    for (uint32_t i = 1; i < tam; i++) {
        if (!RestoValido(nome[i], caixa)) return false;
    }
    return true;
}

typedef struct { char *dados; size_t tam, cap; } Saida;

static void Empurrar(Saida *o, char c)
{
    if (o->tam + 1 < o->cap) o->dados[o->tam++] = c;
    o->dados[o->tam] = '\0';
}

static bool SemMinusculas(const char *nome, uint32_t tam)
{
    for (uint32_t i = 0; i < tam; i++) {
        if (EhMinuscula(nome[i])) return false;
    }
    return true;
}

static char Minuscula(char c) { return EhMaiuscula(c) ? (char)(c - 'A' + 'a') : c; }
static char Maiuscula(char c) { return EhMinuscula(c) ? (char)(c - 'a' + 'A') : c; }

/* "meu_tipo" / "METROS" -> "MeuTipo" / "Metros". */
static void SugerirPascal(const char *nome, uint32_t tam, Saida *o)
{
    bool inicio = true, tudo_maiusculo = SemMinusculas(nome, tam);
    for (uint32_t i = 0; i < tam; i++) {
        if (nome[i] == '_') {
            inicio = true;
            continue;
        }
        Empurrar(o, inicio ? Maiuscula(nome[i]) : (tudo_maiusculo ? Minuscula(nome[i]) : nome[i]));
        inicio = false;
    }
}

/* "somaAte" / "SomaAte" -> "soma_ate" (ou "SOMA_ATE" em constante). */
static void SugerirComSublinhado(const char *nome, uint32_t tam, CaixaNome caixa, Saida *o)
{
    for (uint32_t i = 0; i < tam; i++) {
        bool quebra = i > 0 && EhMaiuscula(nome[i]) && !EhMaiuscula(nome[i - 1]) && nome[i - 1] != '_';
        if (quebra) Empurrar(o, '_');
        Empurrar(o, caixa == CAIXA_MAIUSCULA ? Maiuscula(nome[i]) : Minuscula(nome[i]));
    }
}

static const char *const REGRA[] = {
    [CAIXA_PASCAL] = "tipo, unidade, struct e enum usam PascalCase",
    [CAIXA_MAIUSCULA] = "constante usa MAIUSCULO_COM_SUBLINHADO",
    [CAIXA_SNAKE] = "proc, parametro, campo, membro de enum e variavel usam snake_case",
};

bool SemVerificarCaixa(Semantica *s, int32_t nome, uint32_t pos, CaixaNome caixa)
{
    const char *texto = SemNomeTexto(s, nome);
    uint32_t tam = InternerTam(s->nomes, nome);
    char buffer[128];
    Saida o = {buffer, 0, sizeof buffer};
    if (!SEM_VERIFICA_CAIXA || CaixaConfere(texto, tam, caixa)) return true;
    buffer[0] = '\0';
    if (caixa == CAIXA_PASCAL) SugerirPascal(texto, tam, &o);
    else SugerirComSublinhado(texto, tam, caixa, &o);
    SemErro(s, E0102, pos, "o nome '%s' esta na caixa errada (%s); use '%s'", texto, REGRA[caixa], buffer);
    SemSugerirCaixa(s, nome, pos, buffer);
    return false;
}

static const char *CaminhoDoModulo(const Semantica *s, uint8_t modulo)
{
    return SemNomeTexto(s, s->modulos->itens[modulo].caminho);
}

/* Caminho lento da visibilidade (v1.8): o nome e de outro modulo, ou foi escrito com apelido. Erros E1003 em u->pos. */
bool SemVisivelLento(Semantica *s, const Simbolo *y, const NomeUsado *u)
{
    const char *nome = SemNomeTexto(s, u->nome);
    if (u->modulo == 0 && y->decl != NULL && (y->decl == s->decl_quat || y->decl == s->decl_allocator)) return true; /* v1.11 L14a: o quat e da linguagem, vale em todo modulo; B01a: o Allocator tambem */
    SemHigieneImportUsado(s, u->modulo);
    if (u->modulo == 0) {
        SemErro(s, E1003, u->pos, "'%s' e do modulo \"%s\": importe-o e escreva apelido.%s", nome, CaminhoDoModulo(s, y->decl->modulo), nome);
        SemSugerirQualificar(s, u, y->decl);
        return false;
    }
    if (y->decl == NULL && y->categoria != SIM_NENHUM) {
        return SemErro(s, E1003, u->pos, "'%s' e da linguagem, nao do modulo \"%s\": escreva sem apelido", nome,
                       CaminhoDoModulo(s, u->modulo));
    }
    if (y->decl == NULL || y->decl->modulo != u->modulo) {
        SemParecido sp;
        SemParecidoIniciar(s, E1003, u->pos, u->nome, &sp);
        SemParecidosDeNome(s, u->modulo, &sp);
        return SemErroParecido(s, &sp, "'%s' nao existe no modulo \"%s\"", nome, CaminhoDoModulo(s, u->modulo));
    }
    if (!y->decl->publico) {
        SemParecido sp;
        SemParecidoIniciar(s, E1003, u->pos, u->nome, &sp);
        SemParecidosDeNome(s, u->modulo, &sp);
        return SemErroParecido(s, &sp, "'%s' existe no modulo \"%s\" mas nao e pub", nome, CaminhoDoModulo(s, u->modulo));
    }
    return true;
}

/* v1.8 (D2): nome de topo unico no programa; 'y' declarado em outro modulo diz qual. */
static bool ErroNomeRepetido(Semantica *s, int32_t nome, uint32_t pos, const Simbolo *y)
{
    if (y->decl != NULL && y->decl->modulo != s->modulo_atual) {
        return SemErro(s, E0202, pos, "'%s' ja foi declarado no modulo \"%s\" (nomes de topo sao unicos no programa)",
                       SemNomeTexto(s, nome), CaminhoDoModulo(s, y->decl->modulo));
    }
    if (y->decl != NULL && FonteTrechoDoOffset(s->fonte, y->decl->pos) != s->arquivo_atual) { /* v1.13: outro arquivo do mesmo pacote */
        FontePosicao antes = FonteLocalizarNoArquivo(s->fonte, y->decl->pos);
        return SemErro(s, E0202, pos, "'%s' ja declarado em %s:%u (mesmo pacote)", SemNomeTexto(s, nome), antes.caminho, antes.linha);
    }
    return SemErro(s, E0202, pos, "o nome '%s' ja foi declarado", SemNomeTexto(s, nome));
}

/* v1.8: um import deste arquivo ja usa o nome como apelido. */
static bool ConflitaComApelido(Semantica *s, int32_t nome, uint32_t pos)
{
    if (s->qtd_modulos <= 1 || !ModulosApelidoNoPacote(s->modulos, s->modulo_atual, nome)) return false;
    return !SemErro(s, E0202, pos, "'%s' e o apelido de um import deste arquivo", SemNomeTexto(s, nome));
}

/* IA1-L8: o simbolo mora em globais[d->chave]; o nome puro ainda disputa o apelido de import e o nome reservado pela linguagem (embutido). */
bool SemDeclararGlobal(Semantica *s, const Decl *d, const Simbolo *novo)
{
    Simbolo *y = &s->globais[d->chave];
    if (ConflitaComApelido(s, d->nome, d->pos)) return false;
    if (__builtin_expect(d->nome == SYM_ALLOCATOR, 0)) { /* B01a: tipo embutido; o programa nao o declara de novo */
        return SemErro(s, E0202, d->pos, "o nome '%s' e reservado pela linguagem", SemNomeTexto(s, d->nome));
    }
    if (d->chave != d->nome && s->globais[d->nome].categoria == SIM_PROC_EMBUTIDO) {
        return SemErro(s, E0202, d->pos, "o nome '%s' e reservado pela linguagem", SemNomeTexto(s, d->nome));
    }
    if (y->categoria == SIM_NENHUM || (y->envenenado && y->tipo == TIPO_VENENO)) { /* reservado de nome do veneno: nao disputa o nome */
        *y = *novo;
        return true;
    }
    if (y->categoria == SIM_PROC_EMBUTIDO) {
        return SemErro(s, E0202, d->pos, "o nome '%s' e reservado pela linguagem", SemNomeTexto(s, d->nome));
    }
    return ErroNomeRepetido(s, d->nome, d->pos, y);
}

/* Declaracao de 'nome' no modulo (a cadeia de homonimos so tem modulo importado); NULL = o modulo nao declara o nome. */
static const Decl *DeclDoModulo(const Semantica *s, int32_t nome, uint8_t modulo)
{
    if (s->homonimos == NULL) return NULL;
    for (const Decl *d = s->homonimos[nome]; d != NULL; d = d->homonimo) {
        if (d->modulo == modulo) return d;
    }
    return NULL;
}

/* Nome sem declaracao no modulo em analise nem em globais[nome]: algum outro modulo importado o declara? Devolve o simbolo dele (ja declarado) para o E1003. */
static const Simbolo *SimboloDeOutroModulo(const Semantica *s, int32_t nome, const Simbolo *vazio)
{
    if (s->homonimos == NULL) return vazio;
    for (const Decl *d = s->homonimos[nome]; d != NULL; d = d->homonimo) {
        if (s->globais[d->chave].categoria != SIM_NENHUM) return &s->globais[d->chave];
    }
    return vazio;
}

const Simbolo *SemAcharSimboloLento(const Semantica *s, int32_t nome, uint8_t qualificador)
{
    return SemAcharSimboloEm(s, nome, qualificador, s->modulo_atual);
}

const Simbolo *SemAcharSimboloEm(const Semantica *s, int32_t nome, uint8_t qualificador, uint8_t modulo_do_uso)
{
    uint8_t modulo = qualificador != 0 ? qualificador : modulo_do_uso;
    const Simbolo *y = &s->globais[nome];
    const Decl *propria = modulo != 0 ? DeclDoModulo(s, nome, modulo) : NULL;
    if (propria != NULL) return &s->globais[propria->chave];
    if (qualificador != 0 || y->categoria != SIM_NENHUM) return y;
    return SimboloDeOutroModulo(s, nome, y);
}

/* Os ids saem de prox_local++ em ordem e a tabela e esvaziada a cada corpo (IniciarCorpo): empurrar mantem tabela[id] == local id. */
void SemGravarNomeLocal(Semantica *s, int32_t nome, TipoId tipo, uint32_t pos)
{
    LocalNome *e = VecEmpurrar(&s->nomes_locais);
    *e = (LocalNome){nome, tipo, pos};
}

bool SemDeclararLocal(Semantica *s, const NovoLocal *novo, int32_t *id)
{
    if (novo->nome == s->nome_descarte) return SemErro(s, E0102, novo->pos, "'_' so descarta um valor em let a, _ = f()");
    if (!SemVerificarCaixa(s, novo->nome, novo->pos, CAIXA_SNAKE)) return false;
    if (ConflitaComApelido(s, novo->nome, novo->pos)) return false;
    if (EscopoBuscar(&s->escopo, novo->nome) != NULL) {
        return SemErro(s, E0202, novo->pos, "o nome '%s' ja foi declarado neste proc; a Forja nao tem sombra",
                       SemNomeTexto(s, novo->nome));
    }
    *id = s->prox_local++;
    SemGravarNomeLocal(s, novo->nome, novo->tipo, novo->pos);
    SemMarcarPonteiroDoCorpo(s, novo->tipo);
    LocalInfo local = {novo->nome, novo->tipo, novo->natureza, novo->origem, *id, 0, novo->pos, 0, 0};
    EscopoEmpurrar(&s->escopo, &local);
    return true;
}

uint32_t SemPosDepoisDaPalavra(const Semantica *s, uint32_t pos)
{
    const char *t = s->fonte->texto;
    while (EhLetraOuDigito(t[pos]) || t[pos] == '_') pos++;
    while (EhEspaco(t[pos])) pos++;
    return pos;
}

uint32_t SemPosDoNomeQualificado(const Semantica *s, uint32_t pos)
{
    const char *t = s->fonte->texto;
    while (t[pos] != '.' && t[pos] != '\0') pos++;
    return t[pos] == '.' ? pos + 1 : pos;
}

uint32_t SemPosDoErro(const Semantica *s, uint32_t pos_retorno)
{
    const char *t = s->fonte->texto;
    uint32_t pos = pos_retorno;
    while (t[pos] != '!' && t[pos] != '\0') pos++;
    if (t[pos] == '!') pos++;
    while (EhEspaco(t[pos])) pos++;
    return pos;
}
