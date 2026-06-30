/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * hl_lib.c - public LVO entry points (1992 hotlinks.library ABI)
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <string.h>

#include "private/hl_bases.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <proto/intuition.h>

#include <libraries/hotlinks.h>
#include <libraries/hotlinksbase.h>
#include "private/hl_build.h"
#include "private/hl_debug.h"
#include "private/hl_internal.h"
#include "private/hl_pass.h"
#include "private/hl_req.h"

static struct HLCatalogEntry *
HLNextAccessibleEdition(struct HLPrivateBase *pb, struct HLPBCtx *ctx,
    struct HLCatalogEntry *start)
{
    struct HLCatalogEntry *ce;
    struct MinNode *mn;
    struct HLToken *tok;
    BOOL pastStart;

    pastStart = (start == NULL);

    tok = HLTOK(pb);
    if (tok == NULL) {
        return NULL;
    }

    for (mn = tok->t_Catalog.mlh_Head;
         mn != (struct MinNode *)&tok->t_Catalog.mlh_Tail;
         mn = mn->mln_Succ)
    {
        ce = (struct HLCatalogEntry *)((BYTE *)mn - (BYTE *)&((struct HLCatalogEntry *)0)->hn);
        if (!pastStart) {
            if (ce == start) {
                pastStart = TRUE;
            }
            continue;
        }
        if (HLCheckAccess(pb, &ce->rec, 0) == NOERROR) {
            (void)ctx;
            return ce;
        }
    }
    return NULL;
}

static int
HLHasWriteLock(struct HLPrivateBase *pb, struct HLCatalogEntry *entry,
    ULONG regHandle)
{
    struct HLLockNode *lk;
    struct MinNode *mn;
    struct HLToken *tok;

    tok = HLTOK(pb);
    if (tok == NULL) {
        return 0;
    }

    for (mn = tok->t_Locks.mlh_Head;
         mn != (struct MinNode *)&tok->t_Locks.mlh_Tail;
         mn = mn->mln_Succ)
    {
        lk = (struct HLLockNode *)((BYTE *)mn - (BYTE *)&((struct HLLockNode *)0)->hn);
        if (lk->entry == entry && lk->lockType == LOCK_WRITE &&
            lk->regHandle != regHandle)
        {
            return 1;
        }
    }
    return 0;
}

int
__ASM__ __SAVE_DS__ GetPub(
    __REG__(a0, struct PubBlock *pblock),
    __REG__(a1, int (*filterproc)(void)),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPrivateBase *pb;
    struct HLPBCtx *ctx;
    int err;

    pb = HLGetPrivate(base);
    ctx = HLPBCtxFromBlock(pblock);
    if (ctx == NULL) {
        return INVPARAM;
    }

    err = HLReqEnsureLogin(pb, ctx->reg);
    if (err != NOERROR) {
        return err;
    }

    return HLReqGetPub(pb, pblock, ctx,
        (int (*)(struct PubBlock *))filterproc);
}

int
__ASM__ __SAVE_DS__ PutPub(
    __REG__(a0, struct PubBlock *pblock),
    __REG__(a1, int (*filterproc)(void)),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPrivateBase *pb;
    struct HLPBCtx *ctx;
    int err;

    (void)filterproc;
    pb = HLGetPrivate(base);
    ctx = HLPBCtxFromBlock(pblock);
    if (ctx == NULL) {
        return INVPARAM;
    }

    err = HLReqEnsureLogin(pb, ctx->reg);
    if (err != NOERROR) {
        return err;
    }

    return HLReqPutPub(pb, pblock);
}

