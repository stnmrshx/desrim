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

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <desrim.h>

ds_heap_t ds_heap[DS_TOTAL_HEAPS];
ds_lock_t ds_count_lock;
ds_lock_t ds_chunk_hdr_lock;
ds_lock_t ds_large_chunk_lock;
ds_lock_t ds_small_chunk_lock;

ds_localmem_t ds_chunk_hdr_mem;
ds_size_t ds_max_mem;
ds_size_t ds_total_mem;
ds_size_t ds_oversize_mem;

int ds_num_heaps = DS_NUM_HEAPS;
int ds_initialized;
int ds_oversize_chunk_cnt;
int ds_sizeclass_cnt;
int ds_large_sizeclass_cnt;
int ds_small_sizeclass_cnt;
int ds_large_sizeclass_map[DS_LARGE_SIZECLASS_CNT];
int ds_small_sizeclass_map[DS_SMALL_SIZECLASS_CNT];

int (*ds_heapsel_func)(void) = ds_default_heapsel_func;
void (*ds_err_func)(char *fmt, ...);

int ds_alloc(ds_size_t, void **);
int ds_alloc_chunk_hdr_list(ds_heap_t *, ds_chunk_t **);
int ds_alloc_global_chunk(int, int, ds_chunk_t **);
int ds_alloc_spare_chunk(int, int, ds_chunk_t **);
int ds_alloc_spare_chunk_heap(int, int, ds_chunk_t **);
int ds_free_spare_chunk_heap(int, ds_chunk_t *);
int ds_heap_alloc(int, int, void **, int);
int ds_init(void);

void ds_prevbucket(int, int *);
void ds_free_chunk_hdr(int, ds_chunk_t *);
void ds_free_spare_chunk(int, ds_chunk_t *);
void ds_free_global_chunk(ds_chunk_t *);

void *
ds_malloc(ds_size_t size)
{
	int ret;
	void *p;
	if(size == 0) {
		errno = EINVAL;
		return NULL;
	}
	ret = ds_alloc(size, &p);
	if(ret != 0) {
		errno = ret;
		return NULL;
	}
	return p;
}

void *
ds_calloc(ds_size_t nmemb, ds_size_t size)
{
	int ret;
	void *p;
	ds_size_t realsize;
	if((nmemb == 0) || (size == 0))
		return NULL;
	realsize = nmemb * size;
	ret = ds_alloc(realsize, &p);
	if(ret != 0) {
		errno = ret;
		return NULL;
	}
	memset(p, 0, realsize);
	return p;
}

void *
ds_realloc(void *ptr, ds_size_t size)
{
	int ret;
	void *p;
	ds_chunk_t *ch;
	ds_size_t offset;
	ds_size_t realsize;

	if(size == 0) {
		errno = EINVAL;
		return NULL;
	}

	if(ptr == NULL)
		return ds_malloc(size);

	ch = *(ds_chunk_t **)((char *)ptr - DS_HDRSZ);

	if(ch->ch_magic != DS_CHUNK_MAGIC) {
		ds_err_func("ds_realloc: dafuq bad chunk magic number 0x%x != 0x%x\n",
		    ch->ch_magic, DS_CHUNK_MAGIC);
		errno = EINVAL;
		return NULL;
	}

	offset = (ds_size_t)((char *)ptr - ch->ch_ptr) % ch->ch_division_size;
	realsize = ch->ch_division_size - offset;

	if(size <= realsize)
		return ptr;

	if((offset > DS_HDRSZ) && ((size + DS_HDRSZ) <= ch->ch_division_size)) {
		memmove(((char *)ptr - offset), (const void *)((char *)ptr - DS_HDRSZ), (realsize + DS_HDRSZ));
		return((char *)ptr - offset);
	}
	ret = ds_alloc(size, &p);
	if(ret != 0) {
		errno = ret;
		return NULL;
	}

	memcpy(p, ptr, realsize);
	ds_free(ptr);
	return p;
}

void *
ds_memalign(ds_size_t alignment, ds_size_t size)
{
	int ret;
	char *p;
	ds_size_t shift;

	if((size == 0) || (alignment == 0) || (alignment % DS_HDRSZ))
		return NULL;
	size += alignment - 1;
	ret = ds_alloc(size, (void **)&p);
	if(ret != 0) {
		errno = ret;
		return NULL;
	}

	if((ds_size_t)p % alignment) {
		shift = alignment - ((ds_size_t)p % alignment);
		memmove(*(void **)(p + shift - DS_HDRSZ),
		    *(ds_chunk_t **)(p - DS_HDRSZ), DS_HDRSZ);
		p += shift;
	}
	return((void *)p);
}

void *
ds_valloc(ds_size_t size)
{
	return ds_memalign(sysconf(_SC_PAGESIZE), size);
}

