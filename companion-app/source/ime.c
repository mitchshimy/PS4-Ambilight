// On-screen keyboard for the settings screens' numeric and string rows. See ime.h for the flow.
// Split out of main.c; the code is otherwise as it ran on hardware, with main.c's globals
// replaced by the ImeHost the caller passes in.
#include "ime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <orbis/libkernel.h>
#include <orbis/UserService.h>
#include <orbis/ImeDialog.h>
#include <orbis/CommonDialog.h>

// Real API surface pulled from orbis/ImeDialog.h, orbis/_types/
// ime_dialog.h and orbis/CommonDialog.h -- see the extensive
// verification notes this was ported from for exactly what's
// confirmed vs. assumed (posx/posy units, whether NULL is a valid
// second sceImeDialogInit argument, whether ORBIS_TYPE_NUMBER's layout
// includes '-' for negative fields). None of that re-verified this
// session; ported as-is.

static bool g_imeDialogOpen = false;
static wchar_t g_imeBuffer[64];
static int g_imeItemIndex = -1; // which kMenuItems[] entry this dialog is editing

static void widen_ascii(wchar_t *dst, const char *src, size_t dstCount)
{
    size_t i = 0;
    for (; i < dstCount - 1 && src[i] != '\0'; i++) dst[i] = (wchar_t)(unsigned char)src[i];
    dst[i] = L'\0';
}

// itemIndex must refer to a FIELD_STRING, FIELD_U32, FIELD_I32, or
// FIELD_U16 item -- FIELD_ENUM/FIELD_BOOL are cycled/toggled directly
// by Cross instead (see handle_settings_input()).
void ime_open_field(int itemIndex, const ImeHost *host)
{
    if (g_imeDialogOpen || sceCommonDialogIsUsed()) return;
    const MenuItem *item = &kMenuItems[itemIndex];

    char placeholderAscii[80], titleAscii[80], currentAscii[64];
    if (item->type == FIELD_STRING) {
        snprintf(placeholderAscii, sizeof(placeholderAscii), "192.168.x.x");
        snprintf(currentAscii, sizeof(currentAscii), "%s", host->cfg->wledHost);
    } else {
        const char *pct = (item->unit != UNIT_RAW) ? "%" : "";
        snprintf(placeholderAscii, sizeof(placeholderAscii), "%d to %d%s",
                 settings_display_min(item), settings_display_max(item), pct);
        snprintf(currentAscii, sizeof(currentAscii), "%d", settings_to_display(item, settings_get_i32(host->cfg, item)));
    }
    snprintf(titleAscii, sizeof(titleAscii), "%s", item->label);

    static wchar_t placeholderBuf[80], titleBuf[80];
    widen_ascii(placeholderBuf, placeholderAscii, sizeof(placeholderBuf) / sizeof(placeholderBuf[0]));
    widen_ascii(titleBuf, titleAscii, sizeof(titleBuf) / sizeof(titleBuf[0]));

    memset(g_imeBuffer, 0, sizeof(g_imeBuffer));
    widen_ascii(g_imeBuffer, currentAscii, sizeof(g_imeBuffer) / sizeof(g_imeBuffer[0]));

    OrbisImeDialogSetting setting;
    memset(&setting, 0, sizeof(setting));
    setting.userId = host->userId;
    setting.type = (item->type == FIELD_STRING) ? ORBIS_TYPE_BASIC_LATIN : ORBIS_TYPE_NUMBER;
    setting.supportedLanguages = 0;
    setting.enterLabel = ORBIS_BUTTON_LABEL_DEFAULT;
    setting.inputMethod = ORBIS__DEFAULT;
    setting.filter = NULL;
    setting.option = 0;
    setting.maxTextLength = (uint32_t)(sizeof(g_imeBuffer) / sizeof(g_imeBuffer[0]) - 1);
    setting.inputTextBuffer = g_imeBuffer;
    setting.posx = host->centerX;
    setting.posy = host->centerY;
    setting.horizontalAlignment = ORBIS_H_CENTER;
    setting.verticalAlignment = ORBIS_V_CENTER;
    setting.placeholder = placeholderBuf;
    setting.title = titleBuf;

    int32_t ret = sceImeDialogInit(&setting, NULL);
    printf("[ime] sceImeDialogInit() = %d (field \"%s\")\n", ret, item->label);
    if (ret == 0) {
        g_imeDialogOpen = true;
        g_imeItemIndex = itemIndex;
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "Couldn't open the keyboard (sceImeDialogInit = %d).", ret);
        host->status(msg, true);
    }
}

bool ime_is_open(void) { return g_imeDialogOpen; }

void ime_shutdown(void)
{
    if (!g_imeDialogOpen) return;
    sceImeDialogTerm();
    g_imeDialogOpen = false;
    g_imeItemIndex = -1;
}

// Call once per frame from the main loop while g_imeDialogOpen.
bool ime_update(const ImeHost *host)
{
    if (!g_imeDialogOpen) return false;
    OrbisDialogStatus status = sceImeDialogGetStatus();
    if (status != ORBIS_DIALOG_STATUS_STOPPED) return false;

    OrbisDialogResult result;
    memset(&result, 0, sizeof(result));
    sceImeDialogGetResult(&result);

    const MenuItem *item = (g_imeItemIndex >= 0 && g_imeItemIndex < kMenuItemCount) ? &kMenuItems[g_imeItemIndex] : NULL;

    if (result.endstatus == ORBIS_DIALOG_OK && item != NULL) {
        char typed[64];
        size_t i = 0;
        for (; i < sizeof(typed) - 1 && g_imeBuffer[i] != L'\0'; i++) typed[i] = (char)g_imeBuffer[i];
        typed[i] = '\0';

        char msg[160];
        if (item->type == FIELD_STRING) {
            char *dst = (char *)host->cfg + item->offset;
            snprintf(dst, (size_t)item->max /* buffer size, see STRBUF() in settings.c */, "%s", typed);
            snprintf(msg, sizeof(msg), "%s set to %s.", item->label, dst);
            host->status(msg, false);
        } else {
            // strtol, not atoi: atoi can't distinguish "not a number"
            // from a real 0, and silently writing 0 for garbage input
            // is exactly the kind of accepted-but-wrong save
            // settings_load()'s own hardening was written to prevent
            // on the load side.
            char *end = NULL;
            long v = strtol(typed, &end, 10);
            if (end == typed || *end != '\0') {
                snprintf(msg, sizeof(msg), "\"%s\" isn't a number -- %s unchanged.", typed, item->label);
                host->status(msg, true);
            } else {
                // What was typed is in DISPLAY units (a percent for the
                // percent-style rows); settings_from_display converts and
                // clamps into [item->min, item->max].
                settings_set_i32(host->cfg, item, settings_from_display(item, (int32_t)v));
                char vbuf[24];
                settings_format_display(item, settings_to_display(item, settings_get_i32(host->cfg, item)), vbuf, sizeof(vbuf));
                snprintf(msg, sizeof(msg), "%s set to %s.", item->label, vbuf);
                host->status(msg, false);
            }
        }
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "Keyboard cancelled -- %s unchanged.", item ? item->label : "field");
        host->status(msg, false);
    }

    sceImeDialogTerm();
    g_imeDialogOpen = false;
    g_imeItemIndex = -1;
    return true;
}
