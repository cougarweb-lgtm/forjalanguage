/*
 * forja_rt_nativo.h - interface publica do runtime nativo da Forja (Windows x64): o que o codigo gerado pelo backend x64 chama
 * e o que o ligador espera achar em rt_win.o.
 *
 * Versao: 0.21.0
 * Changelog:
 *   0.21.0 - M1: forja_mapa_{achar,inserir,remover}_{k4,k8} (rt_mapa.c 0.1.3): chave inteira de 4 ou 8 bytes por valor.
 *   0.20.0 - M6e P93k: forja_lista_crescer_alvo (so FORJA_DEV; o BIR a chama com --guarda-memoria).
 *   0.19.0 - IA1-L5a: forja_fmt_buffer (rt_formata.c), forja_fmt_imprimir_erro (eprint; rt_saida_erro.c), forja_fmt_f32_curto e forja_fmt_f64_curto (print de real; rt_real_curto.c).
 *   0.18.0 - M5 P26c: forja_arquivo_abrir_ler, _criar, _tamanho, _ler, _gravar e _fechar (rt_arquivo.c; std:arquivo).
 *   0.17.0 - M5 P26b: forja_proc_argumentos, forja_proc_ambiente e forja_proc_sair (rt_proc.c; std:proc).
 *   0.16.0 - M3 P22a: forja_entrada_dll (DllMain da DLL Forja: ATTACH inicia a saida, DETACH descarrega o stdout e, por FreeLibrary em dev, relata o R0009).
 *   0.15.0 - P18a: forja_traco (traco do diferencial, so chamado por programa construido com --traco).
 *   0.14.0 - M2 P17b: forja_rt_layout de 23 palavras, versao 3: acrescenta o map (sizeof forja_mapa e offsets de chaves, valores, len e epoca) e o cabecalho da soa dinamica de 1 a 8 colunas
 *            (forja_soa_cauda, FORJA_SOA_CABECALHO).
 *   0.13.0 - M2 P16: forja_rt_conta_bits (popcount do bit_set por SWAR em C puro; servico so do rt nativo: o backend C usa __builtin_popcountll).
 *   0.12.0 - M2 P15b: forja_fmt_* (rt_formata.c): inicio, reservar, bytes, alinhado, str, cstr, bool, i64, u64, f32, f64, cstr_final, str_final e imprimir, com as assinaturas do forja_rt.h 0.10.0.
 *   0.11.0 - M2 P14b: forja_rt_layout de 10 palavras, versao 2 (str e [dynamic]T: forja_lista_layout, FORJA_RT_LAYOUT_PALAVRAS); a versao 0.10.0 acima e do P17a (feito em paralelo).
 *   0.10.0 - M2 P17a: soa dinamica (rt_soa.c) e map (rt_mapa.c): forja_mapa de 56 B e as funcoes publicas, com as assinaturas do forja_rt.h 0.10.0.
 *   0.9.0 - M2 P14a: alocacao (rt_aloc.c): heap, frame, listas, Arena e Pool, R0007/R0008/R0009 (assinaturas do forja_rt.h 0.10.0; forja_falha_alocacao recebe int64_t).
 *   0.8.0 - M2 P13b: forja_falha_indice, forja_fatia_confere e forja_vetor_c_confere (assinaturas do forja_rt.h 0.10.0).
 *   0.7.0 - M2 P13a: forja_str_igual e forja_str_de_cstr (assinaturas do forja_rt.h 0.10.0).
 *   0.6.0 - M2 P12b: ForjaTesteFn, forja_teste_registrar e forja_testes_rodar (rt_teste.c); forja_expect passa a valer tambem dentro de teste (relata e segue).
 *   0.5.0 - M2 P12a: forja_assert e forja_expect (fora de teste vira panic R0003), com as assinaturas do forja_rt.h 0.9.1.
 *   0.4.0 - M2 P11b: forja_rt_layout e FORJA_RT_LAYOUT_VERSAO (layout do forja_str conferido pelo compilador).
 *   0.3.0 - M2 P11a: forja_faixa, forja_faixa_u64 (R0001), forja_falha_nulo e forja_nao_nulo (R0005), com as assinaturas do forja_rt.h 0.9.1.
 *   0.2.0 - M2 P10b: forja_print_f32, forja_falha_conversao_f e forja_as_<S>_de_f64 (rt_real.c; funcoes globais, o codigo gerado chama por simbolo;
 *           com FORJA_DEV a faixa do forja_rt.h confere e falha com R0002, sem FORJA_DEV satura).
 *   0.1.0 - P7b do M1: header publico (impressao, falhas, panic, saida, handlers de saida e entrada), com os nomes e as assinaturas
 *           do forja_rt.h 0.9.0; incluido por rt_nativo.c para o compilador conferir cada definicao contra esta lista.
 * Verificacao: construir.bat (rt_win.o compila com -Werror) e python -I ..\tests\nativo\rt\verificar_rt_nativo.py
 */
