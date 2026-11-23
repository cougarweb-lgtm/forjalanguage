/*
 * rt_perfil_saida.c - a escrita dos 2 arquivos do perfil (M8 P1, D-M8-6): o resumo `<base>.perfil.txt` (agregado, sempre completo) e o `<base>.perfil.json`
 * (Chrome Trace Event Format, ate PERFIL_EVENTOS_MAX eventos).
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - M8 P1: primeira versao. Numeros em texto por divisao inteira (perfil_numero: valor / 10^casas), sem printf e sem ponto flutuante: o rt_fmt_f mora no objeto do Ryu e
 *           puxaria o rt_win_ryu.o para todo exe com --perfil; tempo em ns inteiros, ts/dur do JSON saem exatos em microssegundos com 3 casas.
 *
 * Incluido por rt_perfil.c depois dos tipos, dos globais e da conversao de ticks; nao compila sozinho.
 */

#define PERFIL_SAIDA_BYTES 1048576
#define PERFIL_NUMERO_BYTES 24

typedef struct {
    uintptr_t arquivo;
    uint8_t *buf;
    int64_t usado;
    bool falhou;
} PerfilSaida;

/* Abre "<base><sufixo>" e o buffer de 1 MiB. false = sem arquivo (o aviso fica com quem chama). */
static bool ps_abrir(PerfilSaida *s, const char *sufixo)
{
    s->usado = 0;
    s->falhou = false;
    s->buf = (uint8_t *)sys_paginas_reservar(PERFIL_SAIDA_BYTES);
    if (s->buf == NULL)
        return false;
    s->arquivo = perfil_so_abrir(sufixo);
    if (s->arquivo == 0) {
        sys_paginas_liberar(s->buf, PERFIL_SAIDA_BYTES);
        return false;
    }
    return true;
}

static void ps_descarregar(PerfilSaida *s)
{
    if (s->usado > 0 && !s->falhou && !perfil_so_gravar(s->arquivo, s->buf, s->usado))
        s->falhou = true;
    s->usado = 0;
}

/* Descarrega, fecha e solta o buffer; false se alguma gravacao falhou. */
static bool ps_fechar(PerfilSaida *s)
{
    ps_descarregar(s);
    perfil_so_fechar(s->arquivo);
    sys_paginas_liberar(s->buf, PERFIL_SAIDA_BYTES);
    return !s->falhou;
}

static void ps_byte(PerfilSaida *s, char c)
{
    if (s->usado == PERFIL_SAIDA_BYTES)
        ps_descarregar(s);
    s->buf[s->usado++] = (uint8_t)c;
}

static void ps_texto(PerfilSaida *s, const char *texto)
{
    for (; *texto != '\0'; texto++)
        ps_byte(s, *texto);
}

/* valor / 10^casas em texto (ao menos casas + 1 digitos: 5 com 3 casas = "0.005"); devolve o tamanho. dst com PERFIL_NUMERO_BYTES. */
static int64_t perfil_numero(char *dst, uint64_t valor, uint32_t casas)
{
    char t[PERFIL_NUMERO_BYTES];
    int64_t n = 0;
    int64_t i;
    uint32_t digitos = 0;

    while (digitos <= casas || valor != 0) {
        if (casas != 0 && digitos == casas)
            t[n++] = '.';
        t[n++] = (char)('0' + valor % 10u);
        valor /= 10u;
        digitos++;
    }
    for (i = 0; i < n; i++)
        dst[i] = t[n - 1 - i];
    return n;
}

/* O numero alinhado a direita em `largura` colunas (largura 0 = sem preenchimento). */
static void ps_numero(PerfilSaida *s, uint64_t valor, uint32_t casas, int64_t largura)
{
    char t[PERFIL_NUMERO_BYTES];
    int64_t n = perfil_numero(t, valor, casas);
    int64_t i;

    for (i = n; i < largura; i++)
        ps_byte(s, ' ');
    for (i = 0; i < n; i++)
        ps_byte(s, t[i]);
}

/* Milissegundos com 3 casas (arredonda no microssegundo) a partir de ns. */
static void ps_ms(PerfilSaida *s, uint64_t ns, int64_t largura)
{
    ps_numero(s, (ns + 500u) / 1000u, 3, largura);
}

