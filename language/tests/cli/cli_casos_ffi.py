"""Casos de cli da fronteira C: DLL carregada por ctypes, .h valido em gcc, PHP FFI, foreign (DLL da Forja, vendor), forja ligar.

Importado pelo motor verificar_cli.py (que monta a lista CASOS na ordem original); as funcoes comuns vem de la.

Versao: 1.5.0
Changelog: 1.5.0 - 0.21.0 F4 passo 3: CasoForeignLibAusente e CasoForeignSimboloAusente saem do --backend c e conferem o x64 (X0102/X0101 no stdout, stderr vazio; o alvo e a etapa que o x64 traz nao se conferem mais como ausentes).
           1.4.0 - 0.21.0 F3: CasoParidadeB03 roda o md5_paridade com --contra-congelado (so o x64 contra o C congelado); CasoCabecalhoX64IgualAoC e CasoDllFreeLibraryRelataVazamento viram x64 contra valor fixo (esperados\\ffi_cabecalho_<fixture>.h e esperados\\ffi_freelibrary_r0009_err.txt); mesma contagem.
           1.3.0 - IA1-b03: CasoForeignLibAusente e CasoForeignSimboloAusente esperam X0102/X0101 no stdout do backend C (a conferencia de 'link' roda antes do gcc, sem alvo/etapa, stderr vazio); +1 caso (CasoParidadeB03: M-D5).
           1.2.0 - M4 P25b: CasoForeignLibAusente e CasoForeignSimboloAusente levam --backend c explicito (B0004 e do ld; o padrao do dev passou a ser x64) e +2 casos x64 das mesmas fixtures (X0102 e X0101).
           1.1.0 - M3 P22a: +2 casos (CasoCabecalhoX64IgualAoC: o .h do --backend x64 igual ao do C em 9 fixtures; CasoDllFreeLibraryRelataVazamento: R0009 no DETACH por FreeLibrary, x64 e C); as variantes
           --backend x64 dos casos de DLL moram no verificar_cli.py (ComBackendX64).
           1.0.0 - P9a: casos movidos do verificar_cli.py 1.21.0 sem mudar nenhum.
Verificacao: python verificar_cli.py --forja <forja.exe>
"""

import ctypes
import shutil
import subprocess
import sys

from verificar_cli import PastaAqui, TimeoutSegundos, ConferirExecucao, ConstruirDll, Rodar, UltimoJson


def CasoDllCarrega(Forja, PastaSaidas):
    """'pub export proc' vira simbolo limpo (somar); o fj_somar e os procs sem export NAO saem na DLL (int = int64)."""
    Problemas, Dll, _ = ConstruirDll(Forja, PastaSaidas, "biblioteca.fj", "biblioteca")
    if Problemas:
        return Problemas
    Biblioteca = ctypes.CDLL(str(Dll))
    Biblioteca.somar.restype = ctypes.c_int64
    Biblioteca.somar.argtypes = [ctypes.c_int64, ctypes.c_int64]
    Biblioteca.fatorial.restype = ctypes.c_int64
    Biblioteca.fatorial.argtypes = [ctypes.c_int64]
    if Biblioteca.somar(2, 3) != 5:
        Problemas.append(f"somar(2, 3) = {Biblioteca.somar(2, 3)}, esperado 5")
    if Biblioteca.fatorial(10) != 3628800:
        Problemas.append(f"fatorial(10) = {Biblioteca.fatorial(10)}, esperado 3628800")
    for Escondido in ("fj_somar", "fj_fatorial", "fj_interno", "interno"):
        if hasattr(Biblioteca, Escondido):
            Problemas.append(f"a DLL exporta '{Escondido}', que devia ficar escondido")
    return Problemas


def CasoDllTipos(Forja, PastaSaidas):
    """f32, bool, enum, ponteiro para struct e retorno vazio atravessam a fronteira C com o valor certo."""
    Problemas, Dll, _ = ConstruirDll(Forja, PastaSaidas, "biblioteca_tipos.fj", "biblioteca_tipos")
    if Problemas:
        return Problemas
    Biblioteca = ctypes.CDLL(str(Dll))

    class Ponto(ctypes.Structure):
        _fields_ = [("x", ctypes.c_int32), ("y", ctypes.c_int32)]

    Biblioteca.dobrar.restype = ctypes.c_float
    Biblioteca.dobrar.argtypes = [ctypes.c_float]
    Biblioteca.eh_rapido.restype = ctypes.c_bool
    Biblioteca.eh_rapido.argtypes = [ctypes.c_int32]
    Biblioteca.soma_ponto.restype = ctypes.c_int32
    Biblioteca.soma_ponto.argtypes = [ctypes.POINTER(Ponto)]
    Biblioteca.ola.restype = None
    Obtido = (Biblioteca.dobrar(1.5), Biblioteca.eh_rapido(1), Biblioteca.eh_rapido(0), Biblioteca.soma_ponto(ctypes.byref(Ponto(3, 4))))
    if Obtido != (3.0, True, False, 7):
        Problemas.append(f"dobrar/eh_rapido/soma_ponto = {Obtido}, esperado (3.0, True, False, 7)")
    return Problemas


