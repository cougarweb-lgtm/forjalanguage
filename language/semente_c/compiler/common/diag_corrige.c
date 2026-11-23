/* diag_corrige.c - a correcao por maquina por codigo e trecho da causa (P72, D0653 item 1; M-D1 do contrato IA1): para o erro que a analise nao conseguiu consertar sozinha,
 * a tabela REGRAS diz qual edicao de UMA linha resolve (inserir, trocar ou tirar um pedaco perto do offset do erro) e com que confianca. Cada edicao e calculada
 * sobre o texto real do .fj (token, linha, caminho a.b.c, recuo), nunca sobre texto fixo. A 1a regra que casa E cabe na linha vence; ACAO_NENHUMA encerra a busca.
 * Confianca: a regra so e "certa" quando a edicao e unica e nao muda o sentido do programa; as demais saem "provavel" (so o 'forja corrigir --tambem-provavel' as grava).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - P72: primeira versao (E0001, E0101, E0102, E0201, E0202, E0307, E0310, E0311, E0502, E0709, E1001, E1003, E1006).
 * Verificacao: construir.bat && python ..\benchmark\ia_first\verificar_corrigir.py --forja compiler\forja.exe
 */
#include "common/diag_corrige.h"

#include "common/parecido.h"

#include <stdio.h>
#include <string.h>

enum { NOME_MAX = 64, RECUO_MAX = 24, MODULO_MAX = 32, DISTANCIA_STD_MAX = 2 };
#define NAO_ACHEI 0xFFFFFFFFu

typedef enum {
    ACAO_NENHUMA,                /* a causa casa, mas nao ha edicao segura: encerra a busca */
    ACAO_ANTES,                  /* insere 'texto' antes do token do erro */
    ACAO_DEPOIS,                 /* insere depois do token do erro */
    ACAO_DEPOIS_CAMINHO,         /* insere depois do caminho a.b.c que comeca no erro */
    ACAO_FIM_LINHA,              /* insere no fim da linha */
    ACAO_INICIO_LINHA,           /* insere depois do recuo da linha */
    ACAO_ANTES_DECL_TOPO,        /* insere antes da declaracao de topo (coluna 1) que contem o erro */
    ACAO_TROCAR,                 /* troca o token do erro por 'texto' */
    ACAO_REMOVER_TOKEN,          /* tira o token do erro (ou a sequencia de bytes nao ASCII) */
    ACAO_REMOVER_LINHA,          /* esvazia a linha, se ela contem 'texto' */
    ACAO_REMOVER_ATE,            /* tira do erro ate o 1o 'texto' da linha, sem ele */
    ACAO_REMOVER_ATE_INCLUINDO,  /* tira do erro ate o 1o 'texto' da linha, com ele */
    ACAO_REMOVER_ACHADO,         /* tira so o 1o 'texto' da linha a partir do erro */
    ACAO_REMOVER_DECL,           /* tira o let/var antes do nome (reaproveita a variavel) */
    ACAO_REMOVER_QUALIFICADOR,   /* tira o "apelido." antes do nome */
    ACAO_REMOVER_ARGUMENTO,      /* tira a virgula anterior e o argumento do erro */
    ACAO_REMOVER_DO_INICIO,      /* tira do inicio da linha (sem o recuo) ate o erro */
    ACAO_TROCAR_STD,             /* troca o import "std:x" pelo modulo da std mais parecido */
    ACAO_REMOVER_RESTO,          /* tira o que vem depois do token do erro, ate o fim da linha */
    ACAO_TROCAR_CHAMADA,         /* troca o nome da chamada cujo 1o argumento e o erro (new( -> make() */
    ACAO_REESCREVER_SOMA,        /* a + b de str vira f"{a}{b}" */
    ACAO_PUB_NA_DECLARACAO,      /* poe pub na linha do relacionado de declaracao (a que o erro diz nao ser pub) */
    ACAO_INCREMENTO,             /* ++nome ou --nome vira nome += 1 ou nome -= 1 */
    ACAO_TIRAR_PARENTESES_FOR,   /* for (x in xs) vira for x in xs */
    ACAO_TIRAR_INDICE_FOR,       /* for i, x in faixa e for &x in faixa perdem o indice e o & */
    ACAO_COMPARAR_CSTR,          /* a == b de cstr vira (a as str) == (b as str) */
    ACAO_MEMBRO_NO_ENUM,         /* acrescenta o membro que falta ao enum de uma linha so */
    ACAO_FOR_DE_C,               /* for (var i = 0; i < n; i++) vira for i in 0..<n */
    ACAO_ANOTAR_TIPO,            /* let a = [3]int{...} vira let a: [3]int = {...} */
    ACAO_CAMPO_NA_STRUCT         /* acrescenta o campo que falta na struct (declarada em varias linhas) */
} Acao;

typedef struct {
    DiagCodigo codigo;
    const char *trecho_da_causa, *texto, *descricao;
    Acao acao;
    DiagConfianca confianca;
} RegraCorrecao;

/* O que as acoes precisam saber da ocorrencia (tudo em offsets do texto do arquivo). */
typedef struct {
    const Fonte *f;
    const Diag *d;
    const char *t, *causa;
    uint32_t off, fim_token, ini_linha, ini_texto, fim_linha;
    char nome1[NOME_MAX], nome2[NOME_MAX], token[NOME_MAX], recuo[RECUO_MAX];
} Alvo;

/* ---------- a tabela ---------- */