int
__ASM__ __SAVE_DS__ PubInfo(
    __REG__(a0, struct PubBlock *pblock),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPrivateBase *pb;
    struct HLPBCtx *ctx;
    struct HLCatalogEntry *ce;
    int err;

    pb = HLGetPrivate(base);
    ctx = HLPBCtxFromBlock(pblock);
    if (ctx == NULL) {
        return INVPARAM;
    }

    ce = ctx->entry;
    if (ce == NULL) {
        ce = HLFindCatalogEntry(pb, pblock->PRec.ID[0], pblock->PRec.ID[1], 0);
    }
    if (ce == NULL) {
        return INVPARAM;
    }
    if (HLCheckAccess(pb, &ce->rec, 0) != NOERROR) {
        return NOPRIV;
    }
    HLCopyPubRecord(&pblock->PRec, &ce->rec);

    err = HLReqEnsureLogin(pb, ctx->reg);
    if (err != NOERROR) {
        return err;
    }
    if (HLCheckAccess(pb, &pblock->PRec, 1) != NOERROR) {
        return NOPRIV;
    }

    err = HLReqPubInfo(pb, pblock, ce);
    if (err != NOERROR) {
        return err;
    }

    HLCopyPubRecord(&ce->rec, &pblock->PRec);
    HLSaveCatalog(pb);
    ctx->entry = ce;
    return NOERROR;
}

int
__ASM__ __SAVE_DS__ HLSysInfo(
    __REG__(d0, ULONG handle),
    __REG__(d1, int *array),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPrivateBase *pb;
    struct HLRegistration *reg;
    struct HLToken *tok;
    struct MinNode *mn;
    ULONG count;
    ULONG regcount;
    struct HLCatalogEntry *ce;

    if (array == NULL) {
        return INVPARAM;
    }

    pb = HLGetPrivate(base);
    reg = HLFindRegistration(pb, handle);
    if (reg == NULL) {
        return INVPARAM;
    }

    tok = HLTOK(pb);
    count = 0;
    regcount = 0;
    if (tok != NULL) {
        for (mn = tok->t_Catalog.mlh_Head;
             mn != (struct MinNode *)&tok->t_Catalog.mlh_Tail;
             mn = mn->mln_Succ)
        {
            ce = (struct HLCatalogEntry *)((BYTE *)mn -
                (BYTE *)&((struct HLCatalogEntry *)0)->hn);
            if (HLCheckAccess(pb, &ce->rec, 0) == NOERROR) {
                count++;
            }
        }
    }
    for (mn = pb->hl_Registrations.mlh_Head;
         mn != (struct MinNode *)&pb->hl_Registrations.mlh_Tail;
         mn = mn->mln_Succ)
    {
        regcount++;
    }

    array[0] = (int)HL_LIB_VERSION;
    array[1] = (int)count;
    array[2] = (int)regcount;
    array[3] = tok != NULL && tok->t_UserValid;
    return NOERROR;
}

ULONG
__ASM__ __SAVE_DS__ HLRegister(
    __REG__(d0, int id),
    __REG__(a0, struct MsgPort *msgport),
    __REG__(a2, struct Screen *screen),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPrivateBase *pb;
    struct HLRegistration *reg;

    pb = HLGetPrivate(base);
    reg = (struct HLRegistration *)AllocMem(sizeof(struct HLRegistration), MEMF_CLEAR);
    if (reg == NULL) {
        return (ULONG)NOMEMORY;
    }

    pb->hl_NextHandle++;
    reg->handle = pb->hl_NextHandle;
    reg->creatorId = id;
    reg->notifyPort = msgport;
    reg->screen = screen;

    AddTail((struct List *)&pb->hl_Registrations, (struct Node *)&reg->hn);
    return reg->handle;
}

int
__ASM__ __SAVE_DS__ UnRegister(
    __REG__(d0, ULONG handle),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPrivateBase *pb;
    struct HLRegistration *reg;

    pb = HLGetPrivate(base);
    reg = HLFindRegistration(pb, handle);
    if (reg == NULL) {
        return INVPARAM;
    }
    Remove((struct Node *)&reg->hn);
    FreeMem(reg, sizeof(struct HLRegistration));
    return NOERROR;
}