def CasoCabecalhoC(Forja, PastaSaidas):
    """O .h ao lado da DLL tem os prototipos, nenhuma diretiva '#', e passa em gcc -fsyntax-only."""
    Problemas, _, Cabecalho = ConstruirDll(Forja, PastaSaidas, "biblioteca_tipos.fj", "cabecalho_tipos")
    if Problemas:
        return Problemas
    Texto = Cabecalho.read_text(encoding="utf-8")
    for Prototipo in ("float dobrar(float a0);", "bool eh_rapido(int32_t a0);", "int32_t soma_ponto(struct fj_Ponto * a1);", "void ola(void);"):
        Esperado = Prototipo.replace("a1", "a0")
        if Esperado not in Texto:
            Problemas.append(f".h sem o prototipo {Esperado!r}")
    if any(Linha.lstrip().startswith("#") for Linha in Texto.splitlines()):
        Problemas.append(".h tem diretiva '#', que o FFI::cdef do PHP recusa")
    Gcc = shutil.which("gcc")
    if Gcc is None:
        return Problemas + ["gcc fora do PATH: nao deu para conferir o .h"]
    Sintaxe = subprocess.run([Gcc, "-fsyntax-only", "-x", "c", "-include", "stdint.h", "-include", "stdbool.h", str(Cabecalho)],
                             capture_output=True, text=True)
    if Sintaxe.returncode != 0:
        Problemas.append(f".h nao passou em gcc -fsyntax-only: {Sintaxe.stderr.strip()[:160]}")
    return Problemas


def CasoPhpFfi(Forja, PastaSaidas):
    """Se houver PHP com a extensao FFI, o FFI::cdef(.h) carrega a DLL e chama dobrar/soma; senao o caso e um SKIP explicito."""
    Php = shutil.which("php")
    Opcoes = ["-d", "extension=ffi", "-d", "ffi.enable=1"]  # so na linha de comando: o php.ini do usuario nao muda
    if Php is None:
        print("    SKIP PHP FFI: php fora do PATH")
        return []
    if subprocess.run([Php] + Opcoes + ["-r", "exit(class_exists('FFI') ? 0 : 3);"], capture_output=True).returncode != 0:
        print("    SKIP PHP FFI: este php nao tem a extensao ffi")
        return []
    Problemas, Dll, Cabecalho = ConstruirDll(Forja, PastaSaidas, "biblioteca.fj", "php_ffi")
    if Problemas:
        return Problemas
    Codigo = (f"$f = FFI::cdef(file_get_contents('{Cabecalho.as_posix()}'), '{Dll.as_posix()}');"
              "echo $f->somar(2, 3), ',', $f->fatorial(5);")
    Rodada = subprocess.run([Php] + Opcoes + ["-r", Codigo], capture_output=True, text=True)
    if Rodada.returncode != 0 or Rodada.stdout.strip() != "5,120":
        Problemas.append(f"PHP FFI: exit {Rodada.returncode}, stdout {Rodada.stdout.strip()!r}, stderr {Rodada.stderr.strip()[:120]!r}")
    return Problemas


def CasoDllNumericos(Forja, PastaSaidas):
    """i16, u8, u32, uint e f64 atravessam a fronteira C com o valor certo (release: u8 200+100 volta a 44)."""
    Problemas, Dll, _ = ConstruirDll(Forja, PastaSaidas, "biblioteca_numericos.fj", "biblioteca_numericos")
    if Problemas:
        return Problemas
    Biblioteca = ctypes.CDLL(str(Dll))
    Biblioteca.soma_u8.restype = ctypes.c_uint8
    Biblioteca.soma_u8.argtypes = [ctypes.c_uint8, ctypes.c_uint8]
    Biblioteca.soma_i16.restype = ctypes.c_int16
    Biblioteca.soma_i16.argtypes = [ctypes.c_int16, ctypes.c_int16]
    Biblioteca.mascara_baixa.restype = ctypes.c_uint32
    Biblioteca.mascara_baixa.argtypes = [ctypes.c_uint32, ctypes.c_uint32]
    Biblioteca.maior_uint.restype = ctypes.c_uint64
    Biblioteca.maior_uint.argtypes = [ctypes.c_uint64, ctypes.c_uint64]
    Biblioteca.media.restype = ctypes.c_double
    Biblioteca.media.argtypes = [ctypes.c_double, ctypes.c_double]
    Biblioteca.passo_i8.restype = ctypes.c_int8
    Biblioteca.passo_i8.argtypes = [ctypes.c_int8]
    Obtido = (Biblioteca.soma_u8(20, 22), Biblioteca.soma_u8(200, 100), Biblioteca.soma_i16(-300, 100),
              Biblioteca.mascara_baixa(0xABCD, 8), Biblioteca.maior_uint(2**63, 5), Biblioteca.media(1.0, 2.0),
              Biblioteca.passo_i8(-128))
    Esperado = (42, 44, -200, 0xCD, 2**63, 1.5, -127)
    if Obtido != Esperado:
        Problemas.append(f"soma_u8/soma_i16/mascara/maior_uint/media/passo_i8 = {Obtido}, esperado {Esperado}")
    return Problemas


