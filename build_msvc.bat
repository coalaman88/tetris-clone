@echo OFF

where /q cl || (
  echo ERROR: "cl" not found - please run this from the MSVC x64 native tools command prompt.
  exit /b 1
)

set name=program.exe
set compiler=cl
set files=main.c windows.c fonts.c render.c engine.c menu.c
set libs=User32.lib Gdi32.lib Winmm.lib
set warnings=/WX /W4 /wd4996 /wd4201 /wd4152 /wd4100
set debug_warnings=/wd4189 /wd4101
set debugger=/Zi

set includes=
set "includes=%includes% /Idependencies\freetype-lib"
set "includes=%includes% /Idependencies\opengl-lib"
set "includes=%includes% /Idependencies\stb-lib"

if /I [%1]==[debug] (
  echo -DEBUG build-
  (call %caller% "%compiler%" %includes% /nologo %debugger% %warnings% %debug_warnings% %files% %libs% /Fe%name% /D DEBUG_BUILD /link /incremental:no && (call %name%))
  exit /b
)

if /I [%1]==[release] (
  echo -RELEASE build-
  (call %caller% "%compiler%" %includes% /nologo %warnings% %files% %libs% /Fe%name% /link /incremental:no && (call %name%))
  exit /b
)

echo No build configuration! Use "debug" or "release"
exit /b