struct PubBlock *
__ASM__ __SAVE_DS__ AllocPBlock(
    __REG__(d0, ULONG handle),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPrivateBase *pb;
    struct HLRegistration *reg;
    struct PubBlock *pblock;
    struct HLPBCtx *ctx;
    struct HLToken *tok;
    int err;

    pb = HLGetPrivate(base);
    reg = HLFindRegistration(pb, handle);
    if (reg == NULL) {
        return (struct PubBlock *)INVPARAM;
    }

    tok = HLTOK(pb);
    if (tok != NULL && !tok->t_UserValid) {
        err = HLReqLoginUser(pb, reg, NULL);
        if (err != NOERROR) {
            return (struct PubBlock *)err;
        }
    }

    pblock = (struct PubBlock *)AllocMem(sizeof(struct PubBlock), MEMF_CLEAR);
    if (pblock == NULL) {
        return (struct PubBlock *)NOMEMORY;
    }

    ctx = (struct HLPBCtx *)AllocMem(sizeof(struct HLPBCtx), MEMF_CLEAR);
    if (ctx == NULL) {
        FreeMem(pblock, sizeof(struct PubBlock));
        return (struct PubBlock *)NOMEMORY;
    }

    ctx->magic = HL_PB_MAGIC;
    ctx->regHandle = handle;
    ctx->reg = reg;
    pblock->buffer = (char *)ctx;
    pblock->screen = reg->screen;
    pblock->usermp = reg->notifyPort;

    AddTail((struct List *)&pb->hl_PBlocks, (struct Node *)&ctx->hn);

    return pblock;
}

int
__ASM__ __SAVE_DS__ FreePBlock(
    __REG__(a0, struct PubBlock *pblock),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPBCtx *ctx;

    (void)base;
    ctx = HLPBCtxFromBlock(pblock);
    if (ctx == NULL) {
        return INVPARAM;
    }

    if (ctx->file != 0) {
        Close(ctx->file);
        ctx->file = 0;
    }
    Remove((struct Node *)&ctx->hn);
    FreeMem(ctx, sizeof(struct HLPBCtx));
    pblock->buffer = NULL;
    FreeMem(pblock, sizeof(struct PubBlock));
    return NOERROR;
}

int
__ASM__ __SAVE_DS__ SetUser(
    __REG__(d0, ULONG handle),
    __REG__(a0, char *name),
    __REG__(a1, char *password),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPrivateBase *pb;
    struct HLToken *tok;
    struct HLRegistration *reg;
    int err;

    pb = HLGetPrivate(base);
    tok = HLTOK(pb);
    if (tok == NULL) {
        return INVPARAM;
    }

    reg = HLFindRegistration(pb, handle);
    if (reg == NULL) {
        return INVPARAM;
    }

    if (name == NULL && password == NULL) {
        tok->t_UserValid = FALSE;
        tok->t_UserName[0] = 0;
        tok->t_UserGroup[0] = 0;
        tok->t_LoginRequired = TRUE;
        return NOERROR;
    }

    if (name == NULL) {
        return INVPARAM;
    }

    if (password == NULL) {
        return HLReqLoginUser(pb, reg, name);
    }

    if (HLPassEnsureUsers(pb) != NOERROR) {
        return IOERROR;
    }

    err = HLPassVerify(pb, name, password);
    if (err != NOERROR) {
        return INVPARAM;
    }

    Strncpy(tok->t_UserName, name, 31);
    tok->t_UserName[31] = 0;
    if (Stricmp(name, HL_SUPERUSER_NAME) == 0) {
        Strncpy(tok->t_UserGroup, "admin", 31);
    } else {
        Strncpy(tok->t_UserGroup, "users", 31);
    }
    tok->t_UserGroup[31] = 0;
    tok->t_UserValid = TRUE;
    tok->t_LoginRequired = FALSE;
    return NOERROR;
}

int
__ASM__ __SAVE_DS__ ChgPassword(
    __REG__(d0, ULONG handle),
    __REG__(a0, char *name),
    __REG__(a1, char *oldpwd),
    __REG__(a2, char *newpwd),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPrivateBase *pb;

    pb = HLGetPrivate(base);
    return HLPassChange(pb, handle, name, oldpwd, newpwd);
}

