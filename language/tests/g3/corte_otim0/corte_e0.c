{
    /* Corte OTIM0 (V2, plano do Opus): corpo de NativoConstruirExe para o espelho (--corte). IR -> BIR validada -> OtimExecutar (release) -> relato otim e dump passes no stdout, rc 0.
     * sem = --sem-otim do pedido + alocador,isel,divconst (os 3 passos de x64 ainda sao stub na V3). */
    const char *nome = pedido->alvo != NULL ? pedido->alvo : NATIVO_ALVO_PADRAO;
    bool sysv = strcmp(nome, "x86_64-linux") == 0;
    BirNomesMem nomes_alvo = {"memcpy", "memset"};
    BirEntrada entrada;
    BirPrograma programa = {0};
    Diag erro = {0};
    Buf contadores = {0};
    Buf passes = {0};
    OtimRelatos relatos = {&contadores, &passes};
    char sem[512];
    int codigo = 0;

    if (!sysv && strcmp(nome, "x86_64-windows") != 0) {
        DiagImprimirJsonSemFonte(X0003, "alvo desconhecido no backend nativo", stderr);
        return SAIDA_USO;
    }
    if (pedido->dll && sysv) {
        DiagImprimirJsonSemFonte(X0003, NATIVO_X0003_SO, stderr);
        return SAIDA_USO;
    }
    if (sysv) {
        nomes_alvo = (BirNomesMem){"forja_memcpy", "forja_memset"};
    }
    entrada = (BirEntrada){.ir = pedido->ir, .tipos = pedido->tipos, .nomes = pedido->nomes, .fonte = pedido->fonte, .modo = pedido->modo,
                           .teste = pedido->teste, .traco = pedido->traco, .dll = pedido->dll, .poda = true, .sysv = sysv, .nomes_mem = nomes_alvo,
                           .guarda = pedido->guarda_memoria, .checagem = pedido->checagem, .simbolos_dbg = pedido->simbolos, .perfil = pedido->perfil};
    snprintf(sem, sizeof sem, "%s,alocador,isel,divconst", pedido->sem_otim != NULL ? pedido->sem_otim : "");
    if (!BirConstruir(&entrada, &programa, &erro)) {
        DiagImprimirJson(&erro, pedido->fonte, stdout);
        codigo = erro.codigo == X0001 ? 1 : 3;
    } else if (!BirValidar(&programa, &erro)) {
        DiagImprimirJson(&erro, pedido->fonte, stdout);
        codigo = 3;
    } else if (!OtimExecutar(&programa, sem, &relatos, &erro)) {
        DiagImprimirJson(&erro, pedido->fonte, stdout);
        codigo = 3;
    } else {
        fwrite(contadores.dados, 1, contadores.tam, stdout);
        fwrite(passes.dados, 1, passes.tam, stdout);
    }
    BufLiberar(&contadores);
    BufLiberar(&passes);
    BirLiberar(&programa);
    fflush(stdout);
    exit(codigo);
    return codigo;
}
