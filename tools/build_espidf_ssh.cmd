@echo off
setlocal
set "IDF_ROOT=C:\Espressif\v5.5.5\esp-idf"
set "IDF_TOOLS_PATH=C:\Users\subas\.espressif"
call "%IDF_ROOT%\export.bat"
if errorlevel 1 exit /b %errorlevel%
rem Keep the build reproducible when export.bat is run from a restricted shell.
set "PATH=%IDF_TOOLS_PATH%\tools\ninja\1.12.1;%IDF_TOOLS_PATH%\tools\cmake\3.30.2\bin;%IDF_TOOLS_PATH%\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin;%IDF_TOOLS_PATH%\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin;%IDF_TOOLS_PATH%\tools\idf-exe\1.0.3;%IDF_TOOLS_PATH%\tools\ccache\4.12.1;%SystemRoot%\System32;%SystemRoot%;C:\Program Files\Git\cmd;%PATH%"
set "CMAKE_MAKE_PROGRAM=%IDF_TOOLS_PATH%\tools\ninja\1.12.1\ninja.exe"
set "CC=%IDF_TOOLS_PATH%\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin\xtensa-esp32s3-elf-gcc.exe"
set "CXX=%IDF_TOOLS_PATH%\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin\xtensa-esp32s3-elf-g++.exe"
set "ASM=%CC%"
cd /d "%~dp0..\firmware\esp-idf"
if /i "%~1"=="--clean" if exist build\CMakeCache.txt (
    idf.py fullclean
    if errorlevel 1 exit /b %errorlevel%
) else if /i "%~1"=="--clean" if exist build (
    rmdir /s /q build
)
idf.py -D CMAKE_MAKE_PROGRAM="%CMAKE_MAKE_PROGRAM%" -D CMAKE_C_COMPILER="%IDF_TOOLS_PATH%\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin\xtensa-esp32s3-elf-gcc.exe" -D CMAKE_CXX_COMPILER="%IDF_TOOLS_PATH%\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin\xtensa-esp32s3-elf-g++.exe" set-target esp32s3
if errorlevel 1 exit /b %errorlevel%
idf.py build
exit /b %errorlevel%
