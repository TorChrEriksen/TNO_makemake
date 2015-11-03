@echo off

echo *************************************************************************
echo Preparing build location.
echo *************************************************************************
if not exist ..\build (
    echo Directory ..\build not found, creating...
    mkdir ..\build
    echo Directory ..\build created: SUCCESS. &echo.
) else (
    echo Found directory ..\build, deleting contents...
    del ..\build\* /Q
    echo Contents deleted: SUCCESS. & echo.
    
)

pushd ..\build

echo *************************************************************************
echo Compiling.
echo *************************************************************************
cl -FC -Zi ..\code\win32_makemake.cpp user32.lib Gdi32.lib

popd
