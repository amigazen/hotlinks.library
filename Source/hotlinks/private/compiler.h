/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * private/compiler.h - SAS/C and NDK compatibility (not installed)
 */

#ifndef HOTLINKS_PRIVATE_COMPILER_H
#define HOTLINKS_PRIVATE_COMPILER_H

#include <exec/types.h>
#include <clib/compiler-specific.h>

#ifndef HL_INITTABLE_DEFINED
#define HL_INITTABLE_DEFINED 1
struct InitTable
{
    ULONG it_LibSize;
    APTR *it_FuncTable;
    APTR  it_DataTable;
    APTR  it_InitFunc;
};
#endif

struct MyDataInit
{
    ULONG md_Init[19];
};

#endif /* HOTLINKS_PRIVATE_COMPILER_H */
