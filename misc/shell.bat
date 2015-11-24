@echo off

REM echo shell.bat finished
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64
set path=w:\loderunner\misc;%path%
doskey ci=git commit -a -m

