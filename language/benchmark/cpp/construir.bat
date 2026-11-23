@echo off
chcp 65001 >nul 2>&1
:: lint-ok: bat-crlf-obrigatorio arquivo gravado com LF pela ferramenta Write; este .bat so tem comandos simples, sem goto nem label
:: construir.bat - compila o forja.exe (C++20, -O2, -static, so biblioteca padrao).
:: Versao: 1.0.0
:: Changelog: 1.0.0 - primeira versao.
:: Verificacao: construir.bat && forja.exe check ..\corpus\dourado\ok_01_ola.fj
cd /d "%~dp0"
g++ -std=c++20 -O2 -static -Wall -Wextra -o forja.exe main.cpp lexer.cpp parser.cpp verificador.cpp verificador_expr.cpp emissor.cpp diagnostico.cpp
if errorlevel 1 exit /b 1
echo [OK] forja.exe gerado em %~dp0