void ds_free(void *ptr)
{
	int i;
	int lockcnt;
	int heapnum;
	ds_chunk_t *ch;
	ds_class_t *sc;
	ds_lock_t *locks[DS_TOTAL_HEAPS];

	if(ptr == 0)
		return;

	ch = *(ds_chunk_t **)((char *)ptr - DS_HDRSZ);

	if(ch->ch_magic != DS_CHUNK_MAGIC) {
		ds_err_func("ds_free: dafuq bad free magic number 0x%x != 0x%x\n",
		    ch->ch_magic, DS_CHUNK_MAGIC);
		return;
	}

	if(ch->ch_size_class == DS_SIZECLASS_OVERSIZE) {
		ds_lock(&ds_count_lock);
		ds_total_mem -= ch->ch_chunk_size;
		ds_oversize_mem -= ch->ch_chunk_size;
		ds_oversize_chunk_cnt--;
		ds_unlock(&ds_count_lock);
		ds_arena_free(ch->ch_chunk_size, ch->ch_ptr);
		ds_free_chunk_hdr(DS_GLOBAL_HEAP, ch);
		return;
	}

	ptr = (void *)((ds_size_t)ptr - ((ds_size_t)((char *)ptr - ch->ch_ptr) % ch->ch_division_size));
	
	for(i = 0; i < DS_TOTAL_HEAPS; i++) {
		if(ch->ch_owner_heap[i]) {
			sc = &ds_heap[i].hp_size_class[ch->ch_size_class];
			ds_lock(&sc->sc_lock);

			if(ch->ch_owner_heap[i]) {
				locks[0] = &sc->sc_lock;
				lockcnt = 1;
				break;
			}
			ds_unlock(&sc->sc_lock);
		}
	}

	if(i >= DS_TOTAL_HEAPS) {
		for(i = 0; i < DS_TOTAL_HEAPS; i++) {
			sc = &ds_heap[i].hp_size_class[ch->ch_size_class];
			locks[i] = &sc->sc_lock;
			ds_lock(&sc->sc_lock);
			if(ch->ch_owner_heap[i])
				break;
		}

		if(i >= DS_TOTAL_HEAPS) {
			if(ds_err_func != NULL) {
				ds_err_func(
				    "ds_free: heap owner not found for this chunk "
				    "0x%lx 0x%lx\n", ch, ptr);
			}
			return;
		}
		lockcnt = i + 1;
	}

	heapnum = i;
	
	if(ch->ch_division_free_cnt >= ch->ch_division_total_cnt) {
		ds_unlock(&sc->sc_lock);
		if(ds_err_func != NULL) {
			ds_err_func(
				"ds_free: freeing pointer to fullfill the chunk: "
			    "0x%lx\n", ptr);
		}
		return;
	}

	*(char **)ptr = ch->ch_division_list;
	ch->ch_division_list = (char *)ptr;
	ch->ch_division_list_cnt++;
	ch->ch_division_free_cnt++;
	sc->sc_division_free_cnt++;
	
	ds_dq(&ch->ch_fullness_q);

	switch(ch->ch_fullness_grp) {
	case DS_FULLNESS_GRP_EMPTY:
		sc->sc_empty_chunk_cnt--;
		break;

	case DS_FULLNESS_GRP_MAX:
		sc->sc_full_chunk_cnt--;
		break;
	}

	if(ch->ch_division_free_cnt >= ch->ch_division_total_cnt) {
		ch->ch_fullness_grp = DS_FULLNESS_GRP_MAX;
	}
	else {
		ch->ch_fullness_grp = DS_FULLNESS_GRP_CNT *
		    ch->ch_division_free_cnt / ch->ch_division_total_cnt;
	}

	if(ch->ch_division_free_cnt >= ch->ch_division_total_cnt) {
		sc->sc_division_total_cnt -= sc->sc_divisions_per_chunk;
		sc->sc_division_free_cnt -= ch->ch_division_free_cnt;
		sc->sc_chunk_cnt--;
		ch->ch_owner_heap[heapnum] = 0;
		ds_free_spare_chunk(heapnum, ch);
	}
	else if((heapnum != DS_GLOBAL_HEAP) && (ch->ch_fullness_grp == DS_FULLNESS_GRP_MAX) && (((sc->sc_full_chunk_cnt + 1) > DS_MAX_FULL_CHUNKS) || (sc->sc_division_free_cnt > (ch->ch_division_free_cnt + sc->sc_division_free_threshold)))) {
		sc->sc_division_total_cnt -= sc->sc_divisions_per_chunk;
		sc->sc_division_free_cnt -= ch->ch_division_free_cnt;
		sc->sc_chunk_cnt--;
		ch->ch_owner_heap[heapnum] = 0;
		ds_free_global_chunk(ch);
	}
	else {
		ds_nq_front(&sc->sc_fullness_group_list[(int)ch->ch_fullness_grp], &ch->ch_fullness_q);
		if(ch->ch_fullness_grp == DS_FULLNESS_GRP_MAX)
			sc->sc_full_chunk_cnt++;
	}
	
	for(i = lockcnt - 1; i >= 0; i--)
		ds_unlock(locks[i]);
}