def CasoCabecalhoNumericos(Forja, PastaSaidas):
    """O .h dos numericos traz int8_t..uint64_t e double, sem '#', e passa em gcc -fsyntax-only."""
    Problemas, _, Cabecalho = ConstruirDll(Forja, PastaSaidas, "biblioteca_numericos.fj", "cabecalho_numericos")
    if Problemas:
        return Problemas
    Texto = Cabecalho.read_text(encoding="utf-8")
    for Prototipo in ("uint8_t soma_u8(uint8_t a0, uint8_t a1);", "int16_t soma_i16(int16_t a0, int16_t a1);",
                      "uint32_t mascara_baixa(uint32_t a0, uint32_t a1);", "uint64_t maior_uint(uint64_t a0, uint64_t a1);",
                      "double media(double a0, double a1);", "int8_t passo_i8(int8_t a0);"):
        if Prototipo not in Texto:
            Problemas.append(f".h sem o prototipo {Prototipo!r}")
    if any(Linha.lstrip().startswith("#") for Linha in Texto.splitlines()):
        Problemas.append(".h tem diretiva '#', que o FFI::cdef do PHP recusa")
    Gcc = shutil.which("gcc")
    if Gcc is None:
        return Problemas + ["gcc fora do PATH: nao deu para conferir o .h"]
    Sintaxe = subprocess.run([Gcc, "-fsyntax-only", "-x", "c", "-include", "stdint.h", "-include", "stdbool.h", str(Cabecalho)],
                             capture_output=True, text=True)
    if Sintaxe.returncode != 0:
        Problemas.append(f".h nao passou em gcc -fsyntax-only: {Sintaxe.stderr.strip()[:160]}")
    return Problemas


def CasoPhpFfiNumericos(Forja, PastaSaidas):
    """Se houver PHP com FFI, o FFI::cdef(.h) chama soma_u8, maior_uint e media; senao o caso e um SKIP explicito."""
    Php = shutil.which("php")
    Opcoes = ["-d", "extension=ffi", "-d", "ffi.enable=1"]
    if Php is None:
        print("    SKIP PHP FFI numericos: php fora do PATH")
        return []
    if subprocess.run([Php] + Opcoes + ["-r", "exit(class_exists('FFI') ? 0 : 3);"], capture_output=True).returncode != 0:
        print("    SKIP PHP FFI numericos: este php nao tem a extensao ffi")
        return []
    Problemas, Dll, Cabecalho = ConstruirDll(Forja, PastaSaidas, "biblioteca_numericos.fj", "php_ffi_numericos")
    if Problemas:
        return Problemas
    Codigo = (f"$f = FFI::cdef(file_get_contents('{Cabecalho.as_posix()}'), '{Dll.as_posix()}');"
              "echo $f->soma_u8(20, 22), ',', $f->maior_uint(7, 5), ',', $f->media(1.0, 2.0);")
    Rodada = subprocess.run([Php] + Opcoes + ["-r", Codigo], capture_output=True, text=True)
    if Rodada.returncode != 0 or Rodada.stdout.strip() != "42,7,1.5":
        Problemas.append(f"PHP FFI: exit {Rodada.returncode}, stdout {Rodada.stdout.strip()!r}, stderr {Rodada.stderr.strip()[:120]!r}")
    return Problemas


def CasoDllEstouroSai101(Forja, PastaSaidas):
    """DLL de DEV: u8 200+100 estoura e o processo que a carregou sai com 101 e R0002 no stderr (decisao COMPILER-D0002)."""
    Problemas, Dll, _ = ConstruirDll(Forja, PastaSaidas, "biblioteca_numericos.fj", "numericos_dev", Release=False)
    if Problemas:
        return Problemas
    Programa = (f"import ctypes\nd = ctypes.CDLL(r'{Dll}')\nd.soma_u8.restype = ctypes.c_uint8\n"
                "d.soma_u8.argtypes = [ctypes.c_uint8, ctypes.c_uint8]\nprint(d.soma_u8(20, 22), flush=True)\nd.soma_u8(200, 100)\n")
    Rodada = subprocess.run([sys.executable, "-I", "-c", Programa], capture_output=True, text=True, timeout=TimeoutSegundos)
    if Rodada.returncode != 101 or "R0002" not in Rodada.stderr or Rodada.stdout.strip() != "42":
        Problemas.append(f"estouro na DLL de dev: exit {Rodada.returncode}, stdout {Rodada.stdout.strip()!r}, stderr {Rodada.stderr.strip()[:120]!r}")
    return Problemas


class Vec3(ctypes.Structure):
    _fields_ = [("x", ctypes.c_float), ("y", ctypes.c_float), ("z", ctypes.c_float)]


class Par(ctypes.Structure):
    _fields_ = [("a", ctypes.c_int32), ("b", ctypes.c_int32)]


