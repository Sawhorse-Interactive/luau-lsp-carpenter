@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d "%~dp0"

set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release

set TARGET=%2
if "%TARGET%"=="" set TARGET=Luau.LanguageServer.CLI

cmake --build build --target %TARGET% --config %CONFIG% -j%NUMBER_OF_PROCESSORS%
