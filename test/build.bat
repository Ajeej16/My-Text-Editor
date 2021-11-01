@echo off

set console=/subsystem:console

if not exist build mkdir build
pushd build

cl ../main.cpp /link %console% /out:text.exe

popd