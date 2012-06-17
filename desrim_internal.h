/*
 * Desrim - Fast robust and scalable heap manager, like a shrimp beibeh... like a shrimp...
 *
 * Copyright (c) 2012, stnmrshx (stnmrshx@gmail.com)
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met: 
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those
 * of the authors and should not be interpreted as representing official policies, 
 * either expressed or implied, of the FreeBSD Project.
 * 
 */

#ifndef __DESRIM_INT_H__
#define __DESRIM_INT_H__

#include <stdarg.h>
#include <desrim_queue.h>
#include <desrim_plat.h>

#ifndef NULL
#define NULL	(void *)0
#endif

#define DS_LARGE_CHUNKSZ	1048576
#define DS_LARGE_ALLOC_LIMIT	524288
#define DS_LARGE_GRAINSZ	4096
#define DS_LARGE_SIZECLASS_CNT	\
	((DS_LARGE_ALLOC_LIMIT / DS_LARGE_GRAINSZ) - DS_LARGE_CLASS_OFFSET)
#define DS_SMALL_CHUNKSZ	262144
#define DS_SMALL_ALLOC_LIMIT	131072
#define DS_SMALL_GRAINSZ	8
#define DS_SMALL_SIZECLASS_CNT	(DS_SMALL_ALLOC_LIMIT / DS_SMALL_GRAINSZ)
#define DS_SIZECLASS_CNT	\
	(DS_LARGE_SIZECLASS_CNT + DS_SMALL_SIZECLASS_CNT)
#define DS_OVERSIZE_CHUNKSZ	524288
#define DS_CHUNKT_CHUNKSZ	(100 * sizeof(ds_chunk_t))
#define DS_SIZECLASS_OVERSIZE	-1
#define DS_FULLNESS_GRP_CNT	4
#define DS_FULLNESS_GRP_MAX	(DS_FULLNESS_GRP_CNT - 1)
#define DS_FULLNESS_GRP_EMPTY	-1
#define DS_MAX_FULL_CHUNKS	1
#define DS_CHUNK_FREE_THRESHOLD	50
#define DS_MAX_CHUNKT_CNT	10
#define DS_MAX_CHUNK_CNT	5
#define DS_NUM_HEAPS		8
#define DS_TOTAL_HEAPS		(DS_NUM_HEAPS + 1)
#define DS_GLOBAL_HEAP		DS_NUM_HEAPS
#define DS_HDRSZ		8

#define DS_LARGE_CLASS_OFFSET \
	((DS_SMALL_ALLOC_LIMIT + DS_LARGE_GRAINSZ - 1) / DS_LARGE_GRAINSZ)
#define DS_SIZECLASS(size) \
	(((size) <= DS_SMALL_ALLOC_LIMIT) ? \
	ds_small_sizeclass_map[((size) - 1) / DS_SMALL_GRAINSZ] : \
	ds_large_sizeclass_map[((size) - 1) / DS_LARGE_GRAINSZ - \
	DS_LARGE_CLASS_OFFSET])

#define DS_ARENA_RAWALLOC	0x00000001
#define DS_HEAP_GROWOK		0x00000001
#define DS_HEAP_OBSERVE_LIMIT	0x00000002

typedef struct desrim_sizeclass {
	int sc_chunk_cnt;
	int sc_full_chunk_cnt;
	int sc_empty_chunk_cnt;
	int sc_divisions_per_chunk;
	int sc_division_total_cnt;
	int sc_division_free_cnt;
	int sc_division_free_threshold;
	ds_size_t sc_division_size;
	ds_size_t sc_chunk_size;
	ds_queue_t sc_fullness_group_list[DS_FULLNESS_GRP_CNT];
	ds_queue_t sc_empty_list;
	ds_lock_t sc_lock;
} ds_class_t;


#define DS_CHUNK_MAGIC	0x2984AFC3

