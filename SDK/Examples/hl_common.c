/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * hl_common.c - open/register helpers for SDK examples
 */

#include <exec/types.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include <libraries/hotlinks.h>
#include <libraries/hotlinksbase.h>
#include <proto/hotlinks.h>

#include <stdio.h>
#include <string.h>

#include "hl_common.h"

struct HotLinksBase *HotLinksBase = NULL;

int
HLAppOpen(void)
{
    HotLinksBase = (struct HotLinksBase *)OpenLibrary(HOTLINKS_LIBRARY_NAME, 0);
    if (HotLinksBase == NULL) {
        return 0;
    }
    return 1;
}

void
HLAppClose(void)
{
    if (HotLinksBase != NULL) {
        CloseLibrary((struct Library *)HotLinksBase);
        HotLinksBase = NULL;
    }
}

int
HLAppRegister(struct HLApp *app, struct MsgPort *port, struct Screen *scr)
{
    ULONG h;

    if (app == NULL || HotLinksBase == NULL) {
        return INVPARAM;
    }

    app->handle = 0;
    app->pb = NULL;

    h = HLRegister(HL_CREATOR_DEMO, port, scr);
    if (h == (ULONG)NOMEMORY || h == (ULONG)INVPARAM) {
        return (int)h;
    }

    app->handle = h;
    return NOERROR;
}

void
HLAppUnregister(struct HLApp *app)
{
    if (app == NULL) {
        return;
    }
    if (app->pb != NULL) {
        FreePBlock(app->pb);
        app->pb = NULL;
    }
    if (app->handle != 0) {
        UnRegister(app->handle);
        app->handle = 0;
    }
}

struct PubBlock *
HLAppAllocPB(struct HLApp *app)
{
    struct PubBlock *pb;

    if (app == NULL || app->handle == 0) {
        return (struct PubBlock *)INVPARAM;
    }

    pb = AllocPBlock(app->handle);
    if (!HLPBIsError(pb)) {
        app->pb = pb;
    }
    return pb;
}

int
HLPBIsError(struct PubBlock *pb)
{
    if (pb == NULL) {
        return 1;
    }
    if (pb == (struct PubBlock *)INVPARAM) {
        return 1;
    }
    if (pb == (struct PubBlock *)NOMEMORY) {
        return 1;
    }
    if (pb == (struct PubBlock *)NOPRIV) {
        return 1;
    }
    return 0;
}

int
HLFindEditionByName(struct PubBlock *pb, CONST_STRPTR name)
{
    int err;

    if (pb == NULL || name == NULL) {
        return INVPARAM;
    }

    err = FirstPub(pb);
    while (err == NOERROR) {
        if (strcmp(pb->PRec.Name, name) == 0) {
            return NOERROR;
        }
        err = NextPub(pb);
    }

    if (err == NOMOREBLOCKS) {
        return INVPARAM;
    }
    return err;
}

void
HLPrintError(CONST_STRPTR where, int code)
{
    printf("%s: error %ld\n", where, (long)code);
}
