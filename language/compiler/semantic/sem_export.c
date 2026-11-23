/* sem_export.c - 'pub export proc': modificadores (E0903), nome do simbolo exportado (E0902) e tipos que cruzam a ABI C (E0901).
 * Versao: 0.11.4
 * Changelog: 0.11.4 - R0 [D-REDE-1]: E0202 aceita ^Opaco com ^Opaco (e ?^ com ?^) de opacos distintos em modulos diferentes: o ABI C e o mesmo ponteiro (json x net no mesmo programa).
 *            0.11.3 - F4: o mesmo simbolo C em dois modulos exige assinatura identica (E0202 com os dois protos).
 *            0.11.2 - B01a: MotivoForaDaAbi recusa o Allocator (E0901 em export, E0904 em foreign).
 *            0.11.1 - IA1-b06: os tres E0202 de alias de foreign levam a sugestao (renomear o proc da Forja, ou so a dica).
 *            0.11.0- Linguagem v1.12 M3 P21v-a: SemMotivoDoProc (E0909) e proc como parametro/retorno de proc exportado = E0901.
 *            0.10.0 -Linguagem v1.10 (L1): Arena e Pool (tipo_arena/tipo_pool), tambem atras de ^ e ?^, recusados em export (E0901) e foreign (E0904);
 *                     Arena/Pool testados antes da regra de struct dentro de struct em MotivoDoCampo.
 *                     Linguagem v1.10 (A6): ErroSimboloCDeAlias, ConferirAliasDoSimbolo e o simbolo efetivo em SemChecarExterno (E0905/E0202).
 *            0.9.0 - Linguagem v1.9 P3: MotivoForaDaAbi recusa TC_MAPA, TC_BIT_SET, TC_SOA e TC_MATRIZ (E0901 e E0904, D20/D25).
 *            0.8.1 - Linguagem v1.8b P6: SemChecarExterno marca s->tem_externo (liga a origem ORIGEM_C nas chamadas).
 *            0.8.0 - Linguagem v1.8b P5a: MotivoDoProc e o caso TC_PROC em MotivoForaDaAbi ("o parametro k do callback e T: motivo").
 *            0.7.2 - Linguagem v1.8b P4: abi_foreign em TipoDeForeignAtravessa; MotivoDoCampo (struct em struct e array em campo atravessam so em foreign); texto "o campo 'c' e T: motivo".
 *            0.7.1 - Linguagem v1.8 P6a: E0905 deixa o prefixo forja_ nos foreign de modulo da std (mem.fj chama o runtime).
 *            0.7.0 - Linguagem v1.8 P3: E0906 so para 'export' em foreign ('pub foreign' vale); E1004 ('export' e 'main' fora do arquivo raiz).
 *            0.6.2- Linguagem v1.7 P10: comentario de cabecalho cita o cstr devolvido (format_c em heap) no lugar da regra antiga do D0004.
 *            0.6.1 - Linguagem v1.7 (desempenho): SemChecarModificadores devolve o bitmask de DeclTipo presentes (categorias).
 *            0.6.0 - Linguagem v1.7 P5b: D20, retorno cstr/?cstr exportavel (os returns sao conferidos em SemReturn); textos do cstr devolvido.
 *            0.5.2 - Linguagem v1.7 P4: MotivoForaDaAbi recusa ?T (o nicho ?cstr segue o cstr) e TC_TUPLA (E0901 e E0904).
 *            0.5.1 - Linguagem v1.7 P3: MotivoForaDaAbi recusa TC_UNIAO (E0901 e E0904).
 *            0.5.0- Linguagem v1.6 P2: MotivoForaDaAbi recusa TC_DINAMICO (E0901 e E0904; ^[dynamic]T cai nele pelo caso de ponteiro).
 *            0.4.0 - Linguagem v1.5 P2: MotivoForaDaAbi recusa TC_ARRAY e TC_FATIA (E0901 e E0904) com a forma certa '^T + n: u64'.
 *            0.3.0 - Linguagem v1.4 P2: fronteira C de foreign: E0906 (pub/export), NomeDeBibliotecaValido (E0905), SemChecarExterno
 *                    (E0904/E0905); sai a recusa provisoria do P1.
 *            0.2.1 - Linguagem v1.4 P1: recusa provisoria de foreign (E0101), removida no 0.3.0.
 *            0.2.0 - Linguagem v1.3 P7: MotivoForaDaAbi (ponteiros, rawptr, cstr entrando, struct por valor; str e cstr devolvido fora).
 *            0.1.0 - Linguagem v1.1: primeira versao.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 *
 * O proc exportado vira uma funcao C de nome limpo (sem o prefixo fj_) na DLL; PHP (FFI) e Python (ctypes) a chamam direto.
 * So atravessa a fronteira o que tem forma fixa em C: numericos, bool, enum, ^T e ?^T (apontado struct ou que atravessa), rawptr,
 * struct por valor cujos campos atravessam e cstr (entrando; devolvido so com 'return format_c(..., em = heap)' direto, que o chamador
 * libera com forja_liberar). Resultado 'T ! E' e str ficam de fora.
 */
