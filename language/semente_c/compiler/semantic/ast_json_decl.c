/* ast_json_decl.c - declaracoes e documento da AST 1.0 em JSON (comando 'forja ast'); tipos, expressoes e comandos: ast_json.c.
 * Versao: 0.3.2
 * Changelog: 0.3.2 - Linguagem v1.13 L1b (modulo de pasta): "arquivo" do modulo = o do 1o trecho dele (mapa trecho -> pacote, nao o trecho de mesmo indice); cada declaracao conta a linha do arquivo dela e, num pacote de 2+ arquivos, leva "arquivo".
 *            0.3.1 - G3 G0b: quebra de ciclo frontend->semantic (movido de frontend/ para semantic/).
 *            0.3.0 - M6d P92b: "posse" clausula "ends" (param null).
 *0.2.0 - M6c P85: "posse" {clausula, param} no proc foreign com clausula (sem clausula a saida e a de antes).
 *0.1.0 - IA1-I1: declaracoes e documento saem do ast_json.c 0.14.0; ast_versao 1.0.0; ids de topo "<modulo>:<decl>" (repetido no modulo leva "#2"...); Decl "pos" passa a ser o
 *                    inicio da declaracao (pub/export e palavra-chave; o nome fica em "pos_nome"); documento de projeto (uma entrada por modulo, em Modulos.ordem); --sem-pos e --no.
 * Verificacao: construir.bat && forja.exe ast <arq.fj> | python -c "import json,sys; json.load(sys.stdin)"; python ..\tests\ast\verificar_ast.py --forja <forja.exe>
 */
#include "semantic/ast_json.h"
#include "semantic/ast_json_interno.h"

#define AST_VERSAO "1.0.0"

static const char *const NOMES_DECL[] = {"unit", "unit_derivada", "const", "struct", "enum", "proc", "test", "link", "union", "import", "static_assert"};

typedef void (*ImprimeDecl)(Json *j, const Decl *d);

/* Tipo escrito com chave fixa "tipo" (retornos, variantes): o id leva ".../<segmento>". */
static void TipoDoId(Json *j, const char *segmento, int indice, const TipoRef *t)
{
    size_t antes = IdEmpurrar(j, segmento, indice);
    TipoRefJson(j, t);
    IdRestaurar(j, antes);
}

static void DcUnit(Json *j, const Decl *d)
{
    ChaveNome(j, "nome", d->nome);
}

static void DcUnitDerivada(Json *j, const Decl *d)
{
    ChaveNome(j, "nome", d->nome);
    ChaveNome(j, "a", d->nome_a);
    Chave(j, "op");
    BufAnexar(j->b, d->op == OP_MUL ? "\"*\"" : "\"/\"");
    ChaveNome(j, "b", d->nome_b);
}

static void DcConst(Json *j, const Decl *d)
{
    ChaveNome(j, "nome", d->nome);
    ChaveTipoTexto(j, "tipo", d->tipo.tipo);
    ChaveTipo(j, &d->tipo);
    ChaveExpr(j, "valor", d->init);
}

void CampoJson(Json *j, const Campo *c)
{
    AbreObjeto(j, c->pos, AstFimCampo(&j->fim, c));
    ChaveNome(j, "nome", c->nome);
    ChaveTipoTexto(j, "tipo", c->tipo.tipo);
    ChaveTipo(j, &c->tipo);
    if (c->tem_faixa) {
        BufFormatar(j->b, ", \"faixa\": {\"min\": %lld, \"max\": %lld, \"exclusiva\": %s}", (long long)c->faixa_min,
                    (long long)c->faixa_max, c->faixa_excl ? "true" : "false");
    }
    if (c->padrao != NULL) ChaveExpr(j, "padrao", c->padrao);
    BufAnexarN(j->b, "}", 1);
}

