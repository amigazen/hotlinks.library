/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * hl_pass.h - HotLinks:Users password file (private)
 */

#ifndef HOTLINKS_PRIVATE_HL_PASS_H
#define HOTLINKS_PRIVATE_HL_PASS_H

#include "private/hl_internal.h"

#define HL_SUPERUSER_NAME  "root"

int HLPassEnsureUsers(struct HLPrivateBase *pb);
int HLPassVerify(struct HLPrivateBase *pb, char *name, char *password);
int HLPassChange(struct HLPrivateBase *pb, ULONG handle, char *name,
    char *oldpwd, char *newpwd);
int HLPassIsSuperUser(struct HLPrivateBase *pb);

#endif /* HOTLINKS_PRIVATE_HL_PASS_H */
