/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * read_edition.c - export a HotLinks DTXT edition to plain ASCII
 *
 * Demonstrates:
 *   FirstPub/NextPub (or name lookup) -> OpenPub(OPEN_READ)
 *   -> ReadPub/SeekPub -> ClosePub
 *
 * Parses the FORM DTXT wrapper and expands TEXT_TAB / TEXT_NEWLINE commands
 * back to bytes (subset of Soft-Logik hl2text.c).
 *
 * Usage: read_edition <edition_name> <output_file>
 */

#include <exec/types.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/hotlinks.h>

#include <libraries/hotlinks.h>
#include <dos/dos.h>
#include <stdio.h>
#include <string.h>

#include "hl_common.h"

static char VERSTAG[] = "\0$VER: hl-read_edition 1.0 (19.6.2026)";

static int
ReadCompNum(struct PubBlock *pb, int *value)
{
    int total;
    int accum;
    unsigned char byte;

    accum = 0;
    total = 0;

    ReadPub(pb, (char *)&byte, 1);
    total++;
    while (byte & 0x80) {
        accum <<= 7;
        accum += (byte & 0x7f);
        ReadPub(pb, (char *)&byte, 1);
        total++;
    }
    accum += byte;
    *value = accum;
    return total;
}

static void
ProcessDtxtChunk(struct PubBlock *pb, BPTR out, int size)
{
    int command;
    int cflag;
    int clen;
    int skip;
    unsigned char input;
    unsigned char output;

    (void)cflag;

    while (size > 0) {
        ReadPub(pb, (char *)&input, 1);
        size--;

        if (input == 0) {
            skip = ReadCompNum(pb, &command);
            size -= skip;
            skip = ReadCompNum(pb, &cflag);
            size -= skip;
            skip = ReadCompNum(pb, &clen);
            size -= skip;

            if (command == TEXT_TAB) {
                output = 9;
                Write(out, &output, 1);
            } else if (command == TEXT_NEWLINE) {
                output = 10;
                Write(out, &output, 1);
            } else if (clen > 0) {
                SeekPub(pb, clen, SEEK_CURRENT);
                size -= clen;
            }
        } else {
            Write(out, &input, 1);
        }
    }
}

int
main(int argc, char **argv)
{
    struct HLApp app;
    struct PubBlock *pb;
    BPTR out;
    int err;
    int stilldata;
    int chunksize;
    unsigned int chunktype;

    if (argc != 3) {
        printf("Usage: read_edition <edition_name> <output_file>\n");
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
        HLAppUnregister(&app);
        HLAppClose();
        return 20;
    }

    err = HLFindEditionByName(pb, argv[1]);
    if (err != NOERROR) {
        printf("Edition not found: %s\n", argv[1]);
        FreePBlock(pb);
        HLAppUnregister(&app);
        HLAppClose();
        return 20;
    }

    err = OpenPub(pb, OPEN_READ);
    if (err != NOERROR) {
        HLPrintError("OpenPub", err);
        FreePBlock(pb);
        HLAppUnregister(&app);
        HLAppClose();
        return 20;
    }

    out = Open(argv[2], MODE_NEWFILE);
    if (out == 0) {
        ClosePub(pb);
        FreePBlock(pb);
        HLAppUnregister(&app);
        HLAppClose();
        return 20;
    }

    ReadPub(pb, (char *)&chunktype, 4);
    ReadPub(pb, (char *)&stilldata, 4);
    ReadPub(pb, (char *)&chunktype, 4);

    stilldata -= 4;
    while (stilldata > 0) {
        ReadPub(pb, (char *)&chunktype, 4);
        ReadPub(pb, (char *)&chunksize, 4);

        if (chunktype == DTXT) {
            ProcessDtxtChunk(pb, out, chunksize);
            if (chunksize & 1) {
                chunksize++;
            }
        } else {
            if (chunksize & 1) {
                chunksize++;
            }
            if (chunksize > 0) {
                SeekPub(pb, chunksize, SEEK_CURRENT);
            }
        }
        stilldata -= (chunksize + 8);
    }

    Close(out);
    ClosePub(pb);

    printf("Exported \"%s\" (v%lu) to %s\n",
        pb->PRec.Name,
        (unsigned long)pb->PRec.Version,
        argv[2]);

    FreePBlock(pb);
    HLAppUnregister(&app);
    HLAppClose();
    return 0;
}
