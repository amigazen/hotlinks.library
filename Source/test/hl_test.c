/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * hl_test.c - console test harness for hotlinks.library
 *
 * Exercises the publish/subscribe API without GUI requesters by default.
 * Every step is logged to stdout.  Run from CLI:
 *
 *   hl_test           full automated suite
 *   hl_test -k        keep test edition on disk
 *   hl_test -g        also run GetPub/PutPub requesters (needs Intuition)
 *   hl_test -h        usage
 *
 * Requires LIBS:hotlinks.library and SDK headers (Source/include).
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/hotlinks.h>

#include <libraries/hotlinks.h>
#include <libraries/hotlinksbase.h>
#include <stdio.h>
#include <string.h>

#include "hl_test.h"

static char VERSTAG[] = "\0$VER: hl-test 1.0 (19.6.2026)";

struct HotLinksBase *HotLinksBase = NULL;

#define FORM_ID  (('F'<<24)|('O'<<16)|('R'<<8)|('M'))

static void
HLTestPrintAccess(struct PubRecord *rec, char *accstr)
{
    strcpy(accstr, "------");
    if (rec->Access & ACC_AREAD)  accstr[0] = 'r';
    if (rec->Access & ACC_AWRITE) accstr[1] = 'w';
    if (rec->Access & ACC_GREAD)  accstr[2] = 'r';
    if (rec->Access & ACC_GWRITE) accstr[3] = 'w';
    if (rec->Access & ACC_OREAD)  accstr[4] = 'r';
    if (rec->Access & ACC_OWRITE) accstr[5] = 'w';
}

void
HLTestBanner(void)
{
    printf("\n");
    printf("========================================\n");
    printf(" hotlinks.library test harness\n");
    printf("========================================\n");
}

void
HLTestInfo(CONST_STRPTR msg)
{
    printf("[INFO] %s\n", msg);
}

void
HLTestStep(CONST_STRPTR msg)
{
    printf("\n--- %s ---\n", msg);
}

void
HLTestPass(struct HLTestCtx *ctx, CONST_STRPTR name)
{
    printf("[PASS] %s\n", name);
    ctx->pass++;
}

void
HLTestFail(struct HLTestCtx *ctx, CONST_STRPTR name, int got, int want)
{
    printf("[FAIL] %s (got %ld, expected %ld)\n", name, (long)got, (long)want);
    ctx->fail++;
}

void
HLTestCheck(struct HLTestCtx *ctx, CONST_STRPTR name, int got, int want)
{
    if (got == want) {
        HLTestPass(ctx, name);
    } else {
        HLTestFail(ctx, name, got, want);
    }
}

void
HLTestSummary(struct HLTestCtx *ctx)
{
    printf("\n========================================\n");
    printf(" Results: %d passed, %d failed\n", ctx->pass, ctx->fail);
    printf("========================================\n\n");
}

void
HLTestPrintRecord(CONST_STRPTR label, struct PubRecord *rec)
{
    char type[5];
    char acc[8];

    strncpy(type, (char *)&rec->Type, 4);
    type[4] = 0;
    HLTestPrintAccess(rec, acc);
    printf("[INFO] %s: ID=%08lx:%08lx v%lu type=%s access=%s\n",
        label,
        (unsigned long)rec->ID[0],
        (unsigned long)rec->ID[1],
        (unsigned long)rec->Version,
        type, acc);
    printf("[INFO]   name=\"%s\" owner=\"%s\" group=\"%s\"\n",
        rec->Name, rec->Owner, rec->Group);
    printf("[INFO]   desc=\"%.64s%s\"\n",
        rec->Desc, (strlen(rec->Desc) > 64) ? "..." : "");
}

int
HLTestOpen(struct HLTestCtx *ctx)
{
    (void)ctx;
    HotLinksBase = (struct HotLinksBase *)OpenLibrary(HOTLINKS_LIBRARY_NAME, 0);
    if (HotLinksBase == NULL) {
        HLTestInfo("OpenLibrary failed");
        return 0;
    }
    HLTestInfo("OpenLibrary(\"hotlinks.library\", 0) OK");
    return 1;
}