int ds_malloc_size(void *ptr, ds_size_t *realsize)
{
	ds_chunk_t *ch;
	ds_size_t offset;
	ch = *(ds_chunk_t **)((char *)ptr - DS_HDRSZ);

	if(ch->ch_magic != DS_CHUNK_MAGIC) {
		ds_err_func(
		    "ds_malloc_size: dafuq bad chunk magic number 0x%x != 0x%x\n",
		    ch->ch_magic, DS_CHUNK_MAGIC);
		errno = EINVAL;
		return EINVAL;
	}

	offset = (ds_size_t)((char *)ptr - ch->ch_ptr) % ch->ch_division_size;
	*realsize = ch->ch_division_size - offset;
	return 0;
}

int ds_set_heapsel_func(int (*func)(void))
{
	ds_heapsel_func = func;
	return 0;
}

int ds_set_num_heaps(int num_heaps)
{
	if((num_heaps > DS_NUM_HEAPS) || (num_heaps < 1))
		ds_num_heaps = DS_NUM_HEAPS;
	else
		ds_num_heaps = num_heaps;
	return 0;
}

int ds_set_err_func(void (*func)(char *, ...))
{
	ds_err_func = func;
	return 0;
}

int ds_set_max_mem(ds_size_t size)
{
	ds_lock(&ds_count_lock);
	ds_max_mem = size;
	ds_unlock(&ds_count_lock);
	return 0;
}

void ds_get_total_mem(ds_size_t *total_mem)
{
	ds_lock(&ds_count_lock);
	*total_mem = ds_total_mem;
	ds_unlock(&ds_count_lock);
}

int ds_init(void)
{
	int i;
	int j;
	int k;
	int ret;
	int classcnt;
	int divcnt;
	int next_divcnt;
	ds_heap_t *hp;
	ds_class_t *sc;
	ds_class_t *tsc;
	ds_size_t divsz;
	ds_size_t next_divsz;

	DS_INIT_LOCK;

	if(ds_initialized) {
		DS_INIT_UNLOCK;
		return 0;
	}

	ds_plat_init();
	ds_lm_init(&ds_chunk_hdr_mem, 0, 0);
	ds_lock_init(&ds_count_lock);

	classcnt = 0;
	next_divcnt = 0;
	divsz = DS_SMALL_GRAINSZ;
	divcnt = DS_SMALL_CHUNKSZ / divsz;

	for(i = 0; i < DS_SMALL_SIZECLASS_CNT; i++) {
		next_divsz = DS_SMALL_GRAINSZ * (i + 2);
		next_divcnt = DS_SMALL_CHUNKSZ / next_divsz;

		if(next_divcnt != divcnt)
			classcnt++;

		divsz = next_divsz;
		divcnt = next_divcnt;
	}

	next_divcnt = 0;
	divsz = DS_LARGE_GRAINSZ * (1 + DS_LARGE_CLASS_OFFSET);
	divcnt = DS_LARGE_CHUNKSZ / divsz;

	for(i = 0; i < DS_LARGE_SIZECLASS_CNT; i++) {
		next_divsz = DS_LARGE_GRAINSZ * (i + 2 + DS_LARGE_CLASS_OFFSET);
		next_divcnt = DS_LARGE_CHUNKSZ / next_divsz;
		if(next_divcnt != divcnt)
			classcnt++;
		divsz = next_divsz;
		divcnt = next_divcnt;
	}
	ret = ds_arena_alloc((classcnt * sizeof(ds_class_t) * DS_TOTAL_HEAPS), (void **)&tsc, 0, DS_ARENA_RAWALLOC);
	if(ret != 0)
		return ret;

	for(i = 0; i < DS_TOTAL_HEAPS; i++) {
		ds_heap[i].hp_size_class = tsc;
		tsc += classcnt;
	}

	next_divcnt = 0;
	divsz = DS_SMALL_GRAINSZ;
	divcnt = DS_SMALL_CHUNKSZ / divsz;

	for(i = 0; i < DS_SMALL_SIZECLASS_CNT; i++) {
		next_divsz = DS_SMALL_GRAINSZ * (i + 2);
		next_divcnt = DS_SMALL_CHUNKSZ / next_divsz;
		ds_small_sizeclass_map[i] = ds_sizeclass_cnt;
		if(next_divcnt != divcnt) {
			for(j = 0; j < DS_TOTAL_HEAPS; j++) {
				sc = 
					&ds_heap[j].hp_size_class[ds_sizeclass_cnt];
				sc->sc_chunk_size = DS_SMALL_CHUNKSZ;
				sc->sc_division_size = divsz;
				sc->sc_divisions_per_chunk = divcnt;
				sc->sc_division_free_threshold =
				    (sc->sc_divisions_per_chunk *
				    DS_CHUNK_FREE_THRESHOLD) / 100;
				if(sc->sc_division_free_threshold <= 0)
					sc->sc_division_free_threshold = 1;
			}

			ds_sizeclass_cnt++;
			ds_small_sizeclass_cnt++;
		}
		divsz = next_divsz;
		divcnt = next_divcnt;
	}

	next_divcnt = 0;
	divsz = DS_LARGE_GRAINSZ * (1 + DS_LARGE_CLASS_OFFSET);
	divcnt = DS_LARGE_CHUNKSZ / divsz;

	for(i = 0; i < DS_LARGE_SIZECLASS_CNT; i++) {
		next_divsz = DS_LARGE_GRAINSZ * (i + 2 + DS_LARGE_CLASS_OFFSET);
		next_divcnt = DS_LARGE_CHUNKSZ / next_divsz;
		ds_large_sizeclass_map[i] = ds_sizeclass_cnt;
		if(next_divcnt != divcnt) {
			for(j = 0; j < DS_TOTAL_HEAPS; j++) {
				sc =
				    &ds_heap[j].hp_size_class[ds_sizeclass_cnt];
				sc->sc_chunk_size = DS_LARGE_CHUNKSZ;
				sc->sc_division_size = divsz;
				sc->sc_divisions_per_chunk = divcnt;
				sc->sc_division_free_threshold =
				    (sc->sc_divisions_per_chunk * DS_CHUNK_FREE_THRESHOLD) / 100;
				if(sc->sc_division_free_threshold <= 0)
					sc->sc_division_free_threshold = 1;
			}
			ds_sizeclass_cnt++;
			ds_large_sizeclass_cnt++;
		}
		divsz = next_divsz;
		divcnt = next_divcnt;
	}

	for(i = 0; i < DS_TOTAL_HEAPS; i++) {
		hp = &ds_heap[i];
		ds_lock_init(&hp->hp_chunk_hdr_lock);
		ds_lock_init(&hp->hp_large_chunk_lock);
		ds_lock_init(&hp->hp_small_chunk_lock);
		ds_initq(&hp->hp_chunk_hdr_list, hp);
		ds_initq(&hp->hp_large_chunk_list, hp);
		ds_initq(&hp->hp_small_chunk_list, hp);
		
		for(j = 0; j < ds_sizeclass_cnt; j++) {
			sc = &hp->hp_size_class[j];
			ds_lock_init(&sc->sc_lock);
			ds_initq(&sc->sc_empty_list, sc);

			for(k = 0; k < DS_FULLNESS_GRP_CNT; k++)
				ds_initq(&sc->sc_fullness_group_list[k], sc);
		}
	}
	ds_initialized++;
	DS_INIT_UNLOCK;
	return 0;
}

