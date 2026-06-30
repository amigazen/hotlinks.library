/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * hl_test.h - hotlinks.library console test harness helpers
 */

#ifndef HL_TEST_H
#define HL_TEST_H

#include <exec/types.h>
#include <libraries/hotlinks.h>

extern struct HotLinksBase *HotLinksBase;

#define HLTEST_CREATOR_ID  (('T'<<24)|('S'<<16)|('T'<<8)|('1'))
#define HLTEST_EDITION     "hltest_harness"

struct HLTestCtx {
    ULONG handle;
    struct PubBlock *pb;
    struct MsgPort *port;
    int keep_edition;
    int run_gui;
    int pass;
    int fail;
};

void HLTestBanner(void);
void HLTestInfo(CONST_STRPTR msg);
void HLTestStep(CONST_STRPTR msg);
void HLTestPass(struct HLTestCtx *ctx, CONST_STRPTR name);
void HLTestFail(struct HLTestCtx *ctx, CONST_STRPTR name, int got, int want);
void HLTestCheck(struct HLTestCtx *ctx, CONST_STRPTR name, int got, int want);
void HLTestSummary(struct HLTestCtx *ctx);

int HLTestOpen(struct HLTestCtx *ctx);
void HLTestClose(struct HLTestCtx *ctx);
int HLTestRegister(struct HLTestCtx *ctx);
void HLTestUnregister(struct HLTestCtx *ctx);
struct PubBlock *HLTestAllocPB(struct HLTestCtx *ctx);
int HLTestLoginRoot(struct HLTestCtx *ctx);
int HLTestFindEdition(struct PubBlock *pb, CONST_STRPTR name);
void HLTestPrintRecord(CONST_STRPTR label, struct PubRecord *rec);
int HLTestRunAll(struct HLTestCtx *ctx);

#endif /* HL_TEST_H */
