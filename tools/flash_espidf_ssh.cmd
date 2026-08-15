@echo off
setlocal
set "IDF_ROOT=C:\Espressif\v5.5.5\esp-idf"
set "IDF_TOOLS_PATH=C:\Users\subas\.espressif"
call "%IDF_ROOT%\export.bat"
if errorlevel 1 exit /b %errorlevel%
set "PATH=%IDF_TOOLS_PATH%\tools\ninja\1.12.1;%IDF_TOOLS_PATH%\tools\cmake\3.30.2\bin;%IDF_TOOLS_PATH%\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin;%IDF_TOOLS_PATH%\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin;%IDF_TOOLS_PATH%\tools\idf-exe\1.0.3;%IDF_TOOLS_PATH%\tools\ccache\4.12.1;%SystemRoot%\System32;%SystemRoot%;C:\Program Files\Git\cmd;%PATH%"
set "CMAKE_MAKE_PROGRAM=%IDF_TOOLS_PATH%\tools\ninja\1.12.1\ninja.exe"
cd /d "%~dp0..\firmware\esp-idf"
set "PORT=%~1"
if "%PORT%"=="" set "PORT=COM4"
idf.py -p "%PORT%" flash
exit /b %errorlevel%
