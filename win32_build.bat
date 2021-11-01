@echo off

set app_name=my_text_editor
set build_options=
set compiler_flags=-nologo -O2 -Z7 -wd4533
set linker_flags=user32.lib winmm.lib gdi32.lib

if not exist build mkdir build
pushd build

start /b /wait "" "cl.exe" %build_options% %compiler_flags% ..\src\win32_my_text_editor.cpp /link %linker_flags% /out:%app_name%.exe

popd