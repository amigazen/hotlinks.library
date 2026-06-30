/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * hl_bases.h - global library bases for #pragma libcall (include before proto/*.h)
 */

#ifndef HOTLINKS_PRIVATE_HL_BASES_H
#define HOTLINKS_PRIVATE_HL_BASES_H

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <dos/dosextens.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>

extern struct HotLinksBase *HotLinksBase;
extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
extern struct Library *UtilityBase;
extern struct IntuitionBase *IntuitionBase;
extern struct GfxBase *GfxBase;
extern struct Library *GadToolsBase;

#endif /* HOTLINKS_PRIVATE_HL_BASES_H */