#ifndef FORJA_RT_NATIVO_H
#define FORJA_RT_NATIVO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* str da Forja: mesmo layout do forja_rt.h (ponteiro e tamanho em bytes). */
typedef struct forja_str {
    const char *ptr;
    int64_t len;
} forja_str;

/* [dynamic]T da Forja: {ptr, len, cap, alocador, epoca} (32 B); so o layout interessa ao compilador, o tipo do elemento nao. */
typedef struct forja_lista_layout {
    void *ptr;
    int64_t len, cap;
    uint32_t alocador, epoca;
} forja_lista_layout;

/* Cauda do cabecalho da soa[dynamic]S: depois dos ncol ponteiros de coluna (void *col[ncol]) vem len, cap, alocador e epoca (8 * ncol + 24 B); so o tamanho interessa ao compilador. */
typedef struct forja_soa_cauda {
    int64_t len, cap;
    uint32_t alocador, epoca;
} forja_soa_cauda;
#define FORJA_SOA_CABECALHO(ncol) ((uint32_t)(sizeof(void *) * (ncol) + sizeof(forja_soa_cauda)))

/* Layout do str, do [dynamic]T, do map e da soa[dynamic]S neste runtime, em .rdata (23 palavras): {versao, sizeof str, alignof str, offsetof(str.len), sizeof lista, alignof lista,
 * offsetof(len), offsetof(cap), offsetof(alocador), offsetof(epoca), sizeof forja_mapa, offsetof(chaves), offsetof(valores), offsetof(len), offsetof(epoca),
 * cabecalho da soa dinamica de 1 a 8 colunas (8 * ncol + 24)}. O compilador le o simbolo pelo COFF e confere com o layout\ (X0906 se divergir): o codigo gerado so le e grava o
 * map e a soa nos offsets conferidos aqui (ele nunca assume um endereco do runtime). */
#define FORJA_RT_LAYOUT_VERSAO 3u
#define FORJA_RT_LAYOUT_PALAVRAS 23u
extern const uint32_t forja_rt_layout[FORJA_RT_LAYOUT_PALAVRAS];

/* ---- impressao em stdout (uma linha por chamada; "\n" sai como CRLF) ---- */
void forja_print_i32(int32_t valor);
void forja_print_i64(int64_t valor);
void forja_print_u64(uint64_t valor);
void forja_print_bool(bool valor);
void forja_print_texto(const char *texto);
void forja_print_str(forja_str s);

/* ---- str: == por conteudo e 'cstr as str' (dev: NULL e R0005; release: NULL da a str vazia) ---- */
bool forja_str_igual(forja_str a, forja_str b);
forja_str forja_str_de_cstr(const char *c, const char *arquivo, int linha);

/* ---- falhas de execucao: JSON de uma linha em stderr e saida com 101 ---- */
_Noreturn void forja_falha(const char *codigo, const char *causa, const char *arquivo, int linha);
_Noreturn void forja_panic(const char *msg, const char *arquivo, int linha);
_Noreturn void forja_falha_estouro(const char *operacao, int64_t a, int64_t b, const char *arquivo, int linha);
_Noreturn void forja_falha_estouro_u(const char *operacao, uint64_t a, uint64_t b, const char *arquivo, int linha);
_Noreturn void forja_falha_conversao_i(int64_t valor, const char *alvo, const char *arquivo, int linha);
_Noreturn void forja_falha_conversao_u(uint64_t valor, const char *alvo, const char *arquivo, int linha);
_Noreturn void forja_falha_conversao_f(double valor, const char *alvo, const char *arquivo, int linha);
void forja_main_erro(const char *arquivo, int erro);
void forja_assert(bool cond, const char *msg, const char *arquivo, int linha);
void forja_expect(bool cond, const char *texto_cond, const char *arquivo, int linha);