def CasoDllPonteiros(Forja, PastaSaidas):
    """^T, ?^T, rawptr, cstr de entrada e struct por valor (12 bytes por ponteiro escondido, 8 em registrador) atravessam a DLL."""
    Problemas, Dll, _ = ConstruirDll(Forja, PastaSaidas, "biblioteca_ponteiros.fj", "biblioteca_ponteiros")
    if Problemas:
        return Problemas
    B = ctypes.CDLL(str(Dll))
    B.incrementar.argtypes = [ctypes.POINTER(ctypes.c_int32)]
    B.soma_vec.restype, B.soma_vec.argtypes = ctypes.c_float, [Vec3]
    B.escala.restype, B.escala.argtypes = Vec3, [Vec3, ctypes.c_float]
    B.criar_par.restype, B.criar_par.argtypes = Par, [ctypes.c_int32, ctypes.c_int32]
    B.tamanho.restype, B.tamanho.argtypes = ctypes.c_int64, [ctypes.c_char_p]
    B.ler_opcional.restype, B.ler_opcional.argtypes = ctypes.c_int32, [ctypes.POINTER(ctypes.c_int32)]
    B.id_bruto.restype, B.id_bruto.argtypes = ctypes.c_void_p, [ctypes.c_void_p]
    Contador = ctypes.c_int32(41)
    B.incrementar(ctypes.byref(Contador))
    Dobro = B.escala(Vec3(1, 2, 3), 2.0)
    Par7 = B.criar_par(7, -8)
    Obtido = (Contador.value, B.soma_vec(Vec3(1, 2, 3)), (Dobro.x, Dobro.y, Dobro.z), (Par7.a, Par7.b), B.tamanho(b"Grifo"),
              B.ler_opcional(None), B.ler_opcional(ctypes.byref(ctypes.c_int32(9))), B.id_bruto(ctypes.c_void_p(1234)))
    Esperado = (42, 6.0, (2.0, 4.0, 6.0), (7, -8), 5, -1, 9, 1234)
    if Obtido != Esperado:
        Problemas.append(f"incrementar/soma_vec/escala/criar_par/tamanho/ler_opcional/id_bruto = {Obtido}, esperado {Esperado}")
    return Problemas


def CasoCabecalhoPonteiros(Forja, PastaSaidas):
    """O .h define a struct usada por valor, declara a opaca, traz os prototipos de ponteiro/cstr, sem '#', e passa no gcc."""
    Problemas, _, Cabecalho = ConstruirDll(Forja, PastaSaidas, "biblioteca_ponteiros.fj", "cabecalho_ponteiros")
    if Problemas:
        return Problemas
    Texto = Cabecalho.read_text(encoding="utf-8")
    for Trecho in ("struct fj_Grifo;", "struct fj_Vec3 { float f_x; float f_y; float f_z; };", "struct fj_Par { int32_t f_a; int32_t f_b; };",
                   "void incrementar(int32_t * a0);", "float soma_vec(struct fj_Vec3 a0);",
                   "struct fj_Vec3 escala(struct fj_Vec3 a0, float a1);", "struct fj_Par criar_par(int32_t a0, int32_t a1);",
                   "int64_t tamanho(const char * a0);", "void * id_bruto(void * a0);", "int32_t ler_opcional(int32_t * a0);"):
        if Trecho not in Texto:
            Problemas.append(f".h sem {Trecho!r}")
    if "struct fj_Grifo {" in Texto:
        Problemas.append(".h define a struct Grifo, que so aparece atras de ponteiro e devia ficar opaca")
    if any(Linha.lstrip().startswith("#") for Linha in Texto.splitlines()):
        Problemas.append(".h tem diretiva '#', que o FFI::cdef do PHP recusa")
    Gcc = shutil.which("gcc")
    if Gcc is None:
        return Problemas + ["gcc fora do PATH: nao deu para conferir o .h"]
    Sintaxe = subprocess.run([Gcc, "-fsyntax-only", "-x", "c", "-include", "stdint.h", "-include", "stdbool.h", str(Cabecalho)],
                             capture_output=True, text=True)
    if Sintaxe.returncode != 0:
        Problemas.append(f".h nao passou em gcc -fsyntax-only: {Sintaxe.stderr.strip()[:160]}")
    return Problemas