static const RegraCorrecao REGRAS[] = {
    /* ---- E0001: lexico ---- */
    {E0001, "caractere invalido", "", "tire o caractere '$T': a Forja nao o usa", ACAO_REMOVER_TOKEN, DIAG_PROVAVEL},
    {E0001, "caractere fora do ASCII", "", "tire o caractere com acento: nome, tipo e comando so usam a-z, A-Z, 0-9 e _", ACAO_REMOVER_TOKEN, DIAG_PROVAVEL},
    {E0001, "nao e digito binario", "", "tire o digito '$T': em binario so existem 0 e 1", ACAO_REMOVER_TOKEN, DIAG_PROVAVEL},
    {E0001, "nao e digito hexadecimal", "", "tire o caractere '$T': em hexadecimal so existem 0-9 e a-f", ACAO_REMOVER_TOKEN, DIAG_PROVAVEL},
    {E0001, "faltam digitos depois de", "0", "acrescente um digito depois do prefixo (0x0)", ACAO_DEPOIS, DIAG_PROVAVEL},
    {E0001, "sozinho no f\"", "}}", "escreva '}}' para uma chave literal no f\"...\"", ACAO_TROCAR, DIAG_PROVAVEL},
    {E0001, "vazio no f\"", "}", "tire o {} vazio (ou ponha uma expressao entre as chaves)", ACAO_REMOVER_ATE_INCLUINDO, DIAG_PROVAVEL},
    {E0001, "aspas dentro de {...}", "}", "feche a chave antes da aspa final do f\"...\"", ACAO_ANTES, DIAG_PROVAVEL},
    {E0001, "falta a aspa de fechamento", "\"", "feche o texto: acrescente a aspa no fim da linha", ACAO_FIM_LINHA, DIAG_PROVAVEL},

    /* ---- E0101: sintaxe ---- */
    {E0101, "literal de array no cabecalho", "", "", ACAO_NENHUMA, DIAG_SO_DICA},
    {E0101, "fim de linha inesperado; esperado ']'", "]", "feche o colchete: acrescente ']' no fim da linha", ACAO_FIM_LINHA, DIAG_PROVAVEL},
    {E0101, "fim de linha inesperado; esperado ')'", ")", "feche o parentese: acrescente ')' no fim da linha", ACAO_FIM_LINHA, DIAG_PROVAVEL},
    {E0101, "fim de linha inesperado; esperado '{'", " {", "abra o bloco: acrescente '{' no fim da linha", ACAO_FIM_LINHA, DIAG_PROVAVEL},
    {E0101, "(expect recebe so a condicao)", ")", "tire o 2o argumento: expect recebe so a condicao", ACAO_REMOVER_ATE, DIAG_PROVAVEL},
    {E0101, "esperado ']'", "]", "feche o colchete antes de '$T'", ACAO_ANTES, DIAG_PROVAVEL},
    {E0101, "esperado ')'", ")", "feche o parentese antes de '$T'", ACAO_ANTES, DIAG_PROVAVEL},
    {E0101, "esperado nome do membro ou '}'", "", "tire o '$T' a mais", ACAO_REMOVER_TOKEN, DIAG_PROVAVEL},
    {E0101, "esperado '}'", ", ", "separe os campos com ',' antes de '$T'", ACAO_ANTES, DIAG_PROVAVEL},
    {E0101, "esperado ','", ", ", "separe com ',' antes de '$T'", ACAO_ANTES, DIAG_PROVAVEL},
    {E0101, "esperado fim de linha", "\n", "um comando por linha: quebre a linha antes de '$T'", ACAO_ANTES, DIAG_PROVAVEL},
    {E0101, "esperado expressao", "", "tire o '$T': falta uma expressao no lugar dele", ACAO_REMOVER_TOKEN, DIAG_PROVAVEL},
    {E0101, "esperado inteiro", "0", "o valor do membro e um inteiro: troque '$T' por um numero", ACAO_TROCAR, DIAG_PROVAVEL},
    {E0101, "esperado nome da variavel", "valor ", "de um nome a variavel, antes de '$T'", ACAO_ANTES, DIAG_PROVAVEL},
    {E0101, "esperado nome do parametro do callback", "p: ", "de um nome ao parametro do callback (nome: T)", ACAO_ANTES, DIAG_PROVAVEL},
    {E0101, "esperado titulo do teste", "\"teste\" ", "de um titulo ao teste, entre aspas", ACAO_ANTES, DIAG_PROVAVEL},
    {E0101, "esperado o simbolo C entre aspas", "\"$T\"", "ponha o simbolo C entre aspas", ACAO_TROCAR, DIAG_PROVAVEL},
    {E0101, "esperado o 1o argumento de new", "", "deixe so o tipo no 1o argumento", ACAO_REMOVER_ATE, DIAG_PROVAVEL},
    {E0101, "literal de array so tem valores", "= ", "tire o nome do campo: o literal de array so tem valores", ACAO_REMOVER_ATE_INCLUINDO, DIAG_PROVAVEL},
    {E0101, "'='; esperado nome de tipo", "int ", "de o tipo depois do '?': ?int", ACAO_ANTES, DIAG_PROVAVEL},
    {E0101, "esperado nome de tipo", "int", "troque '$T' por um tipo", ACAO_TROCAR, DIAG_PROVAVEL},
    {E0101, "esperado nome do parametro", ")", "feche a lista de parametros", ACAO_ANTES, DIAG_PROVAVEL},
    {E0101, "fim de arquivo inesperado; esperado comando", "}\n", "feche o bloco que ficou aberto: acrescente '}' no fim do arquivo", ACAO_ANTES, DIAG_PROVAVEL},
    {E0101, "bloco foreign vazio", "    link \"lib\"\n", "declare a biblioteca: link \"nome\"", ACAO_ANTES, DIAG_PROVAVEL},
    {E0101, "for com indice ou '&' percorre array", "", "tire o indice (ou o '&') do for: faixa so tem o valor", ACAO_TIRAR_INDICE_FOR, DIAG_PROVAVEL},
    {E0101, "import nao leva pub nem export", "", "tire o pub/export antes do import", ACAO_REMOVER_DO_INICIO, DIAG_PROVAVEL},
    {E0101, "foreign nao leva nome", "", "tire o nome do foreign: cada biblioteca e uma linha link \"nome\"", ACAO_REMOVER_TOKEN, DIAG_PROVAVEL},
    {E0101, "header nao entra no .fj", "link", "troque header por link e ponha o nome da biblioteca", ACAO_TROCAR, DIAG_PROVAVEL},

    /* ---- E0110: so os dois idiomas que cabem numa edicao de uma linha ---- */
    {E0110, "';' termina o comando", "\n", "um comando por linha: troque o ';' por uma quebra de linha", ACAO_TROCAR, DIAG_PROVAVEL},
    {E0110, "'++' soma ou subtrai 1", "", "troque ++ por += 1 (e -- por -= 1), como comando proprio", ACAO_INCREMENTO, DIAG_PROVAVEL},
    {E0110, "'for (...)'", "", "tire os parenteses do for: for x in xs {", ACAO_TIRAR_PARENTESES_FOR, DIAG_PROVAVEL},
    {E0110, "'for (;;)'", "", "troque o for de C por for i in 0..<n", ACAO_FOR_DE_C, DIAG_PROVAVEL},
    {E0110, "'[a, b]' abre o literal de array", "", "declare o tipo na variavel e use chaves: let a: [N]T = {...}", ACAO_ANOTAR_TIPO, DIAG_PROVAVEL},
    {E0110, "devolve o codigo de saida", "{", "tire o '-> tipo': proc main() nao devolve valor (o codigo de saida e os.exit(n))", ACAO_REMOVER_ATE, DIAG_PROVAVEL},

    /* ---- E0102 ---- */
    {E0102, "so descarta um valor em let", "_r", "de um nome ao valor: let _r = ...", ACAO_TROCAR, DIAG_PROVAVEL},

    /* ---- E0201: nome, tipo ou proc nao declarado ---- */
    {E0201, "falta 'proc main()'", "proc main() {\n}\n\n", "acrescente proc main() {}", ACAO_ANTES_DECL_TOPO, DIAG_PROVAVEL},
    {E0201, "nao tem o campo", "", "acrescente o campo '$2' a struct (ou use um campo que ja existe)", ACAO_CAMPO_NA_STRUCT, DIAG_PROVAVEL},
    {E0201, "nao tem o membro", "", "acrescente o membro '$2' ao enum", ACAO_MEMBRO_NO_ENUM, DIAG_PROVAVEL},
    {E0201, "tipo '", "struct $1 {\n}\n\n", "declare o tipo '$1' antes do uso (ou use um tipo que ja existe)", ACAO_ANTES_DECL_TOPO, DIAG_PROVAVEL},
    {E0201, "proc '", "proc $1() {\n}\n\n", "declare o proc '$1' antes do uso (ou chame um que ja existe)", ACAO_ANTES_DECL_TOPO, DIAG_PROVAVEL},
    {E0201, "nao declarado como variavel ou constante", "let $1 = 0\n$R", "declare '$1' antes do uso (ou use um nome que ja existe)", ACAO_INICIO_LINHA, DIAG_PROVAVEL},

    /* ---- E0202: nome repetido ---- */
    {E0202, "ja foi declarado neste proc", "", "reaproveite a variavel: tire o let/var desta 2a declaracao", ACAO_REMOVER_DECL, DIAG_PROVAVEL},
    {E0202, "ja foi declarado neste proc", "$1_2", "troque o nome desta 2a declaracao (a Forja nao tem sombra)", ACAO_TROCAR, DIAG_PROVAVEL},
    {E0202, "e o apelido de um import", "$1_2", "troque o nome: '$1' ja e o apelido de um import", ACAO_TROCAR, DIAG_PROVAVEL},
    {E0202, "ja existe no enum", "$1_2", "troque o nome do membro repetido", ACAO_TROCAR, DIAG_PROVAVEL},
    {E0202, "ja foi declarado", "$1_2", "troque o nome desta 2a declaracao", ACAO_TROCAR, DIAG_PROVAVEL},
    {E0202, "repete o valor", "", "tire o valor explicito: o membro ganha o proximo numero livre", ACAO_REMOVER_RESTO, DIAG_PROVAVEL},
    {E0202, "ja tem alias em outro proc foreign", "proc", "tire este proc e chame o que ja tem o alias", ACAO_REMOVER_LINHA, DIAG_PROVAVEL},
    {E0202, "ja e declarado como proc", "proc", "tire este proc e use o que ja declara o simbolo C", ACAO_REMOVER_LINHA, DIAG_PROVAVEL},
    {E0202, "e reservado pela linguagem", "$1_x", "troque o nome: '$1' e reservado pela linguagem", ACAO_TROCAR, DIAG_PROVAVEL},

    /* ---- E0307 ---- */
    {E0307, "str nao se soma nem faz conta", "", "junte os textos com f\"{a}{b}\"", ACAO_REESCREVER_SOMA, DIAG_PROVAVEL},
    {E0307, "== em cstr compararia enderecos", "", "compare como str: (a as str) == (b as str)", ACAO_COMPARAR_CSTR, DIAG_PROVAVEL},
    {E0307, "len de cstr", " as str", "converta o cstr para str: len(c as str)", ACAO_DEPOIS_CAMINHO, DIAG_PROVAVEL},

    /* ---- E0310: memoria explicita ---- */
    {E0310, "o alloc do delete so recebe um Pool", "", "tire o 'alloc =': o delete so aceita o alloc de um Pool", ACAO_REMOVER_ARGUMENTO, DIAG_PROVAVEL},
    {E0310, "nao se libera um a um", "delete", "tire o delete: o objeto de Arena morre com arena_reset ou arena_destroy", ACAO_REMOVER_LINHA, DIAG_PROVAVEL},
    {E0310, "veio do alocador do chamador", "delete", "tire o delete: quem alocou (o chamador) e quem libera", ACAO_REMOVER_LINHA, DIAG_PROVAVEL},
    {E0310, "veio de um proc foreign", "delete", "tire o delete: a memoria e da biblioteca C", ACAO_REMOVER_LINHA, DIAG_PROVAVEL},
    {E0310, "delete de texto literal", "delete", "tire o delete: texto literal nao se libera", ACAO_REMOVER_LINHA, DIAG_PROVAVEL},
    {E0310, "guarda um texto literal", "delete", "tire o delete: texto literal nao se libera", ACAO_REMOVER_LINHA, DIAG_PROVAVEL},
    {E0310, "delete pede ^T vindo de new", "delete", "tire o delete: so se libera o que veio de new ou de lista", ACAO_REMOVER_LINHA, DIAG_PROVAVEL},
    {E0310, "delete(p^) apaga a lista", "delete", "tire o delete: o ponteiro nao veio de new", ACAO_REMOVER_LINHA, DIAG_PROVAVEL},
    {E0310, "objeto de Pool volta com delete", ", alloc = pool", "devolva ao Pool: delete(x, alloc = pool)", ACAO_DEPOIS_CAMINHO, DIAG_PROVAVEL},
    {E0310, "recebe o endereco", "&", "passe o endereco: &nome", ACAO_ANTES, DIAG_CERTA},
    {E0310, "pede uma fatia", "[..]", "passe a fatia da lista: nome[..]", ACAO_DEPOIS_CAMINHO, DIAG_PROVAVEL},
    {E0310, "alloc recebe heap, frame, scope, Arena ou Pool", "heap", "use um alloc valido: alloc = heap", ACAO_TROCAR, DIAG_PROVAVEL},
    {E0310, "Pool so serve new", "heap", "use alloc = heap: Pool so serve new", ACAO_TROCAR, DIAG_PROVAVEL},
    {E0310, "make de fatia nao existe", "dynamic", "crie a lista com make([dynamic]T, alloc = heap)", ACAO_DEPOIS, DIAG_PROVAVEL},
    {E0310, "make com len so vale", "", "tire o len = e acrescente os elementos um a um com append", ACAO_REMOVER_ARGUMENTO, DIAG_PROVAVEL},
    {E0310, "new([dynamic]T) nao existe", "make", "crie a lista com make([dynamic]T, alloc = heap)", ACAO_TROCAR_CHAMADA, DIAG_PROVAVEL},
    {E0310, "new(^", "", "peca o valor, nao o ponteiro: new(Tipo) ja devolve ^Tipo", ACAO_REMOVER_TOKEN, DIAG_PROVAVEL},
    {E0310, "precisa ser guardado", "let r = ", "guarde o resultado: let r = ... (e libere com delete)", ACAO_INICIO_LINHA, DIAG_PROVAVEL},

    /* ---- E0311: dono de memoria ---- */
    {E0311, "parametro nao recebe lista por valor", "^", "receba o endereco da lista: ^[dynamic]T", ACAO_ANTES, DIAG_PROVAVEL},
    {E0311, "criaria duas donas da mesma memoria", "[..]", "empreste a lista como fatia: nome[..]", ACAO_DEPOIS_CAMINHO, DIAG_PROVAVEL},

    /* ---- E0502, E0708, E0709 ---- */
    {E0708, "nunca e lido", "_$1", "troque o nome por '_$1' (nome que comeca com _ nao pede leitura)", ACAO_TROCAR, DIAG_PROVAVEL},
    {E0502, "sobra argumento", "", "tire o argumento que sobra", ACAO_REMOVER_ARGUMENTO, DIAG_PROVAVEL},
    {E0709, "so descarta o valor devolvido por uma chamada", "_ =", "tire a linha: so uma chamada que devolve valor pode ser descartada com '_ ='", ACAO_REMOVER_LINHA, DIAG_PROVAVEL},

    /* ---- E1001, E1003, E1006: modulos ---- */
    {E1001, "o caminho do import vai sem extensao", ".fj", "tire a extensao .fj do caminho", ACAO_REMOVER_ACHADO, DIAG_PROVAVEL},
    {E1001, "caminho absoluto nao vale", "/", "tire a barra do inicio: o caminho e relativo", ACAO_REMOVER_ACHADO, DIAG_PROVAVEL},
    {E1001, "nao valem no caminho do import", "../", "tire o '../' do caminho: ele parte da pasta do arquivo raiz", ACAO_REMOVER_ACHADO, DIAG_PROVAVEL},
    {E1001, "nao existe; a std tem:", "", "troque pelo modulo da std mais parecido", ACAO_TROCAR_STD, DIAG_PROVAVEL},
    {E1001, "nao existe; a std tem:", "import", "tire o import: esse modulo da std nao existe", ACAO_REMOVER_LINHA, DIAG_PROVAVEL},
    {E1001, "nao encontrado", "import", "tire o import (ou crie o arquivo do modulo): ele nao foi encontrado", ACAO_REMOVER_LINHA, DIAG_PROVAVEL},
    {E1001, "ja usado neste arquivo", "$1_2", "troque o apelido: '$1' ja esta em uso", ACAO_TROCAR, DIAG_PROVAVEL},
    {E1003, "mas nao e pub", "", "declare a linha do modulo com pub", ACAO_PUB_NA_DECLARACAO, DIAG_PROVAVEL},
    {E1003, "e da linguagem, nao do modulo", "", "escreva sem o apelido: o nome e da linguagem", ACAO_REMOVER_QUALIFICADOR, DIAG_PROVAVEL},
    {E1006, "palavra reservada", " as modulo", "de um apelido ao import: import \"...\" as modulo", ACAO_DEPOIS, DIAG_PROVAVEL},
};

