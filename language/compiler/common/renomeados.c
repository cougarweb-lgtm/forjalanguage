/* renomeados.c - os dados da tabela de renomeados (Emenda 4, A32). Veja renomeados.h.
 * FORMATO FIXO, lido pelo gerador da migracao.txt (tests\ia_first\gerar_migracao_doc.py): uma entrada por linha, `{REN_X, "escopo", "antigo", "novo", "nota", recusar},`, e uma linha
 * `[REN_X] = "nome",` por classe em CLASSE_TEXTO. Os textos sao ASCII, sem aspas, sem barra invertida e sem tabulacao. Mudou a tabela: rode o gerador (sem --verificar) e reconstrua.
 * O ultimo campo e 'recusar': true = a forma antiga e E0110 (L3f; o forja migrar a le mesmo assim); false = valeria em silencio. Nao vai para a migracao.txt.
 * Versao: 0.8.0
 * Changelog: 0.8.0 - S3 P4 (L11b-2, D10): 44 linhas REN_LIBM (simbolo C de foreign de libm -> math.<fn> ou sqrt; f32 com sufixo f) e CLASSE_TEXTO de REN_LIBM, REN_LIBM_LINHA, REN_LIBM_AVISO.
 *            0.7.0 - B01d: CLASSE_TEXTO de REN_ARGUMENTO_NOVO ("argumento-novo"); nenhuma entrada nova na tabela.
 *            0.6.0 - S2g-json (J01): 4 entradas param do vendor:json (indice, valor, texto, caminho -> index, value, text, path).
 *            0.5.0 - IA1-L3f: recusar = true em todas as linhas (a forma antiga e E0110; o forja migrar ainda a le).
 *            0.4.0 - IA1-L3c3: as linhas de tipo, membro, proc e param do vendor:json, vendor:compression e vendor:net (73 entradas).
 *            0.3.0 - IA1-L3c2: as linhas de pacote, modulo, qualificador, proc, tipo, membro e param do vendor:banco (db) e dos pacotes vendor; RenomeadoAcharEm.
 *            0.2.0 - IA1-L3c: campo recusar (todas false); RenomeadoBuscarNovo e RenomeadoAchar (o compilador aceita as duas formas); o escopo de rotulo e lista de palavras.
 *            0.1.0 - IA1-L3a: primeira versao (8 entradas decididas pelo contrato: Emenda 4 e L-09; o resto do inventario espera aprovacao do main).
 * Verificacao: construir.bat && teste_common.exe && python -I ..\tests\ia_first\gerar_migracao_doc.py --verificar
 */
#include "common/renomeados.h"

#include <string.h>

static const char *const CLASSE_TEXTO[REN_CLASSE_QTD] = {
    [REN_ROTULO] = "rotulo",
    [REN_VALOR_ALOCADOR] = "valor-alocador",
    [REN_MODULO] = "modulo",
    [REN_QUALIFICADOR] = "qualificador",
    [REN_PROC] = "proc",
    [REN_TIPO] = "tipo",
    [REN_MEMBRO] = "membro",
    [REN_PACOTE] = "pacote",
    [REN_PARAM] = "param",
    [REN_ARGUMENTO_NOVO] = "argumento-novo",
    [REN_LIBM] = "libm",
    [REN_LIBM_LINHA] = "libm-linha",
    [REN_LIBM_AVISO] = "libm-aviso",
};

