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

#include <stdlib.h>
#include <errno.h>
#include <desrim.h>

#if HAVE_MALLOC_H
#include <malloc.h>
#endif

void *
malloc(size_t size)
{
#ifndef NO_ZERO_MALLOCS
	if(size == 0)
		size = 1;
#endif

	return ds_malloc(size);
}

void free(void *ptr)
{
	ds_free(ptr);
}

void *
calloc(size_t nmemb, size_t size)
{
	return ds_calloc(nmemb, size);
}

void *
realloc(void *ptr, size_t size)
{
	return ds_realloc(ptr, size);
}

#if HAVE_REALLOCF
void *
reallocf(void *ptr, size_t size)
{
	void *rslt;
	rslt = ds_realloc(ptr, size);
	if((rslt == NULL) && (ptr != NULL))
		ds_free(ptr);

	return rslt;
}
#endif

#if HAVE_POSIX_MEMALIGN
int
posix_memalign(void **ptr, size_t align, size_t size)
{
	errno = 0;
	*ptr = ds_memalign(align, size);
	return errno;
}
#endif

#if HAVE_MEMALIGN
void *
memalign(size_t align, size_t size)
{
	return ds_memalign(align, size);
}
#endif

#if HAVE_VALLOC
void *
valloc(size_t size)
{
	return ds_valloc(size);
}
#endif

#if HAVE_MALLOC_SIZE
size_t
malloc_size(void *ptr)
{
	int ret;
	size_t size;
	ret = ds_malloc_size(ptr, &size);
	if(ret != 0)
		return 0;
	return size;
}
#endif

#if HAVE_MALLOC_GOOD_SIZE
size_t
malloc_good_size(void *ptr)
{
	return malloc_size(ptr);
}
#endif
