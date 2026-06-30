/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * hl_common.h - shared setup for hotlinks.library SDK examples
 *
 * Typical session:
 *   HLAppOpen -> HLAppRegister -> AllocPBlock -> ... API calls ...
 *   -> FreePBlock -> UnRegister -> HLAppClose
 */

#ifndef HL_SDK_COMMON_H
#define HL_SDK_COMMON_H

#include <exec/types.h>
#include <libraries/hotlinks.h>

extern struct HotLinksBase *HotLinksBase;

/* Four-byte creator ID stamped into new editions (HLRegister first argument). */
#define HL_CREATOR_DEMO  (('D'<<24)|('E'<<16)|('M'<<8)|('O'))

struct HLApp {
    ULONG handle;
    struct PubBlock *pb;
};

int HLAppOpen(void);
void HLAppClose(void);
int HLAppRegister(struct HLApp *app, struct MsgPort *port, struct Screen *scr);
void HLAppUnregister(struct HLApp *app);
struct PubBlock *HLAppAllocPB(struct HLApp *app);
int HLPBIsError(struct PubBlock *pb);
int HLFindEditionByName(struct PubBlock *pb, CONST_STRPTR name);
void HLPrintError(CONST_STRPTR where, int code);

#endif /* HL_SDK_COMMON_H */
