@echo off
chcp 65001 >nul 2>&1
:: rodar_teste_rt.bat - compila teste_rt.c em DEV (-DFORJA_DEV) e em release (-fwrapv) e roda os dois. Versao 0.1.0.
:: gcc -std=c11 -Wall -Wextra -Werror -D__USE_MINGW_ANSI_STDIO=1; so biblioteca padrao.
:: Verificacao: exit 0 so se os dois modos passaram (saida final "OK: N verificacoes" de cada modo).
cd /d "%~dp0"
set FLAGS=-std=c11 -Wall -Wextra -Werror -D__USE_MINGW_ANSI_STDIO=1
echo [DEV] compilando com -DFORJA_DEV
gcc %FLAGS% -DFORJA_DEV -o teste_rt_dev.exe teste_rt.c
if errorlevel 1 exit /b 1
echo [DEV] capturando print_* e executor (linhas JSON em stderr sao falhas deliberadas)
.\teste_rt_dev.exe --gerar-print
.\teste_rt_dev.exe --gerar-runner
.\teste_rt_dev.exe --gerar-runner-filtro
.\teste_rt_dev.exe --gerar-runner-json
.\teste_rt_dev.exe --gerar-runner-vazamento
.\teste_rt_dev.exe --gerar-runner-queda
.\teste_rt_dev.exe
if errorlevel 1 exit /b 1
echo [RELEASE] compilando sem FORJA_DEV, com -fwrapv
gcc %FLAGS% -fwrapv -DFORJA_SEM_CHECAGEM -o teste_rt_release.exe teste_rt.c
if errorlevel 1 exit /b 1
echo [RELEASE] capturando print_* e executor
.\teste_rt_release.exe --gerar-print
.\teste_rt_release.exe --gerar-runner
.\teste_rt_release.exe --gerar-runner-filtro
.\teste_rt_release.exe --gerar-runner-json
.\teste_rt_release.exe --gerar-runner-vazamento
.\teste_rt_release.exe --gerar-runner-queda
.\teste_rt_release.exe
if errorlevel 1 exit /b 1
echo [OK] DEV e release passaram
exit /b 0