def CasoPhpFfiPonteiros(Forja, PastaSaidas):
    """Se houver PHP com FFI, struct por valor, cstr e ponteiro (FFI::addr) funcionam pelo .h; senao o caso e um SKIP explicito."""
    Php = shutil.which("php")
    Opcoes = ["-d", "extension=ffi", "-d", "ffi.enable=1"]
    if Php is None:
        print("    SKIP PHP FFI ponteiros: php fora do PATH")
        return []
    if subprocess.run([Php] + Opcoes + ["-r", "exit(class_exists('FFI') ? 0 : 3);"], capture_output=True).returncode != 0:
        print("    SKIP PHP FFI ponteiros: este php nao tem a extensao ffi")
        return []
    Problemas, Dll, Cabecalho = ConstruirDll(Forja, PastaSaidas, "biblioteca_ponteiros.fj", "php_ffi_ponteiros")
    if Problemas:
        return Problemas
    Codigo = (f"$f = FFI::cdef(file_get_contents('{Cabecalho.as_posix()}'), '{Dll.as_posix()}');"
              "$v = $f->new('struct fj_Vec3'); $v->f_x = 1; $v->f_y = 2; $v->f_z = 3;"
              "$i = $f->new('int32_t'); $i->cdata = 41; $f->incrementar(FFI::addr($i));"
              "echo $f->soma_vec($v), ',', $f->tamanho('Grifo'), ',', $i->cdata;")
    Rodada = subprocess.run([Php] + Opcoes + ["-r", Codigo], capture_output=True, text=True)
    if Rodada.returncode != 0 or Rodada.stdout.strip() != "6,5,42":
        Problemas.append(f"PHP FFI: exit {Rodada.returncode}, stdout {Rodada.stdout.strip()!r}, stderr {Rodada.stderr.strip()[:120]!r}")
    return Problemas


def CasoDllNuloSai101(Forja, PastaSaidas):
    """DLL de DEV: NULL num parametro ^T de proc exportado sai com 101 e R0005 citando o proc (D20); ?^T aceita NULL."""
    Problemas, Dll, _ = ConstruirDll(Forja, PastaSaidas, "biblioteca_ponteiros.fj", "ponteiros_dev", Release=False)
    if Problemas:
        return Problemas
    Programa = (f"import ctypes\nd = ctypes.CDLL(r'{Dll}')\nd.ler_opcional.restype = ctypes.c_int32\n"
                "d.ler_opcional.argtypes = [ctypes.c_void_p]\nprint(d.ler_opcional(None), flush=True)\n"
                "d.incrementar.argtypes = [ctypes.c_void_p]\nd.incrementar(None)\n")
    Rodada = subprocess.run([sys.executable, "-I", "-c", Programa], capture_output=True, text=True, timeout=TimeoutSegundos)
    if Rodada.returncode != 101 or "R0005" not in Rodada.stderr or "incrementar" not in Rodada.stderr or Rodada.stdout.strip() != "-1":
        Problemas.append(f"NULL na DLL de dev: exit {Rodada.returncode}, stdout {Rodada.stdout.strip()!r}, stderr {Rodada.stderr.strip()[:160]!r}")
    return Problemas


def CasoForeignDllDaForja(Forja, PastaSaidas):
    """foreign com link "soma": soma.dll feita pela Forja, achada por -L, -Bdynamic e pelo PATH do run; o exe roda ao lado da DLL."""
    Problemas, _, _ = ConstruirDll(Forja, PastaSaidas, "biblioteca_soma.fj", "soma")
    if Problemas:
        return Problemas
    Usa = PastaSaidas / "usa_soma.fj"
    shutil.copy(PastaAqui / "usa_soma.fj", Usa)
    Problemas += ConferirExecucao(Rodar(Forja, ["run", str(Usa)]), 0, "5")
    Exe = PastaSaidas / "usa.exe"
    Problemas += ConferirExecucao(Rodar(Forja, ["build", str(Usa), "-o", str(Exe)]), 0)
    if not Exe.exists():
        return Problemas + ["build nao gerou usa.exe"]
    Rodada = subprocess.run([str(Exe)], capture_output=True, text=True, timeout=TimeoutSegundos, cwd=PastaSaidas)
    return Problemas + ConferirExecucao(Rodada, 0, "5")


def CasoForeignLibAusente(Forja, PastaSaidas):
    """link de biblioteca que nao existe: X0102 (exit 1) no stdout, com a biblioteca em 'biblioteca' e nada no stderr (0.21.0 F4 passo 3: o pedido C saiu; o padrao x64 e a conferencia)."""
    Rodada = Rodar(Forja, ["build", "foreign_lib_ausente.fj", "-o", str(PastaSaidas / "ausente.exe")])
    Dados = UltimoJson(Rodada.stdout)
    Problemas = ConferirExecucao(Rodada, 1)
    if Dados is None or Dados.get("codigo") != "X0102" or Dados.get("biblioteca") != "biblioteca_que_nao_existe_v14":
        Problemas.append(f"stdout sem X0102 com a biblioteca: {Rodada.stdout.strip()[-200:]!r}")
    if Rodada.stderr.strip():
        Problemas.append(f"stderr devia estar vazio: {Rodada.stderr.strip()[-200:]!r}")
    return Problemas


def CasoForeignSimboloAusente(Forja, PastaSaidas):
    """proc foreign que nao existe na biblioteca: X0101 (exit 1) no stdout com o simbolo e nada no stderr (0.21.0 F4 passo 3: o pedido C saiu; o padrao x64 e a conferencia)."""
    Rodada = Rodar(Forja, ["build", "foreign_simbolo_ausente.fj", "-o", str(PastaSaidas / "simbolo.exe")])
    Dados = UltimoJson(Rodada.stdout)
    Problemas = ConferirExecucao(Rodada, 1)
    if Dados is None or Dados.get("codigo") != "X0101" or Dados.get("simbolo") != "funcao_que_nao_existe_v14":
        Problemas.append(f"stdout sem X0101 com o simbolo: {Rodada.stdout.strip()[-200:]!r}")
    if Rodada.stderr.strip():
        Problemas.append(f"stderr devia estar vazio: {Rodada.stderr.strip()[-200:]!r}")
    return Problemas


