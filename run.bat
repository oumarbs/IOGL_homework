@echo off
set "CURRENT_DIR=%~dp0"
set "CURRENT_DIR=%CURRENT_DIR:~0,-1%"
for /f "usebackq delims=" %%i in (`wsl wslpath -u "%CURRENT_DIR%"`) do set "WSL_DIR=%%i"

wsl -e bash -c "cd '%WSL_DIR%' && make && GALLIUM_DRIVER=d3d12 ./night_fair"