static void DcStruct(Json *j, const Decl *d)
{
    ChaveNome(j, "nome", d->nome);
    Chave(j, "campos");
    BufAnexarN(j->b, "[", 1);
    for (uint32_t i = 0; i < d->ncampos; i++) {
        size_t antes = IdEmpurrar(j, "campos", (int)i);
        if (i) BufAnexar(j->b, ", ");
        CampoJson(j, &d->campos[i]);
        IdRestaurar(j, antes);
    }
    BufAnexarN(j->b, "]", 1);
    if (d->externo) ChaveBool(j, "externo", true);
}

static void DcEnum(Json *j, const Decl *d)
{
    ChaveNome(j, "nome", d->nome);
    Chave(j, "membros");
    BufAnexarN(j->b, "[", 1);
    for (uint32_t i = 0; i < d->nmembros; i++) {
        size_t antes = IdEmpurrar(j, "membros", (int)i);
        if (i) BufAnexar(j->b, ", ");
        AbreObjeto(j, d->membros[i].pos, AstFimMembro(&j->fim, &d->membros[i]));
        ChaveNome(j, "nome", d->membros[i].nome);
        if (d->op == 1) BufFormatar(j->b, ", \"valor\": %lld", (long long)d->membros[i].valor);
        BufAnexarN(j->b, "}", 1);
        IdRestaurar(j, antes);
    }
    BufAnexarN(j->b, "]", 1);
}

void ParamJson(Json *j, const Param *p)
{
    AbreObjeto(j, p->pos, AstFimParam(&j->fim, p));
    LocalRegistrar(j, p->pos);
    ChaveNome(j, "nome", p->nome);
    ChaveTipoTexto(j, "tipo", p->tipo.tipo);
    ChaveTipo(j, &p->tipo);
    if (p->padrao != NULL) ChaveExpr(j, "padrao", p->padrao);
    BufAnexarN(j->b, "}", 1);
}

/* Retorno multiplo: "retornos": [{"nome": "q", "tipo": {...}}, ...] e "erro" (se houver '! E'). */
static void RetornosJson(Json *j, const Decl *d)
{
    Chave(j, "retornos");
    BufAnexarN(j->b, "[", 1);
    for (uint32_t i = 0; i < d->nretornos; i++) {
        size_t antes = IdEmpurrar(j, "retornos", (int)i);
        if (i) BufAnexar(j->b, ", ");
        BufAnexarN(j->b, "{", 1);
        BufAnexar(j->b, "\"nome\": ");
        NomeJson(j, d->retornos[i].nome);
        Chave(j, "tipo");
        TipoDoId(j, "tipo", -1, &d->retornos[i].tipo);
        BufAnexarN(j->b, "}", 1);
        IdRestaurar(j, antes);
    }
    BufAnexarN(j->b, "]", 1);
    if (d->retorno.erro >= 0) ChaveNome(j, "erro", d->retorno.erro);
}

static void DcProc(Json *j, const Decl *d)
{
    ChaveNome(j, "nome", d->nome);
    Chave(j, "params");
    BufAnexarN(j->b, "[", 1);
    for (uint32_t i = 0; i < d->nparams; i++) {
        size_t antes = IdEmpurrar(j, "params", (int)i);
        if (i) BufAnexar(j->b, ", ");
        ParamJson(j, &d->params[i]);
        IdRestaurar(j, antes);
    }
    BufAnexarN(j->b, "]", 1);
    if (d->tem_retorno && d->retorno.forma == TR_TUPLA) {
        RetornosJson(j, d);
    } else if (d->tem_retorno) {
        Chave(j, "retorno");
        TipoDoId(j, "retorno", -1, &d->retorno);
        ChaveTipoTexto(j, "tipo_retorno", d->retorno.tipo);
    }
    if (d->externo) ChaveBool(j, "externo", true);
    else ChaveCorpo(j, "corpo", d->corpo);
    if (d->externo && d->simbolo >= 0) ChaveNome(j, "simbolo", d->simbolo); /* v1.10 (A6) */
    if (d->posse_c == POSSE_C_NENHUMA) return;
    static const char *const CLAUSULAS_POSSE[] = {"", "creates", "creates", "destroys", "takes", "ends"};
    Chave(j, "posse");
    BufFormatar(j->b, "{\"clausula\": \"%s\", \"param\": ", CLAUSULAS_POSSE[d->posse_c]);
    if (d->posse_param < 0) BufAnexar(j->b, "null");
    else NomeJson(j, d->params[d->posse_param].nome);
    BufAnexarN(j->b, "}", 1);
}

