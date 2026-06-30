/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * LibInit.c - ROMTag, DataTab, L_OpenLibs for hotlinks.library
 */

#define __USE_SYSBASE

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <graphics/gfxbase.h>

#include "private/hl_build.h"
#include "private/hl_debug.h"
#include "private/compiler.h"
#include "private/hl_bases.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>

#define EXLIBNAME "hotlinks"
#define EXLIBVER  " 2.0 (19.6.2026)"

const char HL_LibName[] = EXLIBNAME ".library";
const char HL_LibID[]   = EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
extern struct Library *UtilityBase;
extern struct IntuitionBase *IntuitionBase;
extern struct GfxBase *GfxBase;
extern struct Library *GadToolsBase;

ULONG __SAVE_DS__
L_OpenLibs(VOID)
{
    hlDbgPut("L_OpenLibs: enter");

    DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 37);
    if (DOSBase == NULL) {
        return 1;
    }

    UtilityBase = OpenLibrary("utility.library", 37);
    if (UtilityBase == NULL) {
        CloseLibrary((struct Library *)DOSBase);
        DOSBase = NULL;
        return 1;
    }

    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37);
    if (IntuitionBase == NULL) {
        CloseLibrary(UtilityBase);
        UtilityBase = NULL;
        CloseLibrary((struct Library *)DOSBase);
        DOSBase = NULL;
        return 1;
    }

    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 37);
    if (GfxBase == NULL) {
        CloseLibrary((struct Library *)IntuitionBase);
        IntuitionBase = NULL;
        CloseLibrary(UtilityBase);
        UtilityBase = NULL;
        CloseLibrary((struct Library *)DOSBase);
        DOSBase = NULL;
        return 1;
    }

    GadToolsBase = OpenLibrary("gadtools.library", 37);
    if (GadToolsBase == NULL) {
        CloseLibrary((struct Library *)GfxBase);
        GfxBase = NULL;
        CloseLibrary((struct Library *)IntuitionBase);
        IntuitionBase = NULL;
        CloseLibrary(UtilityBase);
        UtilityBase = NULL;
        CloseLibrary((struct Library *)DOSBase);
        DOSBase = NULL;
        return 1;
    }

    hlDbgPut("L_OpenLibs: success");
    return 0;
}

VOID __SAVE_DS__
L_CloseLibs(VOID)
{
    if (GadToolsBase != NULL) {
        CloseLibrary(GadToolsBase);
        GadToolsBase = NULL;
    }
    if (GfxBase != NULL) {
        CloseLibrary((struct Library *)GfxBase);
        GfxBase = NULL;
    }
    if (IntuitionBase != NULL) {
        CloseLibrary((struct Library *)IntuitionBase);
        IntuitionBase = NULL;
    }
    if (UtilityBase != NULL) {
        CloseLibrary(UtilityBase);
        UtilityBase = NULL;
    }
    if (DOSBase != NULL) {
        CloseLibrary((struct Library *)DOSBase);
        DOSBase = NULL;
    }
}

extern struct InitTable InitTab;
extern APTR EndResident;

struct Resident ROMTag = {
    RTC_MATCHWORD,
    &ROMTag,
    &EndResident,
    RTF_AUTOINIT,
    HL_LIB_VERSION,
    NT_LIBRARY,
    0,
    (APTR)HL_LibName,
    (APTR)HL_LibID,
    (APTR)&InitTab
};

APTR EndResident;

struct MyDataInit DataTab = {
    0xE000, 8,  NT_LIBRARY,
    0x80,   10, (ULONG)HL_LibName,
    0xE000, 14, LIBF_SUMUSED | LIBF_CHANGED,
    0xE000, 20, HL_LIB_VERSION,
    0xE000, 22, HL_LIB_REVISION,
    0x80,   24, (ULONG)HL_LibID,
    (ULONG)0
};

#ifdef __SASC
void __regargs __chkabort(void) { }
void __regargs _CXBRK(void)     { }
#endif
