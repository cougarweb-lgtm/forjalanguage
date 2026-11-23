/* diag.h - diagnostico da linguagem: catalogo de codigos, relato do 1o erro e saida JSON (ADR-30).
 * Versao: 0.63.0
 * Changelog: 0.63.0 - Linguagem v1.13 L1a: E1008 (conflito de caminho arquivo x pasta; o mesmo arquivo entraria duas vezes) no fim do enum, depois do E0913.
 *            0.62.0 - M6e P93g: E0913 (ponteiro para dentro de lista/map usado depois da mutacao da colecao) no fim do enum, depois do E0912.
 *            0.61.0 - M6c P85: E0912 (clausula de posse de handle no foreign) no fim do enum, depois do E0342.
 *0.60.0 - M6c P83: E0342 (c_long/c_ulong fora da fronteira com o C) no fim do enum, depois do X0114.
 *            0.59.0 - M5c-b P61: X0113 (membro com TLS numa ligacao ELF crua, sem musl) e X0114 (GOTTPOFF com forma que o relaxamento para LE nao reconhece) no fim do enum, depois do X0112.
 *            0.58.0 - M5b P41: X0112 (ELF --pie: ABS32/ABS32S ou ADDR64 em .text) no fim do enum, depois do X0111.
 *            0.57.0 - M5b P36: X0111 (TLS no ELF: TPOFF32 e GOTTPOFF de membro estatico, ate o M5c-b) no fim do enum, depois do X0110.
 *            0.56.0 - M5b P35: X0110 (relocacao de membro ELF fora do censo, com o tipo e o membro) no fim do enum, depois do E0339.
 *            0.55.0 - M5 P48 (S18): E0339 (tipo nao copiavel: Mutex, Cond, Atomic* de std:sync/std:atomic copiados, passados por valor, devolvidos ou postos em union/map/[dynamic]) no fim do enum, depois do E1007.
 *            0.54.0 - B12: E1007 (#load: arquivo que nao abre ou acima de 16 MiB) no fim do enum, depois do F0001.
 *            0.53.0 - F1 P2: F0001 (formato: check --canonico, fora da forma canonica) no fim do enum, depois do E0338.
 *            0.52.0 - S2b: E0338 (rng de shuffle e choice que nao e ^rand.Rng) no fim do enum, depois do E0337.
 *            0.51.0 - K08b: E0337 (ordenacao: elemento nao ordenavel em binary_search, chave que nao e campo ou campo nao ordenavel em sort_by_key) no fim do enum, depois do E0336.
 *            0.50.0 - B01a: E0336 (tipo embutido Allocator fora do parametro 'alloc: Allocator') no fim do enum, depois do E0110.
 *            0.49.0 - IA1-L3d1: Diag.trocas (onde o forja migrar junta as formas antigas da tabela de renomeados; NULL fora do migrar); nenhum codigo novo no enum.
 *            0.48.0 - IA1-L1: E0110 (forma de outra linguagem: ';', '&&', '||', '!x', 'i++', 'c ? a : b', 'x?', 'fn', ':=', 'elif', 'barra-asterisco','try/catch', bloco na linha, 'main -> int') no fim do enum, depois do R0015.
 *            0.47.0 - IA1-b14: relacionados por ocorrencia (DiagRelPapel, DiagRelacionado, DiagRelacionados, Diag.rel, DiagRelacionar): o campo "relacionados" do pacote do erro deixa de ser sempre []; nenhum codigo novo no enum.
 *            0.46.0 - IA1-b10: R0013 (limite de tempo), R0014 (limite de memoria) e R0015 (limite de saida) no fim do enum, depois do E1006; DiagCodigoForma (a forma_certa do catalogo, para o R-code enriquecido pelo driver).
 *            0.45.0 - IA1-b09: DiagCodigoExplicacao (catalogo para o forja explicar --lista); nenhum codigo novo no enum.
 *            0.44.0 - IA1-b07: forma por ocorrencia (Diag.explicacao e Diag.forma, DiagForma, DiagDescreverOnde): o diagnostico que tem forma propria imprime a explicacao e a forma_certa dele no lugar das do catalogo; nenhum codigo novo no enum.
 *            0.43.0 - IA1-b04: sugestoes por ocorrencia (DiagSugestoes, DiagSugerir, DiagEditar; Diag.sug) e o pacote do erro (trecho, correcao, confianca, sugestao, relacionados, spec, id) em diag_pacote.c; nenhum codigo novo no enum.
 *            0.42.0 - IA1-b03: DiagImprimirJsonFerramenta (B0004 com "ferramenta": {"nome","saida"}); nenhum codigo novo no enum.
 *            0.41.0 - IA1-b02: DiagModoResumo (--resumo) e FORJA_LOG_DIAG; nenhum codigo novo no enum.
 *            0.40.0 - IA1-b01: E1006 (import cujo ultimo segmento e palavra reservada, sem 'as') no fim do enum, depois do R0012.
 *            0.39.0 - M5 P26b: R0012 (exit com codigo fora de 0..255, std:proc) no fim do enum, depois do X0109.
 *            0.38.0- M3b P23y-2: X0109 (limite de DLLs ou de nomes importados do ligador nativo) no fim do enum, depois do X0107.
 *            0.37.0 - M3b P23f: X0107 (secao .tls de membro de biblioteca estatica) no fim do enum, depois do X0108.
 *            0.36.0 - M3b P23e: X0108 (dependencia implicita do .drectve nao encontrada) no fim do enum, depois do X0106.
 *            0.35.0 - M3b P23d: X0106 (simbolo exigido definido em duas bibliotecas estaticas) no fim do enum, depois do B0008.
 *            0.34.0 - M3 P22-O2: B0006 (dependencia nao declarada entre produtos) e B0008 (projeto em build por outro processo) no fim do enum, depois do B0007.
 *            0.33.0 -M3 P22-O1b: B0005 (forja.proj invalido) e B0007 (ciclo de dependencia entre produtos) no fim do enum, depois do E0911.
 *            0.32.0 - Linguagem v1.12 M3 P21v-a: E0909, E0910 e E0911 (proc como valor: assinatura fora da fronteira C, 'as proc' de origem invalida, campo proc sem valor zero) no fim do enum, depois do X0103.
 *            0.31.0 -M3 P19: X0101, X0102 e X0103 (foreign: simbolo nao encontrado, biblioteca nao encontrada, formato que ainda nao liga) no fim do enum, depois do X0105.
 *            0.30.0 - M3 P19a: X0105 (biblioteca achada nao e do tipo pedido: PE de 32 bits no leitor de exportacoes) no fim do enum, depois do R0011.
 *            0.29.0 -Linguagem v1.11 L16b: R0011 (excecao do processador dentro de um teste) no fim do enum, depois do E0335.
 *            0.28.0 -Linguagem v1.11 L15a: E0335 (lista de capacidade fixa [dynamic, N]T) no fim do enum, depois do E0333.
 *            0.27.0 - Linguagem v1.11 L14a: E0333 (quat) no fim do enum, depois do E0334.
 *            0.26.0 -Linguagem v1.11 L13: E0334 (min, max, abs, clamp e sqrt embutidos) no fim do enum, depois do E0332.
 *            0.25.0 -Linguagem v1.11 L12b: E0332 (swizzle e escrita de componente em [N]T) no fim do enum, depois do E0331.
 *            0.24.0 - Linguagem v1.11 L12a: E0331 (conta elemento a elemento em [N]T) no fim do enum, depois do E1005.
 *            0.23.0 -Linguagem v1.11 L10a: E0707 (var nunca alterada), E0708 (let/var nunca lido), E0709 (retorno descartado) e E1005 (import sem uso) no fim do enum, depois do E0329.
 *            0.22.0 -Linguagem v1.11 L9: E0328 (tamanho, alinhamento ou offset de layout) e E0329 (static_assert) no fim do enum, depois do E0330.
 *            0.21.0 - Linguagem v1.11 L8: Diag.calado (erro que e cascata de uma declaracao envenenada: DiagListaGuardar o descarta, nao entra na lista).
 *            0.20.0- Linguagem v1.11 L7: DiagLista (varios erros por execucao, um por declaracao), DiagListaGuardar/Ordenar/Imprimir.
 *            0.19.0 - Linguagem v1.11 L6: DiagImprimirJson poe coluna_fim nos E-codes (comentario abaixo).
 *            0.18.0 - Linguagem v1.11 L1: Diag.extras e DiagAcrescentarExtra (campos extras do E-code depois de forma_certa); E0330 (literal inteiro inexato em real) no fim do enum.
 *            0.17.0 - P18a: X0801, X0802 e X0803 (tipo diferencial) entre X0302 e X0901.
 *            0.16.0 - backend x64 P8b: DiagImprimirJsonExtras.
 *            0.15.0 - backend x64 P3: X0302 (tipo backend) e X0901..X0906 (tipo interno) no fim do enum.
 *            0.14.0 - backend x64 M0-R: X0001, X0002 e X0003 (tipo backend) reservados no fim do enum (faixa X do backend nativo).
 *            0.13.0 - Linguagem v1.9 P1: E0318 (map), E0319 (bit_set), E0320 (soa), E0321 (matrix).
 *            0.12.0- Linguagem v1.8b P1: E0907 (callback), E0908 (tipo opaco), R0010 (excecao C++ no wrapper) e 22 codigos G (forja ligar).
 *            0.11.0 - Linguagem v1.8 P1: E0316 (escopo), E0317 (posse de heap), E1001..E1004 (modulos) e R0009 (vazamento no fim).
 *            0.10.0 - Linguagem v1.7 P2: E0312 (opcional), E0313 (union), E0314 (retorno multiplo), E0315 (texto formatado).
 *            0.9.0 - Linguagem v1.6 P2: E0310 e E0311 (memoria explicita), R0007 e R0008 (alocacao e uso de memoria no runtime).
 *            0.8.0 - Linguagem v1.5 P2: E0308 (array e fatia), E0309 (literal de array), E0421 (indice ou limite constante fora) e R0006 (indice ou fatia fora).
 *            0.7.0 - Linguagem v1.4 P2: E0904 (tipo fora da fronteira C em foreign), E0905 (nome de proc foreign ou de biblioteca),
 *                    E0906 ('pub'/'export' em foreign) e B0004 (ligacao falhou).
 *            0.6.0 - Linguagem v1.3: E0306 (ponteiro), E0307 (texto), E0706 (endereco local devolvido) e R0005 (ponteiro nulo); cabecalho corrigido.
 *            0.5.0 - Linguagem v1.2: E0303 (bits em tipo com sinal), E0304 (transmute), E0305 ('-' em sem sinal), E0609 (T ! E com T == E).
 *            0.4.0 - Linguagem v1.1: E0901..E0903 (export), R0004 (main devolveu erro), B0003 (dll sem pub export).
 *            0.3.0 - Fase 5h: R0001..R0003 (runtime) e B0001..B0002 (build) no fim do enum; DiagExplicarJson (forja explicar)
 *                    e DiagImprimirJsonSemFonte (diagnostico de build sem linha do .fj).
 *            0.2.0 - Fase 5c: E0102 (caixa), E0202 (nome repetido), E0601..E0608 (resultado/erro/switch), E0701..E0705 (fluxo e
 *                    mutabilidade), E0801..E0802 (test/expect). Os 11 codigos anteriores ficam intactos e na mesma ordem.
 *            0.1.0 - Fase 5a: catalogo E0001..E0504 do contrato v1; a causa sobe de 320 para 512 bytes.
 *                    Codigos novos (E06xx resultado, E07xx fluxo, E08xx expect, R00xx runtime) entram com a etapa que os usa.
 * Verificacao: construir.bat && teste_common.exe
 */