int
__ASM__ __SAVE_DS__ FirstPub(
    __REG__(a0, struct PubBlock *pblock),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPrivateBase *pb;
    struct HLPBCtx *ctx;
    struct HLCatalogEntry *ce;

    pb = HLGetPrivate(base);
    ctx = HLPBCtxFromBlock(pblock);
    if (ctx == NULL) {
        return INVPARAM;
    }

    ce = HLNextAccessibleEdition(pb, ctx, NULL);
    if (ce == NULL) {
        return NOMOREBLOCKS;
    }
    HLCopyPubRecord(&pblock->PRec, &ce->rec);
    ctx->entry = ce;
    ctx->iterNext = ce;
    return NOERROR;
}

int
__ASM__ __SAVE_DS__ NextPub(
    __REG__(a0, struct PubBlock *pblock),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPrivateBase *pb;
    struct HLPBCtx *ctx;
    struct HLCatalogEntry *ce;

    pb = HLGetPrivate(base);
    ctx = HLPBCtxFromBlock(pblock);
    if (ctx == NULL || ctx->iterNext == NULL) {
        return INVPARAM;
    }

    ce = HLNextAccessibleEdition(pb, ctx, ctx->iterNext);
    if (ce == NULL) {
        return NOMOREBLOCKS;
    }
    HLCopyPubRecord(&pblock->PRec, &ce->rec);
    ctx->entry = ce;
    ctx->iterNext = ce;
    return NOERROR;
}

int
__ASM__ __SAVE_DS__ RemovePub(
    __REG__(a0, struct PubBlock *pblock),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPrivateBase *pb;
    struct HLPBCtx *ctx;
    struct HLCatalogEntry *ce;

    pb = HLGetPrivate(base);
    ctx = HLPBCtxFromBlock(pblock);
    if (ctx == NULL) {
        return INVPARAM;
    }

    ce = HLFindCatalogEntry(pb, pblock->PRec.ID[0], pblock->PRec.ID[1], 0);
    if (ce == NULL) {
        return INVPARAM;
    }
    if (HLCheckAccess(pb, &ce->rec, 1) != NOERROR) {
        return NOPRIV;
    }
    if (ce->rec.Version != pblock->PRec.Version) {
        return INVPARAM;
    }

    DeleteFile(ce->dataPath);
    Remove((struct Node *)&ce->hn);
    FreeMem(ce, sizeof(struct HLCatalogEntry));
    HLSaveCatalog(pb);
    ctx->entry = NULL;
    return NOERROR;
}

int
__ASM__ __SAVE_DS__ Notify(
    __REG__(a0, struct PubBlock *pblock),
    __REG__(d0, int flag),
    __REG__(d1, int class),
    __REG__(a1, void *userdata),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPrivateBase *pb;
    struct HLPBCtx *ctx;
    struct HLNotifyNode *nn;

    (void)class;
    pb = HLGetPrivate(base);
    ctx = HLPBCtxFromBlock(pblock);
    if (ctx == NULL || ctx->entry == NULL) {
        return INVPARAM;
    }

    if (flag == NOINFORM) {
        return NOERROR;
    }

    nn = (struct HLNotifyNode *)AllocMem(sizeof(struct HLNotifyNode), MEMF_CLEAR);
    if (nn == NULL) {
        return NOMEMORY;
    }
    nn->entry = ctx->entry;
    nn->port = ctx->reg->notifyPort;
    nn->regHandle = ctx->regHandle;
    nn->mode = flag;
    nn->userData = userdata;
    AddTail((struct List *)&pb->hl_Notifies, (struct Node *)&nn->hn);
    return NOERROR;
}