#include "semantic/sem_interno.h"

#include <ctype.h>
#include <stdio.h>

/* Palavras do C e nomes que o proprio C gerado ja usa (stdbool, stdint, libc do forja_rt.h): um simbolo exportado com esses nomes
 * colidiria com a declaracao ou mudaria o sentido do codigo. */
static const char *const NOMES_PROIBIDOS[] = {
    "auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum", "extern", "float", "for",
    "goto", "if", "inline", "int", "long", "register", "restrict", "return", "short", "signed", "sizeof", "static", "struct",
    "switch", "typedef", "union", "unsigned", "void", "volatile", "while", "bool", "true", "false", "int8_t", "int16_t",
    "int32_t", "int64_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t", "size_t", "main", "printf", "fprintf", "snprintf",
    "sprintf", "puts", "putchar", "scanf", "fopen", "fclose", "fread", "fwrite", "malloc", "calloc", "realloc", "free", "exit",
    "abort", "atexit", "system", "getenv", "memcpy", "memmove", "memset", "memcmp", "strlen", "strcmp", "strcpy", "setjmp",
    "longjmp", "abs", "rand", "srand",
};

static bool NomeProibido(const char *nome)
{
    for (size_t k = 0; k < sizeof NOMES_PROIBIDOS / sizeof NOMES_PROIBIDOS[0]; k++) {
        if (strcmp(nome, NOMES_PROIBIDOS[k]) == 0) return true;
    }
    return strncmp(nome, "fj_", 3) == 0 || strncmp(nome, "forja_", 6) == 0;
}

static const char *MotivoForaDaAbi(Semantica *s, TipoId tipo, bool devolvido, char *buf, size_t cap);

/* v1.10 (L1): Arena e Pool de std:mem (tipo_arena/tipo_pool; 0 = std:mem nao importado) nunca atravessam, nem atras de ponteiro. */
static const char MOTIVO_ARENA_POOL[] =
    "Arena e Pool nao atravessam a fronteira C: cada DLL tem o seu proprio alocador (o handle so vale na Forja que o criou); "
    "use a Arena dentro do proc e devolva o resultado por ^T alocado com alloc = heap";

/* B01a: o Allocator e o handle do alocador do runtime desta Forja; so um proc da Forja o recebe do chamador. */
static const char MOTIVO_ALLOCATOR[] =
    "Allocator nao atravessa a fronteira C: e o alocador do runtime da Forja (cada DLL tem o seu); "
    "tire o parametro alloc: Allocator do proc exportado e aloque dentro dele com alloc = heap";

static bool EhArenaOuPool(const Semantica *s, TipoId t)
{
    return (s->tipo_arena != 0 && t == s->tipo_arena) || (s->tipo_pool != 0 && t == s->tipo_pool);
}

