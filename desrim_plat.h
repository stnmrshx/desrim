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

#ifndef __DESRIM_PLAT_H__
#define __DESRIM_PLAT_H__

#include <sys/types.h>
#include <stdlib.h>
#include <pthread.h>

#define DS_LARGE_ARENA_CHUNKSZ	(DS_LARGE_CHUNKSZ * 10)
#define DS_SMALL_ARENA_CHUNKSZ	(DS_SMALL_CHUNKSZ * 50)

#define ds_lock(lkp)	pthread_mutex_lock(lkp)
#define ds_unlock(lkp)	pthread_mutex_unlock(lkp)
#define ds_wait(cop, lkp)	pthread_cond_wait((cop), (lkp))
#define ds_wake(cop)		pthread_cond_broadcast(cop)
#define DS_INIT_LOCK		ds_lock(&ds_init_lock)
#define DS_INIT_UNLOCK		ds_unlock(&ds_init_lock)

typedef pthread_mutex_t ds_lock_t;
typedef pthread_cond_t ds_cond_t;
typedef size_t ds_size_t;

extern ds_lock_t ds_init_lock;

void ds_lock_init(ds_lock_t *);
void ds_cond_init(ds_cond_t *);
void ds_plat_init(void);

#endif /* __DESRIM_PLAT_H__ */