int ds_alloc_chunk_hdr(int heapnum, ds_chunk_t **uch)
{
	int ret;
	char *p;
	ds_heap_t *hp;
	ds_chunk_t *ch;
	ds_size_t realsize;
	hp = &ds_heap[heapnum];
	ret = ds_alloc_chunk_hdr_list(hp, uch);
	if(ret == 0)
		return ret;
	if(heapnum != DS_GLOBAL_HEAP) {
		hp = &ds_heap[DS_GLOBAL_HEAP];
		ret = ds_alloc_chunk_hdr_list(hp, uch);
		if(ret == 0)
			return ret;
	}
	ds_lock(&ds_chunk_hdr_lock);
	ret = ds_lm_alloc(&ds_chunk_hdr_mem, sizeof(ds_chunk_t), (void **)&ch);
	if(ret != 0) {
		ret = ds_arena_alloc(DS_CHUNKT_CHUNKSZ, (void **)&p,
		    &realsize, 0);

		if(ret == 0) {
			ds_lock(&ds_count_lock);
			ds_total_mem += realsize;
			ds_unlock(&ds_count_lock);

			ds_lm_init(&ds_chunk_hdr_mem, realsize, p);
			(void)ds_lm_alloc(&ds_chunk_hdr_mem,
			    sizeof(ds_chunk_t), (void **)&ch);
		}
	}

	ds_unlock(&ds_chunk_hdr_lock);

	if(ret == 0) {
		ds_set_assocq(&ch->ch_fullness_q, ch);
		memset(ch->ch_owner_heap, 0, sizeof(ch->ch_owner_heap));
		ch->ch_magic = DS_CHUNK_MAGIC;
		*uch = ch;
	}

	return ret;
}


void ds_free_chunk_hdr(int heapnum, ds_chunk_t *ch)
{
	ds_heap_t *hp;
	ds_heap_t *ghp;

	ch->ch_magic = 0;
	hp = &ds_heap[heapnum];
	ds_lock(&hp->hp_chunk_hdr_lock);
	if((heapnum != DS_GLOBAL_HEAP) && (hp->hp_chunk_hdr_cnt >= DS_MAX_CHUNKT_CNT)) {
		ghp = &ds_heap[DS_GLOBAL_HEAP];
		ds_lock(&ghp->hp_chunk_hdr_lock);
		ds_nq_front(&ghp->hp_chunk_hdr_list, &ch->ch_fullness_q);
		ghp->hp_chunk_hdr_cnt++;
		ds_unlock(&ghp->hp_chunk_hdr_lock);
	}
	else {
		ds_nq_front(&hp->hp_chunk_hdr_list, &ch->ch_fullness_q);
		hp->hp_chunk_hdr_cnt++;
	}
	ds_unlock(&hp->hp_chunk_hdr_lock);
}


