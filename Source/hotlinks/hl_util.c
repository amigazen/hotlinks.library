/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * hl_util.c - catalog, paths, access checks, dates
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <string.h>

#include "private/hl_bases.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>

#include <libraries/hotlinks.h>
#include "private/hl_debug.h"
#include "private/hl_internal.h"

struct HLPrivateBase *
HLGetPrivate(struct HotLinksBase *base)
{
    return (struct HLPrivateBase *)base;
}

struct HLRegistration *
HLFindRegistration(struct HLPrivateBase *pb, ULONG handle)
{
    struct HLRegistration *reg;
    struct MinNode *mn;

    if (pb == NULL || handle == 0) {
        return NULL;
    }

    for (mn = pb->hl_Registrations.mlh_Head;
         mn != (struct MinNode *)&pb->hl_Registrations.mlh_Tail;
         mn = mn->mln_Succ)
    {
        reg = (struct HLRegistration *)((BYTE *)mn - (BYTE *)&((struct HLRegistration *)0)->hn);
        if (reg->handle == handle) {
            return reg;
        }
    }
    return NULL;
}

struct HLPBCtx *
HLPBCtxFromBlock(struct PubBlock *pblock)
{
    struct HLPBCtx *ctx;

    if (pblock == NULL || pblock->buffer == NULL) {
        return NULL;
    }
    ctx = (struct HLPBCtx *)pblock->buffer;
    if (ctx->magic != HL_PB_MAGIC) {
        return NULL;
    }
    return ctx;
}

struct HLCatalogEntry *
HLFindCatalogEntry(struct HLPrivateBase *pb, ULONG id0, ULONG id1, ULONG version)
{
    struct HLCatalogEntry *ce;
    struct MinNode *mn;
    struct HLToken *tok;

    tok = HLTOK(pb);
    if (tok == NULL) {
        return NULL;
    }

    for (mn = tok->t_Catalog.mlh_Head;
         mn != (struct MinNode *)&tok->t_Catalog.mlh_Tail;
         mn = mn->mln_Succ)
    {
        ce = (struct HLCatalogEntry *)((BYTE *)mn - (BYTE *)&((struct HLCatalogEntry *)0)->hn);
        if (ce->rec.ID[0] == id0 && ce->rec.ID[1] == id1) {
            if (version == 0 || ce->rec.Version == version) {
                return ce;
            }
        }
    }
    return NULL;
}

void
HLCopyPubRecord(struct PubRecord *dst, struct PubRecord *src)
{
    CopyMem(src, dst, (ULONG)sizeof(struct PubRecord));
}

void
HLStampDates(struct PubRecord *rec, BOOL create)
{
    struct DateStamp ds;
    char *d;
    char *t;

    DateStamp(&ds);
    d = (char *)&rec->MDate;
    t = (char *)&rec->MTime;
    d[2] = (char)((ds.ds_Days % 365) / 30 + 1);
    if (d[2] < 1) {
        d[2] = 1;
    }
    if (d[2] > 12) {
        d[2] = 12;
    }
    d[3] = (char)((ds.ds_Days % 30) + 1);
    t[0] = (char)((ds.ds_Minute / 60) % 24);
    t[1] = (char)(ds.ds_Minute % 60);
    t[2] = 0;

    if (create) {
        rec->CDate = rec->MDate;
        rec->CTime = rec->MTime;
    }
}

int
HLCheckAccess(struct HLPrivateBase *pb, struct PubRecord *rec, int write)
{
    ULONG need;
    BOOL owner;
    BOOL group;
    struct HLToken *tok;

    if (rec == NULL) {
        return INVPARAM;
    }

    tok = HLTOK(pb);
    if (tok == NULL) {
        return INVPARAM;
    }

    if (!tok->t_UserValid) {
        need = write ? ACC_AWRITE : ACC_AREAD;
        if (rec->Access & need) {
            return NOERROR;
        }
        return NOPRIV;
    }

    owner = (Stricmp(tok->t_UserName, rec->Owner) == 0);
    group = (Stricmp(tok->t_UserGroup, rec->Group) == 0);

    if (write) {
        if (owner && (rec->Access & ACC_OWRITE)) {
            return NOERROR;
        }
        if (group && (rec->Access & ACC_GWRITE)) {
            return NOERROR;
        }
        if (rec->Access & ACC_AWRITE) {
            return NOERROR;
        }
    } else {
        if (owner && (rec->Access & ACC_OREAD)) {
            return NOERROR;
        }
        if (group && (rec->Access & ACC_GREAD)) {
            return NOERROR;
        }
        if (rec->Access & ACC_AREAD) {
            return NOERROR;
        }
    }
    return NOPRIV;
}

