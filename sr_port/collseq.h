/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef COLLSEQ_H_INCLUDED
#define COLLSEQ_H_INCLUDED

#include "min_max.h"

#define MAX_COLLTYPE	255
#define MIN_COLLTYPE	0
#define XFORM	0
#define XBACK	1

#define ALLOC_XFORM_BUFF(STR1LEN)									\
{													\
	mstr_len_t	lcl_len;									\
													\
	if (0 == TREF(max_lcl_coll_xform_bufsiz))							\
	{												\
		assert(NULL == TREF(lcl_coll_xform_buff));						\
		TREF(max_lcl_coll_xform_bufsiz) = MAX_STRBUFF_INIT;					\
		TREF(lcl_coll_xform_buff) = (char *)malloc(TREF(max_lcl_coll_xform_bufsiz));		\
	} 												\
	lcl_len = STR1LEN;										\
	assert(MAX_STRLEN >= lcl_len);									\
	if (lcl_len > TREF(max_lcl_coll_xform_bufsiz))							\
	{												\
		assert(NULL != TREF(lcl_coll_xform_buff));						\
		free(TREF(lcl_coll_xform_buff));							\
		assert(MAX_STRLEN >= TREF(max_lcl_coll_xform_bufsiz));					\
		while (lcl_len > TREF(max_lcl_coll_xform_bufsiz))					\
			TREF(max_lcl_coll_xform_bufsiz) *= 2;						\
		TREF(max_lcl_coll_xform_bufsiz) = MIN(MAX_STRLEN, TREF(max_lcl_coll_xform_bufsiz));	\
		TREF(lcl_coll_xform_buff) = (char *)malloc(TREF(max_lcl_coll_xform_bufsiz));		\
	}												\
}
/*
   Following two macros are currently used in replication filters, merge command  and binary load to transform
   GTM null subscripts collation to standard null subscript collation and vice versa
*/

#define GTM2STDNULLCOLL(key, len)							\
{											\
	unsigned char *currptr, *ptrtop;						\
											\
	currptr = (unsigned char *)(key);						\
	ptrtop  = (currptr + (len));							\
	assert(currptr < ptrtop);							\
	do {										\
		if ((STR_SUB_PREFIX == *currptr++) && (KEY_DELIMITER == *currptr))	\
			*(currptr - 1) = SUBSCRIPT_STDCOL_NULL;				\
		assert(currptr <= ptrtop);						\
		while ((currptr < ptrtop) && (KEY_DELIMITER != *currptr++))		\
			;								\
	} while ((currptr < ptrtop) && (KEY_DELIMITER != *currptr));			\
	assert(currptr <= ptrtop);							\
}

#define STD2GTMNULLCOLL(key, len)								\
{												\
	unsigned char *currptr, *ptrtop;							\
												\
	currptr = (unsigned char *)(key);							\
	ptrtop  = (currptr + (len));								\
	assert(currptr < ptrtop);								\
	do {											\
		if ((SUBSCRIPT_STDCOL_NULL == *currptr++) && (KEY_DELIMITER == *currptr))	\
			*(currptr - 1) = STR_SUB_PREFIX;					\
		assert(currptr <= ptrtop);							\
		while ((currptr < ptrtop) && (KEY_DELIMITER != *currptr++))			\
			;									\
	} while ((currptr < ptrtop) && (KEY_DELIMITER != *currptr));				\
	assert(currptr <= ptrtop);								\
}


typedef struct collseq_struct {
	struct collseq_struct	*flink;
	int			act;
	int4			(*xform)();
	int4			(*xback)();
	int4			(*version)();
	int4			(*verify)();
	int			argtype;
} collseq;

boolean_t map_collseq(mstr *fspec, collseq *ret_collseq);
collseq *ready_collseq(int act);
int4 do_verify(collseq *csp, unsigned char type, unsigned char ver);
int find_local_colltype(void);
void act_in_gvt(void);

#endif /* COLLSEQ_H_INCLUDED */
