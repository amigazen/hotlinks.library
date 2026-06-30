/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * hl_internal.h - hotlinks.library private state (not installed to SDK)
 *
 * Public HotLinksBase layout matches the 1992 SDK; fields beyond ResPort live
 * in HLPrivateBase and are invisible to client binaries.
 */

#ifndef HOTLINKS_PRIVATE_HL_INTERNAL_H
#define HOTLINKS_PRIVATE_HL_INTERNAL_H

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/lists.h>
#include <exec/semaphores.h>
#include <dos/dos.h>

#include "compiler.h"
#include <libraries/hotlinks.h>
#include <libraries/hotlinksbase.h>

#include "hl_build.h"
#include "hl_token.h"

#define HL_PB_MAGIC        0x484C5042UL
#define HL_INDEX_MAGIC     0x484C4958UL
#define HL_INDEX_VERSION   1

#define HL_ROOT_ASSIGN     "HotLinks:"
#define HL_ROOT_FALLBACK   "SYS:HotLinks"
#define HL_INDEX_NAME      "Index"
#define HL_EDITIONS_DIR    "Editions"
#define HL_USERS_NAME      "Users"

struct HLPrivateBase {
    struct HotLinksBase hb;
    struct HLToken *hl_Token;
    struct MinList hl_Registrations;
    struct MinList hl_PBlocks;
    struct MinList hl_Notifies;
    ULONG hl_NextHandle;
};

#define HLTOK(pb)  ((pb)->hl_Token)

struct HLRegistration {
    struct MinNode hn;
    ULONG handle;
    LONG creatorId;
    struct MsgPort *notifyPort;
    struct Screen *screen;
};

struct HLCatalogEntry {
    struct MinNode hn;
    struct PubRecord rec;
    char dataPath[256];
    ULONG statusStamp;
};

struct HLPBCtx {
    struct MinNode hn;
    ULONG magic;
    ULONG regHandle;
    struct HLRegistration *reg;
    struct HLCatalogEntry *entry;
    struct HLCatalogEntry *iterNext;
    BPTR file;
    LONG fileSize;
    BOOL dirtyMeta;
    BOOL subscribed;
};

struct HLNotifyNode {
    struct MinNode hn;
    struct HLCatalogEntry *entry;
    struct MsgPort *port;
    ULONG regHandle;
    int mode;
    void *userData;
    struct HLMsg msg;
};

struct HLLockNode {
    struct MinNode hn;
    struct HLCatalogEntry *entry;
    ULONG regHandle;
    int lockType;
};

#define HLBASE(b)  ((struct HLPrivateBase *)(b))

struct HLPrivateBase *HLGetPrivate(struct HotLinksBase *base);

struct HLRegistration *HLFindRegistration(struct HLPrivateBase *pb, ULONG handle);
struct HLPBCtx *HLPBCtxFromBlock(struct PubBlock *pblock);
struct HLCatalogEntry *HLFindCatalogEntry(struct HLPrivateBase *pb,
    ULONG id0, ULONG id1, ULONG version);

int HLEnsureRoot(struct HLPrivateBase *pb);
int HLEnsureRootDirs(struct HLPrivateBase *pb);
int HLLoadCatalog(struct HLPrivateBase *pb);
int HLSaveCatalog(struct HLPrivateBase *pb);
void HLStampDates(struct PubRecord *rec, BOOL create);
int HLCheckAccess(struct HLPrivateBase *pb, struct PubRecord *rec, int write);
void HLCopyPubRecord(struct PubRecord *dst, struct PubRecord *src);
int HLBuildDataPath(struct HLPrivateBase *pb, struct PubRecord *rec,
    char *buf, LONG buflen);
int HLNotifyEditionChange(struct HLPrivateBase *pb, struct HLCatalogEntry *entry);

#endif /* HOTLINKS_PRIVATE_HL_INTERNAL_H */
