/*
 * Region-based memory allocation.
 *
 * This code is (slightly!) better than LilltDAP's sillymem region-based
 * allocator, as it allocates an actual pool and draws from that, rather
 * than acting as a linked-list of small chunks.
 */

/*
 *  Copyright 2017, Adriaan de Groot <groot@kde.org>
 *
 *  Redistribution and use is allowed according to the terms of the two-clause BSD license.
 *     https://opensource.org/licenses/BSD-2-Clause
 *     SPDX short identifier: BSD-2-Clause
 */

#ifndef LEAF_REGION_H
#define LEAF_REGION_H

#include <lillydap/mem.h>

#ifdef __cplusplus
extern "C" {
#endif

LillyPool leaf_newpool (void);
void leaf_endpool (LillyPool cango);
void *leaf_alloc (LillyPool pool, size_t szbytes);

#ifdef __cplusplus
}
#endif

#endif
