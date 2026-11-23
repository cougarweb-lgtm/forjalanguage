/* idiomas.c - os textos dos idiomas de outras linguagens (E0110) e o relato comum. Veja idiomas.h.
 * Marcas dos textos (diag_forma.h): $O linha e coluna do arquivo, $L a linha do .fj, $T o trecho a partir do erro. Os textos nao citam nome inventado.
 * A forma_certa comeca pela acao (o --resumo corta no primeiro ponto). O trecho_da_causa de cada linha aparece na causa dela e em nenhuma outra.
 * Versao: 0.2.3
 * Changelog: 0.2.3 - O0 P13: texto do IDI_XOR_CIRCUNFLEXO ('a ^ b' -> 'a ~ b').
 *            0.2.2 - IA1-L3c: a forma_certa do import sem prefixo cita std:file no lugar de std:arquivo.
 *            0.2.1 - IA1-L2 (conferencia do main): comentario LIMITE: em IdiomaTrocaSomar (5 parametros); codigo igual.
 *            0.2.0 - IA1-L2: textos dos 10 idiomas semanticos e IdiomaTrocaSomar/IdiomaSugerir (sugestao aplicavel com ate 2 edicoes).
 *            0.1.0 - IA1-L1: primeira versao.
 * Verificacao: construir.bat && python ..\tests\ia_first\verificar_e0110_l1.py --forja compiler\forja.exe && python ..\tests\ia_first\verificar_m_d4.py --forja compiler\forja.exe
 */
#include "common/idiomas.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