/* v1.8b: motivo de um campo de struct. Struct dentro de struct so atravessa em foreign (abi_foreign); array em campo
 * atravessa em foreign se o elemento base atravessa (layout T[N]). */
static const char *MotivoDoCampo(Semantica *s, TipoId t, bool devolvido, char *buf, size_t cap)
{
    if (EhArenaOuPool(s, t)) return MOTIVO_ARENA_POOL;
    if (TipoClasseDe(&s->tipos, t) == TC_STRUCT && !s->abi_foreign) return "struct dentro de struct ainda nao sai no .h";
    if (TipoClasseDe(&s->tipos, t) == TC_ARRAY && s->abi_foreign) {
        while (TipoClasseDe(&s->tipos, t) == TC_ARRAY) t = (TipoId)TipoDe(&s->tipos, t)->a;
    }
    return MotivoForaDaAbi(s, t, devolvido, buf, cap);
}

/* Struct por valor: o 1o campo que nao atravessa vira "o campo 'c' e <tipo>: <motivo>". */
static const char *MotivoDaStruct(Semantica *s, TipoId tipo, bool devolvido, char *buf, size_t cap)
{
    const Decl *est = SemDeclDaStruct(s, tipo);
    char campo_tipo[96], interno[256];

    if (est == NULL) return "struct sem declaracao";
    for (uint32_t k = 0; k < est->ncampos; k++) {
        TipoId t = est->campos[k].tipo.tipo;
        const char *motivo = MotivoDoCampo(s, t, devolvido, interno, sizeof interno);
        if (motivo == NULL) continue;
        SemDescrever(s, t, campo_tipo, sizeof campo_tipo);
        snprintf(buf, cap, "o campo '%s' e %s: %s", SemNomeTexto(s, est->campos[k].nome), campo_tipo, motivo);
        return buf;
    }
    return NULL;
}

/* v1.8b: callback (TC_PROC): o 1o parametro (1-based) e depois o retorno que nao atravessam; NULL = tudo atravessa. */
static const char *MotivoDoProc(Semantica *s, TipoId tipo, char *buf, size_t cap)
{
    char interno[256], texto[96];
    uint32_t n = TipoProcQtd(&s->tipos, tipo);
    TipoId ret = (TipoId)TipoDe(&s->tipos, tipo)->a;

    for (uint32_t k = 0; k <= n; k++) {
        TipoId t = k < n ? TipoProcParam(&s->tipos, tipo, k) : ret;
        if (k == n && ret == TIPO_VAZIO) break;
        const char *motivo = MotivoForaDaAbi(s, t, false, interno, sizeof interno);
        if (motivo == NULL) continue;
        SemDescrever(s, t, texto, sizeof texto);
        if (k < n) snprintf(buf, cap, "o parametro %u do callback e %s: %s", k + 1, texto, motivo);
        else snprintf(buf, cap, "o retorno do callback e %s: %s", texto, motivo);
        return buf;
    }
    return NULL;
}