static const Renomeado RENOMEADOS[] = {
    {REN_ROTULO, "make new delete format format_c (o alocador)", "em", "alloc", "em = heap vira alloc = heap; `in` e reservada", true},
    {REN_VALOR_ALOCADOR, "o valor do argumento alloc (antes em)", "escopo", "scope", "alloc = scope: solta no fim do bloco", true},
    {REN_ROTULO, "transmute", "para", "to", "transmute(x, para = T) vira transmute(x, to = T)", true},
    {REN_MODULO, "import", "std:arquivo", "std:file", "import std:file no lugar de std:arquivo (read_all e FileError ja sao ingles)", true},
    {REN_QUALIFICADOR, "nome antes do ponto, quando vem de std:arquivo sem as", "arquivo", "file", "arquivo.read_all vira file.read_all", true},
    {REN_PROC, "vendor:db/db", "abrir", "open", "L-09: banco.abrir vira db.open", true},
    {REN_PROC, "vendor:db/db", "executar", "exec", "L-09: banco.executar vira db.exec", true},
    {REN_TIPO, "vendor:db/db", "ErroBanco", "DbError", "L-09: o enum de erro do banco", true},
    {REN_PACOTE, "import vendor:", "banco", "db", "pasta vendor/banco vira vendor/db (import vendor:db/db)", true},
    {REN_PACOTE, "import vendor:", "banco_servidor", "db_server", "pasta vendor/banco_servidor vira vendor/db_server (libpq e mariadb)", true},
    {REN_PACOTE, "import vendor:", "compressao", "compression", "pasta vendor/compressao vira vendor/compression", true},
    {REN_PACOTE, "import vendor:", "rede", "net", "pasta vendor/rede vira vendor/net", true},
    {REN_PACOTE, "import vendor:", "fisica2d", "physics2d", "pasta vendor/fisica2d vira vendor/physics2d (box2d)", true},
    {REN_MODULO, "import", "vendor:banco/banco", "vendor:db/db", "import vendor:db/db no lugar de vendor:banco/banco (arquivo banco.fj vira db.fj)", true},
    {REN_MODULO, "import", "vendor:compressao/compressao", "vendor:compression/compression", "import vendor:compression/compression (arquivo compression.fj)", true},
    {REN_MODULO, "import", "vendor:rede/rede", "vendor:net/net", "import vendor:net/net (arquivo net.fj)", true},
    {REN_QUALIFICADOR, "nome antes do ponto, quando vem de vendor:banco/banco sem as", "banco", "db", "banco.open vira db.open", true},
    {REN_QUALIFICADOR, "nome antes do ponto, quando vem de vendor:compressao/compressao sem as", "compressao", "compression", "compressao.x vira compression.x", true},
    {REN_QUALIFICADOR, "nome antes do ponto, quando vem de vendor:rede/rede sem as", "rede", "net", "rede.x vira net.x", true},
    {REN_PROC, "vendor:db/db", "versao", "version", "versao do SQLite embutido", true},
    {REN_PROC, "vendor:db/db", "fechar", "close", "fecha o banco", true},
    {REN_PROC, "vendor:db/db", "mensagem", "message", "mensagem do ultimo erro", true},
    {REN_PROC, "vendor:db/db", "preparar", "prepare", "prepara a consulta", true},
    {REN_PROC, "vendor:db/db", "finalizar", "finalize", "libera a consulta", true},
    {REN_PROC, "vendor:db/db", "reiniciar", "reset", "volta a consulta ao inicio", true},
    {REN_PROC, "vendor:db/db", "ligar_inteiro", "bind_int", "liga um inteiro ao parametro", true},
    {REN_PROC, "vendor:db/db", "ligar_numero", "bind_float", "liga um real ao parametro", true},
    {REN_PROC, "vendor:db/db", "ligar_texto", "bind_text", "liga um texto ao parametro", true},
    {REN_PROC, "vendor:db/db", "ligar_nulo", "bind_null", "liga NULL ao parametro", true},
    {REN_PROC, "vendor:db/db", "proxima", "next", "anda uma linha", true},
    {REN_PROC, "vendor:db/db", "colunas", "columns", "quantidade de colunas", true},
    {REN_PROC, "vendor:db/db", "coluna_inteiro", "column_int", "inteiro da coluna", true},
    {REN_PROC, "vendor:db/db", "coluna_numero", "column_float", "real da coluna", true},
    {REN_PROC, "vendor:db/db", "coluna_texto", "column_text", "texto da coluna", true},
    {REN_PROC, "vendor:db/db", "ultimo_id", "last_id", "rowid do ultimo INSERT", true},
    {REN_PROC, "vendor:db/db", "alteradas", "changes", "linhas alteradas", true},
    {REN_TIPO, "vendor:db/db", "Banco", "Db", "o banco aberto", true},
    {REN_TIPO, "vendor:db/db", "Consulta", "Query", "a consulta preparada", true},
    {REN_MEMBRO, "vendor:db/db DbError", "nao_abriu", "not_opened", "membro de DbError", true},
    {REN_MEMBRO, "vendor:db/db DbError", "sql_invalido", "invalid_sql", "membro de DbError", true},
    {REN_MEMBRO, "vendor:db/db DbError", "execucao", "execution", "membro de DbError", true},
    {REN_MEMBRO, "vendor:db/db DbError", "parametro", "parameter", "membro de DbError", true},
    {REN_MEMBRO, "vendor:db/db DbError", "sem_memoria", "out_of_memory", "membro de DbError (so deste enum)", true},
    {REN_PARAM, "vendor:db/db", "caminho", "path", "rotulo de open: open(path = ...)", true},
    {REN_PARAM, "vendor:db/db", "indice", "index", "rotulo de bind_* e column_*", true},
    {REN_PARAM, "vendor:db/db", "valor", "value", "rotulo de bind_*", true},
    {REN_TIPO, "vendor:json/json", "ErroJson", "JsonError", "o enum de erro do json", true},
    {REN_TIPO, "vendor:json/json", "TipoJson", "JsonKind", "o enum do tipo do valor json", true},
    {REN_MEMBRO, "vendor:json/json JsonError", "invalido", "invalid", "membro de JsonError", true},
    {REN_MEMBRO, "vendor:json/json JsonError", "ausente", "missing", "membro de JsonError", true},
    {REN_MEMBRO, "vendor:json/json JsonError", "tipo_errado", "wrong_type", "membro de JsonError", true},
    {REN_MEMBRO, "vendor:json/json JsonError", "arquivo", "file", "membro de JsonError", true},
    {REN_MEMBRO, "vendor:json/json JsonError", "sem_memoria", "out_of_memory", "membro de JsonError (so deste enum)", true},
    {REN_MEMBRO, "vendor:json/json JsonKind", "nulo", "null", "membro de JsonKind", true},
    {REN_MEMBRO, "vendor:json/json JsonKind", "booleano", "boolean", "membro de JsonKind", true},
    {REN_MEMBRO, "vendor:json/json JsonKind", "numero", "number", "membro de JsonKind", true},
    {REN_MEMBRO, "vendor:json/json JsonKind", "texto", "text", "membro de JsonKind", true},
    {REN_MEMBRO, "vendor:json/json JsonKind", "lista", "list", "membro de JsonKind", true},
    {REN_MEMBRO, "vendor:json/json JsonKind", "objeto", "object", "membro de JsonKind", true},
    {REN_PROC, "vendor:json/json", "abrir", "parse", "abre (le) um texto JSON", true},
    {REN_PROC, "vendor:json/json", "abrir_str", "parse_str", "abre um str JSON", true},
    {REN_PROC, "vendor:json/json", "abrir_arquivo", "parse_file", "le o arquivo e abre", true},
    {REN_PROC, "vendor:json/json", "fechar", "close", "fecha o documento dono", true},
    {REN_PROC, "vendor:json/json", "tipo", "kind", "tipo do valor", true},
    {REN_PROC, "vendor:json/json", "campo", "field", "vista de um campo do objeto", true},
    {REN_PROC, "vendor:json/json", "tem_campo", "has_field", "o objeto tem o campo", true},
    {REN_PROC, "vendor:json/json", "tamanho", "size", "quantidade de elementos", true},
    {REN_PROC, "vendor:json/json", "chave", "key", "nome do campo de um objeto", true},
    {REN_PROC, "vendor:json/json", "numero", "number", "valor numerico", true},
    {REN_PROC, "vendor:json/json", "inteiro", "integer", "valor inteiro", true},
    {REN_PROC, "vendor:json/json", "texto", "text", "texto emprestado do documento", true},
    {REN_PROC, "vendor:json/json", "booleano", "boolean", "valor true ou false", true},
    {REN_PROC, "vendor:json/json", "campo_numero", "field_number", "campo e leitura numerica", true},
    {REN_PROC, "vendor:json/json", "campo_inteiro", "field_int", "campo e leitura inteira", true},
    {REN_PROC, "vendor:json/json", "campo_texto", "field_text", "campo e leitura de texto", true},
    {REN_PROC, "vendor:json/json", "campo_bool", "field_bool", "campo e leitura booleana", true},
    {REN_PROC, "vendor:json/json", "criar_objeto", "new_object", "objeto novo e vazio", true},
    {REN_PROC, "vendor:json/json", "criar_lista", "new_list", "lista nova e vazia", true},
    {REN_PROC, "vendor:json/json", "poe_numero", "set_number", "poe um numero no objeto", true},
    {REN_PROC, "vendor:json/json", "poe_texto", "set_text", "poe um texto no objeto", true},
    {REN_PROC, "vendor:json/json", "poe_bool", "set_bool", "poe um booleano no objeto", true},
    {REN_PROC, "vendor:json/json", "poe_nulo", "set_null", "poe null no objeto", true},
    {REN_PROC, "vendor:json/json", "poe_objeto", "set_object", "poe um objeto filho no objeto", true},
    {REN_PROC, "vendor:json/json", "poe_lista", "set_list", "poe uma lista filha no objeto", true},
    {REN_PROC, "vendor:json/json", "lista_numero", "push_number", "acrescenta um numero na lista", true},
    {REN_PROC, "vendor:json/json", "lista_texto", "push_text", "acrescenta um texto na lista", true},
    {REN_PROC, "vendor:json/json", "lista_objeto", "push_object", "acrescenta um objeto na lista", true},
    {REN_PROC, "vendor:json/json", "gerar", "dump", "texto JSON do valor", true},
    {REN_PARAM, "vendor:json/json", "nome", "name", "rotulo de field, has_field, field_*, set_*", true},
    {REN_PARAM, "vendor:json/json", "bonito", "pretty", "rotulo de dump", true},
    {REN_PARAM, "vendor:json/json", "objeto", "object", "rotulo do 1o parametro de set_*", true},
    {REN_PARAM, "vendor:json/json", "lista", "list", "rotulo do 1o parametro de push_*", true},
    {REN_PARAM, "vendor:json/json", "indice", "index", "rotulo de item e key", true},
    {REN_PARAM, "vendor:json/json", "valor", "value", "rotulo de set_* e push_*", true},
    {REN_PARAM, "vendor:json/json", "texto", "text", "rotulo de parse e parse_str", true},
    {REN_PARAM, "vendor:json/json", "caminho", "path", "rotulo de parse_file", true},
    {REN_TIPO, "vendor:compression/compression", "ErroZlib", "ZlibError", "o enum de erro da compressao", true},
    {REN_MEMBRO, "vendor:compression/compression ZlibError", "destino_pequeno", "dest_too_small", "membro de ZlibError", true},
    {REN_MEMBRO, "vendor:compression/compression ZlibError", "dados_invalidos", "invalid_data", "membro de ZlibError", true},
    {REN_MEMBRO, "vendor:compression/compression ZlibError", "sem_memoria", "out_of_memory", "membro de ZlibError (so deste enum)", true},
    {REN_PROC, "vendor:compression/compression", "limite", "bound", "tamanho de destino que sempre basta", true},
    {REN_PROC, "vendor:compression/compression", "comprimir", "compress", "comprime", true},
    {REN_PROC, "vendor:compression/compression", "descomprimir", "decompress", "descomprime", true},
    {REN_PARAM, "vendor:compression/compression", "origem", "src", "rotulo de compress e decompress", true},
    {REN_PARAM, "vendor:compression/compression", "destino", "dst", "rotulo de compress e decompress", true},
    {REN_PARAM, "vendor:compression/compression", "nivel", "level", "rotulo de compress", true},
    {REN_PARAM, "vendor:compression/compression", "dados", "data", "rotulo de crc", true},
    {REN_PARAM, "vendor:compression/compression", "anterior", "previous", "rotulo de crc", true},
    {REN_PARAM, "vendor:compression/compression", "tamanho", "size", "rotulo de bound", true},
    {REN_TIPO, "vendor:net/net", "ErroRede", "NetError", "o enum de erro da rede", true},
    {REN_MEMBRO, "vendor:net/net NetError", "url_invalida", "invalid_url", "membro de NetError", true},
    {REN_MEMBRO, "vendor:net/net NetError", "sem_resolver", "cannot_resolve", "membro de NetError", true},
    {REN_MEMBRO, "vendor:net/net NetError", "sem_conexao", "cannot_connect", "membro de NetError", true},
    {REN_MEMBRO, "vendor:net/net NetError", "tempo_esgotado", "timeout", "membro de NetError", true},
    {REN_MEMBRO, "vendor:net/net NetError", "http_erro", "http_error", "membro de NetError", true},
    {REN_MEMBRO, "vendor:net/net NetError", "outro", "other", "membro de NetError", true},
    {REN_MEMBRO, "vendor:net/net NetError", "arquivo", "file", "membro de NetError", true},
    {REN_MEMBRO, "vendor:net/net NetError", "sem_memoria", "out_of_memory", "membro de NetError (so deste enum)", true},
    {REN_PROC, "vendor:net/net", "tamanho_do_arquivo", "file_size", "tamanho em bytes do arquivo", true},
    {REN_PROC, "vendor:net/net", "remover_arquivo", "remove_file", "apaga o arquivo", true},
    {REN_PROC, "vendor:net/net", "baixar_arquivo", "download_file", "baixa uma URL para um arquivo", true},
    {REN_PARAM, "vendor:net/net", "tempo_s", "timeout_s", "rotulo de download_file", true},
    {REN_PARAM, "vendor:net/net", "destino", "dst", "rotulo de download_file", true},
    {REN_LIBM, "foreign de libm (simbolo C)", "sin", "math.sin", "libm f64 vira math.sin (std:math)", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "sinf", "math.sin", "libm f32 vira math.sin (gemea f32 pelo tipo do argumento)", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "cos", "math.cos", "libm f64 vira math.cos", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "cosf", "math.cos", "libm f32 vira math.cos", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "tan", "math.tan", "libm f64 vira math.tan", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "tanf", "math.tan", "libm f32 vira math.tan", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "asin", "math.asin", "libm f64 vira math.asin", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "asinf", "math.asin", "libm f32 vira math.asin", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "acos", "math.acos", "libm f64 vira math.acos", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "acosf", "math.acos", "libm f32 vira math.acos", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "atan", "math.atan", "libm f64 vira math.atan", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "atanf", "math.atan", "libm f32 vira math.atan", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "atan2", "math.atan2", "libm f64 vira math.atan2 (rotulos y e x)", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "atan2f", "math.atan2", "libm f32 vira math.atan2 (rotulos y e x)", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "exp", "math.exp", "libm f64 vira math.exp", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "expf", "math.exp", "libm f32 vira math.exp", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "exp2", "math.exp2", "libm f64 vira math.exp2", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "exp2f", "math.exp2", "libm f32 vira math.exp2", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "log", "math.log", "libm f64 vira math.log", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "logf", "math.log", "libm f32 vira math.log", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "log2", "math.log2", "libm f64 vira math.log2", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "log2f", "math.log2", "libm f32 vira math.log2", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "log10", "math.log10", "libm f64 vira math.log10", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "log10f", "math.log10", "libm f32 vira math.log10", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "pow", "math.pow", "libm f64 vira math.pow (rotulos x e y)", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "powf", "math.pow", "libm f32 vira math.pow (rotulos x e y)", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "cbrt", "math.cbrt", "libm f64 vira math.cbrt", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "cbrtf", "math.cbrt", "libm f32 vira math.cbrt", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "hypot", "math.hypot", "libm f64 vira math.hypot", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "hypotf", "math.hypot", "libm f32 vira math.hypot", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "fmod", "math.fmod", "libm f64 vira math.fmod", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "fmodf", "math.fmod", "libm f32 vira math.fmod", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "floor", "math.floor", "libm f64 vira math.floor", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "floorf", "math.floor", "libm f32 vira math.floor", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "ceil", "math.ceil", "libm f64 vira math.ceil", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "ceilf", "math.ceil", "libm f32 vira math.ceil", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "round", "math.round", "libm f64 vira math.round (metade para longe do zero)", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "roundf", "math.round", "libm f32 vira math.round", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "trunc", "math.trunc", "libm f64 vira math.trunc", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "truncf", "math.trunc", "libm f32 vira math.trunc", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "copysign", "math.copysign", "libm f64 vira math.copysign (rotulos x e y)", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "copysignf", "math.copysign", "libm f32 vira math.copysign (rotulos x e y)", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "sqrt", "sqrt", "libm f64 vira o embutido sqrt (sem math.)", false},
    {REN_LIBM, "foreign de libm (simbolo C)", "sqrtf", "sqrt", "libm f32 vira o embutido sqrt (sem math.)", false},
};

