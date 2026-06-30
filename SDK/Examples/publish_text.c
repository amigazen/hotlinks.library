/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * publish_text.c - publish a plain ASCII file as a DTXT HotLinks edition
 *
 * Demonstrates the publish workflow:
 *   PutPub (edition metadata) -> OpenPub(OPEN_WRITE) with ID[0]=ID[1]=0
 *   -> WritePub/SeekPub (IFF FORM DTXT body) -> ClosePub -> Publish
 *
 * The on-disk body matches the Soft-Logik text2hl layout: FORM DTXT wrapping
 * a DTXT chunk whose payload uses TEXT_TAB / TEXT_NEWLINE commands for
 * control characters (see hotlinks.h TEXT_* constants).
 *
 * Usage: publish_text <edition_name> <ascii_file>
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/hotlinks.h>

#include <libraries/hotlinks.h>
#include <dos/dos.h>
#include <stdio.h>
#include <string.h>

#include "hl_common.h"

static char VERSTAG[] = "\0$VER: hl-publish_text 1.0 (19.6.2026)";

#define FORM_ID  (('F'<<24)|('O'<<16)|('R'<<8)|('M'))

static int
WriteTextCommands(struct PubBlock *pb, char *buff, int len)
{
    int flen;
    char command;
    char cflag;
    char clen;

    flen = 0;
    while (len > 0) {
        if (*buff == 0x09) {
            command = TEXT_TAB;
            cflag = TEXT_FLAGS_TAB;
            clen = 0;
            WritePub(pb, &clen, 1);
            WritePub(pb, &command, 1);
            WritePub(pb, &cflag, 1);
            WritePub(pb, &clen, 1);
            flen += 4;
        } else if (*buff == 0x0a) {
            command = TEXT_NEWLINE;
            cflag = TEXT_FLAGS_NEWLINE;
            clen = 0;
            WritePub(pb, &clen, 1);
            WritePub(pb, &command, 1);
            WritePub(pb, &cflag, 1);
            WritePub(pb, &clen, 1);
            flen += 4;
        } else {
            WritePub(pb, buff, 1);
            flen++;
        }
        len--;
        buff++;
    }
    return flen;
}

int
main(int argc, char **argv)
{
    struct HLApp app;
    struct PubBlock *pb;
    struct FileInfoBlock fib;
    BPTR lock;
    BPTR fi;
    char *buff;
    int err;
    int filesize;
    int newlen;
    int chunksize;
    unsigned int chunktype;
    char desc[64];

    if (argc != 3) {
        printf("Usage: publish_text <edition_name> <ascii_file>\n");
        return 0;
    }

    if (!HLAppOpen()) {
        printf("Could not open %s\n", HOTLINKS_LIBRARY_NAME);
        return 20;
    }

    if (HLAppRegister(&app, NULL, NULL) != NOERROR) {
        HLAppUnregister(&app);
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

    lock = Lock(argv[2], ACCESS_READ);
    if (lock == 0) {
        printf("Could not lock input file: %s\n", argv[2]);
        FreePBlock(pb);
        HLAppUnregister(&app);
        HLAppClose();
        return 20;
    }
    if (!Examine(lock, &fib)) {
        UnLock(lock);
        FreePBlock(pb);
        HLAppUnregister(&app);
        HLAppClose();
        return 20;
    }
    filesize = (int)fib.fib_Size;
    UnLock(lock);

    fi = Open(argv[2], MODE_OLDFILE);
    if (fi == 0) {
        printf("Could not open input file: %s\n", argv[2]);
        FreePBlock(pb);
        HLAppUnregister(&app);
        HLAppClose();
        return 20;
    }

    memset(desc, 0, sizeof(desc));
    strncpy(desc, "SDK publish_text example", sizeof(desc) - 1);

    pb->PRec.ID[0] = 0;
    pb->PRec.ID[1] = 0;
    pb->PRec.Type = DTXT;
    pb->PRec.Access = ACC_DEFAULT | ACC_AREAD | ACC_AWRITE;
    strncpy(pb->PRec.Name, argv[1], sizeof(pb->PRec.Name) - 1);
    strncpy(pb->PRec.Desc, desc, sizeof(pb->PRec.Desc) - 1);

    err = PutPub(pb, 0);
    if (err != NOERROR) {
        HLPrintError("PutPub", err);
        Close(fi);
        FreePBlock(pb);
        HLAppUnregister(&app);
        HLAppClose();
        return 20;
    }

    err = OpenPub(pb, OPEN_WRITE);
    if (err != NOERROR) {
        HLPrintError("OpenPub", err);
        Close(fi);
        FreePBlock(pb);
        HLAppUnregister(&app);
        HLAppClose();
        return 20;
    }

    chunktype = FORM_ID;
    WritePub(pb, (char *)&chunktype, 4);
    chunksize = filesize + 12;
    WritePub(pb, (char *)&chunksize, 4);
    chunktype = DTXT;
    WritePub(pb, (char *)&chunktype, 4);

    WritePub(pb, (char *)&chunktype, 4);
    chunksize = filesize;
    WritePub(pb, (char *)&chunksize, 4);

    buff = (char *)AllocMem(filesize, MEMF_ANY);
    if (buff == NULL) {
        ClosePub(pb);
        Close(fi);
        FreePBlock(pb);
        HLAppUnregister(&app);
        HLAppClose();
        return 20;
    }

    Read(fi, buff, filesize);
    newlen = WriteTextCommands(pb, buff, filesize);

    if (filesize & 1) {
        *buff = 0;
        WritePub(pb, buff, 1);
    }

    FreeMem(buff, filesize);
    Close(fi);

    SeekPub(pb, 4, SEEK_BEGINNING);
    chunksize = newlen + 12;
    WritePub(pb, (char *)&chunksize, 4);
    SeekPub(pb, 8, SEEK_CURRENT);
    chunksize = newlen;
    WritePub(pb, (char *)&chunksize, 4);
    SeekPub(pb, 0, SEEK_END);

    ClosePub(pb);

    err = Publish(pb);
    if (err != NOERROR) {
        HLPrintError("Publish", err);
    } else {
        printf("Published edition \"%s\"  ID %08lx:%08lx  version %lu\n",
            pb->PRec.Name,
            (unsigned long)pb->PRec.ID[0],
            (unsigned long)pb->PRec.ID[1],
            (unsigned long)pb->PRec.Version);
    }

    FreePBlock(pb);
    HLAppUnregister(&app);
    HLAppClose();
    return 0;
}
