/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * hl_token.c - AllocNamedObject catalog broker (datatypes Token pattern)
 *
 * The HLToken persists for the life of the NamedObject registration. 
 * The last CloseLibrary() saves the catalog to disk and drops in-memory entries; 
 * the next OpenLibrary() reloads.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <utility/tagitem.h>
#include <utility/name.h>

#include <proto/exec.h>
#include <proto/utility.h>

#include <libraries/hotlinks.h>
#include "private/hl_debug.h"
#include "private/hl_internal.h"
#include "private/hl_token.h"

static void
HLTokenFreeCatalog(struct HLToken *tok)
{
    struct HLCatalogEntry *ce;
    struct MinNode *mn;

    while (tok->t_Catalog.mlh_Head != (struct MinNode *)&tok->t_Catalog.mlh_Tail) {
        mn = tok->t_Catalog.mlh_Head;
        ce = (struct HLCatalogEntry *)((BYTE *)mn -
            (BYTE *)&((struct HLCatalogEntry *)0)->hn);
        Remove((struct Node *)&ce->hn);
        FreeMem(ce, sizeof(struct HLCatalogEntry));
    }
}

static int
HLTokenCreate(struct HLPrivateBase *pb)
{
    struct HLToken *tok;
    struct NamedObject *no;
    struct TagItem tags[2];
    int err;

    tok = (struct HLToken *)AllocMem(sizeof(struct HLToken), MEMF_CLEAR);
    if (tok == NULL) {
        return NOMEMORY;
    }

    InitSemaphore(&tok->t_Lock);
    NewMinList(&tok->t_Catalog);
    NewMinList(&tok->t_Locks);
    tok->t_UseCnt = 0;
    tok->t_NextSerial = 0;
    tok->t_UserValid = FALSE;
    tok->t_LoginRequired = FALSE;
    tok->t_RootPath[0] = 0;

    tags[0].ti_Tag = ANO_Flags;
    tags[0].ti_Data = NSF_NODUPS;
    tags[1].ti_Tag = TAG_END;

    no = AllocNamedObjectA((STRPTR)HL_CATALOG_TOKEN_NAME, tags);
    if (no == NULL) {
        FreeMem(tok, sizeof(struct HLToken));
        no = FindNamedObject(NULL, (STRPTR)HL_CATALOG_TOKEN_NAME, NULL);
        if (no == NULL) {
            return NOMEMORY;
        }
        tok = (struct HLToken *)no->no_Object;
        ReleaseNamedObject(no);
        if (tok == NULL) {
            return IOERROR;
        }
        pb->hl_Token = tok;
        return HLTokenAttach(pb);
    }

    no->no_Object = (APTR)tok;
    pb->hl_Token = tok;

    err = HLLoadCatalog(pb);
    if (err != NOERROR) {
        hlDbgPut("HLTokenCreate: catalog load warning");
    }

    tok->t_UseCnt = 1;
    ReleaseNamedObject(no);
    hlDbgPut("HLTokenCreate: new catalog token");
    return NOERROR;
}

int
HLTokenAttach(struct HLPrivateBase *pb)
{
    struct NamedObject *no;
    struct HLToken *tok;
    int err;

    if (pb == NULL) {
        return INVPARAM;
    }

    no = FindNamedObject(NULL, (STRPTR)HL_CATALOG_TOKEN_NAME, NULL);
    if (no == NULL) {
        return HLTokenCreate(pb);
    }

    tok = (struct HLToken *)no->no_Object;
    if (tok == NULL) {
        ReleaseNamedObject(no);
        return IOERROR;
    }

    ObtainSemaphore(&tok->t_Lock);
    ReleaseNamedObject(no);

    if (tok->t_UseCnt == 0) {
        err = HLLoadCatalog(pb);
        if (err != NOERROR) {
            hlDbgPut("HLTokenAttach: catalog reload warning");
        }
    }
    tok->t_UseCnt++;
    ReleaseSemaphore(&tok->t_Lock);

    pb->hl_Token = tok;
    hlDbgPut("HLTokenAttach: joined catalog token");
    return NOERROR;
}

void
HLTokenDetach(struct HLPrivateBase *pb)
{
    struct HLToken *tok;
    BOOL last;

    if (pb == NULL || pb->hl_Token == NULL) {
        return;
    }

    tok = pb->hl_Token;
    pb->hl_Token = NULL;

    ObtainSemaphore(&tok->t_Lock);
    if (tok->t_UseCnt > 0) {
        tok->t_UseCnt--;
    }
    last = (tok->t_UseCnt == 0);
    if (last) {
        pb->hl_Token = tok;
        HLSaveCatalog(pb);
        pb->hl_Token = NULL;
        HLTokenFreeCatalog(tok);
        hlDbgPut("HLTokenDetach: catalog saved, memory released");
    } else {
        hlDbgPut("HLTokenDetach: released catalog reference");
    }
    ReleaseSemaphore(&tok->t_Lock);
}
