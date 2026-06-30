/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * edlist.c - list HotLinks editions visible to the current user
 *
 * Demonstrates:
 *   OpenLibrary, HLRegister, AllocPBlock, FirstPub, NextPub, FreePBlock,
 *   UnRegister, CloseLibrary
 *
 * Also shows OpenPub(OPEN_READ) + SeekPub + ReadPub to read the FORM size
 * from each edition body (same technique as Soft-Logik edlist.c).
 *
 * Usage: edlist
 */

#include <exec/types.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/hotlinks.h>

#include <libraries/hotlinks.h>
#include <stdio.h>
#include <string.h>

#include "hl_common.h"

static char VERSTAG[] = "\0$VER: hl-edlist 1.0 (19.6.2026)";

static void
PrintAccess(struct PubBlock *pb, char *accstr)
{
    strcpy(accstr, "------");
    if (pb->PRec.Access & ACC_AREAD)  accstr[0] = 'r';
    if (pb->PRec.Access & ACC_AWRITE) accstr[1] = 'w';
    if (pb->PRec.Access & ACC_GREAD)  accstr[2] = 'r';
    if (pb->PRec.Access & ACC_GWRITE) accstr[3] = 'w';
    if (pb->PRec.Access & ACC_OREAD)  accstr[4] = 'r';
    if (pb->PRec.Access & ACC_OWRITE) accstr[5] = 'w';
}

int
main(void)
{
    struct HLApp app;
    struct PubBlock *pb;
    int err;
    int toted;
    int totlen;
    int len;
    char hltype[5];
    char accstr[7];
    char *t1;
    char *t2;
    static char *months[12] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    char datebuff[32];

    if (!HLAppOpen()) {
        printf("Could not open %s\n", HOTLINKS_LIBRARY_NAME);
        return 20;
    }

    err = HLAppRegister(&app, NULL, NULL);
    if (err != NOERROR) {
        HLPrintError("HLRegister", err);
        HLAppClose();
        return 20;
    }

    pb = HLAppAllocPB(&app);
    if (HLPBIsError(pb)) {
        HLPrintError("AllocPBlock", (int)(LONG)pb);
        HLAppUnregister(&app);
        HLAppClose();
        return 20;
    }

    toted = 0;
    totlen = 0;

    err = FirstPub(pb);
    while (err != NOMOREBLOCKS) {
        if (err != NOERROR) {
            HLPrintError("FirstPub/NextPub", err);
            break;
        }

        toted++;
        len = -1;
        if (OpenPub(pb, OPEN_READ) == NOERROR) {
            SeekPub(pb, 4, SEEK_BEGINNING);
            ReadPub(pb, (char *)&len, 4);
            ClosePub(pb);
            len += 8;
            totlen += len;
        }

        strncpy(hltype, (char *)&pb->PRec.Type, 4);
        hltype[4] = 0;
        PrintAccess(pb, accstr);

        t1 = (char *)&pb->PRec.MDate;
        t2 = (char *)&pb->PRec.MTime;
        if ((t1[2] > 0) && (t1[2] < 13) && (t1[3] > 0) && (t1[3] < 32)) {
            sprintf(datebuff, "%s %2.2d %2.2d:%02.2d:%02.2d",
                months[t1[2] - 1], t1[3], t2[0], t2[1], t2[2]);
        } else {
            strcpy(datebuff, "<unknown>");
        }

        printf("%08lx:%08lx v%-3lu  %s  %8d  %s  %s  %s\n",
            (unsigned long)pb->PRec.ID[0],
            (unsigned long)pb->PRec.ID[1],
            (unsigned long)pb->PRec.Version,
            accstr, len, hltype, datebuff, pb->PRec.Name);

        err = NextPub(pb);
    }

    printf("Editions: %d     Bytes: %d\n", toted, totlen);

    FreePBlock(pb);
    HLAppUnregister(&app);
    HLAppClose();
    return 0;
}
