/* editar_nos.c - as operacoes 'trocar_no' (o texto de um no da AST 1.0 pelo id) e 'trocar_corpo' (o miolo das chaves de um proc ou test) do 'forja editar'.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - Linguagem v1.13 L1b (modulo de pasta): o arquivo do no e o do 1o trecho do modulo, ou, num pacote de 2+ arquivos, o da declaracao de topo do id (ArquivoDoNoDoModulo); antes o trecho de mesmo indice que o modulo.
 *            0.1.0 - IA1-I4: primeira versao. O id e resolvido pelo mesmo AstImprimir do 'forja ast --projeto --no' sobre uma compilacao so de sintaxe (a arvore do parser, a que o 'forja ast' mostra);
 *            "pos" e "fim" do no (recorte fiel, M-A4) dizem a faixa a trocar. O corpo e achado pelos tokens: o '}' que fecha a declaracao e o '{' que casa com ele.
 * Verificacao: construir.bat && python tests\editar\verificar_editar.py --forja <forja.exe>
 */
#include <stdlib.h>
#include <string.h>

#include "driver/editar_interno.h"
#include "semantic/ast_json.h"
#include "semantic/ast_json_ref.h"
#include "frontend/token.h"

/* ---------- trocar_no ---------- */

/* A compilacao so de sintaxe do projeto (nasce na 1a vez e fica em p->sintatica). NULL = erro em p->erro. */
static Compilacao *CompilacaoSintatica(Plano *p)
{
    Compilacao *c;

    if (p->sintatica != NULL) return p->sintatica;
    c = calloc(1, sizeof *c);
    if (c == NULL) FalhaInterna("memoria esgotada alocando a compilacao dos ids");
    if (!FonteLer(&c->fonte, p->q->c->fonte.caminho)) {
        free(c);
        PlanoErro(p, "nao consegui reler '%s'", p->q->c->fonte.caminho);
        return NULL;
    }
    IniciarCompilacao(c);
    c->opcoes = p->q->c->opcoes;
    c->sem.lista.teto = OpcoesMaxErros(c->opcoes);
    if (!AnalisarSintaxe(c, true)) {
        LiberarCompilacao(c);
        free(c);
        PlanoErro(p, "o projeto nao passou na analise de sintaxe ao resolver o id");
        return NULL;
    }
    p->sintatica = c;
    return c;
}

/* [linha, coluna] do membro 'chave' (lista de dois numeros). false = ausente. */
static bool LerPar(const JsonValor *obj, const char *chave, uint32_t *linha, uint32_t *coluna)
{
    const JsonValor *lista = JsonMembro(obj, chave);

    if (lista == NULL || lista->tipo != JSON_LISTA || lista->filho == NULL || lista->filho->prox == NULL) return false;
    *linha = (uint32_t)lista->filho->numero;
    *coluna = (uint32_t)lista->filho->prox->numero;
    return true;
}

/* O arquivo do no 'id' no modulo k: o do modulo; num pacote (pasta com 2+ arquivos), o da declaracao de topo cujo id abre o 'id' (ela diz em que arquivo o no esta). */
static const char *ArquivoDoNoDoModulo(const Compilacao *c, uint32_t k, const char *id)
{
    const Fonte *f = &c->fonte;
    uint32_t primeiro, n = ModulosTrechosDoPacote(&c->modulos, f->ntrechos, (uint8_t)k, &primeiro);

    if (f->ntrechos == 0) return f->caminho;
    for (const Decl *d = c->parser.programa.primeira; n > 1 && d != NULL; d = d->prox) {
        Buf base = {0};
        bool abre;

        if (d->modulo != k) continue;
        AstIdBaseDaDecl(&base, &c->modulos, &c->nomes, d);
        abre = strncmp(id, base.dados, base.tam) == 0 && (id[base.tam] == '\0' || id[base.tam] == '/');
        BufLiberar(&base);
        if (abre) return f->trechos[FonteTrechoDoOffset(f, d->pos)].caminho;
    }
    return f->trechos[primeiro].caminho;
}

/* O arquivo do modulo cujo caminho abre o id ("jogo/fisica:aceleracao/corpo[0]" -> jogo/fisica); o de nome mais longo vence ("std:mem" e "std:mem2"). NULL = nenhum. */
static const char *ArquivoDoId(const Compilacao *c, const char *id)
{
    const char *achado = NULL;
    size_t melhor = 0;

    for (uint32_t k = 0; k < c->modulos.n; k++) {
        int32_t caminho = c->modulos.itens[k].caminho;
        size_t n = InternerTam(&c->nomes, caminho);

        if (n < melhor || strncmp(id, InternerTexto(&c->nomes, caminho), n) != 0 || id[n] != ':') continue;
        melhor = n;
        achado = ArquivoDoNoDoModulo(c, k, id);
    }
    return achado;
}