int
__ASM__ __SAVE_DS__ PubStatus(
    __REG__(a0, struct PubBlock *pblock),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPrivateBase *pb;
    struct HLPBCtx *ctx;
    struct HLCatalogEntry *ce;

    pb = HLGetPrivate(base);
    ctx = HLPBCtxFromBlock(pblock);
    if (ctx == NULL) {
        return INVPARAM;
    }

    ce = HLFindCatalogEntry(pb, pblock->PRec.ID[0], pblock->PRec.ID[1],
        pblock->PRec.Version);
    if (ce == NULL) {
        return INVPARAM;
    }
    if (HLCheckAccess(pb, &ce->rec, 0) != NOERROR) {
        return NOPRIV;
    }
    if (ctx->entry != NULL && ctx->entry->statusStamp != ce->statusStamp) {
        return CHANGED;
    }
    return NOERROR;
}

int
__ASM__ __SAVE_DS__ GetInfo(
    __REG__(a0, struct PubBlock *pblock),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPrivateBase *pb;
    struct HLCatalogEntry *ce;

    pb = HLGetPrivate(base);
    if (HLPBCtxFromBlock(pblock) == NULL) {
        return INVPARAM;
    }

    ce = HLFindCatalogEntry(pb, pblock->PRec.ID[0], pblock->PRec.ID[1], 0);
    if (ce == NULL) {
        return INVPARAM;
    }
    if (HLCheckAccess(pb, &ce->rec, 0) != NOERROR) {
        return NOPRIV;
    }
    HLCopyPubRecord(&pblock->PRec, &ce->rec);
    return NOERROR;
}

int
__ASM__ __SAVE_DS__ SetInfo(
    __REG__(a0, struct PubBlock *pblock),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPrivateBase *pb;
    struct HLCatalogEntry *ce;

    pb = HLGetPrivate(base);
    if (HLPBCtxFromBlock(pblock) == NULL) {
        return INVPARAM;
    }

    ce = HLFindCatalogEntry(pb, pblock->PRec.ID[0], pblock->PRec.ID[1], 0);
    if (ce == NULL) {
        return INVPARAM;
    }
    if (HLCheckAccess(pb, &pblock->PRec, 1) != NOERROR) {
        return NOPRIV;
    }
    HLCopyPubRecord(&ce->rec, &pblock->PRec);
    HLSaveCatalog(pb);
    return NOERROR;
}

int
__ASM__ __SAVE_DS__ LockPub(
    __REG__(a0, struct PubBlock *pblock),
    __REG__(d0, int flags),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPrivateBase *pb;
    struct HLPBCtx *ctx;
    struct HLLockNode *lk;
    struct HLCatalogEntry *ce;
    struct HLToken *tok;

    pb = HLGetPrivate(base);
    tok = HLTOK(pb);
    ctx = HLPBCtxFromBlock(pblock);
    if (ctx == NULL || tok == NULL) {
        return INVPARAM;
    }

    ce = ctx->entry;
    if (ce == NULL) {
        ce = HLFindCatalogEntry(pb, pblock->PRec.ID[0], pblock->PRec.ID[1], 0);
    }
    if (ce == NULL) {
        return INVPARAM;
    }

    if (flags == LOCK_RELEASE) {
        pblock->LFlag = LOCK_RELEASE;
        return NOERROR;
    }

    if (flags == LOCK_WRITE && HLHasWriteLock(pb, ce, ctx->regHandle)) {
        return INUSE;
    }

    lk = (struct HLLockNode *)AllocMem(sizeof(struct HLLockNode), MEMF_CLEAR);
    if (lk == NULL) {
        return NOMEMORY;
    }
    lk->entry = ce;
    lk->regHandle = ctx->regHandle;
    lk->lockType = flags;
    AddTail((struct List *)&tok->t_Locks, (struct Node *)&lk->hn);
    pblock->LFlag = flags;
    return NOERROR;
}