void
HLTestClose(struct HLTestCtx *ctx)
{
    (void)ctx;
    if (HotLinksBase != NULL) {
        CloseLibrary((struct Library *)HotLinksBase);
        HotLinksBase = NULL;
        HLTestInfo("CloseLibrary OK");
    }
}

int
HLTestRegister(struct HLTestCtx *ctx)
{
    ULONG h;

    h = HLRegister(HLTEST_CREATOR_ID, ctx->port, NULL);
    if (h == (ULONG)NOMEMORY || h == (ULONG)INVPARAM) {
        printf("[INFO] HLRegister returned %lu\n", (unsigned long)h);
        return INVPARAM;
    }
    ctx->handle = h;
    printf("[INFO] HLRegister handle=%lu port=%s\n",
        (unsigned long)h,
        ctx->port ? "yes" : "NULL");
    return NOERROR;
}

void
HLTestUnregister(struct HLTestCtx *ctx)
{
    if (ctx->pb != NULL) {
        FreePBlock(ctx->pb);
        ctx->pb = NULL;
        HLTestInfo("FreePBlock OK");
    }
    if (ctx->handle != 0) {
        UnRegister(ctx->handle);
        HLTestInfo("UnRegister OK");
        ctx->handle = 0;
    }
}

struct PubBlock *
HLTestAllocPB(struct HLTestCtx *ctx)
{
    struct PubBlock *pb;

    pb = AllocPBlock(ctx->handle);
    if (pb == (struct PubBlock *)INVPARAM ||
        pb == (struct PubBlock *)NOMEMORY ||
        pb == (struct PubBlock *)NOPRIV)
    {
        printf("[INFO] AllocPBlock returned %ld\n", (long)(ULONG)pb);
        return pb;
    }
    ctx->pb = pb;
    HLTestInfo("AllocPBlock OK");
    return pb;
}

static void
HLTestProbeUsers(void)
{
    BPTR fh;

    fh = Open("HotLinks:Users", MODE_OLDFILE);
    printf("[INFO] probe HotLinks:Users fh=%ld IoErr=%ld\n",
        (long)(ULONG)fh, (long)IoErr());
    if (fh != 0) {
        Close(fh);
    }

    fh = Open("SYS:HotLinks/Users", MODE_OLDFILE);
    printf("[INFO] probe SYS:HotLinks/Users fh=%ld IoErr=%ld\n",
        (long)(ULONG)fh, (long)IoErr());
    if (fh != 0) {
        Close(fh);
    }
}

static void
HLTestResetUsersFile(void)
{
    if (DeleteFile("HotLinks:Users")) {
        HLTestInfo("deleted HotLinks:Users for fresh bootstrap");
    }
    if (DeleteFile("SYS:HotLinks/Users")) {
        HLTestInfo("deleted SYS:HotLinks/Users for fresh bootstrap");
    }
}

int
HLTestLoginRoot(struct HLTestCtx *ctx)
{
    int err;

    HLTestProbeUsers();
    HLTestResetUsersFile();

    err = SetUser(ctx->handle, "root", "");
    printf("[INFO] SetUser(root, \"\") -> %ld\n", (long)err);
    if (err == NOERROR) {
        return err;
    }

    /* Recover from a prior interrupted run that left root:harness in Users. */
    err = SetUser(ctx->handle, "root", "harness");
    printf("[INFO] SetUser(root, \"harness\") retry -> %ld\n", (long)err);
    if (err != NOERROR) {
        return err;
    }

    err = ChgPassword(ctx->handle, "root", "harness", "");
    printf("[INFO] ChgPassword restore empty -> %ld\n", (long)err);
    if (err != NOERROR) {
        return err;
    }

    err = SetUser(ctx->handle, "root", "");
    printf("[INFO] SetUser(root, \"\") after reset -> %ld\n", (long)err);
    return err;
}

