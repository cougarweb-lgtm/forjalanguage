/*
 * rt_proc_win.c - camada sys do std:proc no Windows x64: linha de comando e variavel de ambiente em UTF-8 (M5 P26b).
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - M5 P26b: sys_argumentos (GetCommandLineW + WideCharToMultiByte; divide por rt_linha.c) e sys_ambiente_utf8 (MultiByteToWideChar + GetEnvironmentVariableW):
 *           do 18o ao 21o import de kernel32 (ligar.c, KERNEL32_M1). Mora na unidade do rt_win_ryu.o (rt_ryu.c), NAO no rt_win.c: o gcc decide o que inlinar pelo tamanho da
 *           unidade, e mais codigo no rt_win.o mudava os bytes de exes release que nao importam std:proc (D-M5-11). O ligador so junta este objeto a quem chama forja_proc_*.
 *
 * Prototipos a mao, sem windows.h. Usa sys_heap_alocar e sys_heap_liberar do rt_win.o (globais, declaradas em rt_sys.h).
 * Incluido por rt_ryu.c depois de rt_linha.c; nao compila sozinho.
 */
#include "rt_sys.h"

__declspec(dllimport) unsigned short *GetCommandLineW(void);
__declspec(dllimport) int WideCharToMultiByte(unsigned int pagina, unsigned long flags, const unsigned short *larga, int nlarga, char *destino, int ndestino,
                                              const char *padrao, int *usou_padrao);
__declspec(dllimport) int MultiByteToWideChar(unsigned int pagina, unsigned long flags, const char *origem, int norigem, unsigned short *larga, int nlarga);
__declspec(dllimport) unsigned long GetEnvironmentVariableW(const unsigned short *nome, unsigned short *destino, unsigned long n);

#define RT_PROC_PAGINA_UTF8 65001U /* CP_UTF8 */
/* Nome de variavel de ambiente em UTF-16 (o Windows limita nome a 32767; aqui 512 bastam, nome maior e "nao existe"). */
#define RT_PROC_NOME_AMBIENTE_MAX 512

/* Uma so alocacao: argv (ponteiros), a linha em UTF-8 e o buffer dos argumentos (capacidades pedidas por rt_linha_dividir). */
void sys_argumentos(int64_t *argc, char ***argv)
{
    const unsigned short *larga = GetCommandLineW();
    int tam = WideCharToMultiByte(RT_PROC_PAGINA_UTF8, 0, larga, -1, 0, 0, 0, 0); /* com o NUL */
    char *bloco;
    char *linha;

    *argc = 0;
    *argv = 0;
    if (tam <= 0)
        return;
    bloco = sys_heap_alocar(((int64_t)tam + 2) * 8 + 3 * (int64_t)tam + 2);
    if (bloco == 0)
        return;
    linha = bloco + ((int64_t)tam + 2) * 8;
    if (WideCharToMultiByte(RT_PROC_PAGINA_UTF8, 0, larga, -1, linha, tam, 0, 0) <= 0) {
        sys_heap_liberar(bloco);
        return;
    }
    *argv = (char **)bloco;
    *argc = rt_linha_dividir(linha, linha + tam, *argv);
}

/* Converte o valor (qtd unidades UTF-16, sem NUL) para UTF-8; so grava em destino quando cabe com o NUL. Devolve o tamanho em bytes. */
static int64_t sys_ambiente_converter(const unsigned short *valor, unsigned long qtd, char *destino, int64_t n)
{
    int tam = qtd == 0 ? 0 : WideCharToMultiByte(RT_PROC_PAGINA_UTF8, 0, valor, (int)qtd, 0, 0, 0, 0);

    if (tam < 0)
        return -1;
    if (n > tam) {
        if (tam > 0)
            WideCharToMultiByte(RT_PROC_PAGINA_UTF8, 0, valor, (int)qtd, destino, tam, 0, 0);
        destino[tam] = '\0';
    }
    return tam;
}

int64_t sys_ambiente_utf8(const char *nome, char *destino, int64_t n)
{
    unsigned short nome_larga[RT_PROC_NOME_AMBIENTE_MAX];
    unsigned short *valor;
    unsigned long qtd;
    int64_t tam;

    if (MultiByteToWideChar(RT_PROC_PAGINA_UTF8, 0, nome, -1, nome_larga, RT_PROC_NOME_AMBIENTE_MAX) <= 0)
        return -1;
    qtd = GetEnvironmentVariableW(nome_larga, 0, 0); /* tamanho com o NUL; 0 = nao existe (variavel vazia da 1) */
    if (qtd == 0)
        return -1;
    valor = sys_heap_alocar((int64_t)qtd * 2);
    if (valor == 0)
        return -1;
    tam = sys_ambiente_converter(valor, GetEnvironmentVariableW(nome_larga, valor, qtd), destino, n);
    sys_heap_liberar(valor);
    return tam;
}