#ifndef FORJA_DIAG_H
#define FORJA_DIAG_H

#include <stdio.h>

#include "common/fonte.h"

typedef enum {
    E0001, E0101, E0201, E0301, E0302, E0412, E0420, E0501, E0502, E0503, E0504,
    E0102, E0202, E0601, E0602, E0603, E0604, E0605, E0606, E0607, E0608,
    E0701, E0702, E0703, E0704, E0705, E0801, E0802, E0901, E0902, E0903, E0303, E0304, E0305, E0609,
    E0306, E0307, E0706, E0904, E0905, E0906,
    E0308, E0309, E0421, E0310, E0311,
    E0312, E0313, E0314, E0315, E0316, E0317, E0318, E0319, E0320, E0321, E1001, E1002, E1003, E1004, E0907, E0908,
    R0001, R0002, R0003, R0004, R0005, R0006, R0007, R0008, R0009, R0010, B0001, B0002, B0003, B0004,
    G0001, G0003, G0004, G0005, G0006, G0007, G0008, G0013, G0014, G0016, G0017, G0019, G0021, G0022, G0023, G0025, G0026,
    G0101, G0102, G0103, G0104, G0105, X0001, X0002, X0003, X0302, X0801, X0802, X0803, X0901, X0902, X0903, X0904, X0905, X0906, E0330, E0328, E0329, E0707, E0708, E0709, E1005, E0331, E0332, E0334, E0333, E0335, R0011, X0105, X0101, X0102, X0103, E0909, E0910, E0911, B0005, B0007, B0006, B0008, X0106, X0108, X0107, X0109, R0012, E1006, R0013, R0014, R0015, E0110, E0336, E0337, E0338, F0001, E1007, E0339, X0110, X0111, X0112, X0113, X0114, E0342, E0912, E0913, E1008, DIAG_QTD
} DiagCodigo;

