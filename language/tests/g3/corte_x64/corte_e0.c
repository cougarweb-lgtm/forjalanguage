{
    /* Corte X64 (V3, plano do Opus FORJA-D0785): corpo de NativoConstruirExe para o espelho (--corte). IR -> BIR validada -> OtimExecutar (so o --sem-otim do pedido) -> Alvo.gerar
     * (colheita com listagem, sem tempos) -> dump do Objeto e da listagem no stdout -> 2a geracao sem listagem: repeticao igual|difere. rc 0.
     * Quem sobe a escada (alocador, isel, divconst ainda stub) e a flag --sem-otim da paridade; o corte nao fixa nenhum passe. */
    static const char hex_digitos[] = "0123456789abcdef";
    const char *nome = pedido->alvo != NULL ? pedido->alvo : NATIVO_ALVO_PADRAO;
    const Alvo *alvo = AlvoPorNome(nome);
    bool sysv = strcmp(nome, "x86_64-linux") == 0;
    BirNomesMem nomes_alvo = {"memcpy", "memset"};
    BirEntrada entrada;
    BirPrograma programa = {0};
    Diag erro = {0};
    Buf contadores = {0};
    Buf passes = {0};
    Buf saida = {0};
    OtimRelatos relatos = {&contadores, &passes};
    AlvoListagem lista;
    Objeto obj, obj2;
    AlvoColheita colheita;
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
    AlvoListagemIniciar(&lista);
    ObjIniciar(&obj);
    ObjIniciar(&obj2);
    colheita = (AlvoColheita){.lista = &lista, .tempos = NULL, .rastreio = false, .dbg = NULL, .guarda = pedido->guarda_memoria};
    if (!BirConstruir(&entrada, &programa, &erro)) {
        DiagImprimirJson(&erro, pedido->fonte, stdout);
        codigo = erro.codigo == X0001 ? 1 : 3;
    } else if (!BirValidar(&programa, &erro)) {
        DiagImprimirJson(&erro, pedido->fonte, stdout);
        codigo = 3;
    } else if (!OtimExecutar(&programa, pedido->sem_otim != NULL ? pedido->sem_otim : "", &relatos, &erro)) {
        DiagImprimirJson(&erro, pedido->fonte, stdout);
        codigo = 3;
    } else if (!alvo->gerar(&programa, &obj, &erro, &colheita)) {
        DiagImprimirJson(&erro, pedido->fonte, stdout);
        codigo = 3;
    } else {
        const ObjSecao *texto = NULL;
        bool igual = true;
        AlvoColheita colheita2 = {.lista = NULL, .tempos = NULL, .rastreio = false, .dbg = NULL, .guarda = pedido->guarda_memoria};
        Diag erro2 = {0};

        BufFormatar(&saida, "alvo %s\n", alvo->nome);
        for (size_t s = 0; s < obj.secoes.n && texto == NULL; s++) {
            const ObjSecao *cand = VecEm(&obj.secoes, s);

            if (cand->tipo == OS_TEXTO) {
                texto = cand;
            }
        }
        for (size_t i = 0; i < lista.funcoes.n; i++) {
            const ListaFuncao *f = VecEm(&lista.funcoes, i);

            BufFormatar(&saida, "funcao %zu nome=%s inicio=%u fim=%u pos=%u quadro=%u slots=%u maxargs=%u push=%u fpreg=%u alocacao=%u sondagem=%u unwind=", i,
                        lista.textos.dados + f->nome, f->inicio, f->fim, f->pos, f->quadro, f->slots, f->maxargs, f->push, f->fpreg, f->alocacao, (unsigned)f->sondagem);
            for (unsigned q = 0; q < f->tam_unwind; q++) {
                char h[2] = {hex_digitos[f->unwind[q] >> 4], hex_digitos[f->unwind[q] & 15]};

                BufAnexarLento(&saida, h, 2);
            }
            BufAnexarLento(&saida, "\n", 1);
            for (uint32_t q = 0; q < f->nslots_agregado; q++) {
                BufFormatar(&saida, "slot %zu %u tam=%u alin=%u off=%u\n", i, q, f->slots_agregado[q].tam, f->slots_agregado[q].alinhamento, f->slots_agregado[q].offset);
            }
        }
        for (size_t i = 0; i < lista.instrs.n; i++) {
            const ListaInstr *li = VecEm(&lista.instrs, i);

            BufFormatar(&saida, "instr f=%u off=%u tam=%u origem=%u bloco=%u indice=%u pos=%u texto=%s simbolo=%s bytes=", li->funcao, li->off, li->tam, (unsigned)li->origem,
                        li->bloco, li->indice, li->pos, lista.textos.dados + li->texto, li->simbolo != 0 ? lista.textos.dados + (li->simbolo - 1) : "-");
            if (texto != NULL && (size_t)li->off + li->tam <= texto->bytes.tam) {
                for (uint32_t q = 0; q < li->tam; q++) {
                    unsigned char b = (unsigned char)texto->bytes.dados[li->off + q];
                    char h[2] = {hex_digitos[b >> 4], hex_digitos[b & 15]};

                    BufAnexarLento(&saida, h, 2);
                }
            }
            BufAnexarLento(&saida, "\n", 1);
        }
        for (size_t s = 0; s < obj.secoes.n; s++) {
            const ObjSecao *sec = VecEm(&obj.secoes, s);
            uint64_t fnv = 14695981039346656037ull;

            for (size_t q = 0; q < sec->bytes.tam; q++) {
                fnv = (fnv ^ (unsigned char)sec->bytes.dados[q]) * 1099511628211ull;
            }
            BufFormatar(&saida, "secao %zu nome=%s tipo=%d alin=%u tam=%zu bss=%u fnv=%016llx\n", s, sec->nome, (int)sec->tipo, sec->alinhamento, sec->bytes.tam, sec->tam_bss,
                        (unsigned long long)fnv);
            if (sec->tipo == OS_TEXTO || sec->tipo == OS_PDATA || sec->tipo == OS_XDATA) {
                BufFormatar(&saida, "hex %zu ", s);
                for (size_t q = 0; q < sec->bytes.tam; q++) {
                    unsigned char b = (unsigned char)sec->bytes.dados[q];
                    char h[2] = {hex_digitos[b >> 4], hex_digitos[b & 15]};

                    BufAnexarLento(&saida, h, 2);
                }
                BufAnexarLento(&saida, "\n", 1);
            }
        }
        for (size_t r = 0; r < obj.relocs.n; r++) {
            const ObjReloc *rl = VecEm(&obj.relocs, r);
            const ObjSimbolo *alvo_sim = VecEm(&obj.simbolos, rl->correcao.simbolo);

            BufFormatar(&saida, "reloc secao=%u off=%u tipo=%d simbolo=%s adendo=%lld\n", rl->alvo.secao, rl->alvo.off, (int)rl->correcao.tipo, alvo_sim->nome,
                        (long long)rl->correcao.adendo);
        }
        for (size_t y = 0; y < obj.simbolos.n; y++) {
            const ObjSimbolo *sim = VecEm(&obj.simbolos, y);

            BufFormatar(&saida, "simbolo %zu nome=%s secao=%d off=%u global=%d fraco=%d\n", y, sim->nome, (int)sim->secao, sim->off, sim->global ? 1 : 0, sim->fraco ? 1 : 0);
        }
        if (!alvo->gerar(&programa, &obj2, &erro2, &colheita2) || obj2.secoes.n != obj.secoes.n || obj2.relocs.n != obj.relocs.n || obj2.simbolos.n != obj.simbolos.n) {
            igual = false;
        }
        for (size_t s = 0; igual && s < obj.secoes.n; s++) {
            const ObjSecao *a = VecEm(&obj.secoes, s);
            const ObjSecao *b = VecEm(&obj2.secoes, s);

            if (a->bytes.tam != b->bytes.tam || a->tam_bss != b->tam_bss || a->tipo != b->tipo || (a->bytes.tam > 0 && memcmp(a->bytes.dados, b->bytes.dados, a->bytes.tam) != 0)) {
                igual = false;
            }
        }
        for (size_t r = 0; igual && r < obj.relocs.n; r++) {
            const ObjReloc *a = VecEm(&obj.relocs, r);
            const ObjReloc *b = VecEm(&obj2.relocs, r);

            if (a->alvo.secao != b->alvo.secao || a->alvo.off != b->alvo.off || a->correcao.tipo != b->correcao.tipo || a->correcao.simbolo != b->correcao.simbolo ||
                a->correcao.adendo != b->correcao.adendo) {
                igual = false;
            }
        }
        for (size_t y = 0; igual && y < obj.simbolos.n; y++) {
            const ObjSimbolo *a = VecEm(&obj.simbolos, y);
            const ObjSimbolo *b = VecEm(&obj2.simbolos, y);

            if (strcmp(a->nome, b->nome) != 0 || a->secao != b->secao || a->off != b->off || a->global != b->global || a->fraco != b->fraco) {
                igual = false;
            }
        }
        BufFormatar(&saida, "repeticao %s\n", igual ? "igual" : "difere");
        fwrite(saida.dados, 1, saida.tam, stdout);
    }
    BufLiberar(&saida);
    BufLiberar(&contadores);
    BufLiberar(&passes);
    ObjLiberar(&obj);
    ObjLiberar(&obj2);
    AlvoListagemLiberar(&lista);
    BirLiberar(&programa);
    fflush(stdout);
    exit(codigo);
    return codigo;
}
