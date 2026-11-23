@echo off
chcp 65001 >nul 2>&1
setlocal enabledelayedexpansion
:: instalar.bat - instala o forja.exe numa pasta. Versao 1.0.2 (Fase 5i, Entrega).
:: Changelog: 1.0.2 - 0.21.0 F4 passo 2b: sai a checagem do gcc (FORJA_CC e gcc no PATH); instalar o forja nao exige compilador C.
::            1.0.1 - ORIGEM guardada antes do shift (o shift desloca o %0 e %~dp0 passava a apontar para outra pasta).
:: Uso:  instalar.bat [pasta_destino] [--path]
::   pasta_destino  padrao %LOCALAPPDATA%\Forja\bin (nao precisa de administrador)
::   --path         acrescenta a pasta ao PATH do USUARIO (so se ainda nao estiver; abra um terminal novo depois)
:: O forja.exe e estatico e traz o runtime dentro: instalar = copiar. Nao precisa de compilador C (gcc) nem de FORJA_CC.
:: Desfazer: apague a pasta de destino e tire a entrada do PATH do usuario.
:: Verificacao: instalar.bat %TEMP%\forja_teste   (sem --path)  ->  "forja X.Y.Z"
set ORIGEM=%~dp0
cd /d "%ORIGEM%"
set DESTINO=%LOCALAPPDATA%\Forja\bin
set COM_PATH=0
:LerArgs
if "%~1"=="" goto :ArgsLidos
if /i "%~1"=="--path" (set COM_PATH=1) else (set DESTINO=%~1)
shift
goto :LerArgs
:ArgsLidos

if not exist "%ORIGEM%forja.exe" (
    echo ERRO: forja.exe nao encontrado em %ORIGEM% - rode construir.bat antes.
    exit /b 1
)
if not exist "%DESTINO%" mkdir "%DESTINO%"
if errorlevel 1 (
    echo ERRO: nao consegui criar a pasta %DESTINO%
    exit /b 1
)
copy /y "%ORIGEM%forja.exe" "%DESTINO%\forja.exe" >nul
if errorlevel 1 (
    echo ERRO: nao consegui copiar o forja.exe para %DESTINO% - feche programas que o usam e tente de novo.
    exit /b 1
)
"%DESTINO%\forja.exe" --versao
if errorlevel 1 (
    echo ERRO: o forja.exe instalado nao respondeu a --versao.
    exit /b 1
)

if "%COM_PATH%"=="1" (
    powershell -NoProfile -Command "$d = $env:DESTINO; $u = [Environment]::GetEnvironmentVariable('Path','User'); if ($null -eq $u) { $u = '' }; $itens = $u -split ';' | Where-Object { $_ -ne '' }; if ($itens -contains $d) { 'PATH do usuario: ja continha a pasta' } else { [Environment]::SetEnvironmentVariable('Path', (($itens + $d) -join ';'), 'User'); 'PATH do usuario: pasta acrescentada - abra um terminal novo' }"
    if errorlevel 1 (
        echo ERRO: nao consegui atualizar o PATH do usuario.
        exit /b 1
    )
) else (
    echo PATH: nao alterado ^(use --path para acrescentar %DESTINO% ao PATH do usuario^).
)
echo Instalado em %DESTINO%
exit /b 0
