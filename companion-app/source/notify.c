// notify.c -- see notify.h. The request layout is the one the plugin's Notify() sends (GoldHEN SDK's
// OrbisNotificationRequest, from OSM's PS4-Notify); this app is a plain title, not a GoldHEN plugin, so it
// carries its own copy of the struct and calls the same libkernel function.
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <orbis/libkernel.h>

#include "app_state.h"   // g_cfg: the Notifications switch on Set up
#include "notify.h"

typedef struct {
    int type;               // 0 = a plain notification
    int reqId;
    int priority;
    int msgId;
    int targetId;           // -1 = every user
    int userId;
    int unk1;
    int unk2;
    int appId;
    int errorNum;
    int unk3;
    uint8_t useIconImageUri;
    char message[1024];
    char iconUri[1024];
    char unk[1024];
} AppNotifyRequest;

// Bound under another C name to the same symbol, the way main.c binds sceSystemServiceGetStatus, so this
// can't clash with however the SDK header declares it (or doesn't).
extern int app_sceKernelSendNotificationRequest(int device, void *req, size_t size, int blocking)
    __asm__("sceKernelSendNotificationRequest");

void app_notify(bool essential, const char *fmt, ...)
{
    if (!essential && !g_cfg.notificationsEnabled) return;

    char msg[176];
    va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof(msg), fmt, ap); va_end(ap);

    AppNotifyRequest req;
    memset(&req, 0, sizeof(req));
    req.type = 0;
    req.targetId = -1;
    req.useIconImageUri = 1;
    snprintf(req.message, sizeof(req.message), "PS4 Ambilight: %s", msg);
    snprintf(req.iconUri, sizeof(req.iconUri), "cxml://psnotification/tex_icon_system");
    app_sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}