static void DcLink(Json *j, const Decl *d)
{
    ChaveNome(j, "biblioteca", d->nome);
}

static void DcTest(Json *j, const Decl *d)
{
    ChaveNome(j, "titulo", d->nome);
    ChaveCorpo(j, "corpo", d->corpo);
}

static void DcUniao(Json *j, const Decl *d)
{
    ChaveNome(j, "nome", d->nome);
    Chave(j, "variantes");
    BufAnexarN(j->b, "[", 1);
    for (uint32_t i = 0; i < d->ncampos; i++) {
        if (i) BufAnexar(j->b, ", ");
        TipoDoId(j, "variantes", (int)i, &d->campos[i].tipo);
    }
    BufAnexarN(j->b, "]", 1);
}

static void DcImport(Json *j, const Decl *d)
{
    ImportRegistrar(j, d->nome_a);
    ChaveNome(j, "caminho", d->nome);
    ChaveNome(j, "apelido", d->nome_a);
}

static void DcAssert(Json *j, const Decl *d)
{
    ChaveExpr(j, "chamada", d->init);
}

static const ImprimeDecl DECLARACOES[D_ASSERT + 1] = {
    [D_UNIAO] = DcUniao, [D_IMPORT] = DcImport, [D_ASSERT] = DcAssert,
    [D_UNIDADE_BASE] = DcUnit, [D_UNIDADE_DERIV] = DcUnitDerivada, [D_CONST] = DcConst, [D_STRUCT] = DcStruct,
    [D_ENUM] = DcEnum, [D_PROC] = DcProc, [D_TEST] = DcTest,
    [D_LINK] = DcLink,
};

void DeclJson(Json *j, const Decl *d)
{
    uint32_t inicio = AstInicioDecl(&j->fim, d);
    AbreNo(j, NOMES_DECL[d->tipo_no], inicio, AstFimDecl(&j->fim, d));
    if (d->pos != inicio) ChavePos(j, "pos_nome", d->pos); /* Decl.pos e o nome; "pos" e onde a declaracao comeca */
    if (j->arquivo_decl != NULL && !j->sem_pos) { /* modulo de pasta: as linhas sao do arquivo da declaracao */
        Chave(j, "arquivo");
        TextoJson(j, j->arquivo_decl, strlen(j->arquivo_decl), true);
    }
    if (d->publico) ChaveBool(j, "publico", true);
    if (d->exportado) ChaveBool(j, "exportado", true);
    DECLARACOES[d->tipo_no](j, d);
    BufAnexarN(j->b, "}", 1);
}

/* ---------- documento ---------- */

/* Ids de topo das declaracoes do modulo: o id base e, se o mesmo id ja saiu antes no modulo, "#2", "#3"... (so programa invalido repete nome). */
static void IdsDeTopo(const Json *j, const Vec *decls, Buf *pool, Vec *posicoes)
{
    Buf base = {0};
    for (size_t i = 0; i < decls->n; i++) {
        uint32_t repetidos = 0;
        uint32_t ini = (uint32_t)pool->tam;
        base.tam = 0;
        AstIdBaseDaDecl(&base, j->modulos, j->nomes, *(const Decl *const *)VecEm(decls, i));
        for (size_t k = 0; k < i; k++) {
            const uint32_t *anterior = VecEm(posicoes, k);
            const char *id_anterior = pool->dados + anterior[0];
            bool mesma_base = anterior[1] >= base.tam && memcmp(id_anterior, base.dados, base.tam) == 0;
            if (mesma_base && (anterior[1] == base.tam || id_anterior[base.tam] == '#')) repetidos++; /* o id guardado ja leva o "#n" */
        }
        BufAnexarN(pool, base.dados, base.tam);
        if (repetidos > 0) BufFormatar(pool, "#%u", repetidos + 1);
        uint32_t *p = VecEmpurrar(posicoes);
        p[0] = ini;
        p[1] = (uint32_t)(pool->tam - ini);
    }
    BufLiberar(&base);
}