int ds_alloc_chunk_hdr_list(ds_heap_t *hp, ds_chunk_t **uch)
{
	ds_queue_t *iq;
	ds_lock(&hp->hp_chunk_hdr_lock);

	if(hp->hp_chunk_hdr_cnt <= 0) {
		ds_unlock(&hp->hp_chunk_hdr_lock);
		return ENOMEM;
	}
	iq = ds_nextq(&hp->hp_chunk_hdr_list);
	if(iq == &hp->hp_chunk_hdr_list) {
		ds_unlock(&hp->hp_chunk_hdr_lock);
		return ENOMEM;
	}
	ds_dq(iq);
	hp->hp_chunk_hdr_cnt--;
	ds_unlock(&hp->hp_chunk_hdr_lock);
	*uch = (ds_chunk_t *)ds_get_assocq(iq);
	(*uch)->ch_magic = DS_CHUNK_MAGIC;
	return 0;
}


int ds_alloc(ds_size_t size, void **ptr)
{
	int i;
	int j;
	int ret;
	int heapnum;
	int sizeclass;
	int max_sizeclass;
	ds_chunk_t *ch;

	if(!ds_initialized) {
		ret = ds_init();
		if(ret != 0)
			return ret;
	}
	heapnum = ds_heapsel_func();
	size += DS_HDRSZ;

	if(size > DS_LARGE_ALLOC_LIMIT) {
		ret = ds_alloc_chunk_hdr(heapnum, &ch);
		if(ret != 0)
			return ret;

		ret = ds_arena_alloc(size, (void **)&ch->ch_ptr, &ch->ch_chunk_size, DS_ARENA_RAWALLOC);

		if(ret != 0) {
			ds_free_chunk_hdr(heapnum, ch);
			return ret;
		}

		ds_lock(&ds_count_lock);
		ds_total_mem += ch->ch_chunk_size;
		ds_oversize_mem += ch->ch_chunk_size;
		ds_oversize_chunk_cnt++;
		ds_unlock(&ds_count_lock);
		ch->ch_division_size = ch->ch_chunk_size;
		ch->ch_size_class = DS_SIZECLASS_OVERSIZE;

		*(ds_chunk_t **)ch->ch_ptr = ch;
		*ptr = ch->ch_ptr + DS_HDRSZ;

		return 0;
	}
	sizeclass = DS_SIZECLASS(size);
	ret = ds_heap_alloc(heapnum, sizeclass, ptr, (DS_HEAP_GROWOK | DS_HEAP_OBSERVE_LIMIT));

	if(ret == 0)
		return ret;

	for(i = 0; i < DS_NUM_HEAPS; i++) {
		if(i != heapnum) {
			ret = ds_heap_alloc(i, sizeclass, ptr, 0);
			if(ret == 0)
				return ret;
		}
	}

	if(sizeclass < ds_small_sizeclass_cnt)
		max_sizeclass = ds_small_sizeclass_cnt;
	else
		max_sizeclass = ds_sizeclass_cnt;

	for(i = sizeclass + 1; i < max_sizeclass; i++) {
		for(j = 0; j < DS_NUM_HEAPS; j++) {
			if(j != heapnum) {
				ret = ds_heap_alloc(j, i, ptr, 0);
				if(ret == 0)
					return ret;
			}
		}
	}
	ds_lock(&ds_count_lock);
	if(ds_max_mem > 0) {
		ds_unlock(&ds_count_lock);
		ret = ds_heap_alloc(heapnum, sizeclass, ptr, DS_HEAP_GROWOK);
		if(ret == 0)
			return ret;
	}
	else {
		ds_unlock(&ds_count_lock);
	}
	return ENOMEM;
}


