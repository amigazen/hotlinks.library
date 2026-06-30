/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * hl_debug.h - optional serial debug (define HL_DEBUG in SCOPTIONS)
 */

#ifndef HOTLINKS_PRIVATE_HL_DEBUG_H
#define HOTLINKS_PRIVATE_HL_DEBUG_H

#ifdef HL_DEBUG
#include <proto/dos.h>
extern struct DosLibrary *DOSBase;
#define hlDbgPut(s)  if (DOSBase) PutStr((CONST_STRPTR)(s))
#define hlDbgPutLong(s, v) do { \
    char _hlb[64]; \
    if (DOSBase) { \
        RawDoFmt((CONST_STRPTR)(s), (APTR)&(v), (VOID (*)())PutStr, DOSBase); \
    } \
} while (0)
#else
#define hlDbgPut(s)          ((void)0)
#define hlDbgPutLong(s, v)   ((void)0)
#endif

#endif /* HOTLINKS_PRIVATE_HL_DEBUG_H */