static void ColetarDeclsDoModulo(const Programa *programa, uint8_t modulo, Vec *decls)
{
    VecIniciar(decls, sizeof(const Decl *));
    for (const Decl *d = programa->primeira; d != NULL; d = d->prox) {
        if (d->modulo == modulo) *(const Decl **)VecEmpurrar(decls) = d;
    }
}

/* O 1o trecho do modulo na Fonte unica (raiz = trecho 0; modulo de pasta: o 1o arquivo dele); *n = quantos arquivos tem. */
static uint32_t PrimeiroTrechoDoModulo(const Json *j, uint8_t modulo, uint32_t *n)
{
    uint32_t primeiro;

    *n = ModulosTrechosDoPacote(j->modulos, j->fonte->ntrechos, modulo, &primeiro);
    return primeiro;
}

/* O arquivo do modulo: o do 1o trecho; sem trechos, o arquivo inteiro. */
static const char *ArquivoDoModulo(const Json *j, uint8_t modulo)
{
    uint32_t n, primeiro = PrimeiroTrechoDoModulo(j, modulo, &n);

    if (j->fonte->ntrechos == 0 || n == 0) return j->fonte->caminho;
    return j->fonte->trechos[primeiro].caminho;
}

static void ChaveCaminhoDoModulo(Json *j)
{
    int32_t caminho = j->modulos->itens[j->modulo].caminho;
    Chave(j, "modulo");
    TextoJson(j, InternerTexto(j->nomes, caminho), InternerTam(j->nomes, caminho), false);
}

/* v1.13: as linhas locais da declaracao contam do 1o byte do ARQUIVO dela (o modulo de pasta tem varios); j->arquivo_decl = esse arquivo, so num modulo de 2+ arquivos. */
static void EntrarNoArquivoDaDecl(Json *j, const Decl *d)
{
    const Fonte *f = j->fonte;
    uint32_t trecho, n;

    if (f->ntrechos == 0) return;
    trecho = FonteTrechoDoOffset(f, d->pos);
    PrimeiroTrechoDoModulo(j, j->modulo, &n);
    j->linha_base = f->trechos[trecho].linha_base;
    j->arquivo_decl = n > 1 ? f->trechos[trecho].caminho : NULL;
}

/* ", "declaracoes": [...]" do modulo j->modulo. */
static void DeclaracoesDoModulo(Json *j, const Programa *programa)
{
    Vec decls, posicoes;
    Buf pool = {0};
    VecIniciar(&posicoes, 2 * sizeof(uint32_t));
    ColetarDeclsDoModulo(programa, j->modulo, &decls);
    IdsDeTopo(j, &decls, &pool, &posicoes);
    j->imports.n = 0;
    Chave(j, "declaracoes");
    BufAnexarN(j->b, "[", 1);
    for (size_t i = 0; i < decls.n; i++) {
        const uint32_t *p = VecEm(&posicoes, i);
        if (i) BufAnexar(j->b, ", ");
        j->id.tam = 0;
        BufAnexarN(&j->id, pool.dados + p[0], p[1]);
        EntrarNoArquivoDaDecl(j, *(const Decl *const *)VecEm(&decls, i));
        DeclJson(j, *(const Decl *const *)VecEm(&decls, i));
    }
    BufAnexarN(j->b, "]", 1);
    VecLiberar(&decls);
    VecLiberar(&posicoes);
    BufLiberar(&pool);
}

static void EntrarNoModulo(Json *j, uint8_t modulo)
{
    uint32_t n, primeiro = PrimeiroTrechoDoModulo(j, modulo, &n);

    j->modulo = modulo;
    j->arquivo_decl = NULL;
    j->linha_base = j->fonte->ntrechos == 0 || n == 0 ? 0 : j->fonte->trechos[primeiro].linha_base;
}