/* A faixa [pos, fim) do no com esse id (posicoes locais ao arquivo). false = erro em p->erro. */
static bool FaixaDoNo(Plano *p, const char *id, const char **arquivo, Edicao *faixa)
{
    Compilacao *c = CompilacaoSintatica(p);
    Buf saida = {0};
    Arena arena = {0};
    const char *erro = NULL;
    const JsonValor *doc, *sub;
    bool ok;

    if (c == NULL) return false;
    AstPedido pedido = {.programa = &c->parser.programa, .fonte = &c->fonte, .nomes = &c->nomes, .modulos = &c->modulos, .diag = &c->diag, .no = id, .projeto = true};
    if (!AstImprimir(&pedido, &saida, &erro)) {
        BufLiberar(&saida);
        return PlanoErro(p, "no inexistente: '%s' (o id vem de 'forja ast --projeto', campo \"id\")", id);
    }
    doc = JsonLer(&arena, saida.dados, saida.tam);
    sub = JsonMembro(doc, "subarvore");
    *arquivo = ArquivoDoId(c, id);
    ok = *arquivo != NULL && LerPar(sub, "pos", &faixa->linha, &faixa->coluna) && LerPar(sub, "fim", &faixa->linha_fim, &faixa->coluna_fim);
    if (ok) *arquivo = ArenaCopiarTexto(p->arena, *arquivo, strlen(*arquivo));
    BufLiberar(&saida);
    ArenaLiberar(&arena);
    return ok || PlanoErro(p, "o no '%s' nao tem posicao (no criado pela semantica): nao se troca", id);
}

bool EditarTrocarNo(Plano *p, const char *id, const char *texto)
{
    const char *arquivo = NULL;
    Edicao faixa = {0};

    return FaixaDoNo(p, id, &arquivo, &faixa) && PlanoTrocarFaixa(p, arquivo, &faixa, texto);
}

/* ---------- trocar_corpo ---------- */

static const Token *TokenEm(const Consulta *q, uint32_t i)
{
    return (const Token *)VecEm(&q->fim.toks, i);
}

/* Indices dos tokens '{' e '}' do corpo da declaracao. false = erro em p->erro (nao e proc/test com corpo). */
static bool ChavesDoCorpo(Plano *p, const Decl *d, uint32_t *abre, uint32_t *fecha)
{
    const Consulta *q = p->q;
    uint32_t fim = AstFimDecl(&q->fim, d), i = 0, n = (uint32_t)q->fim.toks.n;

    while (i < n && TokenEm(q, i)->off < fim) i++;
    if (i == 0 || (d->tipo_no != D_PROC && d->tipo_no != D_TEST) || d->externo) return PlanoErro(p, "so proc e test (com corpo) tem corpo para trocar");
    *fecha = i - 1;
    *abre = *(const uint32_t *)VecEm(&q->fim.par, *fecha);
    if (TokenEm(q, *fecha)->tipo != TK_RBRACE || *abre == AST_FIM_NENHUM || TokenEm(q, *abre)->tipo != TK_LBRACE) return PlanoErro(p, "nao achei as chaves do corpo");
    return true;
}

/* Inicio da linha que contem o offset. */
static size_t InicioDaLinha(const char *texto, size_t off)
{
    while (off > 0 && texto[off - 1] != '\n') off--;
    return off;
}

/* So espacos e tabs em [de, ate)? */
static bool SoEspacos(const char *texto, size_t de, size_t ate)
{
    for (size_t k = de; k < ate; k++)
        if (texto[k] != ' ' && texto[k] != '\t') return false;
    return true;
}

/* O texto que fica entre '{' e '}': quebra de linha, o corpo (sem as quebras do fim) e a quebra que leva o '}' para a linha dele. Se o '}' nao abre a linha, a indentacao
 * dele e a da linha do '{'. *fim_troca recebe o offset ate onde a troca vai (o '}' ou o inicio da indentacao dele).
 * LIMITE: parametros 6 (consulta, as duas chaves e o texto sao entradas; miolo e fim_troca sao as duas saidas; funcao de uso unico, struct nao ganha clareza). */
static void Miolo(const Consulta *q, uint32_t abre, uint32_t fecha, const char *corpo, Buf *miolo, size_t *fim_troca)
{
    const char *texto = q->c->fonte.texto;
    size_t pos_fecha = TokenEm(q, fecha)->off, ini_linha = InicioDaLinha(texto, pos_fecha), tam_corpo = strlen(corpo);
    bool abre_a_linha = SoEspacos(texto, ini_linha, pos_fecha);

    while (tam_corpo > 0 && (corpo[tam_corpo - 1] == '\n' || corpo[tam_corpo - 1] == '\r')) tam_corpo--;
    *fim_troca = abre_a_linha ? ini_linha : pos_fecha;
    BufAnexarN(miolo, "\n", 1);
    if (tam_corpo > 0) {
        BufAnexarN(miolo, corpo, tam_corpo);
        BufAnexarN(miolo, "\n", 1);
    }
    if (!abre_a_linha) {
        size_t ini_abre = InicioDaLinha(texto, TokenEm(q, abre)->off), fim_abre = ini_abre;

        while (texto[fim_abre] == ' ' || texto[fim_abre] == '\t') fim_abre++;
        BufAnexarN(miolo, texto + ini_abre, fim_abre - ini_abre);
    }
}

bool EditarTrocarCorpo(Plano *p, const char *id, const char *texto)
{
    const Decl *d = ConsultaAcharDecl(p->q, id);
    uint32_t abre = 0, fecha = 0;
    size_t fim_troca = 0;
    Buf miolo = {0};
    bool ok;

    if (d == NULL) return PlanoErro(p, "'%s' nao e declaracao do projeto (use o nome ou o id de 'forja consultar simbolos')", id);
    if (!ChavesDoCorpo(p, d, &abre, &fecha)) return false;
    Miolo(p->q, abre, fecha, texto, &miolo, &fim_troca);
    ok = PlanoTrocarOffsets(p, TokenEm(p->q, abre)->off + 1, (uint32_t)fim_troca, miolo.dados);
    BufLiberar(&miolo);
    return ok;
}
