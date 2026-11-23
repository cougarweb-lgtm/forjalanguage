import re, sys
p = sys.argv[1]
t = open(p, encoding="utf-8", newline="").read()
n = 0
# campos ponteiro-de-funcao de b2WorldDef (typedef Fcn*) -> void*
for nome in ["frictionCallback", "restitutionCallback", "enqueueTask", "finishTask"]:
    t, k = re.subn(r"b2\w+CallbackFcn\*\s+" + nome + r";", "void* " + nome + ";", t); n += k
# campos ponteiro-de-funcao de b2DebugDraw: void ( *DrawXFcn )( ... ); (pode ocupar varias linhas)
t, k = re.subn(r"void\s*\(\s*\*(Draw\w+Fcn)\s*\)\s*\([^;]*?\)\s*;", r"void* \1;", t, flags=re.S); n += k
open(p, "w", encoding="utf-8", newline="").write(t)
print("substituicoes", n)