int
__ASM__ __SAVE_DS__ OpenPub(
    __REG__(a0, struct PubBlock *pblock),
    __REG__(d0, int flags),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPrivateBase *pb;
    struct HLPBCtx *ctx;
    struct HLCatalogEntry *ce;
    char path[512];
    BPTR fh;
    LONG mode;
    struct HLToken *tok;

    pb = HLGetPrivate(base);
    tok = HLTOK(pb);
    ctx = HLPBCtxFromBlock(pblock);
    if (ctx == NULL || tok == NULL) {
        return INVPARAM;
    }

    if (pblock->PRec.ID[0] == 0 && pblock->PRec.ID[1] == 0) {
        ce = (struct HLCatalogEntry *)AllocMem(sizeof(struct HLCatalogEntry), MEMF_CLEAR);
        if (ce == NULL) {
            return NOMEMORY;
        }
        HLCopyPubRecord(&ce->rec, &pblock->PRec);
        tok->t_NextSerial++;
        ce->rec.ID[0] = (ULONG)ctx->reg->creatorId;
        ce->rec.ID[1] = tok->t_NextSerial;
        ce->rec.Version = 1;
        HLStampDates(&ce->rec, TRUE);
        if (tok->t_UserValid) {
            Strncpy(ce->rec.Owner, tok->t_UserName, 31);
            Strncpy(ce->rec.Group, tok->t_UserGroup, 31);
        } else {
            Strncpy(ce->rec.Owner, "public", sizeof(ce->rec.Owner) - 1);
            Strncpy(ce->rec.Group, "users", sizeof(ce->rec.Group) - 1);
        }
        if (ce->rec.Access == 0) {
            ce->rec.Access = ACC_DEFAULT | ACC_AREAD | ACC_AWRITE;
        }
        HLBuildDataPath(pb, &ce->rec, ce->dataPath, (LONG)sizeof(ce->dataPath));
        AddTail((struct List *)&tok->t_Catalog, (struct Node *)&ce->hn);
        ctx->entry = ce;
        HLCopyPubRecord(&pblock->PRec, &ce->rec);
    } else {
        ce = HLFindCatalogEntry(pb, pblock->PRec.ID[0], pblock->PRec.ID[1], 0);
        if (ce == NULL) {
            return INVPARAM;
        }
        if (HLCheckAccess(pb, &ce->rec, (flags == OPEN_WRITE)) != NOERROR) {
            return NOPRIV;
        }
        ctx->entry = ce;
    }

    if (flags == OPEN_WRITE) {
        if (HLHasWriteLock(pb, ce, ctx->regHandle)) {
            return INUSE;
        }
        ce->rec.Version++;
        HLStampDates(&ce->rec, FALSE);
        HLBuildDataPath(pb, &ce->rec, ce->dataPath, (LONG)sizeof(ce->dataPath));
        mode = MODE_NEWFILE;
        pblock->State = STATE_OPENEDW;
    } else {
        mode = MODE_OLDFILE;
        pblock->State = STATE_OPENEDR;
    }

    Strncpy(path, ce->dataPath, sizeof(path) - 1);
    fh = Open(path, mode);
    if (fh == 0) {
        return IOERROR;
    }

    ctx->file = fh;
    ctx->fileSize = 0;
    pblock->OFlag = flags;
    pblock->FOffset = 0;
    pblock->curpos = 0;
    HLCopyPubRecord(&pblock->PRec, &ce->rec);
    return NOERROR;
}

int
__ASM__ __SAVE_DS__ ReadPub(
    __REG__(a0, struct PubBlock *pblock),
    __REG__(a1, char *buffer),
    __REG__(d0, int len),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPBCtx *ctx;
    LONG got;

    (void)base;
    ctx = HLPBCtxFromBlock(pblock);
    if (ctx == NULL || ctx->file == 0 || buffer == NULL || len <= 0) {
        return INVPARAM;
    }

    got = Read(ctx->file, buffer, len);
    if (got < 0) {
        return IOERROR;
    }
    pblock->FOffset += (unsigned int)got;
    pblock->curpos = (int)pblock->FOffset;
    return (int)got;
}