/* extras: fragmento de objeto JSON sem as chaves ("chave": valor, ...), na ordem fixa do contrato; vazio = nenhum. */
/* calado (L8): a causa envolve TIPO_VENENO ou simbolo envenenado (cascata de um erro ja relatado): DiagListaGuardar o descarta. */
/* Sugestao aplicavel por maquina (D-IA1-5): uma ou mais edicoes de UMA linha, em offsets do texto da Fonte ([ini, fim) vira 'novo'; ini == fim insere).
 * Confianca: certa = unica e nao muda o sentido (so estas o futuro 'forja corrigir --aplicar' grava); provavel; so_dica = so descricao (nedicoes pode ser 0).
 * A 1a sugestao e a principal; as demais saem em "alternativas". Quem tem sugestao imprime o pacote do erro (diag_pacote.c); quem nao tem sai com o pacote vazio de correcao (correcao [] e so_dica; Portao Z z5).
 * Coluna das edicoes = a do diagnostico: BYTES a partir de 1 na linha (o BOM UTF-8 do inicio do arquivo nao conta, FonteLer o descarta). */
typedef enum { DIAG_CERTA, DIAG_PROVAVEL, DIAG_SO_DICA } DiagConfianca;
enum { DIAG_EDICOES_MAX = 2, DIAG_SUGESTOES_MAX = 2, DIAG_NOVO_MAX = 32, DIAG_DESCRICAO_MAX = 120, DIAG_ANTIGO_MAX = 160 };
typedef struct { uint32_t ini, fim; char novo[DIAG_NOVO_MAX]; } DiagEdicao;
typedef struct { uint8_t confianca, nedicoes; DiagEdicao edicoes[DIAG_EDICOES_MAX]; char descricao[DIAG_DESCRICAO_MAX]; } DiagSugestao;
typedef struct { uint8_t n; DiagSugestao itens[DIAG_SUGESTOES_MAX]; } DiagSugestoes;