/* ---- modo teste (forja test --backend x64): o forja_programa registra cada test e chama forja_testes_rodar ---- */
typedef void (*ForjaTesteFn)(void);
/* Registra um teste (nome = o titulo). A tabela tem 4096 entradas; a 4097a e R0003. */
void forja_teste_registrar(const char *nome, ForjaTesteFn fn);
/* Roda os testes na ordem do registro: "ok   <nome>" ou "FALHOU <nome>" em stdout e o resumo "<n> testes, <f> falharam". Uma falha de runtime dentro do teste
 * (R0001..R0006) so o reprova e o proximo roda. Devolve 0 se todos passaram, 1 se algum falhou. */
int forja_testes_rodar(void);

/* ---- faixa de campo (R0001) e ponteiro nulo (R0005): o codigo gerado em dev chama so quando a conta ja falhou (bloco frio) ---- */
int64_t forja_faixa(int64_t valor, int64_t lo, int64_t hi, bool exclusiva, const char *campo, const char *arquivo, int linha);
uint64_t forja_faixa_u64(uint64_t valor, uint64_t lo, uint64_t hi, bool exclusiva, const char *campo, const char *arquivo, int linha);
_Noreturn void forja_falha_nulo(const char *oque, const char *arquivo, int linha);
void *forja_nao_nulo(const void *p, const char *oque, const char *arquivo, int linha);

/* ---- indice, fatia e slice_from (R0006 e R0005): o codigo gerado chama so em dev (release nao confere) ---- */
/* a[i] fora de 0..<n: chamada do bloco frio (R0006, "indice I fora de 0..<N"). */
_Noreturn void forja_falha_indice(int64_t i, int64_t n, const char *arquivo, int linha);
/* a[lo..<hi] (exclusiva) ou a[lo..=hi]: volta se os limites estao em 0..=n e lo ate hi; senao R0006 ("fatia LO..<HI fora de 0..=N"). */
void forja_fatia_confere(int64_t lo, int64_t hi, bool exclusiva, int64_t n, const char *arquivo, int linha);
/* slice_from(p, len = n): volta se n >= 0 e (p != nulo ou n == 0); n < 0 e R0006, p nulo com n > 0 e R0005. */
void forja_vetor_c_confere(const void *p, int64_t n, const char *arquivo, int linha);