/* NULL = o tipo atravessa a fronteira C; senao a causa do E0901. TIPO_VAZIO so vale como retorno (quem chama decide). */
static const char *MotivoForaDaAbi(Semantica *s, TipoId tipo, bool devolvido, char *buf, size_t cap)
{
    const TipoInfo *info = TipoDe(&s->tipos, tipo);

    switch (info->classe) {
    case TC_BOOL: case TC_I32: case TC_INT: case TC_REAL: case TC_ENUM: case TC_RAWPTR:
    case TC_I8: case TC_I16: case TC_U8: case TC_U16: case TC_U32: case TC_UINT: case TC_F64: return NULL;
    case TC_CSTR: return devolvido ? "cstr devolvido so sai com return format_c(..., alloc = heap) direto; dentro de struct nao atravessa" : NULL;
    case TC_TEXTO: return "use cstr";
    case TC_STRUCT:
        if (EhArenaOuPool(s, tipo)) return MOTIVO_ARENA_POOL;
        if (SemEhAllocator(s, tipo)) return MOTIVO_ALLOCATOR;
        return MotivoDaStruct(s, tipo, devolvido, buf, cap);
    case TC_ARRAY:
        return "array nao atravessa a fronteira C nesta versao: passe o 1o elemento e o tamanho (dados: ^T, n: u64) e chame com &a[0] e len(a) as u64";
    case TC_FATIA: return "fatia nao atravessa a fronteira C: passe dados: ^T e n: u64 (&s[0], len(s) as u64)";
    case TC_DINAMICO:
        return "lista [dynamic]T nao atravessa a fronteira C: passe dados: ^T e n: u64 (&l[0], len(l) as u64); a memoria continua da Forja";
    case TC_UNIAO:
        return "union nao atravessa a fronteira C: passe um enum com a variante e o valor em parametros separados";
    case TC_OPCIONAL:
        if (TipoOpcionalNicho(&s->tipos, tipo)) return MotivoForaDaAbi(s, TIPO_CSTR, devolvido, buf, cap);
        return "?T nao atravessa a fronteira C: use ?^T (ponteiro ou none) ou devolva bool e grave o valor por ^T";
    case TC_TUPLA:
        return "retorno multiplo nao atravessa a fronteira C: devolva uma struct ou grave por ponteiro (^T)";
    case TC_PROC: return MotivoDoProc(s, tipo, buf, cap);
    case TC_MAPA:
        return "map nao atravessa a fronteira C: a caixa JSON de DLL chega em versao futura; exporte procs que consultam o map";
    case TC_BIT_SET:
        return "bit_set nao atravessa a fronteira C nesta versao (a caixa JSON de DLL chega em versao futura): passe os membros como bool ou monte um u64 com has(s, key = ...)";
    case TC_SOA:
        return "soa nao atravessa a fronteira C: passe cada coluna como dados: ^T e n: u64 (&a.campo[0], len(a) as u64); a caixa JSON de DLL chega em versao futura";
    case TC_MATRIZ:
        return "matrix nao atravessa a fronteira C nesta versao (a caixa JSON de DLL chega em versao futura): passe ^f32 de &m[0, 0] (ordem por coluna, R*C elementos)";
    case TC_PONTEIRO: case TC_ANULAVEL:
        if (EhArenaOuPool(s, (TipoId)info->a)) return MOTIVO_ARENA_POOL;
        if (TipoClasseDe(&s->tipos, (TipoId)info->a) == TC_STRUCT) return NULL; /* opaca no .h */
        return MotivoForaDaAbi(s, (TipoId)info->a, false, buf, cap);
    default: return "tipo sem forma fixa em C";
    }
}

/* v1.12 P21v-a: o E0909 do proc como valor. NULL = a assinatura de 'tipo' (TC_PROC) atravessa a fronteira C; buf >= 256 bytes. */
const char *SemMotivoDoProc(Semantica *s, TipoId tipo, char *buf, size_t cap)
{
    return MotivoDoProc(s, tipo, buf, cap);
}

static const char MOTIVO_PROC_EXPORT[] = "tipo proc(...) nao sai pela DLL: o .h nao tem ponteiro de funcao da Forja; exporte o proc e deixe o chamador guardar o endereco";

static bool ParametroExportavel(Semantica *s, const Decl *d, const Param *p)
{
    char tipo[96], buf[256];
    const char *motivo = TipoClasseDe(&s->tipos, p->tipo.tipo) == TC_PROC ? MOTIVO_PROC_EXPORT
                                                                         : MotivoForaDaAbi(s, p->tipo.tipo, false, buf, sizeof buf);

    if (motivo == NULL) return true;
    SemDescrever(s, p->tipo.tipo, tipo, sizeof tipo);
    return SemErro(s, E0901, p->pos, "o parametro '%s' do proc exportado '%s' tem tipo '%s', que nao atravessa a fronteira C: %s",
                   SemNomeTexto(s, p->nome), SemNomeTexto(s, d->nome), tipo, motivo);
}