/* O nome "modulo.proc" do id, escapado para JSON (aspas, barra e controles); o resto sai como veio (UTF-8). */
static void ps_nome_json(PerfilSaida *s, uint32_t id)
{
    const char *nome = perfil_nome(id);
    static const char hex[] = "0123456789abcdef";

    for (; *nome != '\0'; nome++) {
        unsigned char c = (unsigned char)*nome;

        if (c == '"' || c == '\\') {
            ps_byte(s, '\\');
            ps_byte(s, (char)c);
        } else if (c < 0x20u) {
            ps_texto(s, "\\u00");
            ps_byte(s, hex[c >> 4]);
            ps_byte(s, hex[c & 15u]);
        } else {
            ps_byte(s, (char)c);
        }
    }
}

/* ---- resumo (.perfil.txt) ---- */

/* O agregado de todos os fios fundido por id: n * PerfilAgg em paginas zeradas (NULL = sem memoria). So lê os fios publicados. */
static PerfilAgg *perfil_fundir(void)
{
    PerfilAgg *fundido = (PerfilAgg *)sys_paginas_reservar((int64_t)perfil_tabela_n * (int64_t)sizeof(PerfilAgg));
    uint32_t qtd = perfil_fios_n < PERFIL_FIOS_MAX ? perfil_fios_n : PERFIL_FIOS_MAX;
    uint32_t f;
    uint32_t id;

    if (fundido == NULL)
        return NULL;
    for (f = 0; f < qtd; f++) {
        const PerfilFio *fio = perfil_lista[f];

        if (fio == NULL)
            continue;
        for (id = 0; id < perfil_tabela_n; id++) {
            fundido[id].chamadas += fio->agg[id].chamadas;
            fundido[id].total += fio->agg[id].total;
            fundido[id].proprio += fio->agg[id].proprio;
        }
    }
    return fundido;
}

/* true se a proc `a` vem antes de `b` no resumo: mais tempo proprio primeiro, empate pelo id (saida deterministica). */
static bool perfil_antes(const PerfilAgg *agg, uint32_t a, uint32_t b)
{
    if (agg[a].proprio != agg[b].proprio)
        return agg[a].proprio > agg[b].proprio;
    return a < b;
}

/* Heapsort: afunda a raiz no max-heap em que o maior e o que vem por ultimo no resumo. */
static void perfil_afundar(const PerfilAgg *agg, uint32_t *v, uint32_t raiz, uint32_t n)
{
    uint32_t filho = 2u * raiz + 1u;
    uint32_t troca;

    while (filho < n) {
        if (filho + 1u < n && perfil_antes(agg, v[filho], v[filho + 1u]))
            filho++;
        if (!perfil_antes(agg, v[raiz], v[filho]))
            return;
        troca = v[raiz];
        v[raiz] = v[filho];
        v[filho] = troca;
        raiz = filho;
        filho = 2u * raiz + 1u;
    }
}

/* Poe v[0..n) na ordem do resumo (heapsort: sem libc e sem recursao). */
static void perfil_ordenar(const PerfilAgg *agg, uint32_t *v, uint32_t n)
{
    uint32_t i;
    uint32_t troca;

    for (i = n / 2u; i > 0; i--)
        perfil_afundar(agg, v, i - 1u, n);
    for (i = n; i > 1; i--) {
        troca = v[0];
        v[0] = v[i - 1u];
        v[i - 1u] = troca;
        perfil_afundar(agg, v, 0, i - 1u);
    }
}

static void perfil_resumo_cabecalho(PerfilSaida *s, uint64_t total_ns, uint64_t soma_ns, uint32_t procs)
{
    ps_texto(s, "perfil forja " PERFIL_VERSAO_FORJA "\ntempo total: ");
    ps_ms(s, total_ns, 0);
    ps_texto(s, " ms\nsoma dos tempos proprios: ");
    ps_ms(s, soma_ns, 0);
    ps_texto(s, " ms\nprocs medidas: ");
    ps_numero(s, procs, 0, 0);
    ps_texto(s, "\neventos no json: ");
    ps_numero(s, perfil_eventos_gravados(), 0, 0);
    ps_texto(s, perfil_truncado ? " (truncado: so o agregado esta completo)\n" : "\n");
    ps_texto(s, "inline: procs inlinadas nao aparecem\n");
    ps_texto(s, "%proprio = proprio / soma dos tempos proprios\n\n");
    ps_texto(s, "    chamadas      total_ms    proprio_ms  %proprio  nome\n");
}

