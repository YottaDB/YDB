/****************************************************************
 *								*
 *	Copyright 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef V15_GDSBT_H
#define V15_GDSBT_H
/* this requires v15_gdsroot.h */

typedef struct
{
	v15_trans_num	curr_tn;
	v15_trans_num	early_tn;
	v15_trans_num	last_mm_sync;		/* Last tn where a full mm sync was done */
	v15_trans_num	header_open_tn;		/* Tn to be compared against jnl tn on open */
	v15_trans_num	mm_tn;			/* Used to see if CCP must update master map */
	uint4		lock_sequence;		/* Used to see if CCP must update lock section */
	uint4		ccp_jnl_filesize;	/* Passes size of journal file if extended */
	volatile uint4	total_blks;		/* Placed here so can be passed to other machines on cluster */
	volatile uint4	free_blocks;
} v15_th_index;


/* Define pointer types for above structures that may be in shared memory and need 64
   bit pointers. */
#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(save)
#  pragma pointer_size(long)
# else
#  error UNSUPPORTED PLATFORM
# endif
#endif

typedef v15_th_index *v15_th_index_ptr_t;

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(restore)
# endif
#endif

#endif