static bool RetornoExportavel(Semantica *s, const Decl *d)
{
    char tipo[96], buf[256];
    const char *motivo;

    if (!d->tem_retorno) return true;
    if (d->retorno.tipo == TIPO_CSTR || (TipoClasseDe(&s->tipos, d->retorno.tipo) == TC_OPCIONAL && TipoOpcionalNicho(&s->tipos, d->retorno.tipo))) {
        return true; /* D20: os returns sao conferidos em SemReturn */
    }
    motivo = TipoClasseDe(&s->tipos, d->retorno.tipo) == TC_PROC ? MOTIVO_PROC_EXPORT : MotivoForaDaAbi(s, d->retorno.tipo, true, buf, sizeof buf);
    if (motivo == NULL) return true;
    SemDescrever(s, d->retorno.tipo, tipo, sizeof tipo);
    return SemErro(s, E0901, d->retorno.pos, "o retorno do proc exportado '%s' tem tipo '%s', que nao atravessa a fronteira C: %s",
                   SemNomeTexto(s, d->nome), tipo, motivo);
}

/* Chamado depois que os tipos do proc foram resolvidos (DeclProc). */
bool SemChecarExportado(Semantica *s, const Decl *d)
{
    if (!d->exportado) return true;
    if (NomeProibido(SemNomeTexto(s, d->nome))) {
        return SemErro(s, E0902, d->pos, "o nome '%s' nao pode ser exportado: e palavra ou nome do C, ou comeca com fj_ ou forja_",
                       SemNomeTexto(s, d->nome));
    }
    for (uint32_t i = 0; i < d->nparams; i++) {
        if (!ParametroExportavel(s, d, &d->params[i])) return false;
    }
    return RetornoExportavel(s, d);
}

/* ---------- foreign ---------- */

/* Posicao do 1o caractere que nao vale em nome de biblioteca (1o: letra ou digito; demais: letra, digito, '_', '.', '+', '-'); -1 = todos valem. */
static int CaractereInvalidoDeBiblioteca(const char *nome, uint32_t tam)
{
    for (uint32_t k = 0; k < tam; k++) {
        unsigned char c = (unsigned char)nome[k];
        bool alnum = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        bool outro = c == '_' || c == '.' || c == '+' || c == '-';
        if (!alnum && !(k > 0 && outro)) return (int)k;
    }
    return -1;
}

/* Tamanho da extensao proibida no fim do nome (.dll, .a, .lib, .so); 0 = sem extensao. */
static uint32_t ExtensaoDeBiblioteca(const char *nome, uint32_t tam)
{
    static const char *const EXTENSOES[] = {".dll", ".a", ".lib", ".so"};
    for (size_t k = 0; k < sizeof EXTENSOES / sizeof EXTENSOES[0]; k++) {
        uint32_t t = (uint32_t)strlen(EXTENSOES[k]);
        if (tam > t && strcmp(nome + tam - t, EXTENSOES[k]) == 0) return t;
    }
    return 0;
}

/* Nome de 'link "x"': vira -lx no gcc, entao o .fj nunca injeta caminho, extensao nem opcao (E0905). */
static bool NomeDeBibliotecaValido(Semantica *s, const Decl *d)
{
    const char *nome = SemNomeTexto(s, d->nome);
    uint32_t tam = InternerTam(s->nomes, d->nome);
    if (tam == 0) return SemErro(s, E0905, d->pos, "o nome da biblioteca esta vazio: escreva link \"m\"");
    if (tam > 64) return SemErro(s, E0905, d->pos, "o nome da biblioteca passa de 64 bytes");
    int ruim = CaractereInvalidoDeBiblioteca(nome, tam);
    if (ruim >= 0) {
        unsigned char c = (unsigned char)nome[ruim];
        return SemErro(s, E0905, d->pos,
                       "o nome da biblioteca '%s' tem o caractere '%c', que nao vale: so letras, digitos, '_', '.', '+' e '-' (o 1o e letra ou digito); caminho e pasta nao entram: a biblioteca fica na pasta do .fj ou nas pastas do gcc",
                       nome, c >= 0x20 && c <= 0x7E ? c : '?');
    }
    uint32_t ext = ExtensaoDeBiblioteca(nome, tam);
    if (ext > 0) return SemErro(s, E0905, d->pos, "escreva so o nome, sem extensao: link \"%.*s\"", (int)(tam - ext), nome);
    return true;
}

