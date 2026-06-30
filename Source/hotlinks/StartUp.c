/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * StartUp.c - LVO trap and lifecycle for hotlinks.library
 *
 * FuncTab order matches Soft-Logik 1992 LVO offsets (append-only).
 */

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/alib.h>

#include "private/hl_bases.h"
#include <proto/utility.h>

#include <libraries/hotlinks.h>
#include <libraries/hotlinksbase.h>
#include "private/hl_build.h"
#include "private/hl_debug.h"
#include "private/hl_internal.h"
#include "private/hl_token.h"

extern ULONG L_OpenLibs(void);
extern void L_CloseLibs(void);

extern struct Resident ROMTag;
extern const char HL_LibName[];
extern const char HL_LibID[];
extern struct MyDataInit DataTab;

struct HotLinksBase *HotLinksBase = NULL;

struct ExecBase        *SysBase = NULL;
struct DosLibrary      *DOSBase = NULL;
struct Library         *UtilityBase = NULL;
struct IntuitionBase   *IntuitionBase = NULL;
struct GfxBase         *GfxBase = NULL;
struct Library         *GadToolsBase = NULL;

int __ASM__ __SAVE_DS__ GetPub(__REG__(a0, struct PubBlock *),
    __REG__(a1, int (*)()), __REG__(a6, struct HotLinksBase *));
int __ASM__ __SAVE_DS__ PutPub(__REG__(a0, struct PubBlock *),
    __REG__(a1, int (*)()), __REG__(a6, struct HotLinksBase *));
int __ASM__ __SAVE_DS__ PubInfo(__REG__(a0, struct PubBlock *),
    __REG__(a6, struct HotLinksBase *));
int __ASM__ __SAVE_DS__ HLSysInfo(__REG__(d0, ULONG), __REG__(d1, int *),
    __REG__(a6, struct HotLinksBase *));
ULONG __ASM__ __SAVE_DS__ HLRegister(__REG__(d0, int),
    __REG__(a0, struct MsgPort *), __REG__(a2, struct Screen *),
    __REG__(a6, struct HotLinksBase *));
int __ASM__ __SAVE_DS__ UnRegister(__REG__(d0, ULONG),
    __REG__(a6, struct HotLinksBase *));
struct PubBlock * __ASM__ __SAVE_DS__ AllocPBlock(__REG__(d0, ULONG),
    __REG__(a6, struct HotLinksBase *));
int __ASM__ __SAVE_DS__ FreePBlock(__REG__(a0, struct PubBlock *),
    __REG__(a6, struct HotLinksBase *));
int __ASM__ __SAVE_DS__ SetUser(__REG__(d0, ULONG), __REG__(a0, char *),
    __REG__(a1, char *), __REG__(a6, struct HotLinksBase *));
int __ASM__ __SAVE_DS__ ChgPassword(__REG__(d0, ULONG), __REG__(a0, char *),
    __REG__(a1, char *), __REG__(a2, char *), __REG__(a6, struct HotLinksBase *));
int __ASM__ __SAVE_DS__ FirstPub(__REG__(a0, struct PubBlock *),
    __REG__(a6, struct HotLinksBase *));
int __ASM__ __SAVE_DS__ NextPub(__REG__(a0, struct PubBlock *),
    __REG__(a6, struct HotLinksBase *));
int __ASM__ __SAVE_DS__ RemovePub(__REG__(a0, struct PubBlock *),
    __REG__(a6, struct HotLinksBase *));
int __ASM__ __SAVE_DS__ Notify(__REG__(a0, struct PubBlock *), __REG__(d0, int),
    __REG__(d1, int), __REG__(a1, void *), __REG__(a6, struct HotLinksBase *));
int __ASM__ __SAVE_DS__ PubStatus(__REG__(a0, struct PubBlock *),
    __REG__(a6, struct HotLinksBase *));
