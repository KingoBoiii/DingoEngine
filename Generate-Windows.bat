@echo off
pushd %~dp0\
call vendor\premake\bin\premake5.exe vs2026
popd
PAUSE