const Idioma IDIOMAS[IDI_QTD] = {
    [IDI_PONTO_E_VIRGULA] = {IDI_PONTO_E_VIRGULA,
        "forma de outra linguagem: '%s' termina o comando em C, Java e JS; na Forja um comando por linha, sem ';'",
        {E0110, "na Forja um comando por linha, sem ';'",
         "Na $O a linha '$L' termina o comando com ';', que e de C, Java e JS: na Forja cada comando ocupa uma linha e nao leva ';'.",
         "Tire o ';' da linha '$L' (um comando por linha)."}},
    [IDI_FOR_ESTILO_C] = {IDI_FOR_ESTILO_C,
        "forma de outra linguagem: '%s' e o for de C, Java e JS (inicio; condicao; passo); na Forja e for i in 0..<n",
        {E0110, "na Forja e for i in 0..<n",
         "Na $O a linha '$L' e um for no estilo de C, Java ou JS; a Forja percorre uma faixa com for nome in a..<b (ou a..=b) e uma colecao com for item in lista.",
         "Troque o cabecalho da linha '$L' por for i in 0..<n { (n exclusivo; a..=b inclui o fim)."}},
    [IDI_E_LOGICO] = {IDI_E_LOGICO,
        "forma de outra linguagem: '%s' e o E logico de C, Java, JS e Go; na Forja e 'and'",
        {E0110, "na Forja e 'and'",
         "Na $O a linha '$L' usa '&&', o E logico de outras linguagens; na Forja os operadores logicos sao palavras (and, or, not) e '&' e so bit a bit.",
         "Troque '&&' por 'and' na linha '$L'."}},
    [IDI_OU_LOGICO] = {IDI_OU_LOGICO,
        "forma de outra linguagem: '%s' e o OU logico de C, Java, JS e Go; na Forja e 'or'",
        {E0110, "na Forja e 'or'",
         "Na $O a linha '$L' usa '||', o OU logico de outras linguagens; na Forja os operadores logicos sao palavras (and, or, not) e '|' e so bit a bit.",
         "Troque '||' por 'or' na linha '$L'."}},
    [IDI_NEGACAO] = {IDI_NEGACAO,
        "forma de outra linguagem: '%s' e a negacao logica de C, Java, JS e Go; na Forja e 'not'",
        {E0110, "na Forja e 'not'",
         "Na $O a linha '$L' nega com '!', a negacao de outras linguagens; na Forja a negacao logica e a palavra not ('!' so aparece no tipo T ! E).",
         "Troque '!' por 'not' na linha '$L'."}},
    [IDI_INCREMENTO] = {IDI_INCREMENTO,
        "forma de outra linguagem: '%s' soma ou subtrai 1 em C, Java e JS; na Forja e 'i += 1' e 'i -= 1'",
        {E0110, "na Forja e 'i += 1' e 'i -= 1'",
         "Na $O a linha '$L' soma ou subtrai 1 com ++ ou --, que a Forja nao tem; a conta e um comando de atribuicao.",
         "Troque ++ por += 1 (e -- por -= 1) na linha '$L'; a variavel precisa ser var."}},
    [IDI_TERNARIO] = {IDI_TERNARIO,
        "forma de outra linguagem: 'c ? a : b' e o operador ternario de C, Java e JS; na Forja e um if com else",
        {E0110, "na Forja e um if com else",
         "Na $O a linha '$L' usa o operador ternario; a Forja nao tem expressao condicional, so o comando if/else.",
         "Declare a variavel com var, escolha o valor num if com else que atribui, e use a variavel no lugar do ternario da linha '$L'."}},
    [IDI_TRY_RUST] = {IDI_TRY_RUST,
        "forma de outra linguagem: 'x?' repassa o erro em Rust; na Forja e 'try x'",
        {E0110, "na Forja e 'try x'",
         "Na $O a linha '$L' repassa o erro com '?' depois da expressao, como em Rust; na Forja o repasse e a palavra try antes da chamada.",
         "Tire o '?' e escreva try antes da expressao na linha '$L' (o proc precisa devolver T ! E ou ! E do mesmo E)."}},
    [IDI_BLOCO_NA_LINHA] = {IDI_BLOCO_NA_LINHA,
        "forma de outra linguagem: '%s' com comando na mesma linha; na Forja o corpo vai nas linhas de baixo",
        {E0110, "na Forja o corpo vai nas linhas de baixo",
         "Na $O a linha '$L' escreve o corpo do bloco na mesma linha do '{'; na Forja o corpo (mesmo de um comando so) fica nas linhas de baixo e o '}' tem linha propria.",
         "Quebre a linha '$L' depois do '{' e antes do '}': o corpo vai em linhas proprias."}},
    [IDI_PALAVRA_DE_PROC] = {IDI_PALAVRA_DE_PROC,
        "forma de outra linguagem: '%s' declara funcao em Rust, Go, Python e JS; na Forja e 'proc'",
        {E0110, "na Forja e 'proc'",
         "Na $O a linha '$L' declara a funcao com uma palavra de outra linguagem; na Forja toda funcao e declarada com proc.",
         "Troque a palavra do inicio da linha '$L' por proc."}},
    [IDI_ATRIBUICAO_CURTA] = {IDI_ATRIBUICAO_CURTA,
        "forma de outra linguagem: '%s' declara variavel em Go; na Forja e 'let nome = valor'",
        {E0110, "na Forja e 'let nome = valor'",
         "Na $O a linha '$L' declara a variavel com ':=', que e de Go; na Forja a declaracao leva let (imutavel) ou var (mutavel), e o tipo e inferido.",
         "Escreva let antes do nome e troque ':=' por '=' na linha '$L' (var se o valor vai mudar)."}},
    [IDI_ELIF] = {IDI_ELIF,
        "forma de outra linguagem: '%s' e o senao-se de Python e Ruby; na Forja e 'else if'",
        {E0110, "na Forja e 'else if'",
         "Na $O a linha '$L' encadeia o senao-se com uma palavra so; na Forja sao duas palavras: else if.",
         "Troque a palavra por else if na linha '$L'."}},
    [IDI_COMENTARIO_DE_BLOCO] = {IDI_COMENTARIO_DE_BLOCO,
        "forma de outra linguagem: '%s' abre comentario de bloco em C, Java e JS; na Forja o comentario e '//' ate o fim da linha",
        {E0110, "na Forja o comentario e '//'",
         "Na $O a linha '$L' abre um comentario de bloco; a Forja so tem o comentario de linha, que comeca em // e vai ate o fim da linha.",
         "Escreva // no inicio de cada linha do comentario a partir da linha '$L' e tire o fim do bloco."}},
    [IDI_TRY_CATCH] = {IDI_TRY_CATCH,
        "forma de outra linguagem: '%s' trata excecao; na Forja o erro e um valor e o tratamento e o switch",
        {E0110, "na Forja o erro e um valor",
         "Na $O a linha '$L' abre um bloco try com catch, que e excecao; a Forja nao tem excecao: o proc devolve T ! E, e quem chama usa try (repassa), or_else (padrao) ou switch.",
         "Chame o proc em uma linha e trate o resultado com switch r in f() { case T: ... case E: ... } (ou try / or_else) no lugar do bloco da linha '$L'."}},
    [IDI_MAIN_COM_RETORNO] = {IDI_MAIN_COM_RETORNO,
        "forma de outra linguagem: '%s' devolve o codigo de saida em C e Go; na Forja 'proc main()' nao devolve valor e o codigo de saida e os.exit(n)",
        {E0110, "devolve o codigo de saida em C",
         "Na $O a linha '$L' faz o main devolver um inteiro; na Forja proc main() nao devolve valor e o codigo de saida se escolhe com exit(n) de std:os.",
         "Tire o '->' e o tipo da linha '$L', acrescente import \"std:os\" e termine com os.exit(n) onde o codigo de saida for n (0 a 255)."}},
    [IDI_NULO] = {IDI_NULO,
        "forma de outra linguagem: '%s' e o valor nulo de C, Go, Java, JS e Python; na Forja e 'none'",
        {E0110, "na Forja e 'none'",
         "Na $O a linha '$L' usa '$1', o valor nulo de outras linguagens; na Forja a ausencia de valor e none, e o tipo precisa aceita-lo (?T ou ?^T).",
         "Troque '$1' por none na linha '$L' (e declare o tipo como ?T ou ?^T, ex.: let p: ?^int = none)."}},
    [IDI_BOOL_ALHEIO] = {IDI_BOOL_ALHEIO,
        "forma de outra linguagem: '%s' e o booleano de Python, C e outras; na Forja e 'true' ou 'false' em minusculas",
        {E0110, "em minusculas",
         "Na $O a linha '$L' escreve '$1' com maiuscula; na Forja os valores booleanos sao true e false, sempre em minusculas.",
         "Escreva true ou false em minusculas no lugar de '$1' na linha '$L'."}},
    [IDI_RANGE] = {IDI_RANGE,
        "forma de outra linguagem: '%s' gera a faixa em Python; na Forja e 'a..<b'",
        {E0110, "na Forja e 'a..<b'",
         "Na $O a linha '$L' usa range(...), a faixa de Python; a Forja escreve a faixa com ..< (fim exclusivo) ou ..= (fim incluso) e a percorre com for i in 0..<n.",
         "Troque range(n) por 0..<n e range(a, b) por a..<b na linha '$L' (a..=b inclui o fim)."}},
    [IDI_TIPO_TEXTO] = {IDI_TIPO_TEXTO,
        "forma de outra linguagem: '%s' e o tipo texto de Java, C#, Go, Rust e C++; na Forja e 'str'",
        {E0110, "na Forja e 'str'",
         "Na $O a linha '$L' declara o tipo texto como '$1'; na Forja o tipo do texto e str (cstr so para foreign).",
         "Troque o tipo '$1' por str na linha '$L'."}},
    [IDI_ARRAY_COLCHETE] = {IDI_ARRAY_COLCHETE,
        "forma de outra linguagem: '%s' abre o literal de array em Python, JS e Rust; na Forja e '{a, b}' com o tipo declarado",
        {E0110, "na Forja e '{a, b}'",
         "Na $O a linha '$L' escreve o literal de array entre colchetes; na Forja o literal usa chaves, {1, 2}, e o tipo vem do contexto: let a: [2]int = {1, 2}.",
         "Troque [ ] por { } e declare o tipo na linha '$L' (let a: [N]T = {...}, com N o numero exato de elementos)."}},
    [IDI_IMPRIME] = {IDI_IMPRIME,
        "forma de outra linguagem: '%s' imprime em C, JS, Go e Java; na Forja e print(...)",
        {E0110, "na Forja e print(...)",
         "Na $O a linha '$L' imprime com '$1'; na Forja imprime-se com print(valor), uma linha por chamada, e texto com valores se monta num f\"...{x}...\".",
         "Troque '$1' por print na linha '$L' (print(f\"valor {x}\") junta texto e valores)."}},
    [IDI_TAMANHO] = {IDI_TAMANHO,
        "forma de outra linguagem: '%s' le o tamanho como metodo ou campo em Rust, JS, Java e C++; na Forja e len(x)",
        {E0110, "na Forja e len(x)",
         "Na $O a linha '$L' le o tamanho com '$1'; na Forja array, lista e texto nao tem metodo nem campo: o tamanho vem do proc len(x).",
         "Troque pelo proc len(x) na linha '$L' (len(lista), len(array), len(texto))."}},
    [IDI_ACRESCENTAR] = {IDI_ACRESCENTAR,
        "forma de outra linguagem: '%s' acrescenta ao fim como metodo em Python, JS, Rust e Java; na Forja e append(&x, item = v)",
        {E0110, "na Forja e append(&x, item = v)",
         "Na $O a linha '$L' acrescenta com '$1' como metodo da lista; na Forja lista nao tem metodo: o proc append recebe o endereco da lista e o valor em item.",
         "Troque pelo proc append(&x, item = v) na linha '$L' (a lista precisa ser var)."}},
    [IDI_IMPORT_SEM_STD] = {IDI_IMPORT_SEM_STD,
        "forma de outra linguagem: import \"%s\" nomeia a biblioteca padrao como Python ou Go; na Forja ela leva o prefixo std:",
        {E0110, "leva o prefixo std:",
         "Na $O a linha '$L' importa a biblioteca padrao pelo nome puro; na Forja um import sem prefixo busca um arquivo do projeto, e a biblioteca padrao e import \"std:nome\".",
         "Escreva o caminho com o prefixo na linha '$L' (import \"std:os\", \"std:mem\" ou \"std:file\")."}},
    [IDI_THROW] = {IDI_THROW,
        "forma de outra linguagem: '%s' lanca excecao em Java, JS, C++ e Python; na Forja o erro volta com return",
        {E0110, "o erro volta com return",
         "Na $O a linha '$L' lanca o erro com '$1'; a Forja nao tem excecao: o proc devolve T ! E e o erro sai por return do membro do enum de erro.",
         "Troque '$1' por return na linha '$L' (o proc precisa devolver T ! E ou ! E, e o valor e um membro do enum de erro, ex.: return Erro.falhou)."}},
    [IDI_XOR_CIRCUNFLEXO] = {IDI_XOR_CIRCUNFLEXO,
        "forma de outra linguagem: '%s' entre dois valores e o xor de C, Java e JS; na Forja o xor binario e 'a ~ b'",
        {E0110, "o xor binario e 'a ~ b'",
         "Na $O a linha '$L' usa '^' como xor; na Forja '^' depois de um valor e a desreferencia de ponteiro (p^) e o xor e o '~' binario (a ~ b, so em inteiro sem sinal).",
         "Troque '^' por '~' na linha '$L' (a ~ b e o xor; os operandos precisam ser inteiros sem sinal do mesmo tipo)."}},
};