typedef struct desrim_chunk {
	int ch_magic;
	short ch_size_class;
	char ch_fullness_grp;
	char ch_owner_heap[DS_TOTAL_HEAPS];
	int ch_division_total_cnt;
	int ch_division_free_cnt;
	int ch_division_list_cnt;
	ds_size_t ch_division_size;
	ds_size_t ch_chunk_size;
	char *ch_ptr;
	char *ch_curptr;
	char *ch_division_list;
	ds_queue_t ch_fullness_q;
} ds_chunk_t;


typedef struct desrim_heap {
	int hp_chunk_hdr_cnt;
	int hp_large_chunk_cnt;
	int hp_small_chunk_cnt;
	ds_queue_t hp_chunk_hdr_list;
	ds_queue_t hp_large_chunk_list;
	ds_queue_t hp_small_chunk_list;
	ds_lock_t hp_chunk_hdr_lock;
	ds_lock_t hp_large_chunk_lock;
	ds_lock_t hp_small_chunk_lock;
	ds_class_t *hp_size_class;
} ds_heap_t;


typedef struct desrim_localmem {
	char *lm_ptr;
	ds_size_t lm_size;
} ds_localmem_t;


typedef struct desrim_classstat {
	int cl_chunk_cnt;
	int cl_full_chunk_cnt;
	int cl_empty_chunk_cnt;
	ds_size_t cl_chunk_size;
	ds_size_t cl_total_heap_mem;
	ds_size_t cl_total_heap_free_mem;
	ds_size_t cl_division_size;
	int cl_division_total_cnt;
	int cl_division_free_cnt;
} ds_classstat_t;


typedef struct desrim_heapstat {
	int hs_chunk_cnt;
	int hs_chunk_hdr_free_cnt;
	int hs_large_chunk_free_cnt;
	int hs_small_chunk_free_cnt;
	int hs_division_total_cnt;
	int hs_division_free_cnt;
	ds_size_t hs_total_heap_mem;
	ds_size_t hs_total_heap_free_mem;
	ds_classstat_t hs_size_class[DS_SIZECLASS_CNT];
} ds_heapstat_t;


typedef struct desrim_stat {
	int cs_chunk_cnt;
	int cs_oversize_chunk_cnt;
	int cs_global_chunk_free_cnt;
	int cs_global_large_chunk_free_cnt;
	int cs_global_small_chunk_free_cnt;
	int cs_global_division_total_cnt;
	int cs_global_division_free_cnt;
	int cs_global_chunk_hdr_free_cnt;
	int cs_total_chunk_free_cnt;
	int cs_total_large_chunk_free_cnt;
	int cs_total_small_chunk_free_cnt;
	int cs_total_division_total_cnt;
	int cs_total_division_free_cnt;
	int cs_total_chunk_hdr_free_cnt;
	ds_size_t cs_max_mem;
	ds_size_t cs_total_mem;
	ds_size_t cs_total_free_mem;
	ds_size_t cs_oversize_mem;
	ds_size_t cs_total_heap_mem;
	ds_size_t cs_total_heap_free_mem;
	ds_size_t cs_small_heap_mem;
	ds_size_t cs_small_heap_free_mem;
	ds_size_t cs_large_heap_mem;
	ds_size_t cs_large_heap_free_mem;
	int cs_heap_cnt;
	int cs_size_class_cnt;
	ds_heapstat_t cs_heap[DS_TOTAL_HEAPS];
} ds_stat_t;


int ds_arena_alloc(ds_size_t, void **, ds_size_t *, int);
int ds_default_heapsel_func(void);
int ds_lm_alloc(ds_localmem_t *, ds_size_t, void **);

void ds_arena_free(ds_size_t, void *);
void ds_lm_init(ds_localmem_t *, ds_size_t, void *);

extern int ds_num_heaps;
extern int (*ds_heapsel_func)(void);
extern void (*ds_err_func)(char *, ...);

#endif /* __DESRIM_INT_H__ */