def CasoParidadeB03(Forja, PastaSaidas):
    """M-D5 (IA1-b03): b03\\md5_paridade.py --contra-congelado (0.21.0 F3: so o x64 contra o lado C congelado, sem gcc; (codigo, linha, coluna) em todos os err_*.fj, X0101/X0102 iguais menos alvo/etapa, import sem texto solto, B0004 do ld com ferramenta)."""
    Script = PastaAqui / "b03" / "md5_paridade.py"
    Rodada = subprocess.run([sys.executable, "-I", str(Script), "--forja", str(Forja), "--contra-congelado", "--saida", str(PastaSaidas / "md5")], capture_output=True, text=True,
                            encoding="utf-8", errors="replace", timeout=TimeoutSegundos)
    if Rodada.returncode != 0:
        return [f"md5_paridade.py saiu {Rodada.returncode}: {Rodada.stdout.strip()[-300:]!r}"]
    return []


def CasoForeignLibAusenteX64(Forja, PastaSaidas):
    """M4 P25b: a mesma foreign_lib_ausente.fj no x64 (o padrao do dev): X0102 (exit 1) no stdout, com a biblioteca em 'biblioteca'."""
    Rodada = Rodar(Forja, ["build", "--backend", "x64", "foreign_lib_ausente.fj", "-o", str(PastaSaidas / "ausente_x64.exe")])
    Dados = UltimoJson(Rodada.stdout)
    Problemas = ConferirExecucao(Rodada, 1)
    if Dados is None or Dados.get("codigo") != "X0102" or Dados.get("biblioteca") != "biblioteca_que_nao_existe_v14":
        Problemas.append(f"stdout sem X0102 com a biblioteca: {Rodada.stdout.strip()[-200:]!r}")
    return Problemas


def CasoForeignSimboloAusenteX64(Forja, PastaSaidas):
    """M4 P25b: a mesma foreign_simbolo_ausente.fj no x64 (o padrao do dev): X0101 (exit 1) no stdout, com o proc e o simbolo ausentes."""
    Rodada = Rodar(Forja, ["build", "--backend", "x64", "foreign_simbolo_ausente.fj", "-o", str(PastaSaidas / "simbolo_x64.exe")])
    Dados = UltimoJson(Rodada.stdout)
    Problemas = ConferirExecucao(Rodada, 1)
    if Dados is None or Dados.get("codigo") != "X0101" or Dados.get("simbolo") != "funcao_que_nao_existe_v14":
        Problemas.append(f"stdout sem X0101 com o simbolo: {Rodada.stdout.strip()[-200:]!r}")
    return Problemas


def CasoDllComForeign(Forja, PastaSaidas):
    """DLL com foreign: raiz(16.0) == 4.0; o .h traz so raiz; fj_sqrt e fjx_sqrt nao saem da DLL."""
    Problemas, Dll, Cabecalho = ConstruirDll(Forja, PastaSaidas, "dll_com_foreign.fj", "com_foreign")
    if Problemas:
        return Problemas
    B = ctypes.CDLL(str(Dll))
    B.raiz.restype, B.raiz.argtypes = ctypes.c_double, [ctypes.c_double]
    if B.raiz(16.0) != 4.0:
        Problemas.append(f"raiz(16.0) = {B.raiz(16.0)}, esperado 4.0")
    Texto = Cabecalho.read_text(encoding="utf-8")
    if "double raiz(double a0);" not in Texto or "sqrt" in Texto:
        Problemas.append(f".h sem 'double raiz(double a0);' ou com sqrt: {Texto!r}")
    for Escondido in ("fj_c_sqrt", "fjx_c_sqrt"):
        if hasattr(B, Escondido):
            Problemas.append(f"a DLL exporta '{Escondido}'")
    return Problemas


def CasoArrayDll(Forja, PastaSaidas):
    """DLL de dev com array interno: soma_quadrados(8) == 140, pega(1) == 2, .h sem fj_Arr/Fatia; pega(5) sai 101 com R0006."""
    Problemas, Dll, Cabecalho = ConstruirDll(Forja, PastaSaidas, "biblioteca_arrays.fj", "arrays_dev", Release=False)
    if Problemas:
        return Problemas
    Programa = (f"import ctypes\nd = ctypes.CDLL(r'{Dll}')\nd.soma_quadrados.restype = ctypes.c_int64\n"
                "d.soma_quadrados.argtypes = [ctypes.c_int32]\nd.pega.restype = ctypes.c_int32\nd.pega.argtypes = [ctypes.c_int32]\n"
                "print(d.soma_quadrados(8), d.pega(1), flush=True)\nd.pega(5)\n")
    Rodada = subprocess.run([sys.executable, "-I", "-c", Programa], capture_output=True, text=True, timeout=TimeoutSegundos)
    if Rodada.returncode != 101 or "R0006" not in Rodada.stderr or Rodada.stdout.strip() != "140 2":
        Problemas.append(f"DLL com array: exit {Rodada.returncode}, stdout {Rodada.stdout.strip()!r}, stderr {Rodada.stderr.strip()[:160]!r}")
    Texto = Cabecalho.read_text(encoding="utf-8")
    if "fj_Arr" in Texto or "Fatia" in Texto:
        Problemas.append(f".h com tipo de array ou fatia: {Texto!r}")
    return Problemas