static void perfil_resumo_linha(PerfilSaida *s, const PerfilAgg *agg, uint32_t id, uint64_t soma_ns)
{
    uint64_t proprio_ns = perfil_ticks_para_ns(agg[id].proprio);
    uint64_t permil = soma_ns == 0 ? 0 : (proprio_ns * 1000u + soma_ns / 2u) / soma_ns;

    ps_numero(s, agg[id].chamadas, 0, 12);
    ps_ms(s, perfil_ticks_para_ns(agg[id].total), 14);
    ps_ms(s, proprio_ns, 14);
    ps_numero(s, permil, 1, 10);
    ps_texto(s, "  ");
    ps_texto(s, perfil_nome(id));
    ps_byte(s, '\n');
}

/* Grava <base>.perfil.txt: 1 linha por proc que rodou, em ordem de tempo proprio decrescente. */
static bool perfil_escrever_resumo(uint64_t total_ticks)
{
    PerfilSaida s;
    PerfilAgg *agg = perfil_fundir();
    uint32_t *ids;
    uint32_t procs = 0;
    uint32_t id;
    uint64_t soma_ns = 0;

    if (agg == NULL)
        return false;
    ids = (uint32_t *)sys_paginas_reservar((int64_t)perfil_tabela_n * (int64_t)sizeof(uint32_t));
    if (ids == NULL || !ps_abrir(&s, ".perfil.txt"))
        return false;
    for (id = 0; id < perfil_tabela_n; id++) {
        if (agg[id].chamadas == 0)
            continue;
        ids[procs++] = id;
        soma_ns += perfil_ticks_para_ns(agg[id].proprio);
    }
    perfil_ordenar(agg, ids, procs);
    perfil_resumo_cabecalho(&s, perfil_ticks_para_ns(total_ticks), soma_ns, procs);
    for (id = 0; id < procs; id++)
        perfil_resumo_linha(&s, agg, ids[id], soma_ns);
    return ps_fechar(&s);
}

/* ---- trace (.perfil.json) ---- */

/* Um evento X: ts e dur em microssegundos com 3 casas (o valor em ns, com a virgula 3 casas antes), pid 1, tid = id do fio. */
static void perfil_json_evento(PerfilSaida *s, uint64_t palavra0, uint64_t palavra1)
{
    ps_texto(s, "{\"name\":\"");
    ps_nome_json(s, (uint32_t)(palavra1 & PERFIL_ID_MAX));
    ps_texto(s, "\",\"ph\":\"X\",\"ts\":");
    ps_numero(s, perfil_ticks_para_ns(palavra0 >> 8), 3, 0);
    ps_texto(s, ",\"dur\":");
    ps_numero(s, perfil_ticks_para_ns(palavra1 >> 20), 3, 0);
    ps_texto(s, ",\"pid\":1,\"tid\":");
    ps_numero(s, palavra0 & 255u, 0, 0);
    ps_byte(s, '}');
}

/* Grava <base>.perfil.json com os eventos do buffer global (na ordem em que terminaram). */
static bool perfil_escrever_json(void)
{
    PerfilSaida s;
    uint32_t qtd = perfil_eventos_gravados();
    uint32_t i;

    if (!ps_abrir(&s, ".perfil.json"))
        return false;
    ps_texto(&s, "{\"traceEvents\":[");
    for (i = 0; i < qtd; i++) {
        ps_texto(&s, i == 0 ? "\n" : ",\n");
        perfil_json_evento(&s, perfil_eventos[2u * i], perfil_eventos[2u * i + 1u]);
    }
    ps_texto(&s, "\n],\"displayTimeUnit\":\"ns\",\"otherData\":{\"forja\":\"" PERFIL_VERSAO_FORJA "\",\"truncado\":");
    ps_texto(&s, perfil_truncado ? "true" : "false");
    ps_texto(&s, ",\"eventos\":");
    ps_numero(&s, qtd, 0, 0);
    ps_texto(&s, "}}\n");
    return ps_fechar(&s);
}
