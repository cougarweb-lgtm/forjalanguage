@echo off
chcp 65001 >nul 2>&1
:: construir.bat - gera forja.exe nesta pasta. Versao 1.0.0 - gcc -std=c11 -O2 -static, so biblioteca padrao.
:: Verificacao: python ..\medir\rodar_suite.py --compilador "%~dp0forja.exe" --corpus ..\corpus\dourado --registro _tentativas.jsonl
cd /d "%~dp0"
gcc -std=c11 -O2 -static -Wall -Wextra -o forja.exe main.c util.c lexer.c parser.c parser_stmt.c parser_decl.c check_types.c check_expr.c check_call.c check_stmt.c check_decl.c emit_expr.c emit_stmt.c
exit /b %errorlevel%