def CasoListaDll(Forja, PastaSaidas):
    """DLL de dev com lista e new: soma_ate(10) == 45, vida_de == 100, .h sem fj_Din; soltar duas vezes sai 101 com R0008."""
    Problemas, Dll, Cabecalho = ConstruirDll(Forja, PastaSaidas, "biblioteca_listas.fj", "listas_dev", Release=False)
    if Problemas:
        return Problemas
    B = ctypes.CDLL(str(Dll))
    B.soma_ate.restype, B.soma_ate.argtypes = ctypes.c_int64, [ctypes.c_int32]
    B.novo_grifo.restype, B.novo_grifo.argtypes = ctypes.c_void_p, []
    B.vida_de.restype, B.vida_de.argtypes = ctypes.c_int32, [ctypes.c_void_p]
    B.soltar.restype, B.soltar.argtypes = None, [ctypes.c_void_p]
    Soma = B.soma_ate(10)
    Grifo = B.novo_grifo()
    Vida = B.vida_de(Grifo)
    B.soltar(Grifo)
    if (Soma, Vida) != (45, 100):
        Problemas.append(f"soma_ate(10)/vida_de = {(Soma, Vida)}, esperado (45, 100)")
    if "fj_Din" in Cabecalho.read_text(encoding="utf-8"):
        Problemas.append(".h com tipo de lista (fj_Din)")
    Programa = (f"import ctypes\nd = ctypes.CDLL(r'{Dll}')\nd.novo_grifo.restype = ctypes.c_void_p\nd.soltar.argtypes = [ctypes.c_void_p]\n"
                "g = d.novo_grifo()\nd.soltar(g)\nprint(1, flush=True)\nd.soltar(g)\n")
    Rodada = subprocess.run([sys.executable, "-I", "-c", Programa], capture_output=True, text=True, timeout=TimeoutSegundos)
    if Rodada.returncode != 101 or "R0008" not in Rodada.stderr:
        Problemas.append(f"soltar duas vezes: exit {Rodada.returncode}, stdout {Rodada.stdout.strip()!r}, stderr {Rodada.stderr.strip()[:160]!r}")
    return Problemas


def CasoExportCstrDll(Forja, PastaSaidas):
    """DLL de dev com cstr e ?cstr: saudacao/talvez devolvem texto C, forja_liberar solta; liberar duas vezes sai 101 (R0008)."""
    Problemas, Dll, Cabecalho = ConstruirDll(Forja, PastaSaidas, "biblioteca_texto.fj", "texto_dev", Release=False)
    if Problemas:
        return Problemas
    B = ctypes.CDLL(str(Dll))
    B.saudacao.restype, B.saudacao.argtypes = ctypes.c_void_p, [ctypes.c_int32]
    B.talvez.restype, B.talvez.argtypes = ctypes.c_void_p, [ctypes.c_int32]
    B.forja_liberar.restype, B.forja_liberar.argtypes = None, [ctypes.c_void_p]
    P = B.saudacao(5)
    if ctypes.string_at(P) != b"ola 5":
        Problemas.append(f"saudacao(5) = {ctypes.string_at(P)!r}, esperado b'ola 5'")
    B.forja_liberar(P)
    if B.talvez(-1) is not None:
        Problemas.append("talvez(-1) nao devolveu NULL")
    Q = B.talvez(3)
    if Q is None or ctypes.string_at(Q) != b"n=3":
        Problemas.append(f"talvez(3) = {Q!r}, esperado b'n=3'")
    if Q is not None:
        B.forja_liberar(Q)
    Texto = Cabecalho.read_text(encoding="utf-8")
    for Trecho in ("saudacao(", "talvez(", "forja_liberar("):
        if Trecho not in Texto:
            Problemas.append(f".h sem {Trecho!r}")
    Programa = (f"import ctypes\nd = ctypes.CDLL(r'{Dll}')\nd.saudacao.restype = ctypes.c_void_p\nd.forja_liberar.argtypes = [ctypes.c_void_p]\n"
                "p = d.saudacao(5)\nd.forja_liberar(p)\nprint(1, flush=True)\nd.forja_liberar(p)\n")
    Rodada = subprocess.run([sys.executable, "-I", "-c", Programa], capture_output=True, text=True, timeout=TimeoutSegundos)
    if Rodada.returncode != 101 or "R0008" not in Rodada.stderr:
        Problemas.append(f"forja_liberar duas vezes: exit {Rodada.returncode}, stderr {Rodada.stderr.strip()[:160]!r}")
    return Problemas