int __ASM__ __SAVE_DS__ GetInfo(__REG__(a0, struct PubBlock *),
    __REG__(a6, struct HotLinksBase *));
int __ASM__ __SAVE_DS__ SetInfo(__REG__(a0, struct PubBlock *),
    __REG__(a6, struct HotLinksBase *));
int __ASM__ __SAVE_DS__ LockPub(__REG__(a0, struct PubBlock *), __REG__(d0, int),
    __REG__(a6, struct HotLinksBase *));
int __ASM__ __SAVE_DS__ OpenPub(__REG__(a0, struct PubBlock *), __REG__(d0, int),
    __REG__(a6, struct HotLinksBase *));
int __ASM__ __SAVE_DS__ ReadPub(__REG__(a0, struct PubBlock *),
    __REG__(a1, char *), __REG__(d0, int), __REG__(a6, struct HotLinksBase *));
int __ASM__ __SAVE_DS__ WritePub(__REG__(a0, struct PubBlock *),
    __REG__(a1, char *), __REG__(d0, int), __REG__(a6, struct HotLinksBase *));
int __ASM__ __SAVE_DS__ SeekPub(__REG__(a0, struct PubBlock *), __REG__(d0, int),
    __REG__(d1, int), __REG__(a6, struct HotLinksBase *));
int __ASM__ __SAVE_DS__ ClosePub(__REG__(a0, struct PubBlock *),
    __REG__(a6, struct HotLinksBase *));
int __ASM__ __SAVE_DS__ Publish(__REG__(a0, struct PubBlock *),
    __REG__(a6, struct HotLinksBase *));
int __ASM__ __SAVE_DS__ Subscribe(__REG__(a0, struct PubBlock *),
    __REG__(a6, struct HotLinksBase *));
int __ASM__ __SAVE_DS__ NewPassword(__REG__(d0, ULONG),
    __REG__(a6, struct HotLinksBase *));
int __ASM__ __SAVE_DS__ UnSubscribe(__REG__(a0, struct PubBlock *),
    __REG__(a6, struct HotLinksBase *));

LONG __ASM__ LibStart(void);
struct HotLinksBase * __ASM__ __SAVE_DS__ InitLib(
    __REG__(a6, struct ExecBase *sysbase),
    __REG__(a0, BPTR seglist),
    __REG__(d0, struct HotLinksBase *base));
struct HotLinksBase * __ASM__ __SAVE_DS__ OpenLib(
    __REG__(a6, struct HotLinksBase *base));
BPTR __ASM__ __SAVE_DS__ CloseLib(__REG__(a6, struct HotLinksBase *base));
BPTR __ASM__ __SAVE_DS__ ExpungeLib(__REG__(a6, struct HotLinksBase *base));
ULONG __ASM__ ExtFuncLib(void);

APTR FuncTab[];

struct InitTable InitTab = {
    (ULONG)sizeof(struct HLPrivateBase),
    (APTR *)FuncTab,
    (APTR)&DataTab,
    (APTR)InitLib
};

APTR FuncTab[] = {
    (APTR)OpenLib,
    (APTR)CloseLib,
    (APTR)ExpungeLib,
    (APTR)ExtFuncLib,
    (APTR)GetPub,
    (APTR)PutPub,
    (APTR)PubInfo,
    (APTR)HLSysInfo,
    (APTR)HLRegister,
    (APTR)UnRegister,
    (APTR)AllocPBlock,
    (APTR)FreePBlock,
    (APTR)SetUser,
    (APTR)ChgPassword,
    (APTR)FirstPub,
    (APTR)NextPub,
    (APTR)RemovePub,
    (APTR)Notify,
    (APTR)PubStatus,
    (APTR)GetInfo,
    (APTR)SetInfo,
    (APTR)LockPub,
    (APTR)OpenPub,
    (APTR)ReadPub,
    (APTR)WritePub,
    (APTR)SeekPub,
    (APTR)ClosePub,
    (APTR)Publish,
    (APTR)Subscribe,
    (APTR)NewPassword,
    (APTR)UnSubscribe,
    (APTR)((LONG)-1)
};