int
__ASM__ __SAVE_DS__ WritePub(
    __REG__(a0, struct PubBlock *pblock),
    __REG__(a1, char *buffer),
    __REG__(d0, int len),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPBCtx *ctx;
    LONG wrote;

    (void)base;
    ctx = HLPBCtxFromBlock(pblock);
    if (ctx == NULL || ctx->file == 0 || buffer == NULL || len <= 0) {
        return INVPARAM;
    }

    wrote = Write(ctx->file, buffer, len);
    if (wrote != len) {
        return IOERROR;
    }
    pblock->FOffset += (unsigned int)wrote;
    pblock->curpos = (int)pblock->FOffset;
    ctx->fileSize = pblock->curpos;
    ctx->dirtyMeta = TRUE;
    return NOERROR;
}

int
__ASM__ __SAVE_DS__ SeekPub(
    __REG__(a0, struct PubBlock *pblock),
    __REG__(d0, int offset),
    __REG__(d1, int mode),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPBCtx *ctx;
    LONG pos;

    (void)base;
    ctx = HLPBCtxFromBlock(pblock);
    if (ctx == NULL || ctx->file == 0) {
        return INVPARAM;
    }

    if (mode == SEEK_BEGINNING) {
        pos = Seek(ctx->file, (LONG)offset, OFFSET_BEGINNING);
    } else if (mode == SEEK_END) {
        pos = Seek(ctx->file, (LONG)offset, OFFSET_END);
    } else {
        pos = Seek(ctx->file, (LONG)offset, OFFSET_CURRENT);
    }

    if (pos < 0) {
        return IOERROR;
    }
    pblock->FOffset = (unsigned int)pos;
    pblock->curpos = (int)pos;
    return (int)pos;
}

int
__ASM__ __SAVE_DS__ ClosePub(
    __REG__(a0, struct PubBlock *pblock),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPrivateBase *pb;
    struct HLPBCtx *ctx;

    pb = HLGetPrivate(base);
    ctx = HLPBCtxFromBlock(pblock);
    if (ctx == NULL || ctx->file == 0) {
        return INVPARAM;
    }

    Close(ctx->file);
    ctx->file = 0;
    pblock->State = 0;
    pblock->OFlag = 0;

    if (ctx->dirtyMeta && ctx->entry != NULL) {
        HLCopyPubRecord(&ctx->entry->rec, &pblock->PRec);
        HLSaveCatalog(pb);
        HLNotifyEditionChange(pb, ctx->entry);
        ctx->dirtyMeta = FALSE;
    }
    return NOERROR;
}

int
__ASM__ __SAVE_DS__ Publish(
    __REG__(a0, struct PubBlock *pblock),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPrivateBase *pb;
    struct HLPBCtx *ctx;

    pb = HLGetPrivate(base);
    ctx = HLPBCtxFromBlock(pblock);
    if (ctx == NULL || ctx->entry == NULL) {
        return INVPARAM;
    }
    HLCopyPubRecord(&ctx->entry->rec, &pblock->PRec);
    HLSaveCatalog(pb);
    HLNotifyEditionChange(pb, ctx->entry);
    return NOERROR;
}

int
__ASM__ __SAVE_DS__ Subscribe(
    __REG__(a0, struct PubBlock *pblock),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPBCtx *ctx;

    (void)base;
    ctx = HLPBCtxFromBlock(pblock);
    if (ctx == NULL) {
        return INVPARAM;
    }
    ctx->subscribed = TRUE;
    return NOERROR;
}

int
__ASM__ __SAVE_DS__ NewPassword(
    __REG__(d0, ULONG handle),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPrivateBase *pb;
    struct HLRegistration *reg;

    pb = HLGetPrivate(base);
    reg = HLFindRegistration(pb, handle);
    if (reg == NULL) {
        return INVPARAM;
    }
    return HLReqNewPassword(pb, reg);
}

int
__ASM__ __SAVE_DS__ UnSubscribe(
    __REG__(a0, struct PubBlock *pblock),
    __REG__(a6, struct HotLinksBase *base))
{
    struct HLPBCtx *ctx;

    (void)base;
    ctx = HLPBCtxFromBlock(pblock);
    if (ctx == NULL) {
        return INVPARAM;
    }
    ctx->subscribed = FALSE;
    return NOERROR;
}