bool IdiomaRelatar(Diag *d, IdiomaId id, uint32_t off, const char *achado)
{
    return DiagRelatar(d, E0110, off, IDIOMAS[id].causa, achado);
}

/* LIMITE: parametros 5 (a troca acumulada + o texto + a faixa [ini, fim) + o texto novo: uma troca so; agrupar criaria um tipo de uso unico). */
void IdiomaTrocaSomar(IdiomaTroca *t, const char *texto, uint32_t ini, uint32_t fim, const char *novo)
{
    bool uma_linha = fim >= ini && memchr(texto + ini, '\n', fim - ini) == NULL;

    if (t->falhou) return;
    if (t->n >= DIAG_EDICOES_MAX || !uma_linha || fim - ini > DIAG_ANTIGO_MAX || strlen(novo) >= DIAG_NOVO_MAX) {
        t->falhou = true;
        return;
    }
    t->itens[t->n++] = (DiagEdicao){ini, fim, ""};
    snprintf(t->itens[t->n - 1].novo, sizeof t->itens[0].novo, "%s", novo);
}

void IdiomaSugerir(Diag *d, DiagConfianca confianca, const char *descricao, const IdiomaTroca *t)
{
    bool aplica = t != NULL && !t->falhou && t->n > 0 && confianca != DIAG_SO_DICA;

    DiagSugerir(&d->sug, aplica ? confianca : DIAG_SO_DICA, "%s", descricao);
    if (!aplica) return;
    for (uint32_t i = 0; i < t->n; i++) DiagEditar(&d->sug, t->itens[i].ini, t->itens[i].fim, t->itens[i].novo);
}

static bool PedeEspacoAntes(char antes)
{
    return isalnum((unsigned char)antes) || antes == '_' || antes == ')' || antes == ']' || antes == '"';
}

/* LIMITE: parametros 5 (Diag e Fonte sao os dois donos do texto; [ini, fim) e a palavra descrevem uma troca so; agrupar criaria um tipo de uso unico). */
void IdiomaSugerirPalavra(Diag *d, const Fonte *f, uint32_t ini, uint32_t fim, const char *palavra)
{
    char novo[DIAG_NOVO_MAX];
    char antes = ini > 0 ? f->texto[ini - 1] : '\0', depois = f->texto[fim];
    bool espaco_depois = depois != ' ' && depois != '\t' && depois != '\r' && depois != '\n' && depois != '\0';

    snprintf(novo, sizeof novo, "%s%s%s", PedeEspacoAntes(antes) ? " " : "", palavra, espaco_depois ? " " : "");
    DiagSugerir(&d->sug, DIAG_CERTA, "troque pelo '%s' da Forja", palavra);
    DiagEditar(&d->sug, ini, fim, novo);
}