static const size_t REGRAS_QTD = sizeof REGRAS / sizeof REGRAS[0];

/* ---------- o alvo ---------- */

static bool EhNomeChar(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static bool EhEspaco(char c)
{
    return c == ' ' || c == '\t';
}

/* Copia ate n bytes (cortando em NOME_MAX - 1) e fecha com NUL. */
static void CopiarTexto(char *destino, const char *origem, size_t n)
{
    if (n >= NOME_MAX) n = NOME_MAX - 1;
    memcpy(destino, origem, n);
    destino[n] = '\0';
}

static void PrepararAlvo(Alvo *a, const Diag *d, const Fonte *f)
{
    const char *t = f->texto;
    uint32_t off = d->off < f->tam ? d->off : (uint32_t)f->tam;

    a->f = f;
    a->d = d;
    a->t = t;
    a->causa = d->causa;
    a->off = off;
    a->ini_linha = off;
    while (a->ini_linha > 0 && t[a->ini_linha - 1] != '\n') a->ini_linha--;
    a->ini_texto = a->ini_linha;
    while (EhEspaco(t[a->ini_texto])) a->ini_texto++;
    a->fim_linha = off;
    while (t[a->fim_linha] != '\0' && t[a->fim_linha] != '\n') a->fim_linha++;
    if (a->fim_linha > a->ini_linha && t[a->fim_linha - 1] == '\r') a->fim_linha--;
    a->fim_token = off + DiagTamanhoDoToken(f, off);
    if ((unsigned char)t[off] >= 0x80) {
        while ((unsigned char)t[a->fim_token] >= 0x80) a->fim_token++; /* acento: a sequencia inteira de bytes nao ASCII */
    }
    DiagNomeEntreAspas(d->causa, 0, a->nome1);
    DiagNomeEntreAspas(d->causa, 1, a->nome2);
    CopiarTexto(a->token, t + off, a->fim_token > off ? a->fim_token - off : 0);
    CopiarTexto(a->recuo, t + a->ini_linha, a->ini_texto - a->ini_linha < RECUO_MAX - 1 ? a->ini_texto - a->ini_linha : 0);
}

/* ---------- texto da edicao ---------- */

typedef struct {
    char *saida;
    size_t cap, uso;
    bool estourou;
} Saida;

static void Escrever(Saida *s, const char *texto, size_t n)
{
    if (s->uso + n >= s->cap) {
        s->estourou = true;
        return;
    }
    memcpy(s->saida + s->uso, texto, n);
    s->uso += n;
    s->saida[s->uso] = '\0';
}

static const char *ValorDaMarca(const Alvo *a, char marca)
{
    switch (marca) {
    case '1': return a->nome1;
    case '2': return a->nome2;
    case 'T': return a->token;
    case 'R': return a->recuo;
    default: return NULL;
    }
}

/* Expande $1 $2 $T $R de 'modelo' em 'saida'; false se o resultado nao coube. */
static bool Expandir(const Alvo *a, const char *modelo, char *saida, size_t cap)
{
    Saida s = {saida, cap, 0, false};

    saida[0] = '\0';
    for (const char *p = modelo; *p != '\0'; p++) {
        const char *valor = p[0] == '$' ? ValorDaMarca(a, p[1]) : NULL;
        if (valor != NULL) {
            Escrever(&s, valor, strlen(valor));
            p++;
        } else {
            Escrever(&s, p, 1);
        }
    }
    return !s.estourou;
}

/* Monta a edicao [ini, fim) -> modelo expandido. */
static bool Poe(const Alvo *a, DiagEdicao *e, uint32_t ini, uint32_t fim, const char *modelo)
{
    if (fim < ini || fim > a->f->tam) return false;
    e->ini = ini;
    e->fim = fim;
    return Expandir(a, modelo, e->novo, sizeof e->novo);
}

/* ---------- procura na linha ---------- */

/* Offset da 1a ocorrencia de 'texto' em [de, fim_linha); NAO_ACHEI se nao ha. */
static uint32_t Procurar(const Alvo *a, uint32_t de, const char *texto)
{
    size_t n = strlen(texto);

    for (uint32_t i = de; i + n <= a->fim_linha; i++) {
        if (memcmp(a->t + i, texto, n) == 0) return i;
    }
    return NAO_ACHEI;
}

/* O token do erro e algo escrito (nao o fim da linha nem do arquivo). */
static bool TokenUtil(const Alvo *a)
{
    char c = a->t[a->off];
    return c != '\0' && c != '\n' && c != '\r';
}

/* Fim (exclusivo) do caminho a.b.c que comeca em 'de'. */
static uint32_t FimDoCaminho(const Alvo *a, uint32_t de)
{
    uint32_t i = de;

    while (EhNomeChar(a->t[i]) || (a->t[i] == '.' && EhNomeChar(a->t[i + 1]))) i++;
    return i;
}

/* Inicio da declaracao de topo (letra na coluna 1, fora os import) que contem a linha do erro. */
static uint32_t InicioDaDeclaracaoDeTopo(const Alvo *a)
{
    uint32_t p = a->ini_linha;

    for (;;) {
        char c = a->t[p];
        bool letra = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        if (letra && strncmp(a->t + p, "import", 6) != 0) return p;
        if (p == 0) return 0;
        p--;
        while (p > 0 && a->t[p - 1] != '\n') p--;
    }
}

/* ---------- as acoes ---------- */

static bool AcaoAntesDeclTopo(const Alvo *a, DiagEdicao *e, const char *texto)
{
    uint32_t p = InicioDaDeclaracaoDeTopo(a);

    return Poe(a, e, p, p, texto);
}

static bool AcaoRemoverAte(const Alvo *a, DiagEdicao *e, const char *texto, bool incluindo)
{
    uint32_t p = Procurar(a, a->off, texto);

    if (p == NAO_ACHEI) return false;
    return Poe(a, e, a->off, incluindo ? p + (uint32_t)strlen(texto) : p, "");
}

static bool AcaoRemoverAchado(const Alvo *a, DiagEdicao *e, const char *texto)
{
    uint32_t p = Procurar(a, a->off, texto);

    return p != NAO_ACHEI && Poe(a, e, p, p + (uint32_t)strlen(texto), "");
}

static bool AcaoRemoverDecl(const Alvo *a, DiagEdicao *e)
{
    uint32_t fim = a->off;
    uint32_t ini;

    while (fim > a->ini_texto && EhEspaco(a->t[fim - 1])) fim--;
    ini = fim;
    while (ini > a->ini_texto && EhNomeChar(a->t[ini - 1])) ini--;
    if (fim - ini != 3 || (memcmp(a->t + ini, "let", 3) != 0 && memcmp(a->t + ini, "var", 3) != 0)) return false;
    return Poe(a, e, ini, a->off, "");
}

static bool AcaoRemoverQualificador(const Alvo *a, DiagEdicao *e)
{
    uint32_t ini = a->off;

    if (ini == 0 || a->t[ini - 1] != '.') return false;
    ini--;
    while (ini > 0 && EhNomeChar(a->t[ini - 1])) ini--;
    return ini < a->off - 1 && Poe(a, e, ini, a->off, "");
}

static bool AcaoRemoverArgumento(const Alvo *a, DiagEdicao *e)
{
    uint32_t i = a->off;

    while (i > a->ini_linha && a->t[i - 1] != ',') i--;
    return i > a->ini_linha && Poe(a, e, i - 1, a->fim_token, "");
}

static bool AcaoRemoverLinha(const Alvo *a, DiagEdicao *e, const char *exige)
{
    return Procurar(a, a->ini_texto, exige) != NAO_ACHEI && Poe(a, e, a->ini_texto, a->fim_linha, "");
}

/* O nome do modulo da causa ("std:x nao existe; a std tem: a, b, c") mais parecido com x, a distancia 1..DISTANCIA_STD_MAX; false se nenhum. */
static bool AcaoTrocarStd(const Alvo *a, DiagEdicao *e)
{
    const char *nome = strstr(a->causa, "std:");
    const char *p = strstr(a->causa, "a std tem: ");
    size_t tam_nome = 0, tam_melhor = 0;
    const char *melhor = NULL;
    uint32_t menor = DISTANCIA_STD_MAX + 1;
    char novo[MODULO_MAX + 8];

    if (nome == NULL || p == NULL) return false;
    nome += 4;
    while (nome[tam_nome] != '\0' && nome[tam_nome] != ' ') tam_nome++;
    for (p += strlen("a std tem: "); *p != '\0';) {
        size_t n = 0;
        uint32_t dist;
        while (p[n] != '\0' && p[n] != ',') n++;
        dist = Parecido(nome, tam_nome, p, n);
        if (dist > 0 && dist < menor && n < MODULO_MAX) {
            menor = dist;
            melhor = p;
            tam_melhor = n;
        }
        p += n;
        while (*p == ',' || *p == ' ') p++;
    }
    if (melhor == NULL) return false;
    snprintf(novo, sizeof novo, "\"std:%.*s\"", (int)tam_melhor, melhor);
    return Poe(a, e, a->off, a->fim_token, novo);
}

static bool AcaoDepoisCaminho(const Alvo *a, DiagEdicao *e, const char *texto)
{
    uint32_t fim = FimDoCaminho(a, a->off);

    return fim > a->off && Poe(a, e, fim, fim, texto);
}

/* Os dois operandos (caminhos a.b.c) de uma conta "a OP b" cujo operador comeca no erro: [ini1, fim1) e [ini2, fim2). */
typedef struct { uint32_t ini1, fim1, ini2, fim2; } Operandos;

static bool LerOperandos(const Alvo *a, Operandos *o)
{
    uint32_t i = a->off;

    while (i > a->ini_texto && EhEspaco(a->t[i - 1])) i--;
    o->fim1 = i;
    while (i > a->ini_texto && (EhNomeChar(a->t[i - 1]) || a->t[i - 1] == '.')) i--;
    o->ini1 = i;
    i = a->fim_token;
    while (EhEspaco(a->t[i])) i++;
    o->ini2 = i;
    o->fim2 = FimDoCaminho(a, i);
    return o->fim1 > o->ini1 && o->fim2 > o->ini2;
}

/* "a + b" de str vira f"{a}{b}"; "a == b" de cstr vira (a as str) == (b as str). */
static bool AcaoReescreverConta(const Alvo *a, DiagEdicao *e, bool soma)
{
    Operandos o;
    char novo[DIAG_NOVO_MAX];
    int n;

    if (!LerOperandos(a, &o)) return false;
    if (soma) {
        n = snprintf(novo, sizeof novo, "f\"{%.*s}{%.*s}\"", (int)(o.fim1 - o.ini1), a->t + o.ini1, (int)(o.fim2 - o.ini2), a->t + o.ini2);
    } else {
        n = snprintf(novo, sizeof novo, "(%.*s as str) == (%.*s as str)", (int)(o.fim1 - o.ini1), a->t + o.ini1, (int)(o.fim2 - o.ini2), a->t + o.ini2);
    }
    return n < (int)sizeof novo && Poe(a, e, o.ini1, o.fim2, novo);
}

/* Poe o membro novo no fim do enum de uma linha so ("enum Cor { verde }"), a declaracao que vem como relacionado do tipo; $2 e o nome do membro. */
static bool AcaoMembroNoEnum(const Alvo *a, DiagEdicao *e)
{
    for (uint8_t k = 0; k < a->d->rel.n; k++) {
        uint32_t i = a->d->rel.itens[k].off;
        uint32_t ini = i;
        if (a->d->rel.itens[k].papel != DIAG_REL_TIPO || i >= a->f->tam) continue;
        while (a->t[i] != '\0' && a->t[i] != '\n') i++;
        while (i > ini && a->t[i - 1] != '}') i--;
        if (i <= ini) continue;
        i--;
        while (i > ini && EhEspaco(a->t[i - 1])) i--;
        return Poe(a, e, i, i, a->t[i - 1] == '{' ? "$2" : ", $2");
    }
    return false;
}

/* Poe o campo que falta no fim da linha de abertura da struct (o relacionado de tipo): "\n    campo: int = 0"; $2 e o nome do campo. */
static bool AcaoCampoNaStruct(const Alvo *a, DiagEdicao *e)
{
    for (uint8_t k = 0; k < a->d->rel.n; k++) {
        uint32_t i = a->d->rel.itens[k].off;
        if (a->d->rel.itens[k].papel != DIAG_REL_TIPO || i >= a->f->tam) continue;
        while (a->t[i] != '\0' && a->t[i] != '\n') i++;
        if (i > 0 && a->t[i - 1] == '\r') i--;
        if (i == 0 || a->t[i - 1] != '{') continue;
        return Poe(a, e, i, i, "\n    $2: int = 0");
    }
    return false;
}

/* for (var i = 0; i < n; i++) { vira for i in 0..<n { (so a forma de contador simples; o erro cai no ';', por isso parte do inicio da linha). */
static bool AcaoForDeC(const Alvo *a, DiagEdicao *e)
{
    uint32_t abre = Procurar(a, a->ini_texto, "(");
    uint32_t fecha = a->fim_linha;
    char inner[96], var[NOME_MAX], lo[NOME_MAX], hi[NOME_MAX], novo[DIAG_NOVO_MAX];

    while (fecha > a->ini_texto && a->t[fecha - 1] != ')') fecha--;
    if (abre == NAO_ACHEI || fecha <= abre + 1 || fecha - abre - 2 >= sizeof inner) return false;
    snprintf(inner, sizeof inner, "%.*s", (int)(fecha - abre - 2), a->t + abre + 1);
    if (sscanf(inner, " %*[a-z] %63[A-Za-z0-9_] = %63[^;]; %*[A-Za-z0-9_] < %63[^;];", var, lo, hi) != 3) return false;
    if (hi[0] == '=' || strstr(inner, "++") == NULL) return false;
    if (snprintf(novo, sizeof novo, "for %s in %s..<%s", var, lo, hi) >= (int)sizeof novo) return false;
    return Poe(a, e, a->ini_texto, fecha, novo);
}

/* let a = [3]int{1, 2} vira let a: [3]int = {1, 2} (o tipo vai para a declaracao). */
static bool AcaoAnotarTipo(const Alvo *a, DiagEdicao *e)
{
    uint32_t brace = Procurar(a, a->off, "{");
    uint32_t i = a->off;
    uint32_t ini;
    char novo[DIAG_NOVO_MAX];

    if (a->t[a->off] != '[' || a->t[a->off + 1] < '0' || a->t[a->off + 1] > '9' || brace == NAO_ACHEI) return false;
    while (i > a->ini_texto && EhEspaco(a->t[i - 1])) i--;
    if (i == a->ini_texto || a->t[i - 1] != '=') return false;
    i--;
    while (i > a->ini_texto && EhEspaco(a->t[i - 1])) i--;
    ini = i;
    while (ini > a->ini_texto && EhNomeChar(a->t[ini - 1])) ini--;
    if (ini == i) return false;
    if (snprintf(novo, sizeof novo, "%.*s: %.*s = ", (int)(i - ini), a->t + ini, (int)(brace - a->off), a->t + a->off) >= (int)sizeof novo) return false;
    return Poe(a, e, ini, brace, novo);
}

/* Troca o nome da chamada que abre logo antes do erro ("new(" + erro). */
static bool AcaoTrocarChamada(const Alvo *a, DiagEdicao *e, const char *novo_nome)
{
    uint32_t fim = a->off;
    uint32_t ini;

    if (fim == 0 || a->t[fim - 1] != '(') return false;
    fim--;
    ini = fim;
    while (ini > 0 && EhNomeChar(a->t[ini - 1])) ini--;
    return ini < fim && Poe(a, e, ini, fim, novo_nome);
}

/* ++nome e --nome (so os que comecam no erro) viram nome += 1 e nome -= 1. */
static bool AcaoIncremento(const Alvo *a, DiagEdicao *e)
{
    char op = a->t[a->off];
    uint32_t fim = FimDoCaminho(a, a->off + 2);
    char novo[DIAG_NOVO_MAX];

    if ((op != '+' && op != '-') || a->t[a->off + 1] != op || fim == a->off + 2) return false;
    snprintf(novo, sizeof novo, "%.*s %c= 1", (int)(fim - a->off - 2), a->t + a->off + 2, op);
    return Poe(a, e, a->off, fim, novo);
}

/* for (x in xs) { vira for x in xs { (o for de C com ';' nao se converte). */
static bool AcaoTirarParentesesFor(const Alvo *a, DiagEdicao *e)
{
    uint32_t abre = Procurar(a, a->off, "(");
    uint32_t fecha = a->fim_linha;
    char novo[DIAG_NOVO_MAX];

    while (fecha > a->off && a->t[fecha - 1] != ')') fecha--;
    if (abre == NAO_ACHEI || fecha <= abre + 1) return false;
    if (snprintf(novo, sizeof novo, "for %.*s", (int)(fecha - abre - 2), a->t + abre + 1) >= (int)sizeof novo) return false;
    return strchr(novo, ';') == NULL && Poe(a, e, a->off, fecha, novo);
}

/* "for i, x in faixa" perde "i, " e "for &x in faixa" perde o '&'. */
static bool AcaoTirarIndiceFor(const Alvo *a, DiagEdicao *e)
{
    uint32_t p = a->ini_texto + 4;
    uint32_t fim = p;

    if (strncmp(a->t + a->ini_texto, "for ", 4) != 0) return false;
    if (a->t[p] == '&') return Poe(a, e, p, p + 1, "");
    while (fim < a->off && a->t[fim] != ',') fim++;
    if (a->t[fim] != ',') return false;
    fim++;
    while (EhEspaco(a->t[fim])) fim++;
    return Poe(a, e, p, fim, "");
}

/* Poe "pub " no inicio da linha do 1o relacionado de declaracao, tipo ou assinatura (a coisa que o erro diz nao ser pub). */
static bool AcaoPubNaDeclaracao(const Alvo *a, DiagEdicao *e)
{
    for (uint8_t k = 0; k < a->d->rel.n; k++) {
        const DiagRelacionado *r = &a->d->rel.itens[k];
        uint32_t ini = r->off;
        if (r->papel > DIAG_REL_ASSINATURA || ini >= a->f->tam) continue;
        while (ini > 0 && a->t[ini - 1] != '\n') ini--;
        while (EhEspaco(a->t[ini])) ini++;
        return Poe(a, e, ini, ini, "pub ");
    }
    return false;
}

/* Calcula a edicao da regra; false se a regra nao serve a esta linha. Despacho por acao (cada ramo chama uma funcao). */
static bool CalcularEdicao(const Alvo *a, const RegraCorrecao *r, DiagEdicao *e)
{
    switch (r->acao) {
    case ACAO_ANTES: return Poe(a, e, a->off, a->off, r->texto);
    case ACAO_DEPOIS: return TokenUtil(a) && Poe(a, e, a->fim_token, a->fim_token, r->texto);
    case ACAO_DEPOIS_CAMINHO: return AcaoDepoisCaminho(a, e, r->texto);
    case ACAO_FIM_LINHA: return Poe(a, e, a->fim_linha, a->fim_linha, r->texto);
    case ACAO_INICIO_LINHA: return Poe(a, e, a->ini_texto, a->ini_texto, r->texto);
    case ACAO_ANTES_DECL_TOPO: return AcaoAntesDeclTopo(a, e, r->texto);
    case ACAO_TROCAR: return TokenUtil(a) && Poe(a, e, a->off, a->fim_token, r->texto);
    case ACAO_REMOVER_TOKEN: return TokenUtil(a) && Poe(a, e, a->off, a->fim_token, "");
    case ACAO_REMOVER_LINHA: return AcaoRemoverLinha(a, e, r->texto);
    case ACAO_REMOVER_ATE: return AcaoRemoverAte(a, e, r->texto, false);
    case ACAO_REMOVER_ATE_INCLUINDO: return AcaoRemoverAte(a, e, r->texto, true);
    case ACAO_REMOVER_ACHADO: return AcaoRemoverAchado(a, e, r->texto);
    case ACAO_REMOVER_DECL: return AcaoRemoverDecl(a, e);
    case ACAO_REMOVER_QUALIFICADOR: return AcaoRemoverQualificador(a, e);
    case ACAO_REMOVER_ARGUMENTO: return AcaoRemoverArgumento(a, e);
    case ACAO_REMOVER_DO_INICIO: return a->off > a->ini_texto && Poe(a, e, a->ini_texto, a->off, "");
    case ACAO_TROCAR_STD: return AcaoTrocarStd(a, e);
    case ACAO_REMOVER_RESTO: return a->fim_linha > a->fim_token && Poe(a, e, a->fim_token, a->fim_linha, "");
    case ACAO_TROCAR_CHAMADA: return AcaoTrocarChamada(a, e, r->texto);
    case ACAO_REESCREVER_SOMA: return AcaoReescreverConta(a, e, true);
    case ACAO_COMPARAR_CSTR: return AcaoReescreverConta(a, e, false);
    case ACAO_MEMBRO_NO_ENUM: return AcaoMembroNoEnum(a, e);
    case ACAO_FOR_DE_C: return AcaoForDeC(a, e);
    case ACAO_CAMPO_NA_STRUCT: return AcaoCampoNaStruct(a, e);
    case ACAO_ANOTAR_TIPO: return AcaoAnotarTipo(a, e);
    case ACAO_PUB_NA_DECLARACAO: return AcaoPubNaDeclaracao(a, e);
    case ACAO_INCREMENTO: return AcaoIncremento(a, e);
    case ACAO_TIRAR_PARENTESES_FOR: return AcaoTirarParentesesFor(a, e);
    case ACAO_TIRAR_INDICE_FOR: return AcaoTirarIndiceFor(a, e);
    default: return false;
    }
}

/* ---------- a busca e a saida ---------- */

/* A 1a regra do codigo cuja causa casa e cuja edicao cabe nesta linha; NULL se nenhuma (ou se uma regra NENHUMA casou antes). */
static const RegraCorrecao *EscolherRegra(const Alvo *a, const Diag *d, DiagEdicao *e)
{
    for (size_t i = 0; i < REGRAS_QTD; i++) {
        const RegraCorrecao *r = &REGRAS[i];
        if (r->codigo != d->codigo || strstr(d->causa, r->trecho_da_causa) == NULL) continue;
        if (r->acao == ACAO_NENHUMA) return NULL;
        if (CalcularEdicao(a, r, e)) return r;
    }
    return NULL;
}

/* Sugestao so_dica que ja existe (texto da analise): ganha a edicao e sobe para provavel, no maximo (nunca certa: a analise a quis so dica). */
static bool AcrescentarEdicao(DiagSugestoes *s, const RegraCorrecao *r, const DiagEdicao *e)
{
    DiagSugestao *principal = &s->itens[0];

    if (e->fim - e->ini > DIAG_ANTIGO_MAX) return false;
    principal->edicoes[0] = *e;
    principal->nedicoes = 1;
    principal->confianca = (uint8_t)(r->confianca == DIAG_CERTA ? DIAG_PROVAVEL : r->confianca);
    return true;
}

/* Diagnostico sem sugestao: abre a da regra (descricao com as marcas expandidas) e poe a edicao. */
static bool AbrirSugestao(DiagSugestoes *s, const Alvo *a, const RegraCorrecao *r, const DiagEdicao *e)
{
    char descricao[DIAG_DESCRICAO_MAX];

    if (!Expandir(a, r->descricao, descricao, sizeof descricao)) return false;
    return DiagSugerir(s, r->confianca, "%s", descricao) && DiagEditar(s, e->ini, e->fim, e->novo);
}

bool DiagCorrigirPadrao(const Diag *d, const Fonte *f, DiagSugestoes *saida)
{
    bool tem = d->sug.n > 0;
    DiagEdicao e;
    const RegraCorrecao *regra;
    Alvo a;

    if (tem && (d->sug.itens[0].nedicoes > 0 || d->sug.itens[0].confianca != DIAG_SO_DICA)) return false;
    PrepararAlvo(&a, d, f);
    regra = EscolherRegra(&a, d, &e);
    if (regra == NULL) return false;
    *saida = d->sug;
    return tem ? AcrescentarEdicao(saida, regra, &e) : AbrirSugestao(saida, &a, regra, &e);
}