/* Relacionados (IA1-b14, contrato 1.1 item 4): o que a IA precisa ver para resolver o erro sem abrir o arquivo. Cada um e um offset do texto da Fonte
 * (a linha dele sai como "texto" na impressao, no maximo DIAG_REL_TEXTO_MAX bytes) mais uma nota curta. No maximo DIAG_REL_MAX por diagnostico. */
typedef enum { DIAG_REL_DECLARACAO, DIAG_REL_TIPO, DIAG_REL_ASSINATURA, DIAG_REL_PARECIDO, DIAG_REL_USO_ANTERIOR } DiagRelPapel;
enum { DIAG_REL_MAX = 5, DIAG_REL_NOTA_MAX = 96, DIAG_REL_TEXTO_MAX = 120 };
typedef struct { uint8_t papel; uint32_t off; char nota[DIAG_REL_NOTA_MAX]; } DiagRelacionado;
typedef struct { uint8_t n; DiagRelacionado itens[DIAG_REL_MAX]; } DiagRelacionados;

/* Forma por ocorrencia (D-IA1-5): explicacao e forma_certa escritas para ESTA ocorrencia (com os nomes do arquivo); vazias = valem as do catalogo. */
enum { DIAG_FORMA_MAX = 256 };
typedef struct {
    bool tem; bool calado; DiagCodigo codigo; uint32_t off; char causa[512]; char extras[384]; DiagSugestoes sug; DiagRelacionados rel;
    char explicacao[DIAG_FORMA_MAX], forma[DIAG_FORMA_MAX];
    struct RenTrocas *trocas; /* L3d1: lista das formas antigas usadas (renomeados.h); NULL = ninguem pede, o registro nao custa nada */
} Diag;