int
HLTestFindEdition(struct PubBlock *pb, CONST_STRPTR name)
{
    int err;

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

static int
HLTestWriteTextCommands(struct PubBlock *pb, char *buff, int len)
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

static int
HLTestWriteDtxtBody(struct PubBlock *pb, CONST_STRPTR body)
{
    int bodylen;
    int newlen;
    int chunksize;
    unsigned int chunktype;
    char pad;

    bodylen = (int)strlen(body);

    chunktype = FORM_ID;
    WritePub(pb, (char *)&chunktype, 4);
    chunksize = bodylen + 12;
    WritePub(pb, (char *)&chunksize, 4);
    chunktype = DTXT;
    WritePub(pb, (char *)&chunktype, 4);

    WritePub(pb, (char *)&chunktype, 4);
    chunksize = bodylen;
    WritePub(pb, (char *)&chunksize, 4);

    newlen = HLTestWriteTextCommands(pb, (char *)body, bodylen);
    if (bodylen & 1) {
        pad = 0;
        WritePub(pb, &pad, 1);
    }

    SeekPub(pb, 4, SEEK_BEGINNING);
    chunksize = newlen + 12;
    WritePub(pb, (char *)&chunksize, 4);
    SeekPub(pb, 8, SEEK_CURRENT);
    chunksize = newlen;
    WritePub(pb, (char *)&chunksize, 4);
    SeekPub(pb, 0, SEEK_END);

    return NOERROR;
}

static int
HLTestPublishDtxt(struct HLTestCtx *ctx, struct PubBlock *pb,
    CONST_STRPTR name, CONST_STRPTR body)
{
    int err;

    memset(&pb->PRec, 0, sizeof(pb->PRec));
    pb->PRec.ID[0] = 0;
    pb->PRec.ID[1] = 0;
    pb->PRec.Type = DTXT;
    pb->PRec.Access = ACC_DEFAULT | ACC_AREAD | ACC_AWRITE;
    strncpy(pb->PRec.Name, name, sizeof(pb->PRec.Name) - 1);
    strncpy(pb->PRec.Desc, "hl_test harness edition", sizeof(pb->PRec.Desc) - 1);

    HLTestInfo("OpenPub(OPEN_WRITE) with zero ID (create edition)");
    err = OpenPub(pb, OPEN_WRITE);
    if (err != NOERROR) {
        return err;
    }

    HLTestPrintRecord("after OpenPub", &pb->PRec);
    HLTestWriteDtxtBody(pb, body);

    HLTestInfo("ClosePub after write");
    err = ClosePub(pb);
    if (err != NOERROR) {
        return err;
    }

    HLTestInfo("Publish");
    err = Publish(pb);
    if (err == NOERROR) {
        HLTestPrintRecord("published", &pb->PRec);
    }
    (void)ctx;
    return err;
}

static int
HLTestRepublishDtxt(struct HLTestCtx *ctx, struct PubBlock *pb,
    CONST_STRPTR body)
{
    int err;

    HLTestInfo("OpenPub(OPEN_WRITE) on existing edition (new version)");
    err = OpenPub(pb, OPEN_WRITE);
    if (err != NOERROR) {
        return err;
    }
    printf("[INFO] new version %lu\n", (unsigned long)pb->PRec.Version);
    HLTestWriteDtxtBody(pb, body);

    err = ClosePub(pb);
    if (err != NOERROR) {
        return err;
    }

    err = Publish(pb);
    if (err == NOERROR) {
        HLTestPrintRecord("republished", &pb->PRec);
    }
    (void)ctx;
    return err;
}

static int
HLTestFilterDtxt(struct PubBlock *pb)
{
    if (pb->PRec.Type == DTXT) {
        return ACCEPT;
    }
    return NOACCEPT;
}

static void
HLTestRemoveIfPresent(struct HLTestCtx *ctx, struct PubBlock *pb,
    CONST_STRPTR name)
{
    int err;

    err = HLTestFindEdition(pb, name);
    if (err != NOERROR) {
        HLTestInfo("no prior test edition to remove");
        return;
    }
    HLTestPrintRecord("removing old", &pb->PRec);
    err = RemovePub(pb);
    printf("[INFO] RemovePub -> %ld\n", (long)err);
    if (err == NOERROR) {
        HLTestPass(ctx, "cleanup old edition");
    } else {
        HLTestFail(ctx, "cleanup old edition", err, NOERROR);
    }
}

static void
HLTestSysInfo(struct HLTestCtx *ctx)
{
    int array[8];
    int err;

    memset(array, 0, sizeof(array));
    err = HLSysInfo(ctx->handle, array);
    printf("[INFO] HLSysInfo -> %ld  [0]=%ld [1]=%ld [2]=%ld [3]=%ld\n",
        (long)err,
        (long)array[0], (long)array[1],
        (long)array[2], (long)array[3]);
    HLTestCheck(ctx, "HLSysInfo", err, NOERROR);
}

static void
HLTestCatalogWalk(struct HLTestCtx *ctx, struct PubBlock *pb)
{
    int err;
    int count;

    count = 0;
    err = FirstPub(pb);
    while (err == NOERROR) {
        count++;
        HLTestPrintRecord("catalog entry", &pb->PRec);
        err = NextPub(pb);
    }
    printf("[INFO] catalog walk: %d editions, last err=%ld\n",
        count, (long)err);
    HLTestCheck(ctx, "FirstPub/NextPub terminal", err, NOMOREBLOCKS);
}

static void
HLTestReadBody(struct HLTestCtx *ctx, struct PubBlock *pb)
{
    int err;
    int got;
    unsigned int formtype;
    int formsize;
    char buf[16];

    err = OpenPub(pb, OPEN_READ);
    HLTestCheck(ctx, "OpenPub(READ)", err, NOERROR);
    if (err != NOERROR) {
        return;
    }

    got = ReadPub(pb, (char *)&formtype, 4);
    if (got == 4 && formtype == (unsigned int)FORM_ID) {
        HLTestPass(ctx, "ReadPub FORM id");
    } else {
        HLTestFail(ctx, "ReadPub FORM id", got, 4);
    }
    ReadPub(pb, (char *)&formsize, 4);
    ReadPub(pb, buf, 4);
    buf[4] = 0;
    printf("[INFO] IFF FORM size=%d inner=%.4s\n", formsize, buf);

    err = ClosePub(pb);
    HLTestCheck(ctx, "ClosePub(READ)", err, NOERROR);
}

static void
HLTestSetInfoRoundtrip(struct HLTestCtx *ctx, struct PubBlock *pb)
{
    int err;
    char saved[256];

    strncpy(saved, pb->PRec.Desc, sizeof(saved) - 1);
    strncpy(pb->PRec.Desc, "hl_test updated description", sizeof(pb->PRec.Desc) - 1);

    err = SetInfo(pb);
    printf("[INFO] SetInfo -> %ld\n", (long)err);
    HLTestCheck(ctx, "SetInfo", err, NOERROR);

    err = GetInfo(pb);
    HLTestCheck(ctx, "GetInfo after SetInfo", err, NOERROR);
    if (err == NOERROR) {
        if (strcmp(pb->PRec.Desc, "hl_test updated description") == 0) {
            HLTestPass(ctx, "GetInfo desc matches");
        } else {
            HLTestFail(ctx, "GetInfo desc matches", 1, 0);
        }
    }

    strncpy(pb->PRec.Desc, saved, sizeof(pb->PRec.Desc) - 1);
    SetInfo(pb);
}

static void
HLTestLocks(struct HLTestCtx *ctx, struct PubBlock *pb)
{
    int err;

    err = LockPub(pb, LOCK_READ);
    printf("[INFO] LockPub(READ) -> %ld LFlag=%ld\n",
        (long)err, (long)pb->LFlag);
    HLTestCheck(ctx, "LockPub(READ)", err, NOERROR);

    err = LockPub(pb, LOCK_RELEASE);
    HLTestCheck(ctx, "LockPub(RELEASE)", err, NOERROR);

    err = LockPub(pb, LOCK_WRITE);
    printf("[INFO] LockPub(WRITE) -> %ld\n", (long)err);
    HLTestCheck(ctx, "LockPub(WRITE)", err, NOERROR);

    err = LockPub(pb, LOCK_RELEASE);
    HLTestCheck(ctx, "LockPub(RELEASE) after write", err, NOERROR);
}

static void
HLTestSubscribeNotify(struct HLTestCtx *ctx, struct PubBlock *pb,
    CONST_STRPTR body)
{
    struct HLMsg *msg;
    int err;
    int status;
    ULONG portmask;
    struct PubBlock *pb2;
    ULONG id0;
    ULONG id1;
    ULONG ver;

    id0 = pb->PRec.ID[0];
    id1 = pb->PRec.ID[1];
    ver = pb->PRec.Version;

    err = Subscribe(pb);
    HLTestCheck(ctx, "Subscribe", err, NOERROR);

    err = Notify(pb, INFORM, 0, NULL);
    HLTestCheck(ctx, "Notify(INFORM)", err, NOERROR);

    status = PubStatus(pb);
    printf("[INFO] PubStatus before update -> %ld\n", (long)status);
    HLTestCheck(ctx, "PubStatus unchanged", status, NOERROR);

    pb2 = AllocPBlock(ctx->handle);
    if (pb2 == (struct PubBlock *)INVPARAM ||
        pb2 == (struct PubBlock *)NOMEMORY)
    {
        HLTestFail(ctx, "AllocPBlock for republish", (int)(ULONG)pb2, NOERROR);
        UnSubscribe(pb);
        return;
    }

    pb2->PRec.ID[0] = id0;
    pb2->PRec.ID[1] = id1;
    pb2->PRec.Version = ver;
    strncpy(pb2->PRec.Name, pb->PRec.Name, sizeof(pb2->PRec.Name) - 1);
    pb2->PRec.Type = pb->PRec.Type;
    pb2->PRec.Access = pb->PRec.Access;

    HLTestInfo("republishing new version to trigger notify");
    err = GetInfo(pb2);
    if (err != NOERROR) {
        HLTestFail(ctx, "GetInfo before republish", err, NOERROR);
        FreePBlock(pb2);
        UnSubscribe(pb);
        return;
    }
    err = HLTestRepublishDtxt(ctx, pb2, body);
    HLTestCheck(ctx, "republish edition", err, NOERROR);

    status = PubStatus(pb);
    printf("[INFO] PubStatus after update -> %ld (expect CHANGED=%ld)\n",
        (long)status, (long)CHANGED);
    if (status == CHANGED) {
        HLTestPass(ctx, "PubStatus CHANGED");
    } else {
        HLTestFail(ctx, "PubStatus CHANGED", status, CHANGED);
    }

    err = GetInfo(pb);
    HLTestCheck(ctx, "GetInfo after change", err, NOERROR);
    if (err == NOERROR) {
        printf("[INFO] subscriber sees version %lu\n",
            (unsigned long)pb->PRec.Version);
    }

    if (ctx->port != NULL) {
        portmask = 1UL << ctx->port->mp_SigBit;
        if (Wait(portmask) & portmask) {
            while ((msg = (struct HLMsg *)GetMsg(ctx->port)) != NULL) {
                printf("[INFO] HLMsg class=%ld id=%u return=%ld\n",
                    (long)msg->HLClass, (unsigned int)msg->ID,
                    (long)msg->Return);
                if (msg->HLClass == HLCLASS && msg->ID == HLMSGID_NOTIFY) {
                    HLTestPass(ctx, "HLMSGID_NOTIFY received");
                }
                ReplyMsg((struct Message *)msg);
            }
        }
    } else {
        HLTestInfo("no MsgPort registered; skipped HLMsg wait");
    }

    Notify(pb, NOINFORM, 0, NULL);
    UnSubscribe(pb);
    FreePBlock(pb2);
}

static void
HLTestGuiRequesters(struct HLTestCtx *ctx, struct PubBlock *pb)
{
    int err;
    struct PubBlock *pb2;

    HLTestStep("GUI requester tests (-g)");
    HLTestInfo("GetPub will open edition picker (cancel with Cancel for IOERROR)");

    err = GetPub(pb, (int (*)(struct PubBlock *))HLTestFilterDtxt);
    printf("[INFO] GetPub -> %ld\n", (long)err);
    if (err == NOERROR) {
        HLTestPass(ctx, "GetPub selected edition");
        HLTestPrintRecord("GetPub result", &pb->PRec);
    } else if (err == IOERROR) {
        HLTestInfo("GetPub cancelled by user (IOERROR)");
    } else {
        HLTestFail(ctx, "GetPub", err, NOERROR);
    }

    pb2 = AllocPBlock(ctx->handle);
    if (pb2 == (struct PubBlock *)INVPARAM ||
        pb2 == (struct PubBlock *)NOMEMORY)
    {
        return;
    }
    strncpy(pb2->PRec.Name, "hltest_gui_putpub", sizeof(pb2->PRec.Name) - 1);
    strncpy(pb2->PRec.Desc, "GUI PutPub test", sizeof(pb2->PRec.Desc) - 1);
    pb2->PRec.Type = DTXT;
    pb2->PRec.Access = ACC_DEFAULT | ACC_AREAD | ACC_AWRITE;

    HLTestInfo("PutPub will open new-edition dialog");
    err = PutPub(pb2, 0);
    printf("[INFO] PutPub -> %ld\n", (long)err);
    if (err == NOERROR) {
        HLTestPass(ctx, "PutPub OK");
        HLTestPrintRecord("PutPub result", &pb2->PRec);
    } else if (err == IOERROR) {
        HLTestInfo("PutPub cancelled (IOERROR)");
    }

    FreePBlock(pb2);
    (void)ctx;
}

static void
HLTestPasswordCli(struct HLTestCtx *ctx)
{
    int err;

    HLTestStep("ChgPassword CLI (root)");
    err = ChgPassword(ctx->handle, "root", "", "harness");
    printf("[INFO] ChgPassword root '' -> 'harness' -> %ld\n", (long)err);
    HLTestCheck(ctx, "ChgPassword set", err, NOERROR);

    err = SetUser(ctx->handle, "root", "harness");
    HLTestCheck(ctx, "SetUser with new password", err, NOERROR);

    err = ChgPassword(ctx->handle, "root", "harness", "");
    printf("[INFO] ChgPassword restore empty password -> %ld\n", (long)err);
    HLTestCheck(ctx, "ChgPassword restore", err, NOERROR);

    err = SetUser(ctx->handle, "root", "");
    HLTestCheck(ctx, "SetUser restore login", err, NOERROR);
}

int
HLTestRunAll(struct HLTestCtx *ctx)
{
    struct PubBlock *pb;
    int err;
    static const char kBody[] = "HotLinks harness line one\nLine two\n";

    HLTestStep("open library");
    if (!HLTestOpen(ctx)) {
        HLTestFail(ctx, "OpenLibrary", 0, 1);
        return 20;
    }
    HLTestPass(ctx, "OpenLibrary");

    HLTestStep("register + login");
    ctx->port = CreateMsgPort();
    if (ctx->port == NULL) {
        HLTestFail(ctx, "CreateMsgPort", 0, 1);
        HLTestClose(ctx);
        return 20;
    }
    HLTestInfo("CreateMsgPort OK");

    err = HLTestRegister(ctx);
    HLTestCheck(ctx, "HLRegister", err, NOERROR);
    if (err != NOERROR) {
        DeleteMsgPort(ctx->port);
        HLTestClose(ctx);
        return 20;
    }

    err = HLTestLoginRoot(ctx);
    if (err == IOERROR) {
        HLTestCheck(ctx, "SetUser(root) Users bootstrap", err, NOERROR);
        HLTestInfo("SetUser failed: could not create/read HotLinks:Users "
            "(see probe IoErr above)");
    } else {
        HLTestCheck(ctx, "SetUser(root)", err, NOERROR);
    }
    if (err != NOERROR) {
        HLTestInfo("SetUser failed; cannot continue (AllocPBlock would open login GUI)");
        HLTestUnregister(ctx);
        DeleteMsgPort(ctx->port);
        HLTestClose(ctx);
        return 20;
    }

    pb = HLTestAllocPB(ctx);
    if (pb == (struct PubBlock *)INVPARAM ||
        pb == (struct PubBlock *)NOMEMORY ||
        pb == (struct PubBlock *)NOPRIV)
    {
        HLTestFail(ctx, "AllocPBlock", (int)(ULONG)pb, NOERROR);
        HLTestUnregister(ctx);
        DeleteMsgPort(ctx->port);
        HLTestClose(ctx);
        return 20;
    }
    HLTestPass(ctx, "AllocPBlock");

    HLTestStep("HLSysInfo");
    HLTestSysInfo(ctx);

    HLTestStep("catalog before publish");
    HLTestCatalogWalk(ctx, pb);

    HLTestStep("cleanup + publish test edition");
    HLTestRemoveIfPresent(ctx, pb, HLTEST_EDITION);
    err = HLTestPublishDtxt(ctx, pb, HLTEST_EDITION, kBody);
    HLTestCheck(ctx, "publish DTXT edition", err, NOERROR);

    HLTestStep("catalog after publish");
    HLTestCatalogWalk(ctx, pb);

    err = HLTestFindEdition(pb, HLTEST_EDITION);
    HLTestCheck(ctx, "find test edition", err, NOERROR);

    HLTestStep("read edition body");
    HLTestReadBody(ctx, pb);

    HLTestStep("SetInfo / GetInfo");
    HLTestSetInfoRoundtrip(ctx, pb);

    HLTestStep("LockPub");
    HLTestLocks(ctx, pb);

    HLTestStep("Subscribe / Notify / republish");
    HLTestSubscribeNotify(ctx, pb, "HotLinks harness REPUBLISH\n");

    HLTestStep("SetUser logout + login required flag");
    err = SetUser(ctx->handle, NULL, NULL);
    printf("[INFO] SetUser logout -> %ld\n", (long)err);
    HLTestCheck(ctx, "SetUser logout", err, NOERROR);
    err = HLTestLoginRoot(ctx);
    HLTestCheck(ctx, "SetUser re-login", err, NOERROR);

    HLTestPasswordCli(ctx);

    if (ctx->run_gui) {
        HLTestGuiRequesters(ctx, pb);
    }

    if (!ctx->keep_edition) {
        HLTestStep("remove test edition");
        err = HLTestFindEdition(pb, HLTEST_EDITION);
        if (err == NOERROR) {
            err = RemovePub(pb);
            printf("[INFO] RemovePub -> %ld\n", (long)err);
            HLTestCheck(ctx, "RemovePub cleanup", err, NOERROR);
        }
    } else {
        HLTestInfo("keeping test edition (-k)");
    }

    HLTestStep("shutdown");
    HLTestUnregister(ctx);
    DeleteMsgPort(ctx->port);
    ctx->port = NULL;
    HLTestClose(ctx);

    return (ctx->fail > 0) ? 20 : 0;
}

static void
HLTestUsage(void)
{
    printf("Usage: hl_test [-k] [-g] [-h]\n");
    printf("  -k   keep test edition on disk after run\n");
    printf("  -g   run GetPub/PutPub requesters (interactive)\n");
    printf("  -h   this help\n");
}

int
main(int argc, char **argv)
{
    struct HLTestCtx ctx;
    int i;
    int rc;

    memset(&ctx, 0, sizeof(ctx));

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-k") == 0) {
            ctx.keep_edition = 1;
        } else if (strcmp(argv[i], "-g") == 0) {
            ctx.run_gui = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-?") == 0) {
            HLTestUsage();
            return 0;
        } else {
            printf("Unknown option: %s\n", argv[i]);
            HLTestUsage();
            return 10;
        }
    }

    HLTestBanner();
    if (ctx.run_gui) {
        HLTestInfo("GUI tests enabled (-g)");
    }
    if (ctx.keep_edition) {
        HLTestInfo("will keep test edition (-k)");
    }

    rc = HLTestRunAll(&ctx);
    HLTestSummary(&ctx);
    return rc;
}