LONG
__ASM__ LibStart(void)
{
    return -1;
}

struct HotLinksBase *
__ASM__ __SAVE_DS__ InitLib(
    __REG__(a6, struct ExecBase *sysbase),
    __REG__(a0, BPTR seglist),
    __REG__(d0, struct HotLinksBase *base))
{
    struct HLPrivateBase *pb;

    hlDbgPut("InitLib: enter");
    HotLinksBase = base;
    pb = HLGetPrivate(base);

    pb->hb.LibNode.lib_Node.ln_Type = NT_LIBRARY;
    pb->hb.LibNode.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
    pb->hb.LibNode.lib_Version = HL_LIB_VERSION;
    pb->hb.LibNode.lib_Revision = HL_LIB_REVISION;
    pb->hb.LibNode.lib_IdString = (STRPTR)HL_LibID;

    SysBase = sysbase;
    pb->hb.SegList = (ULONG)seglist;
    pb->hb.SysLib = (ULONG)sysbase;
    pb->hb.DosLib = 0;
    pb->hb.ResPort = 0;
    pb->hb.Flags = 0;
    pb->hb.Pad = 0;

    if (L_OpenLibs() != 0) {
        hlDbgPut("InitLib: L_OpenLibs failed");
        return (struct HotLinksBase *)NULL;
    }
    pb->hb.DosLib = (ULONG)DOSBase;

    NewMinList(&pb->hl_Registrations);
    NewMinList(&pb->hl_PBlocks);
    NewMinList(&pb->hl_Notifies);
    pb->hl_Token = NULL;
    pb->hl_NextHandle = 0;

    hlDbgPut("InitLib: success");
    return base;
}

struct HotLinksBase *
__ASM__ __SAVE_DS__ OpenLib(__REG__(a6, struct HotLinksBase *base))
{
    struct HLPrivateBase *pb;

    base->LibNode.lib_OpenCnt++;
    base->LibNode.lib_Flags &= ~LIBF_DELEXP;

    if (base->LibNode.lib_OpenCnt == 1) {
        pb = HLGetPrivate(base);
        if (HLTokenAttach(pb) != NOERROR) {
            base->LibNode.lib_OpenCnt--;
            return (struct HotLinksBase *)NULL;
        }
    }
    return base;
}

BPTR
__ASM__ __SAVE_DS__ CloseLib(__REG__(a6, struct HotLinksBase *base))
{
    BPTR seg;
    struct HLPrivateBase *pb;

    base->LibNode.lib_OpenCnt--;
    if (base->LibNode.lib_OpenCnt == 0) {
        pb = HLGetPrivate(base);
        HLTokenDetach(pb);
        if (base->LibNode.lib_Flags & LIBF_DELEXP) {
            seg = ExpungeLib(base);
            return seg;
        }
    }
    return 0;
}

BPTR
__ASM__ __SAVE_DS__ ExpungeLib(__REG__(a6, struct HotLinksBase *base))
{
    struct HLPrivateBase *pb;
    BPTR seg;

    pb = HLGetPrivate(base);

    if (base->LibNode.lib_OpenCnt != 0) {
        base->LibNode.lib_Flags |= LIBF_DELEXP;
        return 0;
    }

    if (pb->hl_Token != NULL) {
        HLTokenDetach(pb);
    }

    L_CloseLibs();

    seg = (BPTR)pb->hb.SegList;
    Remove(&base->LibNode);
    FreeMem((APTR)((BYTE *)base - base->LibNode.lib_NegSize),
        base->LibNode.lib_NegSize + base->LibNode.lib_PosSize);
    HotLinksBase = NULL;
    return seg;
}

ULONG
__ASM__ ExtFuncLib(void)
{
    return 0;
}