int ds_heap_alloc(int heapnum, int sizeclass, void **ptr, int flags)
{
	int i;
	int ret;
	ds_class_t *sc;
	ds_chunk_t *ch;
	ds_queue_t *iq;
	ds_queue_t *head;

	sc = &ds_heap[heapnum].hp_size_class[sizeclass];
	ds_lock(&sc->sc_lock);

	if(sc->sc_division_free_cnt > 0) {
		for(i = 0; i < DS_FULLNESS_GRP_CNT; i++) {
			head = &sc->sc_fullness_group_list[i];
			iq = ds_nextq(head);
			if(iq != head) {
				ch = (ds_chunk_t *)ds_get_assocq(iq);
				break;
			}
		}
		ds_dq(iq);
		if(ch->ch_fullness_grp == DS_FULLNESS_GRP_MAX)
			sc->sc_full_chunk_cnt--;
	}
	else {
		if(!(flags & DS_HEAP_GROWOK)) {
			ds_unlock(&sc->sc_lock);
			return ENOMEM;
		}

		ret = ds_alloc_global_chunk(heapnum, sizeclass, &ch);
		if(ret != 0) {
			ret = ds_alloc_spare_chunk(heapnum, sizeclass, &ch);
			if(ret != 0) {
				ds_lock(&ds_count_lock);

				if((flags & DS_HEAP_OBSERVE_LIMIT) && (ds_max_mem > 0) && (ds_total_mem >= ds_max_mem)) {
					ds_unlock(&ds_count_lock);
					ds_unlock(&sc->sc_lock);
					return ENOMEM;
				}
				ds_unlock(&ds_count_lock);
				ret = ds_alloc_chunk_hdr(heapnum, &ch);
				if(ret != 0) {
					ds_unlock(&sc->sc_lock);
					return ret;
				}
				ret = ds_arena_alloc(sc->sc_chunk_size, (void **)&ch->ch_ptr, &ch->ch_chunk_size, 0);
				if(ret != 0) {
					ds_free_chunk_hdr(heapnum, ch);
					ds_unlock(&sc->sc_lock);
					return ret;
				}
				ds_lock(&ds_count_lock);
				ds_total_mem += ch->ch_chunk_size;
				ds_unlock(&ds_count_lock);
				ch->ch_size_class = sizeclass;
			}
			ch->ch_curptr = ch->ch_ptr;
			ch->ch_division_free_cnt = sc->sc_divisions_per_chunk;
			ch->ch_division_total_cnt = sc->sc_divisions_per_chunk;
			ch->ch_division_list_cnt = 0;
			ch->ch_division_size = sc->sc_division_size;
			ch->ch_size_class = sizeclass;
			ch->ch_fullness_grp = DS_FULLNESS_GRP_MAX;
		}
		ch->ch_owner_heap[heapnum] = 1;
		sc->sc_chunk_cnt++;
		sc->sc_division_total_cnt += sc->sc_divisions_per_chunk;
		sc->sc_division_free_cnt += ch->ch_division_free_cnt;
	}
	if(ch->ch_division_list_cnt > 0) {
		*ptr = ch->ch_division_list;
		ch->ch_division_list = *(char **)ch->ch_division_list;
		ch->ch_division_list_cnt--;
	}
	else {
		*ptr = ch->ch_curptr;
		ch->ch_curptr += sc->sc_division_size;
	}

	*(ds_chunk_t **)*ptr = ch;
	*(char **)ptr += DS_HDRSZ;

	ch->ch_division_free_cnt--;
	sc->sc_division_free_cnt--;

	if(ch->ch_division_free_cnt <= 0) {
		ch->ch_fullness_grp = DS_FULLNESS_GRP_EMPTY;
		sc->sc_empty_chunk_cnt++;
		ds_nq_front(&sc->sc_empty_list, &ch->ch_fullness_q);
	}
	else {
		ch->ch_fullness_grp = DS_FULLNESS_GRP_CNT *
		    ch->ch_division_free_cnt / ch->ch_division_total_cnt;

		if(ch->ch_fullness_grp == DS_FULLNESS_GRP_MAX)
			sc->sc_full_chunk_cnt++;

		ds_nq_front(
		    &sc->sc_fullness_group_list[(int)ch->ch_fullness_grp],
		    &ch->ch_fullness_q);
	}
	ds_unlock(&sc->sc_lock);
	return 0;
}


int ds_alloc_global_chunk(int heapnum, int sizeclass, ds_chunk_t **uch)
{
	ds_class_t *sc;
	ds_chunk_t *ch;
	ds_queue_t *iq;
	ds_queue_t *head;

	sc = &ds_heap[DS_GLOBAL_HEAP].hp_size_class[sizeclass];
	head = &sc->sc_fullness_group_list[DS_FULLNESS_GRP_MAX];

	ds_lock(&sc->sc_lock);
	iq = ds_nextq(head);

	if(iq == head) {
		ds_unlock(&sc->sc_lock);
		return ENOMEM;
	}
	ds_dq(iq);
	ch = (ds_chunk_t *)ds_get_assocq(iq);
	ch->ch_owner_heap[DS_GLOBAL_HEAP] = 0;

	sc->sc_chunk_cnt--;
	sc->sc_full_chunk_cnt--;
	sc->sc_division_total_cnt -= sc->sc_divisions_per_chunk;
	sc->sc_division_free_cnt -= ch->ch_division_free_cnt;
	ds_unlock(&sc->sc_lock);
	*uch = ch;
	return 0;
}


void ds_free_global_chunk(ds_chunk_t *ch)
{
	ds_class_t *sc;
	sc = &ds_heap[DS_GLOBAL_HEAP].hp_size_class[ch->ch_size_class];
	ds_lock(&sc->sc_lock);
	sc->sc_chunk_cnt++;
	sc->sc_full_chunk_cnt++;
	sc->sc_division_total_cnt += sc->sc_divisions_per_chunk;
	sc->sc_division_free_cnt += ch->ch_division_free_cnt;
	ch->ch_owner_heap[DS_GLOBAL_HEAP] = 1;
	ds_nq_front(&sc->sc_fullness_group_list[DS_FULLNESS_GRP_MAX], &ch->ch_fullness_q);
	ds_unlock(&sc->sc_lock);
}