enum { QTD_RENOMEADOS = sizeof RENOMEADOS / sizeof RENOMEADOS[0] };

size_t RenomeadosContagem(void)
{
    return QTD_RENOMEADOS;
}

const Renomeado *RenomeadoPorIndice(size_t indice)
{
    return indice < QTD_RENOMEADOS ? &RENOMEADOS[indice] : NULL;
}

const Renomeado *RenomeadoBuscar(RenClasse classe, const char *antigo)
{
    for (size_t k = 0; k < QTD_RENOMEADOS; k++) {
        if (RENOMEADOS[k].classe == classe && strcmp(RENOMEADOS[k].antigo, antigo) == 0) return &RENOMEADOS[k];
    }
    return NULL;
}

const Renomeado *RenomeadoBuscarNovo(RenClasse classe, const char *novo)
{
    for (size_t k = 0; k < QTD_RENOMEADOS; k++) {
        if (RENOMEADOS[k].classe == classe && strcmp(RENOMEADOS[k].novo, novo) == 0) return &RENOMEADOS[k];
    }
    return NULL;
}

/* O escopo tem 'palavra' como palavra inteira (separadores: espaco e parenteses). */
static bool EscopoTemPalavra(const char *escopo, const char *palavra)
{
    size_t tam = strlen(palavra);

    for (const char *p = escopo; *p != '\0';) {
        size_t n = strcspn(p, " ()");

        if (n == tam && memcmp(p, palavra, tam) == 0) return true;
        p += n > 0 ? n : 1;
    }
    return false;
}

