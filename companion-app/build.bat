SETLOCAL EnableDelayedExpansion

Rem ============================================================
Rem Adapted from two REAL, verified OpenOrbis sample build.bats:
Rem   samples/SDL2/SDL2/build.bat        (rendering/input libraries)
Rem   samples/net_http/net_http/build.bat (HTTP/SSL/Net libraries)
Rem Merged, not guessed -- every library name below was checked
Rem against one of those two real, working scripts.
Rem
Rem TEXT RENDERING: SDL2_ttf was never actually linkable in this SDK
Rem snapshot (header only, no compiled lib -- and note the libraries=
Rem line below never actually contained -lSDL2_ttf either, despite an
Rem earlier version of this comment implying it did). Resolved by
Rem going straight to FreeType: main.c's draw_text() now goes through
Rem source/text_render.c, which calls real FreeType (ft2build.h /
Rem FT_FREETYPE_H) directly against libSceFreeType -- already linked
Rem below via -lSceFreeType, no new library needed. The clang compile
Rem line now also adds an -I for the SDK's freetype2 include dir so
Rem <ft2build.h> resolves.
Rem ============================================================

set PKG_TITLE="PS4 Ambilight"
set PKG_VERSION="1.00"
set PKG_ASSETS="assets"
set PKG_TITLE_ID="SHMY00091"
set PKG_CONTENT_ID="IV0000-SHMY00091_00-PS4AMBILIGHT0000"

Rem Base libraries copied verbatim from samples/SDL2/build.bat, minus
Rem SDL2_image (this app has no images to load, only fonts + solid
Rem rects) plus samples/net_http/build.bat's Net/Ssl/Http libraries for
Rem the plugin self-update feature.
Rem CommonDialog/ImeDialog (on-screen keyboard, orbis/CommonDialog.h +
Rem orbis/ImeDialog.h): unlike every other library above, this pairing
Rem is NOT confirmed against a working sample -- this SDK has no
Rem IME-dialog sample at all (samples/keyboard is a *physical*
Rem keyboard, a different API). Named by pattern-match only: every
Rem other library on this line follows header-name -> Sce-prefixed
Rem library name exactly (Http.h->SceHttp, Pad.h->ScePad,
Rem UserService.h->SceUserService, 7 for 7 with zero exceptions), so
Rem CommonDialog.h/ImeDialog.h -> SceCommonDialog/SceImeDialog by that
Rem same pattern. Also worth knowing: this SDK's public repo ships lib/
Rem as an empty placeholder (its own README says so) -- every .so
Rem here, including these two, has to already exist in your own
Rem E:\OpenOrbis\lib\ from whatever stub-generation process produced
Rem the others. If SceCommonDialog/SceImeDialog aren't there yet,
Rem generate/add them the same way the rest were.
Rem
Rem SceSystemService (orbis/SystemService.h -> sceSystemServiceLoadExec):
Rem added for the clean-exit fix -- main() now hands control back to
Rem the XMB via sceSystemServiceLoadExec("exit", NULL) instead of just
Rem falling off the end of main(), matching how apollo-ps4 and
Rem ItemzFlow both close (see main.c's own comment at the call site).
Rem The library was already present at runtime without this --
Rem libSceSystemService.sprx shows up in this app's own putty.log
Rem crash dump as an already-loaded dynamic library -- so this is only
Rem adding the link-time symbol, not a new runtime dependency.
set libraries=-lc -lkernel -lc++ -lSceUserService -lSceVideoOut -lSceAudioOut -lScePad -lSceSysmodule -lSceFreeType -lSDL2 -lSceNet -lSceSsl -lSceHttp -lSceCommonDialog -lSceImeDialog -lSceSystemService

set intdir=%1
set targetname=%~2
set outputPath=%3

set outputElf=%intdir%\%targetname%.elf
set outputOelf=%intdir%\%targetname%.oelf

@mkdir %intdir%

Rem freetype2 include added for text_render.c's <ft2build.h> / FT_FREETYPE_H
Rem -- standard FreeType2 install layout puts headers under an SDK
Rem freetype2\ subfolder, not directly in include\. If your toolchain
Rem snapshot places them elsewhere, adjust this one -I.
for %%f in (source\*.c) do (
    clang --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"%OO_PS4_TOOLCHAIN%\include" -I"%OO_PS4_TOOLCHAIN%\include\freetype2" -I"%OO_PS4_TOOLCHAIN%\include\c++\v1" -I"include" -c -o %intdir%\%%~nf.o %%f
)

set obj_files=
for %%f in (%1\*.o) do set obj_files=!obj_files! .\%%f

ld.lld -m elf_x86_64 -pie --script "%OO_PS4_TOOLCHAIN%\link.x" --eh-frame-hdr -o "%outputElf%" "-L%OO_PS4_TOOLCHAIN%\lib" %libraries% --verbose "%OO_PS4_TOOLCHAIN%\lib\crt1.o" %obj_files%

%OO_PS4_TOOLCHAIN%\bin\windows\create-fself.exe -in "%outputElf%" --out "%outputOelf%" --eboot "eboot.bin" --paid 0x3800000000000011

copy "eboot.bin" %outputPath%\eboot.bin

%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_new sce_sys/param.sfo
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_setentry sce_sys/param.sfo APP_TYPE --type Integer --maxsize 4 --value 1
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_setentry sce_sys/param.sfo APP_VER --type Utf8 --maxsize 8 --value %PKG_VERSION%
Rem ATTRIBUTE bit 0x02 = "Enter Button Assignment for the common dialog:
Rem Cross button" (confirmed against the real PS4 param.sfo ATTRIBUTE flag
Rem table). Previously left at 0, which sets neither this bit nor bit 0x20
Rem ("Assigned by the System Software") -- and with neither bit set, common
Rem dialogs launched by THIS title (the WLED-host on-screen keyboard, via
Rem sceImeDialogInit) came up with Circle as confirm and Cross as cancel,
Rem even though the console's own system keyboard (e.g. entering a WiFi
Rem password from Settings) uses Cross=confirm. That's what was actually
Rem behind the "X closes the keyboard instead of selecting a character"
Rem report -- not a controller-input bug, a packaging attribute this
Rem specific title ships with. Setting bit 0x02 pins this title's own
Rem common dialogs to Cross=select/Circle=cancel, matching the rest of the
Rem system, regardless of the console's global Enter Button Assignment
Rem setting.
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_setentry sce_sys/param.sfo ATTRIBUTE --type Integer --maxsize 4 --value 2
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