FIXTURES_DLL = ("biblioteca.fj", "biblioteca_tipos.fj", "biblioteca_numericos.fj", "biblioteca_ponteiros.fj", "biblioteca_listas.fj", "biblioteca_arrays.fj",
                "biblioteca_texto.fj", "biblioteca_soma.fj", "dll_com_foreign.fj")


def CasoCabecalhoX64IgualAoC(Forja, PastaSaidas):
    """M3 P22a: o .h do build --modo dll --backend x64 byte a byte igual ao valor fixo do .h do backend C (0.21.0 F3: esperados\\ffi_cabecalho_<fixture>.h), em todas as fixtures de DLL."""
    Problemas = []
    for Fonte in FIXTURES_DLL:
        Base = Fonte.removesuffix(".fj")
        Rodada = Rodar(Forja, ["build", "--modo", "dll", "--release", "--backend", "x64", Fonte, "-o", str(PastaSaidas / f"{Base}_hx.dll")])
        HX = PastaSaidas / f"{Base}_hx.h"
        Fixa = PastaAqui / "esperados" / f"ffi_cabecalho_{Base}.h"
        Problemas += [f"{Fonte}: {P}" for P in ConferirExecucao(Rodada, 0)]
        if HX.exists() and HX.read_bytes() != Fixa.read_bytes():
            Problemas.append(f"{Fonte}: .h do x64 difere do valor fixo do C (esperados\\ffi_cabecalho_{Base}.h)")
        elif not HX.exists():
            Problemas.append(f"{Fonte}: o x64 nao gravou o .h")
    return Problemas


def CasoDllFreeLibraryRelataVazamento(Forja, PastaSaidas):
    """DLL de dev com um bloco de heap vivo: FreeLibrary relata R0009 no stderr e o processo segue (sem ExitProcess, D29 da v1.8); 0.21.0 F3: o stderr do x64 e o valor fixo do C (esperados\\ffi_freelibrary_r0009_err.txt)."""
    Dll = PastaSaidas / "vazamento_x64.dll"
    Rodada = Rodar(Forja, ["build", "--modo", "dll", "--backend", "x64", "biblioteca_listas.fj", "-o", str(Dll)])
    Problemas = ConferirExecucao(Rodada, 0)
    Programa = (f"import ctypes\nd = ctypes.CDLL(r'{Dll}')\nd.novo_grifo.restype = ctypes.c_void_p\nd.novo_grifo()\n"
                "k = ctypes.WinDLL('kernel32')\nk.FreeLibrary.argtypes = [ctypes.c_void_p]\nprint(k.FreeLibrary(ctypes.c_void_p(d._handle)), flush=True)\nprint('vivo', flush=True)\n")
    Saida = subprocess.run([sys.executable, "-I", "-c", Programa], capture_output=True, text=True, timeout=TimeoutSegundos)
    Fixa = (PastaAqui / "esperados" / "ffi_freelibrary_r0009_err.txt").read_text(encoding="utf-8")
    if Saida.returncode != 0 or Saida.stdout.split() != ["1", "vivo"] or Saida.stderr != Fixa:
        Problemas.append(f"x64: FreeLibrary com bloco vivo: exit {Saida.returncode}, stdout {Saida.stdout.strip()!r}, stderr {Saida.stderr.strip()[:160]!r} (difere do valor fixo do C)")
    return Problemas


def CasoVendor(Forja, PastaSaidas):
    """import "vendor:soma_ffi": o modulo e a soma.dll ficam em vendor\\ ao lado do raiz; run acha o -L e o PATH da pasta vendor."""
    Pasta = PastaSaidas / "vendor_caso"
    (Pasta / "vendor").mkdir(parents=True, exist_ok=True)
    shutil.copy(PastaAqui / "usa_vendor.fj", Pasta / "usa_vendor.fj")
    shutil.copy(PastaAqui / "vendor" / "soma_ffi.fj", Pasta / "vendor" / "soma_ffi.fj")
    Problemas, _, _ = ConstruirDll(Forja, Pasta / "vendor", "biblioteca_soma.fj", "soma")
    if Problemas:
        return Problemas
    Rodada = Rodar(Forja, ["run", str(Pasta / "usa_vendor.fj")])
    return ConferirExecucao(Rodada, 0, "5")


def CasoLigarSemArgumento(Forja, PastaSaidas):
    """forja ligar sem nada: o script embutido recusa com G0104 (exit 2); exige Python na maquina."""
    Rodada = Rodar(Forja, ["ligar"])
    Problemas = ConferirExecucao(Rodada, 2)
    Dados = UltimoJson(Rodada.stderr)
    if Dados is None or Dados.get("codigo") != "G0104":
        Problemas.append(f"stderr sem G0104: {Rodada.stderr.strip()[:160]!r}")
    return Problemas
