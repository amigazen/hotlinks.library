/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * hl_req.h - Intuition/gadtools requesters (private)
 */

#ifndef HOTLINKS_PRIVATE_HL_REQ_H
#define HOTLINKS_PRIVATE_HL_REQ_H

#include "private/hl_internal.h"

int HLReqEnsureLogin(struct HLPrivateBase *pb, struct HLRegistration *reg);
int HLReqLoginUser(struct HLPrivateBase *pb, struct HLRegistration *reg,
    char *nameHint);
int HLReqGetPub(struct HLPrivateBase *pb, struct PubBlock *pblock,
    struct HLPBCtx *ctx, int (*filterproc)(struct PubBlock *));
int HLReqPutPub(struct HLPrivateBase *pb, struct PubBlock *pblock);
int HLReqPubInfo(struct HLPrivateBase *pb, struct PubBlock *pblock,
    struct HLCatalogEntry *ce);
int HLReqNewPassword(struct HLPrivateBase *pb, struct HLRegistration *reg);

#endif /* HOTLINKS_PRIVATE_HL_REQ_H */
