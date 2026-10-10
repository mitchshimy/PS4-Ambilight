// Run from companion-app/. Build (needs the /tmp/stub/orbis/libkernel.h from testing.md too):
//   gcc -w -I. -Iinclude -I../plugin/include -I/tmp/stub -Itests/stubs -o /tmp/test_ime \
//       tests/test_ime.c source/ime.c source/settings.c source/presets.c source/config.c tests/test_sce_stubs.c
//
// source/ime.c was split out of main.c. This plays the part of the system keyboard with fake
// sceImeDialog* functions (tests/stubs/orbis/ImeDialog.h declares their types) and checks what
// ime.c does with it: what it opens, what it applies, what it refuses. It cannot check the real
// SDK's behaviour, only ime.c's own logic.
#include "ime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <orbis/ImeDialog.h>

// ---- fake system keyboard ----
static int  g_initCalls, g_termCalls, g_initRet;
static int  g_commonUsed;
static OrbisDialogStatus g_status;
static OrbisDialogEndStatus g_end;
static OrbisImeDialogSetting g_seen;           // what ime.c passed to sceImeDialogInit
static wchar_t g_seenPlaceholder[80], g_seenTitle[80], g_seenInitial[64];
static wchar_t *g_buf;                         // ime.c's own buffer: where "typing" goes

int32_t sceImeDialogInit(OrbisImeDialogSetting *s, void *x)
{
    g_initCalls++;
    g_seen = *s;
    wcsncpy(g_seenPlaceholder, s->placeholder, 79);
    wcsncpy(g_seenTitle, s->title, 79);
    wcsncpy(g_seenInitial, s->inputTextBuffer, 63);
    g_buf = s->inputTextBuffer;
    return g_initRet;
}
OrbisDialogStatus sceImeDialogGetStatus(void) { return g_status; }
int32_t sceImeDialogGetResult(OrbisDialogResult *r) { r->endstatus = g_end; return 0; }
int32_t sceImeDialogTerm(void) { g_termCalls++; return 0; }
int sceCommonDialogIsUsed(void) { return g_commonUsed; }

// ---- helpers ----
static char g_msg[256]; static int g_isErr, g_statusCalls;
static void status_cb(const char *m, bool e) { snprintf(g_msg, sizeof(g_msg), "%s", m); g_isErr = e; g_statusCalls++; }

static void type_text(const char *s)
{
    size_t i = 0;
    for (; s[i]; i++) g_buf[i] = (wchar_t)(unsigned char)s[i];
    g_buf[i] = 0;
}
static void reset_fake(void)
{
    g_initCalls = g_termCalls = g_initRet = g_commonUsed = 0;
    g_status = ORBIS_DIALOG_STATUS_RUNNING; g_end = ORBIS_DIALOG_OK;
    g_msg[0] = 0; g_isErr = 0; g_statusCalls = 0;
}
static int first_of(int type) { for (int i = 0; i < kMenuItemCount; i++) if (kMenuItems[i].type == type) return i; return -1; }

static void close_and_finish(const ImeHost *h, const char *typed, OrbisDialogEndStatus end)
{
    if (typed) type_text(typed);
    g_end = end; g_status = ORBIS_DIALOG_STATUS_STOPPED;
    assert(ime_update(h));
    assert(!ime_is_open());
}

