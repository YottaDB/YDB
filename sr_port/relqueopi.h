/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*	relqueopi - C-callable relative queue interlocked routines
 *
 *	These routines perform interlocked operations on doubly-linked
 *	relative queues.  They are designed to emulate the VAX machine
 *	instructions (and corresponding VAX C library routines) after
 *	which they are named.
 *
 *	INSQHI - insert entry into queue at head, interlocked
 *	INSQTI - insert entry into queue at tail, interlocked
 *	REMQHI - remove entry from queue at head, interlocked
 *	REMQTI - remove entry from queue at tail, interlocked
 *
 */

#ifndef VMS

int             insqhi2(que_ent_ptr_t new, que_head_ptr_t base);
int             insqti2(que_ent_ptr_t new, que_head_ptr_t base);
void_ptr_t	remqhi1(que_head_ptr_t base);
void_ptr_t	remqti1(que_head_ptr_t base);

#define INSQHI(N,B) insqhi2((que_ent_ptr_t)(N), (que_head_ptr_t)(B))
#define INSQTI(N,B) insqti2((que_ent_ptr_t)(N), (que_head_ptr_t)(B))
#define REMQHI(B)   remqhi1((que_head_ptr_t)(B))
#define REMQTI(B)   remqti1((que_head_ptr_t)(B))

#else
# ifndef __vax
/* Don't declare these routines for Vax since the routine names are macros on that platform */
int		insqhi(que_ent_ptr_t, que_head_ptr_t);
int		insqti(que_ent_ptr_t, que_head_ptr_t);
# endif
void_ptr_t	remqhi(que_head_ptr_t base);
void_ptr_t	remqti(que_head_ptr_t base);

#define INSQHI(N,B) insqhi((que_ent_ptr_t)(N), (que_head_ptr_t)(B))
#define INSQTI(N,B) insqti((que_ent_ptr_t)(N), (que_head_ptr_t)(B))
#define REMQHI(B)   remqhi((que_head_ptr_t)(B))
#define REMQTI(B)   remqti((que_head_ptr_t)(B))

#endif

