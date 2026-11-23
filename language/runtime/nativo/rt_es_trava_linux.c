/*
 * rt_es_trava_linux.c - file.lock do std:file no Linux x86-64: o modo FORJA_FS_MODO_TRAVA do forja_fs_abrir por flock (D-G3-L2b).
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - D-G3-L2b: primeira versao (saiu do rt_es_linux.c, que passaria de 800 linhas). open O_WRONLY|O_CREAT (sem zerar, o OPEN_ALWAYS do Windows) + flock 73 LOCK_EX|LOCK_NB; EWOULDBLOCK = OCUPADO.
 *           O flock vale por open file description: um 2o open do MESMO processo conflita, como o share 0 do Windows. O DELETE_ON_CLOSE do Windows nao existe no Linux: o caminho fica no buffer do
 *           slot e o es_liberar (rt_es_linux.c) faz unlink ANTES do close, ainda com o flock. DIVERGENCIA: se o processo morre sem fechar o handle, o arquivo FICA (o Windows o apaga na saida);
 *           quem cria a trava deve tolerar um arquivo velho (a disputa e pelo flock, nao pela existencia). Corrida do unlink: quem abriu o arquivo antes de o dono o apagar, e toma o flock
 *           depois do close dele, ve st_nlink = 0 e reabre (ate ES_TENTATIVAS_TRAVA vezes), para nunca segurar um arquivo ja apagado enquanto outro cria o novo.
 *
 * Incluido por rt_linux_es.c depois de rt_es_linux.c (usa es_abrir_fd, es_guardar e as constantes ES_*); nao compila sozinho.
 */

/* st_nlink do fd (0 = o arquivo foi apagado); 1 se o fstat falha (nao ha como saber: segue como vivo). */
static int64_t es_nlink(int64_t fd)
{
    int64_t st[ES_STAT_PALAVRAS];

    return sysl_chamar2(SYSL_FSTAT, fd, (int64_t)st) == 0 ? st[ES_STAT_NLINK] : 1;
}

/* Abre (cria sem zerar) e toma o flock LOCK_EX|LOCK_NB. fd com o flock, ou -errno (-EAGAIN = outro handle tem o arquivo). */
static int64_t es_abrir_trava(const char *caminho)
{
    for (int32_t tentativa = 0; tentativa < ES_TENTATIVAS_TRAVA; tentativa++) {
        int64_t fd = es_abrir_fd(caminho, ES_O_TRAVA, ES_MODO_NOVO);
        int64_t r;

        if (sysl_falhou(fd))
            return fd;
        r = sysl_chamar2(SYSL_FLOCK, fd, ES_LOCK_EX_NB);
        if (r != 0) {
            sysl_chamar1(SYSL_CLOSE, fd);
            return r;
        }
        if (es_nlink(fd) > 0)
            return fd;
        sysl_chamar1(SYSL_CLOSE, fd);
    }
    return -ES_EAGAIN;
}

/* Guarda o fd da trava com uma copia do caminho no buffer do slot (o es_liberar apaga o arquivo no close). Sem memoria: apaga, fecha e devolve 0 com OUTRO. */
static uint64_t es_guardar_trava(int64_t fd, const char *caminho, int32_t *erro)
{
    int64_t n = 0;
    uint8_t *copia;

    while (caminho[n] != '\0')
        n++;
    copia = sys_heap_alocar(n + 1);
    if (copia == 0) {
        sysl_chamar1(SYSL_UNLINK, (int64_t)caminho);
        sysl_chamar1(SYSL_CLOSE, fd);
        *erro = FORJA_FS_ERR_OUTRO;
        return 0;
    }
    for (int64_t i = 0; i <= n; i++)
        copia[i] = (uint8_t)caminho[i];
    return es_guardar(fd, FORJA_FS_TIPO_ARQUIVO, copia, 0, erro);
}
