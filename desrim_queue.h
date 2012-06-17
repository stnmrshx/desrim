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

#ifndef _DESRIM_QUEUE_H_
#define _DESRIM_QUEUE_H_

typedef struct ds_queue {
        struct ds_queue *cq_next;
        struct ds_queue *cq_last;
        void *cq_assoc;
} ds_queue_t;


extern void ds_initq(ds_queue_t *, void *);
extern void ds_nq(ds_queue_t *, ds_queue_t *);
extern void ds_nq_front(ds_queue_t *, ds_queue_t *);
extern void ds_dq(ds_queue_t *);
extern void ds_set_assocq(ds_queue_t *, void *);
extern void *ds_get_assocq(ds_queue_t *);
extern int ds_emptyq(ds_queue_t *cq);
extern ds_queue_t *ds_nextq(ds_queue_t *);
extern ds_queue_t *ds_lastq(ds_queue_t *);

#define ds_initq(cq, assoc) \
{ \
	(cq)->cq_next = (cq); \
	(cq)->cq_last = (cq); \
	(cq)->cq_assoc = (assoc); \
}


#define ds_nq(head, cq) \
{ \
	(cq)->cq_next = (head); \
	(cq)->cq_last = (head)->cq_last; \
	(cq)->cq_last->cq_next = (cq); \
	(head)->cq_last = (cq); \
}


#define ds_nq_front(head, cq) \
{ \
	(cq)->cq_last = (head); \
	(cq)->cq_next = (head)->cq_next; \
	(cq)->cq_next->cq_last = (cq); \
	(head)->cq_next = (cq); \
}


#define ds_dq(cq) \
{ \
	(cq)->cq_next->cq_last = (cq)->cq_last; \
	(cq)->cq_last->cq_next = (cq)->cq_next; \
}


#define ds_nextq(cq) ((cq)->cq_next)
#define ds_lastq(cq) ((cq)->cq_last)
#define ds_emptyq(cq) ((cq)->cq_next == cq)
#define ds_get_assocq(cq) ((cq)->cq_assoc)
#define ds_set_assocq(cq, assoc) ((cq)->cq_assoc = (void *)(long)(assoc))

#endif /* _DESRIM_QUEUE_H_ */
