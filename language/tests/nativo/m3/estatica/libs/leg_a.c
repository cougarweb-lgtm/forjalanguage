/* leg_a.c - M3b P23g: membro de libfx_leg.a. Exercita, de dentro de uma estatica de terceiro, a camada fina do rt_win_legado.o: printf, fprintf, snprintf, sscanf, strcasecmp, hypotf, sincosf,
 * __popcountdi2 (o gcc o chama para __builtin_popcountll em x86-64 base), stat64i32 e opendir/readdir/closedir. Tudo declarado a mao, sem headers: o gcc nao troca as chamadas por inline
 * da UCRT e a .a pede exatamente os simbolos que raylib, box2d e miniaudio pedem. No backend C o gcc resolve os mesmos nomes pela libmingwex; no x64 vem do rt_win_legado.o. */
typedef struct FxDir FxDir;
struct FxEntrada {
    int d_ino;
    unsigned short d_reclen;
    unsigned short d_namlen;
    char d_name[260];
};

int printf(const char *formato, ...);
int fprintf(void *arquivo, const char *formato, ...);
int snprintf(char *destino, unsigned long long tamanho, const char *formato, ...);
void *__acrt_iob_func(unsigned int indice);
int strcasecmp(const char *a, const char *b);
int sscanf(const char *entrada, const char *formato, ...);
float hypotf(float x, float y);
void sincosf(float x, float *seno, float *cosseno);
int stat64i32(const char *caminho, void *destino);
FxDir *opendir(const char *caminho);
struct FxEntrada *readdir(FxDir *dir);
int closedir(FxDir *dir);
int _mkdir(const char *caminho);
int _rmdir(const char *caminho);
int remove(const char *caminho);
void *fopen(const char *caminho, const char *modo);
int fclose(void *arquivo);

/* printf com inteiro, texto, real e caractere; devolve o numero de caracteres escritos. */
int fx_leg_printf(int n, const char *texto)
{
    return printf("printf %d %s %5.2f|%c\n", n, texto, 3.14159, 'x');
}

/* fprintf no stderr (indice 2 do __acrt_iob_func). */
int fx_leg_fprintf_stderr(int n)
{
    return fprintf(__acrt_iob_func(2), "erro %d\n", n);
}

/* snprintf com corte: devolve o tamanho exigido e fecha com NUL. */
int fx_leg_snprintf(char *destino, int cap, int n)
{
    return snprintf(destino, (unsigned long long)cap, "%05d-%s", n, "snp");
}

/* strcasecmp: bit 0 = "ABC" == "abc"; bit 1 = "a" < "B"; bit 2 = "b" > "A". */
int fx_leg_strcasecmp(void)
{
    return (strcasecmp("ABC", "abc") == 0) | ((strcasecmp("a", "B") < 0) << 1) | ((strcasecmp("b", "A") > 0) << 2);
}

/* hypotf, sincosf e popcount escritos por printf; devolve o popcount de 0xF0F0 mais o de -1 (8 + 64). */
int fx_leg_matematica(void)
{
    volatile float tres = 3.0f, quatro = 4.0f, meio = 0.5f; /* volatile: o gcc nao dobra as chamadas em constantes */
    volatile unsigned long long mascara = 0xF0F0ULL, todos = ~0ULL;
    float seno = 0.0f, cosseno = 0.0f;

    sincosf(meio, &seno, &cosseno);
    printf("hypotf %.3f sincosf %.3f %.3f\n", (double)hypotf(tres, quatro), (double)seno, (double)cosseno);
    return __builtin_popcountll(mascara) + __builtin_popcountll(todos);
}

/* sscanf (o do legado, que o cJSON importa): inteiro, texto e real de "12 abc 3.5"; imprime o lido e devolve quantos campos leu (3). */
int fx_leg_sscanf(void)
{
    int inteiro = 0;
    char texto[8] = {0};
    double real = 0.0;
    int lidos = sscanf("12 abc 3.5", "%d %7s %lf", &inteiro, texto, &real);

    printf("sscanf %d %s %.1f\n", inteiro, texto, real);
    return lidos;
}

/* Cria fx_leg_dir com a.txt e b.txt, lista (sem . e ..), consulta o modo com stat64i32 e apaga tudo; devolve quantas entradas listou. */
int fx_leg_diretorio(void)
{
    struct FxEntrada *entrada;
    char modo[64] = {0};
    FxDir *dir;
    int total = 0;

    _mkdir("fx_leg_dir");
    fclose(fopen("fx_leg_dir/a.txt", "w"));
    fclose(fopen("fx_leg_dir/b.txt", "w"));
    stat64i32("fx_leg_dir", modo);
    printf("modo 0x%x\n", (unsigned int)(*(unsigned short *)(modo + 6) & 0xF000));
    dir = opendir("fx_leg_dir");
    while (dir != 0 && (entrada = readdir(dir)) != 0) {
        if (entrada->d_name[0] == '.')
            continue;
        printf("entrada %s %d\n", entrada->d_name, (int)entrada->d_namlen);
        total++;
    }
    if (dir != 0)
        closedir(dir);
    remove("fx_leg_dir/a.txt");
    remove("fx_leg_dir/b.txt");
    _rmdir("fx_leg_dir");
    return total;
}
