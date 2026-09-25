#ifndef PLUGIN_COMMON_H
#define PLUGIN_COMMON_H

// config.c (copied verbatim from ps4_ambient_light's plugin source)
// expects this header to provide debug_printf. This app is a
// standalone homebrew application, not a GoldHEN plugin, so none of
// the real plugin_common.h's hook/attr_public/GOLDHEN_PATH machinery
// applies here -- only config.c's one dependency is stubbed.
#include <stdio.h>
#ifndef __FINAL__
#define debug_printf(a, args...) printf("[config] " a, ##args)
#else
#define debug_printf(a, args...)
#endif

// BUG FIX: every sceKernelOpen() call in this app that means to
// create-or-overwrite a file was passing the literal "0x200 | 0x001"
// with a comment claiming that's O_TRUNC|O_CREAT. It isn't, on this
// OS. Orbis (the PS4's kernel) is FreeBSD-derived, and sceKernelOpen
// is a thin wrapper straight onto the real BSD open(2) syscall, whose
// flag bits are 0x0001=O_WRONLY, 0x0200=O_CREAT, 0x0400=O_TRUNC (NOT
// the Linux/glibc values, which some of this file's flags were
// evidently guessed from). So "0x200 | 0x001" is actually
// O_CREAT|O_WRONLY, with O_TRUNC's 0x400 bit missing entirely.
//
// For a path being written for the very first time this is invisible
// -- O_CREAT alone against a nonexistent file already starts it at
// length 0. It only shows up against a path that already has content
// from a previous write: the new data overwrites the front of the
// file, but if the new write is shorter than whatever was already
// there, the old trailing bytes are never discarded. do_plugin_update()
// in main.c hits this on its staging file (PLUGIN_PRX_PATH ".new"),
// which can carry leftover bytes from an earlier update attempt: the
// SHA-256 verified against the release is computed while streaming
// the download (so it's correct), but the file that then actually
// gets copied/renamed into place can be longer than what was
// verified, and PLUGIN_PRX_PATH's real on-disk hash stops matching
// GitHub's published checksum from then on -- "update available"
// every launch even though the release itself never changed.
//
// Named here (rather than re-guessing 0x601 inline at each call site)
// so every "create or overwrite" sceKernelOpen() across this app --
// config.c's ini writer, main.c's tmp download / checksum sidecar /
// plugins.ini rewrite / staged .prx copy -- shares one correct value.
#define ORBIS_O_WRONLY 0x0001
#define ORBIS_O_CREAT  0x0200
#define ORBIS_O_TRUNC  0x0400
#define ORBIS_O_CREAT_TRUNC_WRONLY (ORBIS_O_CREAT | ORBIS_O_TRUNC | ORBIS_O_WRONLY)

#endif // PLUGIN_COMMON_H