int ds_alloc_spare_chunk(int heapnum, int sizeclass, ds_chunk_t **uch)
{
	int ret;
	ret = ds_alloc_spare_chunk_heap(heapnum, sizeclass, uch);
	if(ret == 0)
		return 0;
	ret = ds_alloc_spare_chunk_heap(DS_GLOBAL_HEAP, sizeclass, uch);
	return ret;
}


int ds_alloc_spare_chunk_heap(int heapnum, int sizeclass, ds_chunk_t **uch)
{
	ds_lock_t *lp;
	ds_heap_t *hp;
	ds_queue_t *iq;
	ds_queue_t *head;

	hp = &ds_heap[heapnum];

	if((sizeclass < ds_small_sizeclass_cnt) && (DS_LARGE_CHUNKSZ != DS_SMALL_CHUNKSZ)) {
		lp = &hp->hp_small_chunk_lock;
		ds_lock(lp);
		if(hp->hp_small_chunk_cnt <= 0) {
			ds_unlock(lp);
			return ENOMEM;
		}
		head = &hp->hp_small_chunk_list;
		hp->hp_small_chunk_cnt--;
	} else {
		lp = &hp->hp_large_chunk_lock;
		ds_lock(lp);

		if(hp->hp_large_chunk_cnt <= 0) {
			ds_unlock(lp);
			return ENOMEM;
		}
		head = &hp->hp_large_chunk_list;
		hp->hp_large_chunk_cnt--;
	}
	iq = ds_nextq(head);
	ds_dq(iq);
	*uch = (ds_chunk_t *)ds_get_assocq(iq);
	ds_unlock(lp);
	return 0;
}

void ds_free_spare_chunk(int heapnum, ds_chunk_t *ch)
{
	int ret;
	ret = ds_free_spare_chunk_heap(heapnum, ch);
	if(ret == 0)
		return;
	(void)ds_free_spare_chunk_heap(DS_GLOBAL_HEAP, ch);
}

int ds_free_spare_chunk_heap(int heapnum, ds_chunk_t *ch)
{
	int *cnt;
	ds_lock_t *lp;
	ds_heap_t *hp;
	ds_queue_t *head;
	hp = &ds_heap[heapnum];
	if((ch->ch_size_class < ds_small_sizeclass_cnt) && (DS_LARGE_CHUNKSZ != DS_SMALL_CHUNKSZ)) {
		cnt = &hp->hp_small_chunk_cnt;
		head = &hp->hp_small_chunk_list;
		lp = &hp->hp_small_chunk_lock;
		ds_lock(lp);
	} else {
		cnt = &hp->hp_large_chunk_cnt;
		head = &hp->hp_large_chunk_list;
		lp = &hp->hp_large_chunk_lock;
		ds_lock(lp);
	}

	if((heapnum != DS_GLOBAL_HEAP) && (*cnt >= DS_MAX_CHUNK_CNT)) {
		ds_unlock(lp);
		return EFBIG;
	}
	(*cnt)++;
	ds_nq_front(head, &ch->ch_fullness_q);
	ds_unlock(lp);
	return 0;
}

int ds_lm_alloc(ds_localmem_t *lm, ds_size_t size, void **ptr)
{
	if(size > lm->lm_size)
		return ENOMEM;
	*ptr = lm->lm_ptr;
	lm->lm_ptr += size;
	lm->lm_size -= size;
	return 0;
}


void ds_lm_init(ds_localmem_t *lm, ds_size_t size, void *ptr)
{
	lm->lm_ptr = ptr;
	lm->lm_size = size;
}

