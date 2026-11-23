{
    /* Corte BIR0 (D0778, plano_bir0): corpo de NativoConstruirExe para o espelho (--corte). Igual ao corte de corte_e1.fj: IR -> BIR validada -> dump no stdout, rc 0. */
    const char *nome = pedido->alvo != NULL ? pedido->alvo : NATIVO_ALVO_PADRAO;
    bool sysv = strcmp(nome, "x86_64-linux") == 0;
    BirNomesMem nomes_alvo = {"memcpy", "memset"};
    BirEntrada entrada;
    BirPrograma programa = {0};
    Diag erro = {0};
    Buf dump = {0};
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
    if (!BirConstruir(&entrada, &programa, &erro)) {
        DiagImprimirJson(&erro, pedido->fonte, stdout);
        codigo = erro.codigo == X0001 ? 1 : 3;
    } else if (!BirValidar(&programa, &erro)) {
        DiagImprimirJson(&erro, pedido->fonte, stdout);
        codigo = 3;
    } else {
        BirImprimir(&programa, &dump);
        fwrite(dump.dados, 1, dump.tam, stdout);
    }
    BufLiberar(&dump);
    BirLiberar(&programa);
    fflush(stdout);
    exit(codigo);
    return codigo;
}