static bool TipoDeForeignAtravessa(Semantica *s, TipoId tipo, uint32_t pos, const char *oque, const char *proc)
{
    char texto[96], buf[256];
    s->abi_foreign = true;
    const char *motivo = MotivoForaDaAbi(s, tipo, false, buf, sizeof buf);
    s->abi_foreign = false;

    if (motivo == NULL) return true;
    SemDescrever(s, tipo, texto, sizeof texto);
    return SemErro(s, E0904, pos, "%s do proc foreign '%s' tem tipo '%s', que nao atravessa a fronteira C: %s", oque, proc, texto, motivo);
}

bool ErroSimboloCDeAlias(Semantica *s, int32_t nome, uint32_t pos)
{
    SemErro(s, E0202, pos, "'%s' e o simbolo C de um proc foreign com alias (o C gerado teria dois fj_%s); use outro nome",
            SemNomeTexto(s, nome), SemNomeTexto(s, nome));
    SemSugerirRenomearProc(s, nome, pos);
    return false;
}

static bool SimboloCEhIdentificador(const char *texto, uint32_t tam)
{
    if (tam == 0 || !(isalpha((unsigned char)texto[0]) || texto[0] == '_')) return false;
    for (uint32_t i = 1; i < tam; i++) {
        if (!(isalnum((unsigned char)texto[i]) || texto[i] == '_')) return false;
    }
    return true;
}

/* IA1-L8: o simbolo C de alias e por modulo ([modulo * qtd_nomes + id]); a tabela nasce no 1o alias (a arena vem zerada). NULL com criar = false e sem tabela. */
static uint8_t *SlotDoSimboloC(Semantica *s, uint8_t modulo, int32_t simbolo, bool criar)
{
    if (s->simbolo_c_usado == NULL) {
        if (!criar) return NULL;
        s->simbolo_c_usado = ArenaAlocar(s->arena, (size_t)s->qtd_nomes * (s->qtd_modulos > 1 ? s->qtd_modulos : 1));
    }
    return &s->simbolo_c_usado[(s->qtd_modulos > 1 ? (size_t)modulo * s->qtd_nomes : 0) + (size_t)simbolo];
}

/* O nome (de proc) e o simbolo C do alias de algum foreign do mesmo modulo. */
bool SemSimboloCEhAlias(Semantica *s, uint8_t modulo, int32_t nome)
{
    const uint8_t *usado = SlotDoSimboloC(s, modulo, nome, false);
    return usado != NULL && *usado != 0;
}

/* v1.10 (A6): 'proc c_remove = "remove"(...)'. O simbolo C e um identificador, nao colide com outro nome de topo (o C gerado
 * tem fj_<simbolo>) e nao e o simbolo de outro alias. 'proc foo = "foo"' e valido (alias igual ao nome). */
