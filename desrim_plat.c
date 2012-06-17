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
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include "desrim.h"


pthread_key_t ds_threadkey;
pthread_mutex_t ds_init_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ds_heapcnt_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ds_large_arena_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ds_small_arena_lock = PTHREAD_MUTEX_INITIALIZER;

ds_localmem_t ds_large_arena_mem;
ds_localmem_t ds_small_arena_mem;

int ds_heapcnt[DS_NUM_HEAPS];
int ds_gotkey;

ds_size_t ds_sys_pagesize;

void ds_key_destruct(void *);
int ds_mmap(ds_size_t, void **);

void ds_plat_init(void)
{
	ds_lm_init(&ds_large_arena_mem, 0, 0);
	ds_lm_init(&ds_small_arena_mem, 0, 0);
	ds_sys_pagesize = getpagesize();
	if(pthread_key_create(&ds_threadkey, ds_key_destruct) == 0) {
		ds_gotkey++;
	}
	else if(ds_err_func != NULL) {
		ds_err_func(
		    "ds_plat_init: dafuq failed to create the thread key (%d): %s",
		    errno, strerror(errno));
	}
}


void ds_key_destruct(void *arg)
{
	(void)ds_lock(&ds_heapcnt_lock);
	if(arg == NULL)
		ds_heapcnt[0]--;
	else
		ds_heapcnt[(int)(long)arg - 1]--;
	(void)ds_unlock(&ds_heapcnt_lock);
}

int ds_default_heapsel_func(void)
{
	int i;
	int heapmin;
	int heapnum;
	void *specval;
		
	if(!ds_gotkey)
		return 0;

	specval = pthread_getspecific(ds_threadkey);
	if(specval != NULL)
		return((int)(long)specval - 1);
	heapnum = 0;

	(void)ds_lock(&ds_heapcnt_lock);
	heapmin = ds_heapcnt[0];

	for(i = 1; (i < ds_num_heaps) && (heapmin > 0); i++) {
		if(ds_heapcnt[i] < heapmin) {
			heapnum = i;
			heapmin = ds_heapcnt[i];
		}
	}
	ds_heapcnt[heapnum]++;
	(void)ds_unlock(&ds_heapcnt_lock);

	if(pthread_setspecific(ds_threadkey, (const void *)(long)(heapnum + 1)) != 0) {
		(void)ds_lock(&ds_heapcnt_lock);
		ds_heapcnt[heapnum]--;
		ds_heapcnt[0]++;
		(void)ds_unlock(&ds_heapcnt_lock);
		return 0;
	}
	return heapnum;
}

void ds_lock_init(ds_lock_t *lock)
{
	pthread_mutexattr_t mattr;

	(void)pthread_mutexattr_init(&mattr);
	(void)pthread_mutex_init(lock, &mattr);
	(void)pthread_mutexattr_destroy(&mattr);
}

void ds_cond_init(ds_cond_t *cond)
{
	pthread_condattr_t cattr;
	(void)pthread_condattr_init(&cattr);
	(void)pthread_cond_init(cond, &cattr);
	(void)pthread_condattr_destroy(&cattr);
}

int ds_arena_alloc(ds_size_t size, void **ptr, ds_size_t *realsize, int flags)
{
	int ret;
	void *p;
	ds_lock_t *lp;
	ds_localmem_t *lm;
	ds_size_t chunksz;

	switch(size) {
	case DS_LARGE_CHUNKSZ:
		lm = &ds_large_arena_mem;
		lp = &ds_large_arena_lock;
		break;

#if DS_SMALL_CHUNKSZ != DS_LARGE_CHUNKSZ
	case DS_SMALL_CHUNKSZ:
		lm = &ds_small_arena_mem;
		lp = &ds_small_arena_lock;
		break;
#endif

	default:
		lm = 0;
		lp = 0;
		flags |= DS_ARENA_RAWALLOC;
		break;
	}

	if(flags & DS_ARENA_RAWALLOC) {
		size = ((size + ds_sys_pagesize - 1) / ds_sys_pagesize)
		    * ds_sys_pagesize;
		ret = ds_mmap(size, ptr);
		if(ret != 0)
			return ret;
		if(realsize)
			*realsize = size;
		return 0;
	}
	ds_lock(lp);
	if(ds_lm_alloc(lm, size, ptr) != 0) {
		if(size == DS_LARGE_CHUNKSZ)
			chunksz = DS_LARGE_ARENA_CHUNKSZ;
		else
			chunksz = DS_SMALL_ARENA_CHUNKSZ;
		ret = ds_mmap(chunksz, &p);
		if(ret != 0) {
			ds_unlock(lp);
			return ret;
		}
		ds_lm_init(lm, chunksz, p);
		(void)ds_lm_alloc(lm, size, ptr);
	}
	ds_unlock(lp);
	if(realsize)
		*realsize = size;
	return 0;
}

void ds_arena_free(ds_size_t size, void *ptr)
{
	int ret;
	int err;

	ret = munmap(ptr, size);
	if((ret != 0) && (ds_err_func != NULL)) {
		if(errno == 0)
			err = ENOMEM;
		else
			err = errno;
		ds_err_func(
		    "ds_munmap: dafuq, failed to free of %lx bytes (%d): %s",
		    (unsigned long)size, err, strerror(err));
	}
}


int ds_mmap(ds_size_t size, void **ptr)
{
	int err;
	void *val;
	val = mmap(0, size, (PROT_READ | PROT_WRITE | PROT_EXEC), (MAP_ANON | MAP_PRIVATE), -1, 0);
	if(val == MAP_FAILED) {
		if(errno == 0)
			err = ENOMEM;
		else
			err = errno;
		if(ds_err_func != NULL) {
			ds_err_func(
			    "ds_mmap: dafuq failed to allocate of %lx bytes (%d): %s",
			    (unsigned long)size, err, strerror(err));
		}
		errno = err;
		return err;
	}
	*ptr = val;
	return 0;
}