/* ---- alocacao (rt_aloc.c; rt_rastreio.c em dev): R0007 sem memoria, R0008 handle/liberar/Pool/frame, R0009 vazamento no fim (so dev) ---- */
#define FORJA_R0007 "R0007"
#define FORJA_R0008 "R0008"
#define FORJA_R0009 "R0009"
#define FORJA_ALOC_HEAP 1u
#define FORJA_ALOC_FRAME 2u
/* "<oque> (<valor>)" em R0007. */
_Noreturn void forja_falha_alocacao(const char *oque, int64_t valor, const char *arquivo, int linha);
/* alocador: FORJA_ALOC_HEAP, FORJA_ALOC_FRAME ou o handle de uma Arena/Pool. Dev rastreia o heap (80 B de cabecalho desde o P70); release nao. */
void *forja_mem_alocar(uint32_t alocador, int64_t bytes, const char *arquivo, int linha);
void *forja_heap_alocar(int64_t bytes, const char *arquivo, int linha);
void *forja_heap_realocar(void *velho, int64_t bytes_novo, const char *arquivo, int linha);
/* delete: NULL e ponteiro do frame nao fazem nada; dev confere a marca (R0008). */
void forja_mem_liberar(void *p, const char *arquivo, int linha);
bool forja_no_frame(const void *p);
void *forja_frame_alocar(int64_t bytes, const char *arquivo, int linha);
void *forja_frame_realocar(void *velho, int64_t bytes_velho, int64_t bytes_novo, const char *arquivo, int linha);
void forja_frame_reset(void);
/* Listas [dynamic]T: cap 0 vira ptr nulo; crescer: cap 0..3 vira 8, depois dobra; epoca so no frame. */
void *forja_lista_criar(uint32_t alocador, int64_t cap, int64_t tam_elem, uint32_t *epoca, const char *arquivo, int linha);
void *forja_lista_crescer(void *ptr, int64_t len, int64_t *cap, int64_t tam_elem, uint32_t alocador, uint32_t *epoca, const char *arquivo, int linha);
void forja_lista_liberar(void *ptr, uint32_t alocador, const char *arquivo, int linha);
#ifdef FORJA_DEV
void *forja_lista_crescer_alvo(forja_lista_layout *lista, int64_t tam_elem, int64_t alvo, const char *arquivo, int linha); /* M6e P93k: so com --guarda-memoria */
#endif
/* Arena e Pool: handle = (geracao << 12) | idx. */
uint32_t forja_arena_criar(int64_t bloco);
void forja_arena_limpar(uint32_t h);
void forja_arena_destruir(uint32_t h);
int64_t forja_arena_usado(uint32_t h);
uint32_t forja_pool_criar(int64_t por_bloco);
void forja_pool_devolver(uint32_t h, void *p, const char *arquivo, int linha);
void forja_pool_destruir(uint32_t h);
int64_t forja_pool_vivos(uint32_t h);
/* Blocos de heap vivos: so dev rastreia (release devolve 0). */
int64_t forja_rt_vivos_qtd(void);
#ifdef FORJA_DEV
/* std:mem snapshot/report_since (P71a, rt_rastreio.c): so dev; em release o compilador tira as chamadas. */
uint32_t forja_memsnap_serie(void);
int64_t forja_memsnap_vivos(void);
int64_t forja_memsnap_bytes(void);
void forja_memsnap_relatar(uint32_t serie, int64_t vivos_antes, int64_t bytes_antes);
/* std:mem track_handle/untrack_handle (P71b, rt_handles.c): so dev; em release o compilador tira as chamadas. */
void forja_handle_track(const char *kind, void *h);
void forja_handle_untrack(void *h);
/* Foreign com clausula creates/destroys/takes (M6c P88, rt_handles.c): o codigo gerado chama em dev; em release o compilador nem emite a chamada. */
void forja_handle_nasce(const char *kind, void *h, int32_t indireto, const char *destruidor);
void forja_handle_solta(void *h);
/* Foreign com clausula `ends` (M6d P92e, rt_libmem.c): o codigo gerado chama em dev depois do proc que encerra a lib; sem tabela de lib registrada nao faz nada. */
void forja_libmem_encerrou(const char *simbolo);
/* Lista do frame usada depois do frame_reset e R0008; indice de lista com a mesma conferencia. */
void forja_lista_viva(uint32_t epoca, const char *arquivo, int linha);
int64_t forja_indice_lista(int64_t i, int64_t n, uint32_t epoca, const char *arquivo, int linha);
#endif

/* ---- soa dinamica (rt_soa.c): as ncol colunas moram num bloco unico; a coluna k comeca na soma de A16(cap * tams[j]) para j < k ---- */
/* Bytes do bloco de cap linhas, ou -1 se alguma conta passar do limite. */
int64_t forja_soa_total(const int64_t *tams, uint32_t ncol, int64_t cap);
void forja_soa_criar(void **col, const int64_t *tams, uint32_t ncol, int64_t cap, uint32_t alocador, uint32_t *epoca, const char *arquivo, int linha);
void forja_soa_crescer(void **col, const int64_t *tams, uint32_t ncol, int64_t len, int64_t *cap, uint32_t alocador, uint32_t *epoca, const char *arquivo, int linha);
void forja_soa_liberar(void **col, uint32_t alocador, const char *arquivo, int linha);