/* Um modulo no documento de projeto: {"modulo", "arquivo", "declaracoes"}. */
static void ModuloDoProjeto(Json *j, const Programa *programa, uint8_t modulo)
{
    EntrarNoModulo(j, modulo);
    BufAnexarN(j->b, "{", 1);
    BufAnexar(j->b, "\"modulo\": ");
    TextoJson(j, InternerTexto(j->nomes, j->modulos->itens[modulo].caminho), InternerTam(j->nomes, j->modulos->itens[modulo].caminho), false);
    if (!j->sem_pos) {
        Chave(j, "arquivo");
        TextoJson(j, ArquivoDoModulo(j, modulo), strlen(ArquivoDoModulo(j, modulo)), true);
    }
    DeclaracoesDoModulo(j, programa);
    BufAnexarN(j->b, "}", 1);
}

static void ModulosDoProjeto(Json *j, const Programa *programa)
{
    uint32_t n = j->modulos->n;
    Chave(j, "modulos");
    BufAnexarN(j->b, "[", 1);
    for (uint32_t i = 0; i < n; i++) {
        if (i) BufAnexar(j->b, ", ");
        ModuloDoProjeto(j, programa, n > 1 ? j->modulos->ordem[i] : 0); /* pos-ordem das dependencias: folhas primeiro, raiz por ultimo */
    }
    BufAnexarN(j->b, "]", 1);
}

static void ImprimirDocumento(Json *j, const AstPedido *p)
{
    BufAnexar(j->b, "{\"arquivo\": ");
    if (j->sem_pos) BufAnexar(j->b, "null");
    else TextoJson(j, j->fonte->caminho, strlen(j->fonte->caminho), true);
    BufAnexar(j->b, ", \"ast_versao\": \"" AST_VERSAO "\"");
    if (p->projeto) {
        BufAnexar(j->b, ", \"projeto\": true");
        ModulosDoProjeto(j, p->programa);
    } else {
        EntrarNoModulo(j, 0);
        ChaveCaminhoDoModulo(j);
        DeclaracoesDoModulo(j, p->programa);
    }
    BufAnexar(j->b, "}\n");
}

/* --no <id>: o documento so com o objeto daquele no. false = id inexistente. */
static bool EscreverSubarvore(const Buf *cheio, const AstPedido *p, Buf *saida)
{
    size_t ini, fim;
    if (!AstNoExtrair(cheio->dados, cheio->tam, p->no, &ini, &fim)) return false;
    BufAnexar(saida, "{\"ast_versao\": \"" AST_VERSAO "\", \"no_id\": ");
    {
        Json t = {.b = saida};
        TextoJson(&t, p->no, strlen(p->no), false);
    }
    BufAnexar(saida, ", \"subarvore\": ");
    BufAnexarN(saida, cheio->dados + ini, fim - ini);
    BufAnexar(saida, "}\n");
    return true;
}

static void LiberarJson(Json *j)
{
    AstFimLiberar(&j->fim);
    VecLiberar(&j->linhas);
    VecLiberar(&j->locais);
    VecLiberar(&j->imports);
    BufLiberar(&j->guardados);
    BufLiberar(&j->id);
}

bool AstImprimir(const AstPedido *p, Buf *saida, const char **erro)
{
    Buf cheio = {0};
    Json j = {.b = p->no != NULL ? &cheio : saida, .fonte = p->fonte, .nomes = p->nomes, .sem = p->sem, .modulos = p->modulos, .refs = p->refs,
              .tipos = p->sem != NULL, .sem_pos = p->sem_pos};
    bool ok = true;

    if (!AstFimIniciar(&j.fim, p->fonte, p->nomes, p->diag)) {
        *erro = "erro lexico ao reler o fonte para as posicoes finais";
        return false;
    }
    VecIniciar(&j.locais, sizeof(AstGuardado));
    VecIniciar(&j.imports, sizeof(AstGuardado));
    MapearLinhas(&j);
    ImprimirDocumento(&j, p);
    if (p->no != NULL && !EscreverSubarvore(&cheio, p, saida)) {
        *erro = "no inexistente: o id vem de 'forja ast' (campo \"id\")";
        ok = false;
    }
    LiberarJson(&j);
    BufLiberar(&cheio);
    return ok;
}
