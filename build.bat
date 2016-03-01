@echo off

pushd w:\loderunner

set CommonCompilerFlags= -DLL -MTd -nologo -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4127 -wd4201 -wd4100 -wd4189 -wd4505 -wd4706 -DBUILD_INTERNAL=1 -DBUILD_SLOW=0 -DBUILD_WIN32=1 -FC -Z7 -Fm
set CommonLinkerFlags= -incremental:no -opt:ref winmm.lib user32.lib gdi32.lib

if not defined DevEnvDir (
    call misc\shell.bat
)

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

@del /Q *.pdb > NUL 2> NUL
@del /Q *.gmi > NUL 2> NUL

REM 32-bit build
REM cl %CommonCompilerFlags% ..\loderunner\src\win32_loderunner.cpp /link -subsystem:windows,5.1 %CommonLinkerFlags%

REM 64-bit build

cl %CommonCompilerFlags% ..\loderunner\src\loderunner.cpp -LD /link -incremental:no -PDB:loderunner_%random%.pdb /EXPORT:GameUpdateAndRender -OUT:gamelib.dll
cl %CommonCompilerFlags% ..\loderunner\src\win32_loderunner.cpp /link %CommonLinkerFlags%

popd
popd