int main(void)
{
    AmbientConfig cfg; settings_set_defaults(&cfg);
    ImeHost host = { &cfg, 7, 960.0f, 540.0f, status_cb };
    int sIdx = first_of(FIELD_STRING), nIdx = first_of(FIELD_U16);
    if (nIdx < 0) nIdx = first_of(FIELD_U32);
    assert(sIdx >= 0 && nIdx >= 0);
    const MenuItem *num = &kMenuItems[nIdx];

    // Opening a numeric row: the right keyboard, the row's label, the current value, centred.
    reset_fake();
    assert(!ime_is_open());
    ime_open_field(nIdx, &host);
    assert(ime_is_open() && g_initCalls == 1);
    assert(g_seen.type == ORBIS_TYPE_NUMBER && g_seen.userId == 7);
    assert(g_seen.posx == 960.0f && g_seen.posy == 540.0f);
    char want[24]; snprintf(want, sizeof(want), "%d", settings_to_display(num, settings_get_i32(&cfg, num)));
    char got[24]; wcstombs(got, g_seenInitial, sizeof(got)); assert(!strcmp(got, want));
    char title[80]; wcstombs(title, g_seenTitle, sizeof(title)); assert(!strcmp(title, num->label));
    printf("opens a numeric row with its label and current value: PASSED\n");

    // While the system keyboard is still up, ime_update does nothing and reports nothing.
    g_status = ORBIS_DIALOG_STATUS_RUNNING;
    assert(!ime_update(&host) && ime_is_open() && g_statusCalls == 0);
    // A second open request while one is up is ignored.
    ime_open_field(nIdx, &host); assert(g_initCalls == 1);
    printf("waits while open, ignores a second open: PASSED\n");

    // Typing a number applies it, in display units, clamped into the row's range.
    int32_t lo = settings_display_min(num);
    char t[24]; snprintf(t, sizeof(t), "%d", lo);
    close_and_finish(&host, t, ORBIS_DIALOG_OK);
    assert(settings_to_display(num, settings_get_i32(&cfg, num)) == lo);
    assert(!g_isErr && strstr(g_msg, "set to") && g_termCalls == 1);
    ime_open_field(nIdx, &host);
    close_and_finish(&host, "99999999", ORBIS_DIALOG_OK);
    int32_t now = settings_to_display(num, settings_get_i32(&cfg, num));
    assert(now >= settings_display_min(num) && now <= settings_display_max(num));
    printf("a typed number is applied and clamped: PASSED\n");

    // Garbage and an empty entry are rejected and the value stays.
    int32_t before = settings_get_i32(&cfg, num);
    ime_open_field(nIdx, &host); close_and_finish(&host, "12abc", ORBIS_DIALOG_OK);
    assert(settings_get_i32(&cfg, num) == before && g_isErr && strstr(g_msg, "isn't a number"));
    ime_open_field(nIdx, &host); close_and_finish(&host, "", ORBIS_DIALOG_OK);
    assert(settings_get_i32(&cfg, num) == before && g_isErr);
    printf("garbage and empty input leave the value alone: PASSED\n");

    // Cancel leaves the value and says so.
    ime_open_field(nIdx, &host); close_and_finish(&host, "1", ORBIS_DIALOG_USER_CANCELED);
    assert(settings_get_i32(&cfg, num) == before && !g_isErr && strstr(g_msg, "cancelled"));
    printf("cancel changes nothing: PASSED\n");

    // A string row: the basic-latin keyboard, starts from the current text, writes the host.
    ime_open_field(sIdx, &host);
    assert(g_seen.type == ORBIS_TYPE_BASIC_LATIN);
    char hostNow[64]; wcstombs(hostNow, g_seenInitial, sizeof(hostNow)); assert(!strcmp(hostNow, cfg.wledHost));
    close_and_finish(&host, "10.1.2.3", ORBIS_DIALOG_OK);
    assert(!strcmp(cfg.wledHost, "10.1.2.3") && !g_isErr);
    printf("a string row edits the config's text: PASSED\n");

    // The system refusing to open: error shown, not marked open. Another system dialog up: no open.
    reset_fake(); g_initRet = -1;
    ime_open_field(nIdx, &host);
    assert(!ime_is_open() && g_isErr && strstr(g_msg, "Couldn't open"));
    reset_fake(); g_commonUsed = 1;
    ime_open_field(nIdx, &host);
    assert(!ime_is_open() && g_initCalls == 0);
    printf("refused opens are reported or skipped: PASSED\n");

    // Shutdown closes an open dialog once, and is harmless when nothing is open.
    reset_fake(); ime_shutdown(); assert(g_termCalls == 0);
    ime_open_field(nIdx, &host); ime_shutdown();
    assert(!ime_is_open() && g_termCalls == 1);
    assert(!ime_update(&host));   // nothing open: nothing to do
    printf("shutdown closes once: PASSED\n");

    printf("ALL IME CHECKS PASSED\n");
    return 0;
}
