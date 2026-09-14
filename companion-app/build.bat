SETLOCAL EnableDelayedExpansion

Rem ============================================================
Rem Adapted from two REAL, verified OpenOrbis sample build.bats:
Rem   samples/SDL2/SDL2/build.bat        (rendering/input libraries)
Rem   samples/net_http/net_http/build.bat (HTTP/SSL/Net libraries)
Rem Merged, not guessed -- every library name below was checked
Rem against one of those two real, working scripts.
Rem
Rem ONE UNVERIFIED THING, flagged rather than silently assumed:
Rem main.c's draw_text() calls TTF_Init/TTF_OpenFont/TTF_RenderText_
Rem Blended/TTF_CloseFont/TTF_Quit (SDL2_ttf's API). The real SDL2
Rem sample's own build.bat links "-lSDL2 -lSDL2_image" and
Rem "-lSceFreeType" but does NOT link "-lSDL2_ttf" anywhere -- and no
Rem compiled SDL2_ttf library was found in this SDK snapshot's lib/
Rem folder, only the header (include/SDL2/SDL_ttf.h). That means
Rem -lSDL2_ttf below is NOT confirmed to actually link. Before this
Rem will build as-is, one of these needs to happen:
Rem   1. Build SDL2_ttf from source against this SDK's real
Rem      libSceFreeType (confirmed present) and add the resulting
Rem      library here, or
Rem   2. Replace draw_text() in main.c with direct FreeType calls
Rem      (freetype.h is confirmed present, libSceFreeType.so is
Rem      confirmed present -- this is the more-work-but-definitely-
Rem      possible path), or
Rem   3. Strip text entirely for a v1 and rely on rects/highlighting
Rem      only (fastest to get something actually building).
Rem This build.bat currently takes option-shaped approach 1 (still
Rem links -lSDL2_ttf) so the gap is visible as a link failure rather
Rem than silently swapped for option 3 without saying so.
Rem ============================================================

set PKG_TITLE="PS4 Ambient Light Companion"
set PKG_VERSION="1.00"
set PKG_ASSETS="assets"
set PKG_TITLE_ID="BREW00091"
set PKG_CONTENT_ID="IV0000-BREW00091_00-AMBIENTCOMPANION0"

Rem Base libraries copied verbatim from samples/SDL2/build.bat, minus
Rem SDL2_image (this app has no images to load, only fonts + solid
Rem rects) plus samples/net_http/build.bat's Net/Ssl/Http libraries for
Rem the plugin self-update feature.
set libraries=-lc -lkernel -lc++ -lSceUserService -lSceVideoOut -lSceAudioOut -lScePad -lSceSysmodule -lSceFreeType -lSDL2 -lSDL2_ttf -lSceNet -lSceSsl -lSceHttp

set intdir=%1
set targetname=%~2
set outputPath=%3

set outputElf=%intdir%\%targetname%.elf
set outputOelf=%intdir%\%targetname%.oelf

@mkdir %intdir%

for %%f in (source\*.c) do (
    clang --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"%OO_PS4_TOOLCHAIN%\include" -I"%OO_PS4_TOOLCHAIN%\include\c++\v1" -I"include" -c -o %intdir%\%%~nf.o %%f
)

set obj_files=
for %%f in (%1\*.o) do set obj_files=!obj_files! .\%%f

ld.lld -m elf_x86_64 -pie --script "%OO_PS4_TOOLCHAIN%\link.x" --eh-frame-hdr -o "%outputElf%" "-L%OO_PS4_TOOLCHAIN%\lib" %libraries% --verbose "%OO_PS4_TOOLCHAIN%\lib\crt1.o" %obj_files%

%OO_PS4_TOOLCHAIN%\bin\windows\create-fself.exe -in "%outputElf%" --out "%outputOelf%" --eboot "eboot.bin" --paid 0x3800000000000011

copy "eboot.bin" %outputPath%\eboot.bin
del "eboot.bin"

cd ..
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_new sce_sys/param.sfo
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_setentry sce_sys/param.sfo APP_TYPE --type Integer --maxsize 4 --value 1
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_setentry sce_sys/param.sfo APP_VER --type Utf8 --maxsize 8 --value %PKG_VERSION%
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_setentry sce_sys/param.sfo ATTRIBUTE --type Integer --maxsize 4 --value 0
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_setentry sce_sys/param.sfo CATEGORY --type Utf8 --maxsize 4 --value "gd"
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_setentry sce_sys/param.sfo CONTENT_ID --type Utf8 --maxsize 48 --value %PKG_CONTENT_ID%
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_setentry sce_sys/param.sfo DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_setentry sce_sys/param.sfo SYSTEM_VER --type Integer --maxsize 4 --value 0
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_setentry sce_sys/param.sfo TITLE --type Utf8 --maxsize 128 --value %PKG_TITLE%
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_setentry sce_sys/param.sfo TITLE_ID --type Utf8 --maxsize 12 --value %PKG_TITLE_ID%
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_setentry sce_sys/param.sfo VERSION --type Utf8 --maxsize 8 --value %PKG_VERSION%

set module_files=
for %%f in (sce_module\*) do set module_files=!module_files! sce_module/%%~nxf

set asset_fonts_files=
for %%f in (assets\fonts\*) do set asset_fonts_files=!asset_fonts_files! assets/fonts/%%~nxf

%OO_PS4_TOOLCHAIN%\bin\windows\create-gp4.exe -out pkg.gp4 --content-id=%PKG_CONTENT_ID% --files "eboot.bin sce_sys/about/right.sprx sce_sys/param.sfo sce_sys/icon0.png %module_files% %asset_fonts_files%"

%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe pkg_build pkg.gp4 .
