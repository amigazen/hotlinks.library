/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * subscribe_demo.c - subscribe to an edition and wait for change notifications
 *
 * Demonstrates:
 *   HLRegister with a MsgPort, Subscribe, Notify(INFORM), PubStatus
 *
 * When another application updates the edition (Publish after ClosePub on a
 * write session), hotlinks.library sends a struct HLMsg to the port passed
 * to HLRegister.  HLClass is HLCLASS (3); hm_ID is HLMSGID_NOTIFY.
 *
 * Usage: subscribe_demo <edition_name>
 */

#include <exec/types.h>
#include <exec/ports.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/hotlinks.h>

#include <libraries/hotlinks.h>
#include <stdio.h>
#include <string.h>

#include "hl_common.h"

static char VERSTAG[] = "\0$VER: hl-subscribe_demo 1.0 (19.6.2026)";

int
main(int argc, char **argv)
{
    struct HLApp app;
    struct PubBlock *pb;
    struct MsgPort *port;
    struct HLMsg *msg;
    int err;
    int loops;
    ULONG sigs;
    ULONG portmask;

    if (argc != 2) {
        printf("Usage: subscribe_demo <edition_name>\n");
        return 0;
    }

    if (!HLAppOpen()) {
        printf("Could not open %s\n", HOTLINKS_LIBRARY_NAME);
        return 20;
    }

    port = CreateMsgPort();
    if (port == NULL) {
        HLAppClose();
        return 20;
    }

    if (HLAppRegister(&app, port, NULL) != NOERROR) {
        DeleteMsgPort(port);
        HLAppClose();
        return 20;
    }

    pb = HLAppAllocPB(&app);
    if (HLPBIsError(pb)) {
        HLAppUnregister(&app);
        DeleteMsgPort(port);
        HLAppClose();
        return 20;
    }

    err = HLFindEditionByName(pb, argv[1]);
    if (err != NOERROR) {
        printf("Edition not found: %s\n", argv[1]);
        FreePBlock(pb);
        HLAppUnregister(&app);
        DeleteMsgPort(port);
        HLAppClose();
        return 20;
    }

    err = Subscribe(pb);
    if (err != NOERROR) {
        HLPrintError("Subscribe", err);
        FreePBlock(pb);
        HLAppUnregister(&app);
        DeleteMsgPort(port);
        HLAppClose();
        return 20;
    }

    err = Notify(pb, INFORM, 0, NULL);
    if (err != NOERROR) {
        HLPrintError("Notify", err);
        FreePBlock(pb);
        HLAppUnregister(&app);
        DeleteMsgPort(port);
        HLAppClose();
        return 20;
    }

    printf("Subscribed to \"%s\" (%08lx:%08lx v%lu)\n",
        pb->PRec.Name,
        (unsigned long)pb->PRec.ID[0],
        (unsigned long)pb->PRec.ID[1],
        (unsigned long)pb->PRec.Version);
    printf("Waiting for HLMSGID_NOTIFY on port %s (Ctrl-C to exit)...\n",
        port->mp_Node.ln_Name ? port->mp_Node.ln_Name : (STRPTR)"(unnamed)");

    portmask = 1UL << port->mp_SigBit;
    loops = 0;

    while (loops < 600) {
        err = PubStatus(pb);
        if (err == CHANGED) {
            printf("PubStatus: edition changed (version now %lu)\n",
                (unsigned long)pb->PRec.Version);
            err = GetInfo(pb);
            if (err == NOERROR) {
                printf("  refreshed metadata, version %lu\n",
                    (unsigned long)pb->PRec.Version);
            }
        }

        sigs = Wait(portmask | SIGBREAKF_CTRL_C);
        if (sigs & SIGBREAKF_CTRL_C) {
            break;
        }

        while ((msg = (struct HLMsg *)GetMsg(port)) != NULL) {
            if (msg->HLClass == HLCLASS && msg->ID == HLMSGID_NOTIFY) {
                printf("Notify message: Return=%ld Flags=%lu\n",
                    (long)msg->Return, (unsigned long)msg->Flags);
            }
            ReplyMsg((struct Message *)msg);
        }

        loops++;
    }

    Notify(pb, NOINFORM, 0, NULL);
    UnSubscribe(pb);

    FreePBlock(pb);
    HLAppUnregister(&app);
    DeleteMsgPort(port);
    HLAppClose();
    return 0;
}