static bool ConferirAliasDoSimbolo(Semantica *s, const Decl *d)
{
    const char *texto = SemNomeTexto(s, d->simbolo);
    if (!SimboloCEhIdentificador(texto, InternerTam(s->nomes, d->simbolo))) {
        return SemErro(s, E0905, d->pos_simbolo, "o simbolo C \"%s\" nao e um identificador C (letras, digitos e '_', sem comecar por digito)", texto);
    }
    if (d->simbolo == d->nome) return true;
    const Simbolo *y = &s->globais[d->simbolo];
    if (d->modulo == 0 && y->categoria != SIM_NENHUM && y->categoria != SIM_PROC_EMBUTIDO) { /* IA1-L8: so a raiz tem nome Forja sem prefixo; o C do modulo tem fj_<L><tag>_ */
        SemErro(s, E0202, d->pos_simbolo, "o simbolo C \"%s\" ja e o nome de um tipo, constante ou proc da Forja (o C gerado teria dois fj_%s)", texto, texto);
        SemSugerirSoDica(s, E0202, "renomeie o tipo, a constante ou o proc da Forja que usa o simbolo C do alias");
        return false;
    }
    uint8_t *usado = SlotDoSimboloC(s, d->modulo, d->simbolo, true);
    if (*usado != 0) {
        SemErro(s, E0202, d->pos_simbolo, "o simbolo C \"%s\" ja tem alias em outro proc foreign", texto);
        SemSugerirSoDica(s, E0202, "um simbolo C tem um alias so: tire este proc e chame o que ja tem alias");
        return false;
    }
    *usado = 1;
    return true;
}

/* R0 [D-REDE-1]: ^T e ?^T para struct opaca de foreign sao o mesmo ponteiro no C, mesmo com opacos de modulos diferentes. */
static bool PonteiroOpacoEquivalente(Semantica *s, TipoId a, TipoId b)
{
    const TipoInfo *ka = TipoDe(&s->tipos, a);
    const TipoInfo *kb = TipoDe(&s->tipos, b);
    if (ka->classe != kb->classe) return false;
    if (ka->classe != TC_PONTEIRO && ka->classe != TC_ANULAVEL) return false;
    return SemTipoEhOpaco(s, ka->a) && SemTipoEhOpaco(s, kb->a);
}

/* F4: parametros e retorno de dois protos do mesmo simbolo C coincidem: TipoId igual (hash-consing) ou equivalencia de ABI (R0). */
static bool TipoIgualAbi(Semantica *s, TipoId a, TipoId b)
{
    return a == b || PonteiroOpacoEquivalente(s, a, b);
}

static bool AssinaturasIguais(Semantica *s, const Decl *a, const Decl *b)
{
    if (a->nparams != b->nparams || a->tem_retorno != b->tem_retorno) return false;
    for (uint32_t i = 0; i < a->nparams; i++) {
        if (!TipoIgualAbi(s, a->params[i].tipo.tipo, b->params[i].tipo.tipo)) return false;
    }
    return !a->tem_retorno || TipoIgualAbi(s, a->retorno.tipo, b->retorno.tipo);
}

/* F4: o mesmo simbolo C declarado em 2+ modulos vale com assinatura identica; diferente = E0202 com os dois protos (um simbolo, dois ABIs). */
static bool ConferirAssinaturaDoSimboloC(Semantica *s, const Decl *d)
{
    int32_t simbolo = d->simbolo >= 0 ? d->simbolo : d->nome;
    if (s->simbolo_c_decl == NULL) s->simbolo_c_decl = ArenaAlocar(s->arena, (size_t)s->qtd_nomes * sizeof(const Decl *));
    const Decl *antes = s->simbolo_c_decl[simbolo];
    if (antes == NULL) {
        s->simbolo_c_decl[simbolo] = d;
        return true;
    }
    if (antes == d || antes->modulo == d->modulo || AssinaturasIguais(s, antes, d)) return true;
    const char *texto = SemNomeTexto(s, simbolo);
    const char *onde = antes->modulo == 0 ? "o arquivo raiz" : SemNomeTexto(s, s->modulos->itens[antes->modulo].apelido);
    SemErro(s, E0202, d->simbolo >= 0 ? d->pos_simbolo : d->pos,
            "o simbolo C \"%s\" ja e declarado como proc '%s' (%s) com outra assinatura; o mesmo simbolo C em varios modulos exige parametros e retorno identicos",
            texto, SemNomeTexto(s, antes->nome), onde);
    SemSugerirSoDica(s, E0202, "copie a assinatura do proc foreign que ja declara esse simbolo C, ou use outro simbolo");
    return false;
}