/* Acrescenta um relacionado (nota em printf) ao diagnostico pendente. false = lista cheia ou o mesmo papel no mesmo offset ja esta la (nada entra). */
bool DiagRelacionar(Diag *d, DiagRelPapel papel, uint32_t off, const char *fmt, ...) __attribute__((format(gnu_printf, 4, 5)));

/* Define a explicacao e a forma_certa da ocorrencia, depois de DiagRelatar (texto longo e cortado em DIAG_FORMA_MAX - 1 bytes; NULL ou "" deixa o do catalogo). */
void DiagForma(Diag *d, const char *explicacao, const char *forma);

/* "linha 2, coluna 9 de a.fj" para o offset (o nome do arquivo sem pasta; sem nome, so linha e coluna): o "onde" das formas por ocorrencia. */
void DiagDescreverOnde(const Fonte *f, uint32_t off, char *saida, size_t cap);

/* Abre uma sugestao (descricao em printf; a 1a e a principal). false = lista cheia (nada entra). */
bool DiagSugerir(DiagSugestoes *s, DiagConfianca confianca, const char *fmt, ...) __attribute__((format(gnu_printf, 3, 4)));

/* Acrescenta a edicao [ini, fim) -> novo a ultima sugestao aberta. false = sem sugestao aberta, edicoes cheias, 'novo' longo demais ou trecho antigo
 * maior que DIAG_ANTIGO_MAX (a edicao nao entra). A edicao precisa ficar numa linha so. */
bool DiagEditar(DiagSugestoes *s, uint32_t ini, uint32_t fim, const char *novo);

/* Guarda o diagnostico (a compilacao para no 1o) e devolve false, para uso em "return DiagRelatar(...)". */
bool DiagRelatar(Diag *d, DiagCodigo codigo, uint32_t off, const char *fmt, ...) __attribute__((format(gnu_printf, 4, 5)));

/* Acrescenta "chave": valor_json (valor ja em JSON, com aspas se for texto) aos extras do diagnostico, depois de DiagRelatar. Cabe ou nada entra. */
void DiagAcrescentarExtra(Diag *d, const char *chave, const char *valor_json);

/* Lista de diagnosticos da semantica (L7): um erro por declaracao (o 1o), no maximo 'teto' guardados (teto 0 = DIAG_TETO_PADRAO; acima de
 * DIAG_MAX vale DIAG_MAX). modulo[k] = posicao do modulo do item k na ordem do driver (chave de DiagListaOrdenar). cortado = houve erro alem do teto. */