/* ---- map (rt_mapa.c): chaves e valores densos na ordem de insercao + tabela de slots (sondagem linear, carga <= 1/2); chave str por conteudo ---- */
typedef struct forja_mapa {
    void *chaves;    /* cap chaves de tam_chave bytes, densas, na ordem de insercao */
    void *valores;   /* cap valores de tam_valor bytes, mesmo indice da chave */
    uint32_t *slots; /* nslots entradas: 0 = vazio, senao indice denso + 1 */
    int64_t len;
    int64_t cap;
    uint32_t nslots; /* potencia de 2 >= 2 * cap, minimo 16; 0 sem bloco */
    uint32_t alocador;
    uint32_t epoca; /* quadro do frame (alocador 2), senao 0 */
} forja_mapa;
_Static_assert(sizeof(forja_mapa) == 56, "forja_mapa: 56 bytes");
void forja_mapa_criar(forja_mapa *m, uint32_t alocador, int64_t cap, int64_t tam_chave, int64_t tam_valor, const char *arquivo, int linha);
/* Indice denso da chave ou -1. */
int64_t forja_mapa_achar(const forja_mapa *m, const void *chave, int64_t tam_chave, bool texto, const char *arquivo, int linha);
/* Indice denso da chave (existente ou nova; a nova entra no fim, o valor fica por conta de quem chama). */
int64_t forja_mapa_inserir(forja_mapa *m, const void *chave, int64_t tam_chave, bool texto, int64_t tam_valor, const char *arquivo, int linha);
/* Remove a chave (se existir); o ultimo par denso ocupa o lugar do removido. */
void forja_mapa_remover(forja_mapa *m, const void *chave, int64_t tam_chave, bool texto, int64_t tam_valor, const char *arquivo, int linha);
/* Entradas por valor para chave inteira de 4 ou 8 bytes (so o BIR chama): o mesmo resultado de forja_mapa_<nome> com tam_chave 4 ou 8 e texto falso. */
int64_t forja_mapa_achar_k4(const forja_mapa *m, uint32_t chave, const char *arquivo, int linha);
int64_t forja_mapa_achar_k8(const forja_mapa *m, uint64_t chave, const char *arquivo, int linha);
int64_t forja_mapa_inserir_k4(forja_mapa *m, uint32_t chave, int64_t tam_valor, const char *arquivo, int linha);
int64_t forja_mapa_inserir_k8(forja_mapa *m, uint64_t chave, int64_t tam_valor, const char *arquivo, int linha);
void forja_mapa_remover_k4(forja_mapa *m, uint32_t chave, int64_t tam_valor, const char *arquivo, int linha);
void forja_mapa_remover_k8(forja_mapa *m, uint64_t chave, int64_t tam_valor, const char *arquivo, int linha);
void forja_mapa_liberar(forja_mapa *m, const char *arquivo, int linha);

/* ---- 'as' de real (f32 vai a f64 antes): dev confere a faixa (R0002), release satura (NaN vira 0) ---- */
int8_t forja_as_i8_de_f64(double v, const char *arquivo, int linha);
int16_t forja_as_i16_de_f64(double v, const char *arquivo, int linha);
int32_t forja_as_i32_de_f64(double v, const char *arquivo, int linha);
int64_t forja_as_i64_de_f64(double v, const char *arquivo, int linha);
uint8_t forja_as_u8_de_f64(double v, const char *arquivo, int linha);
uint16_t forja_as_u16_de_f64(double v, const char *arquivo, int linha);
uint32_t forja_as_u32_de_f64(double v, const char *arquivo, int linha);
uint64_t forja_as_u64_de_f64(double v, const char *arquivo, int linha);

/* ---- texto formatado f"...", format e format_c (rt_formata.c): buffer unico; fmt = largura (bits 0..7), precisao (8..12; 0 = %g), zeros (13), hexa (14) ---- */
void forja_fmt_inicio(void);
void forja_fmt_reservar(int64_t n);
void forja_fmt_bytes(const char *p, int64_t n);
void forja_fmt_alinhado(const char *p, int64_t n, uint32_t fmt);
void forja_fmt_str(forja_str s, uint32_t fmt);
void forja_fmt_cstr(const char *p, uint32_t fmt);
void forja_fmt_bool(bool v, uint32_t fmt);
void forja_fmt_i64(int64_t v, uint32_t fmt);
void forja_fmt_u64(uint64_t v, uint32_t fmt);
void forja_fmt_f32(double v, uint32_t fmt); /* f32 chega alargado a double, como no forja_rt.h */
void forja_fmt_f64(double v, uint32_t fmt);
const char *forja_fmt_cstr_final(uint32_t alocador, const char *arquivo, int linha);
forja_str forja_fmt_str_final(uint32_t alocador, const char *arquivo, int linha);
void forja_fmt_imprimir(void);
const char *forja_fmt_buffer(int64_t *len);         /* dados e tamanho do buffer do f"..." (rt_formata.c) */
void forja_fmt_imprimir_erro(void);                  /* eprint: buffer + '\n' em stderr (rt_saida_erro.c, objeto do Ryu) */
void forja_fmt_f32_curto(double v, uint32_t fmt);    /* print de f32 pela forma mais curta que volta igual (rt_real_curto.c, objeto do Ryu) */
void forja_fmt_f64_curto(double v, uint32_t fmt);