int
HLEnsureRootDirs(struct HLPrivateBase *pb)
{
    BPTR lock;
    char path[280];
    struct HLToken *tok;

    tok = HLTOK(pb);
    if (tok == NULL || tok->t_RootPath[0] == 0) {
        return INVPARAM;
    }

    Strncpy(path, tok->t_RootPath, sizeof(path) - 1);
    path[sizeof(path) - 1] = 0;
    if (path[strlen(path) - 1] != ':') {
        lock = CreateDir(path);
        if (lock == 0) {
            lock = Lock(path, ACCESS_READ);
            if (lock == 0) {
                return IOERROR;
            }
            UnLock(lock);
        } else {
            UnLock(lock);
        }
        AddPart(path, HL_EDITIONS_DIR, (LONG)sizeof(path));
    } else {
        AddPart(path, HL_EDITIONS_DIR, (LONG)sizeof(path));
    }
    lock = CreateDir(path);
    if (lock == 0) {
        lock = Lock(path, ACCESS_READ);
        if (lock == 0) {
            return IOERROR;
        }
        UnLock(lock);
    } else {
        UnLock(lock);
    }
    return NOERROR;
}

int
HLEnsureRoot(struct HLPrivateBase *pb)
{
    BPTR lock;
    struct HLToken *tok;

    tok = HLTOK(pb);
    if (tok == NULL) {
        return INVPARAM;
    }

    if (tok->t_RootPath[0] == 0) {
        lock = Lock((STRPTR)HL_ROOT_ASSIGN, ACCESS_READ);
        if (lock != 0) {
            UnLock(lock);
            Strncpy(tok->t_RootPath, HL_ROOT_ASSIGN, sizeof(tok->t_RootPath) - 1);
        } else {
            Strncpy(tok->t_RootPath, HL_ROOT_FALLBACK, sizeof(tok->t_RootPath) - 1);
        }
        tok->t_RootPath[sizeof(tok->t_RootPath) - 1] = 0;
    }

    return HLEnsureRootDirs(pb);
}

int
HLBuildDataPath(struct HLPrivateBase *pb, struct PubRecord *rec,
    char *buf, LONG buflen)
{
    char part[64];
    LONG n;
    ULONG v;
    int i;
    struct HLToken *tok;
    static const char hx[] = "0123456789ABCDEF";

    tok = HLTOK(pb);
    if (tok == NULL) {
        return INVPARAM;
    }

    if (HLEnsureRoot(pb) != NOERROR) {
        return IOERROR;
    }

    Strncpy(buf, tok->t_RootPath, buflen - 1);
    if (buf[strlen(buf) - 1] != ':') {
        AddPart(buf, HL_EDITIONS_DIR, buflen);
    } else {
        AddPart(buf, HL_EDITIONS_DIR, buflen);
    }

    n = 0;
    v = rec->ID[0];
    for (i = 7; i >= 0; i--) {
        part[n++] = hx[(v >> (i * 4)) & 0xF];
    }
    part[n++] = '_';
    v = rec->ID[1];
    for (i = 7; i >= 0; i--) {
        part[n++] = hx[(v >> (i * 4)) & 0xF];
    }
    part[n++] = '_';
    part[n++] = 'v';
    v = rec->Version;
    if (v >= 100) {
        part[n++] = (char)('0' + (v / 100) % 10);
    }
    if (v >= 10) {
        part[n++] = (char)('0' + (v / 10) % 10);
    }
    part[n++] = (char)('0' + v % 10);
    part[n++] = '.';
    part[n++] = 'i';
    part[n++] = 'f';
    part[n++] = 'f';
    part[n] = 0;

    AddPart(buf, part, buflen);
    return NOERROR;
}