void ds_get_stats(ds_stat_t *cstat)
{
	int i;
	int j;
	int n;
	ds_size_t s;
	ds_heap_t *hp;
	memset(cstat, 0, sizeof(ds_stat_t));
	for(i = 0; i < ds_sizeclass_cnt; i++)
		for(j = 0; j < DS_TOTAL_HEAPS; j++)
			ds_lock(&ds_heap[j].hp_size_class[i].sc_lock);

	for(i = 0; i < DS_TOTAL_HEAPS; i++) {
		hp = &ds_heap[i];
		ds_lock(&hp->hp_small_chunk_lock);
		ds_lock(&hp->hp_large_chunk_lock);
	}
	cstat->cs_heap_cnt = DS_TOTAL_HEAPS;
	cstat->cs_size_class_cnt = ds_sizeclass_cnt;
	cstat->cs_global_small_chunk_free_cnt =
	    ds_heap[DS_GLOBAL_HEAP].hp_small_chunk_cnt;
	cstat->cs_global_large_chunk_free_cnt =
	    ds_heap[DS_GLOBAL_HEAP].hp_large_chunk_cnt;
	cstat->cs_global_chunk_free_cnt =
	    ds_heap[DS_GLOBAL_HEAP].hp_small_chunk_cnt +
	    ds_heap[DS_GLOBAL_HEAP].hp_large_chunk_cnt;
	cstat->cs_global_chunk_hdr_free_cnt =
	    ds_heap[DS_GLOBAL_HEAP].hp_chunk_hdr_cnt;
	ds_lock(&ds_count_lock);
	cstat->cs_max_mem = ds_max_mem;
	cstat->cs_total_mem = ds_total_mem;
	cstat->cs_oversize_mem = ds_oversize_mem;
	cstat->cs_oversize_chunk_cnt = ds_oversize_chunk_cnt;
	ds_unlock(&ds_count_lock);

	for(i = 0; i < DS_TOTAL_HEAPS; i++) {
		hp = &ds_heap[i];
		cstat->cs_total_small_chunk_free_cnt +=
		    ds_heap[i].hp_small_chunk_cnt;
		cstat->cs_total_large_chunk_free_cnt +=
		    ds_heap[i].hp_large_chunk_cnt;
		cstat->cs_total_chunk_free_cnt +=
		    ds_heap[i].hp_small_chunk_cnt +
		    ds_heap[i].hp_large_chunk_cnt;
		cstat->cs_total_chunk_hdr_free_cnt +=
		    ds_heap[i].hp_chunk_hdr_cnt;
		cstat->cs_heap[i].hs_chunk_hdr_free_cnt =
		    ds_heap[i].hp_chunk_hdr_cnt;
		cstat->cs_heap[i].hs_small_chunk_free_cnt =
		    ds_heap[i].hp_small_chunk_cnt;
		cstat->cs_heap[i].hs_large_chunk_free_cnt =
		    ds_heap[i].hp_large_chunk_cnt;
		for(j = 0; j < ds_sizeclass_cnt; j++) {
			cstat->cs_heap[i].hs_size_class[j].cl_full_chunk_cnt =
			    hp->hp_size_class[j].sc_full_chunk_cnt;
			cstat->cs_heap[i].hs_size_class[j].cl_empty_chunk_cnt =
			    hp->hp_size_class[j].sc_empty_chunk_cnt;
			s = hp->hp_size_class[j].sc_chunk_size;
			cstat->cs_heap[i].hs_size_class[j].cl_chunk_size = s;
			n = hp->hp_size_class[j].sc_chunk_cnt;
			cstat->cs_chunk_cnt += n;
			cstat->cs_heap[i].hs_chunk_cnt += n;
			cstat->cs_heap[i].hs_size_class[j].cl_chunk_cnt = n;
			cstat->cs_total_heap_mem += s * n;
			cstat->cs_heap[i].hs_total_heap_mem += s * n;
			cstat->cs_heap[i].hs_size_class[j].cl_total_heap_mem = s * n;
			if(j < ds_small_sizeclass_cnt)
				cstat->cs_small_heap_mem += s * n;
			else
				cstat->cs_large_heap_mem += s * n;
			s = hp->hp_size_class[j].sc_division_size;
			cstat->cs_heap[i].hs_size_class[j].cl_division_size = s;
			n = hp->hp_size_class[j].sc_division_total_cnt;
			cstat->cs_total_division_total_cnt += n;
			cstat->cs_heap[i].hs_division_total_cnt += n;
			cstat->cs_heap[i].hs_size_class[j].cl_division_total_cnt = n;
			n = hp->hp_size_class[j].sc_division_free_cnt;
			cstat->cs_total_division_free_cnt += n;
			cstat->cs_heap[i].hs_division_free_cnt += n;
			cstat->cs_heap[i].hs_size_class[j].cl_division_free_cnt = n;
			cstat->cs_total_heap_free_mem += s * n;
			cstat->cs_heap[i].hs_total_heap_free_mem += s * n;
			cstat->cs_heap[i].hs_size_class[j].cl_total_heap_free_mem = s * n;
			if(j < ds_small_sizeclass_cnt)
				cstat->cs_small_heap_free_mem += s * n;
			else
				cstat->cs_large_heap_free_mem += s * n;
		}
	}
	cstat->cs_total_free_mem = cstat->cs_total_heap_free_mem +
	    ((ds_size_t)cstat->cs_total_small_chunk_free_cnt *
	    DS_SMALL_CHUNKSZ) +
	    ((ds_size_t)cstat->cs_total_large_chunk_free_cnt *
	    DS_LARGE_CHUNKSZ);

	cstat->cs_global_division_total_cnt = 
	    cstat->cs_heap[DS_GLOBAL_HEAP].hs_division_total_cnt;

	cstat->cs_global_division_free_cnt = 
	    cstat->cs_heap[DS_GLOBAL_HEAP].hs_division_free_cnt;
	
	for(i = (DS_TOTAL_HEAPS - 1); i >= 0; i--) {
		hp = &ds_heap[i];
		ds_unlock(&hp->hp_large_chunk_lock);
		ds_unlock(&hp->hp_small_chunk_lock);
	}

	for(i = (ds_sizeclass_cnt - 1); i >= 0; i--)
		for(j = (DS_TOTAL_HEAPS - 1); j >= 0; j--)
			ds_unlock(&ds_heap[j].hp_size_class[i].sc_lock);
}