/* ---- bit_set (rt_nativo.c): so o rt nativo tem este servico; o backend C emite __builtin_popcountll ---- */
/* Quantidade de bits ligados de v (IR_CONJUNTO op_aux 6); reentrante, sem estado. */
int64_t forja_rt_conta_bits(uint64_t v);

/* ---- traco do diferencial (rt_nativo.c, P18a): so o rt nativo e o preludio do emit_c (EMIT_TRACO) o tem ---- */
/* Escreve em stderr, numa escrita so, "@traco <funcao> <instr> <linha>:<coluna> 0x<16 hex>\n" (funcao cortada em 160 bytes); reentrante, sem estado. */
void forja_traco(const char *funcao, uint32_t instr, uint32_t linha, uint32_t coluna, uint64_t bits);

/* ---- std:proc (rt_proc.c) ---- */
/* Argumentos do processo: ponteiro para um vetor de forja_str (UTF-8, vivo ate o fim do processo) e a quantidade em *quantos; nunca NULL. */
void *forja_proc_argumentos(int64_t *quantos);
/* Valor da variavel de ambiente (UTF-8, copia viva ate o fim); NULL = nao existe, "" = definida e vazia. */
const char *forja_proc_ambiente(const char *nome);
/* Descarrega o stdout e sai com o codigo, sem o relatorio R0009; codigo fora de 0..255 = R0012. Nao volta. */
void forja_proc_sair(int64_t codigo);

/* ---- std:arquivo (rt_arquivo.c) ---- */
/* Abre para ler / cria (ou zera) para gravar, em modo binario; caminho UTF-8. Devolve o identificador (nunca 0) ou 0 com SYS_ARQ_NAO_EXISTE (1), SYS_ARQ_SEM_PERMISSAO (2) ou SYS_ARQ_OUTRO (3) em *erro. */
uint64_t forja_arquivo_abrir_ler(const char *caminho, int32_t *erro);
uint64_t forja_arquivo_criar(const char *caminho, int32_t *erro);
/* Tamanho em bytes (int64_t); negativo = desconhecido. */
int64_t forja_arquivo_tamanho(uint64_t arquivo);
/* Um lote de leitura: quantos bytes leu (0 = fim do arquivo) ou -1; grava os quantos bytes (lotes de 1 GiB) e devolve quantos gravou ou -1; fecha (0 = ok, -1 = falha). */
int64_t forja_arquivo_ler(uint64_t arquivo, uint8_t *destino, int64_t quantos);
int64_t forja_arquivo_gravar(uint64_t arquivo, const uint8_t *origem, int64_t quantos);
int32_t forja_arquivo_fechar(uint64_t arquivo);

/* ---- saida do programa ---- */
/* Registra fn para rodar em forja_sair, na ordem inversa; ate 32 handlers (o 33o e R0003). */
void forja_rt_ao_sair(void (*fn)(void));
/* Roda os handlers, descarrega o stdout e encerra o processo com o codigo. Nao volta. */
_Noreturn void forja_sair(int codigo);
/* Entrada do PE: inicia a saida, chama forja_programa e sai com o codigo dele. */
_Noreturn void forja_entrada(void);
/* Entrada do PE de uma DLL (DllMain): ATTACH inicia a saida; DETACH descarrega o stdout e, por FreeLibrary (reservado == NULL) em dev, relata o R0009 sem sair. Devolve 1. */
int forja_entrada_dll(void *instancia, uint32_t motivo, void *reservado);
/* Fornecido pelo codigo gerado (o ligador resolve contra o simbolo global do programa). */
int forja_programa(void);

#endif