int
HLLoadCatalog(struct HLPrivateBase *pb)
{
    BPTR fh;
    char path[280];
    ULONG magic;
    ULONG version;
    ULONG count;
    ULONG i;
    struct HLCatalogEntry *ce;
    LONG got;
    ULONG pathLen;
    char *pathBuf;
    struct HLToken *tok;

    tok = HLTOK(pb);
    if (tok == NULL) {
        return INVPARAM;
    }

    if (HLEnsureRoot(pb) != NOERROR) {
        return IOERROR;
    }

    Strncpy(path, tok->t_RootPath, sizeof(path) - 1);
    AddPart(path, HL_INDEX_NAME, (LONG)sizeof(path));

    fh = Open(path, MODE_OLDFILE);
    if (fh == 0) {
        return NOERROR;
    }

    got = Read(fh, (APTR)&magic, 4);
    if (got != 4 || magic != HL_INDEX_MAGIC) {
        Close(fh);
        return IOERROR;
    }
    Read(fh, (APTR)&version, 4);
    Read(fh, (APTR)&count, 4);
    (void)version;

    for (i = 0; i < count; i++) {
        ce = (struct HLCatalogEntry *)AllocMem(sizeof(struct HLCatalogEntry), MEMF_CLEAR);
        if (ce == NULL) {
            Close(fh);
            return NOMEMORY;
        }
        Read(fh, (APTR)&ce->rec, (LONG)sizeof(struct PubRecord));
        Read(fh, (APTR)&pathLen, 4);
        if (pathLen >= sizeof(ce->dataPath)) {
            pathLen = sizeof(ce->dataPath) - 1;
        }
        pathBuf = ce->dataPath;
        Read(fh, pathBuf, (LONG)pathLen);
        pathBuf[pathLen] = 0;
        if (pathLen & 1) {
            char pad;
            Read(fh, &pad, 1);
        }
        AddTail((struct List *)&tok->t_Catalog, (struct Node *)&ce->hn);
    }

    Close(fh);
    return NOERROR;
}

int
HLSaveCatalog(struct HLPrivateBase *pb)
{
    BPTR fh;
    char path[280];
    ULONG magic;
    ULONG version;
    ULONG count;
    struct HLCatalogEntry *ce;
    struct MinNode *mn;
    ULONG pathLen;
    struct HLToken *tok;

    tok = HLTOK(pb);
    if (tok == NULL) {
        return INVPARAM;
    }

    if (HLEnsureRoot(pb) != NOERROR) {
        return IOERROR;
    }

    Strncpy(path, tok->t_RootPath, sizeof(path) - 1);
    AddPart(path, HL_INDEX_NAME, (LONG)sizeof(path));

    fh = Open(path, MODE_NEWFILE);
    if (fh == 0) {
        return IOERROR;
    }

    magic = HL_INDEX_MAGIC;
    version = HL_INDEX_VERSION;
    count = 0;
    for (mn = tok->t_Catalog.mlh_Head;
         mn != (struct MinNode *)&tok->t_Catalog.mlh_Tail;
         mn = mn->mln_Succ)
    {
        count++;
    }

    Write(fh, (APTR)&magic, 4);
    Write(fh, (APTR)&version, 4);
    Write(fh, (APTR)&count, 4);

    for (mn = tok->t_Catalog.mlh_Head;
         mn != (struct MinNode *)&tok->t_Catalog.mlh_Tail;
         mn = mn->mln_Succ)
    {
        ce = (struct HLCatalogEntry *)((BYTE *)mn - (BYTE *)&((struct HLCatalogEntry *)0)->hn);
        Write(fh, (APTR)&ce->rec, (LONG)sizeof(struct PubRecord));
        pathLen = (ULONG)strlen(ce->dataPath);
        Write(fh, (APTR)&pathLen, 4);
        Write(fh, ce->dataPath, (LONG)pathLen);
        if (pathLen & 1) {
            char pad = 0;
            Write(fh, &pad, 1);
        }
    }

    Close(fh);
    return NOERROR;
}

int
HLNotifyEditionChange(struct HLPrivateBase *pb, struct HLCatalogEntry *entry)
{
    struct HLNotifyNode *nn;
    struct MinNode *mn;

    for (mn = pb->hl_Notifies.mlh_Head;
         mn != (struct MinNode *)&pb->hl_Notifies.mlh_Tail;
         mn = mn->mln_Succ)
    {
        nn = (struct HLNotifyNode *)((BYTE *)mn - (BYTE *)&((struct HLNotifyNode *)0)->hn);
        if (nn->entry == entry && nn->port != NULL) {
            nn->msg.HLClass = HLCLASS;
            nn->msg.ID = HLMSGID_NOTIFY;
            nn->msg.PB = NULL;
            nn->msg.Flags = 0;
            nn->msg.Return = NOERROR;
            nn->msg.mess.mn_Length = (UWORD)sizeof(struct HLMsg);
            nn->msg.mess.mn_ReplyPort = NULL;
            PutMsg(nn->port, (struct Message *)&nn->msg);
        }
    }
    if (entry != NULL) {
        entry->statusStamp++;
    }
    (void)pb;
    return NOERROR;
}
