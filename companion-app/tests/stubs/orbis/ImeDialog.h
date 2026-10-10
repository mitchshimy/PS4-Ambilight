// TEST-ONLY stand-in for the OpenOrbis SDK's <orbis/ImeDialog.h> (and the dialog types from
// CommonDialog.h), so source/ime.c builds on a PC. It declares only what ime.c uses, with the
// SDK's names; tests/test_ime.c supplies fake sceIme* / sceCommonDialog* functions that play the
// part of the system keyboard. It checks ime.c's own logic, not the real SDK's types, which only
// the CI package build and a console exercise.
#ifndef TEST_STUB_IME_DIALOG_H
#define TEST_STUB_IME_DIALOG_H
#include <stdint.h>
#include <stddef.h>
#include <wchar.h>

typedef enum { ORBIS_TYPE_BASIC_LATIN, ORBIS_TYPE_NUMBER } OrbisImeType;
typedef enum { ORBIS_BUTTON_LABEL_DEFAULT } OrbisImeEnterLabel;
typedef enum { ORBIS__DEFAULT } OrbisImeInputMethod;
typedef enum { ORBIS_H_CENTER } OrbisImeHorizontalAlignment;
typedef enum { ORBIS_V_CENTER } OrbisImeVerticalAlignment;
typedef enum { ORBIS_DIALOG_STATUS_NONE, ORBIS_DIALOG_STATUS_RUNNING, ORBIS_DIALOG_STATUS_STOPPED } OrbisDialogStatus;
typedef enum { ORBIS_DIALOG_OK, ORBIS_DIALOG_USER_CANCELED } OrbisDialogEndStatus;

typedef struct {
    OrbisDialogEndStatus endstatus;
} OrbisDialogResult;

typedef struct {
    int32_t userId;
    OrbisImeType type;
    uint64_t supportedLanguages;
    OrbisImeEnterLabel enterLabel;
    OrbisImeInputMethod inputMethod;
    void *filter;
    uint32_t option;
    uint32_t maxTextLength;
    wchar_t *inputTextBuffer;
    float posx, posy;
    OrbisImeHorizontalAlignment horizontalAlignment;
    OrbisImeVerticalAlignment verticalAlignment;
    const wchar_t *placeholder;
    const wchar_t *title;
} OrbisImeDialogSetting;

int32_t sceImeDialogInit(OrbisImeDialogSetting *setting, void *extended);
OrbisDialogStatus sceImeDialogGetStatus(void);
int32_t sceImeDialogGetResult(OrbisDialogResult *result);
int32_t sceImeDialogTerm(void);
int sceCommonDialogIsUsed(void);
#endif