/* O escopo tem todas as palavras da lista (terminada em NULL; lista vazia = qualquer escopo). */
static bool EscopoTemPalavras(const char *escopo, const char *const *palavras)
{
    for (size_t i = 0; palavras[i] != NULL; i++) {
        if (!EscopoTemPalavra(escopo, palavras[i])) return false;
    }
    return true;
}

const Renomeado *RenomeadoAcharEm(RenClasse classe, const char *const *palavras, const char *nome, bool *era_antigo)
{
    for (size_t k = 0; k < QTD_RENOMEADOS; k++) {
        const Renomeado *r = &RENOMEADOS[k];
        bool antigo = strcmp(r->antigo, nome) == 0;

        if (r->classe != classe || (!antigo && strcmp(r->novo, nome) != 0)) continue;
        if (!EscopoTemPalavras(r->escopo, palavras)) continue;
        *era_antigo = antigo;
        return r;
    }
    return NULL;
}

const Renomeado *RenomeadoAchar(RenClasse classe, const char *palavra, const char *nome, bool *era_antigo)
{
    const char *const palavras[2] = {palavra, NULL}; /* palavra NULL = lista vazia */

    return RenomeadoAcharEm(classe, palavras, nome, era_antigo);
}

const char *RenomeadoClasseTexto(RenClasse classe)
{
    return (unsigned)classe < REN_CLASSE_QTD ? CLASSE_TEXTO[classe] : "?";
}