/* Chamado depois que os tipos do proc foreign foram resolvidos (DeclProc). */
bool SemChecarExterno(Semantica *s, const Decl *d)
{
    const char *nome = SemNomeTexto(s, d->nome);
    const char *simbolo = d->simbolo >= 0 ? SemNomeTexto(s, d->simbolo) : nome; /* v1.10: o que vai ao C */
    char oque[96];

    s->tem_externo = true;
    bool da_std = d->modulo != 0 && s->modulos->itens[d->modulo].origem == MOD_STD; /* foreign interno da std: forja_* vale (D13) */
    if (strcmp(simbolo, "main") == 0 || strncmp(simbolo, "fj_", 3) == 0 || (!da_std && strncmp(simbolo, "forja_", 6) == 0)) {
        return SemErro(s, E0905, d->simbolo >= 0 ? d->pos_simbolo : d->pos,
                       "o proc foreign '%s' colidiria com o C gerado: 'main' e nomes que comecam com fj_ ou forja_ sao do compilador", simbolo);
    }
    if (d->simbolo >= 0 && !ConferirAliasDoSimbolo(s, d)) return false;
    for (uint32_t i = 0; i < d->nparams; i++) {
        snprintf(oque, sizeof oque, "o parametro '%s'", SemNomeTexto(s, d->params[i].nome));
        if (!TipoDeForeignAtravessa(s, d->params[i].tipo.tipo, d->params[i].pos, oque, nome)) return false;
    }
    if (d->tem_retorno && !TipoDeForeignAtravessa(s, d->retorno.tipo, d->retorno.pos, "o retorno", nome)) return false;
    return ConferirAssinaturaDoSimboloC(s, d);
}

/* v1.8 (E1004): 'export' e 'main' so valem no arquivo raiz; o modulo importado e uma biblioteca do programa. */
static bool ConferirSoNaRaiz(Semantica *s, const Decl *d)
{
    if (d->modulo == 0) return true;
    if (d->exportado) {
        return SemErro(s, E1004, d->pos, "'export' so vale no arquivo raiz (a DLL exporta os procs do arquivo que o forja recebe)");
    }
    if (d->tipo_no == D_PROC && d->nome == SYM_MAIN) {
        return SemErro(s, E1004, d->pos, "'main' so vale no arquivo raiz; este arquivo e o modulo \"%s\"",
                       SemNomeTexto(s, s->modulos->itens[d->modulo].caminho));
    }
    return true;
}

/* 'export' pede 'pub' e so vale em proc; 'pub' nao vale em test; 'export' nao vale em foreign (E0906); nome de biblioteca (E0905);
 * 'export' e 'main' so no arquivo raiz (E1004). Roda antes de qualquer outra checagem de declaracao. */
bool SemChecarModificadores(Semantica *s, uint32_t *categorias)
{
    uint32_t presentes = 0;
    for (const Decl *d = s->programa->primeira; d != NULL; d = d->prox) {
        presentes |= 1u << d->tipo_no;
        if ((d->externo || d->tipo_no == D_LINK) && d->exportado) {
            return SemErro(s, E0906, d->pos,
                           "'export' nao vale em foreign: quem exporta e a Forja, por 'pub export proc'; 'pub foreign' deixa os procs visiveis a quem importa o modulo");
        }
        if (!ConferirSoNaRaiz(s, d)) return false;
        if (d->tipo_no == D_LINK && !NomeDeBibliotecaValido(s, d)) return false;
        if (d->exportado && !d->publico)
            return SemErro(s, E0903, d->pos, "'export' precisa de 'pub' antes: escreva 'pub export proc %s'", SemNomeTexto(s, d->nome));
        if (d->exportado && d->tipo_no != D_PROC)
            return SemErro(s, E0903, d->pos, "'export' so vale em proc; esta declaracao nao e um proc");
        if (d->publico && d->tipo_no == D_TEST)
            return SemErro(s, E0903, d->pos, "'pub' nao vale em test");
    }
    *categorias = presentes; /* bit por DeclTipo: o passe A pula as categorias ausentes */
    return true;
}