enum { DIAG_MAX = 100, DIAG_TETO_PADRAO = 20 };
typedef struct { Diag itens[DIAG_MAX]; uint32_t modulo[DIAG_MAX]; uint32_t n, teto; bool cortado; } DiagLista;

/* Copia o diagnostico pendente de d para a lista e limpa d->tem (a analise segue na proxima declaracao). Lista cheia: cortado = true, nada entra e
 * devolve false. Diagnostico calado (L8): limpa d e devolve true sem guardar nada. */
bool DiagListaGuardar(DiagLista *l, Diag *d, uint32_t modulo);

/* Ordem de saida: por modulo (chave de DiagListaGuardar), depois por off. Estavel (empate mantem a ordem em que foram guardados). */
void DiagListaOrdenar(DiagLista *l);

/* Uma linha JSON por item, na ordem da lista (sem linha de resumo, mesmo cortado). */
void DiagListaImprimir(const DiagLista *l, const Fonte *f, FILE *saida);

/* Uma linha JSON: codigo, tipo, arquivo, linha, coluna, causa, explicacao, forma_certa e os extras do Diag.
 * So nos E-codes entra "coluna_fim" (L6), entre coluna e causa: coluna + bytes do token (exclusivo; 1 se nao ha token). */
void DiagImprimirJson(const Diag *d, const Fonte *f, FILE *saida);

/* Igual a DiagImprimirJson, com os campos extras do backend inseridos DEPOIS de forma_certa. extras_json e um fragmento de objeto sem as chaves
 * e sem virgula inicial, ja na ordem fixa alvo, etapa, ir, sugestao, interno, relato (quem monta e o nativo\nativo_erro.c); NULL ou "" = nenhum. */
void DiagImprimirJsonExtras(const Diag *d, const Fonte *f, const char *extras_json, FILE *saida);

/* --resumo (IA1-b02): com 'ligado', todo diagnostico impresso em stdout/stderr sai so com codigo, arquivo, linha, coluna, causa e sugestao
 * (a da ocorrencia, quando o Diag tem o extra "sugestao"; senao a forma_certa do catalogo ate o primeiro ponto, ponto e virgula, parenteses ou exemplo, no maximo 90 bytes);
 * sem linha do .fj, so codigo, causa e sugestao. Arquivo que nao e stdout nem stderr (o erro.json do relato) fica completo.
 * FORJA_LOG_DIAG=<arq.jsonl>: cada diagnostico impresso em stdout/stderr tambem e anexado, na mesma forma, ao arquivo; sem a variavel nada e gravado. */
void DiagModoResumo(bool ligado);

/* Texto do codigo ("E0301") e busca inversa para "forja explicar" (-1 se nao existe). */
const char *DiagCodigoTexto(DiagCodigo codigo);
/* Texto fixo do catalogo para o codigo (a "explicacao" do forja explicar). */
const char *DiagCodigoExplicacao(DiagCodigo codigo);
/* Texto fixo do catalogo para a forma_certa do codigo (o driver a poe na linha JSON de R-code que o programa escreve). */
const char *DiagCodigoForma(DiagCodigo codigo);
int DiagCodigoPorTexto(const char *texto);

/* forja explicar: uma linha JSON {codigo, tipo, explicacao, forma_certa} do catalogo. */
void DiagExplicarJson(DiagCodigo codigo, FILE *saida);

/* Diagnostico de build sem linha do .fj (B0001, B0002): uma linha JSON com a causa dada pelo chamador. */
void DiagImprimirJsonSemFonte(DiagCodigo codigo, const char *causa, FILE *saida);

/* Igual, com "ferramenta": {"nome": <ferramenta>, "saida": <o que ela escreveu>} depois de forma_certa (B0004: o stderr do gcc/ld so existe dentro do JSON). O --resumo nao leva a ferramenta. */
void DiagImprimirJsonFerramenta(DiagCodigo codigo, const char *causa, const char *ferramenta, const char *saida_ferramenta, FILE *saida);

#endif
