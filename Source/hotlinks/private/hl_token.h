/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * hl_token.h - system-wide HotLinks catalog via utility.library NamedObject
 *
 * Replaces the Soft-Logik resident broker: the shared HLToken is registered
 * with AllocNamedObject() (same idea as datatypes.library and its Token).
 * Every OpenLibrary() participant finds or creates "HotLinks.catalog".
 * The token outlives individual library opens; the last close saves to disk.
 */

#ifndef HOTLINKS_PRIVATE_HL_TOKEN_H
#define HOTLINKS_PRIVATE_HL_TOKEN_H

#include <exec/types.h>
#include <exec/lists.h>
#include <exec/semaphores.h>

struct HLPrivateBase;

#define HL_CATALOG_TOKEN_NAME  "HotLinks.catalog"

struct HLToken
{
    struct SignalSemaphore t_Lock;
    ULONG                t_UseCnt;
    struct MinList         t_Catalog;
    struct MinList         t_Locks;
    ULONG                t_NextSerial;
    BOOL                 t_UserValid;
    BOOL                 t_LoginRequired;
    char                 t_UserName[32];
    char                 t_UserGroup[32];
    char                 t_RootPath[256];
};

int HLTokenAttach(struct HLPrivateBase *pb);
void HLTokenDetach(struct HLPrivateBase *pb);

#endif /* HOTLINKS_PRIVATE_HL_TOKEN_H */
