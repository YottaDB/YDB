/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef PARM_POOL_H_INCLUDED
#define PARM_POOL_H_INCLUDED

#ifdef DEBUG
#  define PARM_POOL_INIT_CAP		2				/* Initial debug capacity */
#  define MAX_SET_COUNT			3				/* Max parameter sets stored at one time */
#  define MAX_TOTAL_SLOTS		(MAX_SET_COUNT * MAX_ACTUALS)	/* Max total slots allowed in the pool */
#else
#  define PARM_POOL_INIT_CAP		8				/* Initial pro capacity */
#endif

#ifdef GTM64
#  define LV_VALS_PER_SLOT		1				/* 64: number of lv_vals that one slot fits */
#  define SLOTS_NEEDED_FOR_SET(parms)	(parms + 2)			/* 64: slots needed for params, frame, and mask_and_cnt */
#else
#  define LV_VALS_PER_SLOT		2				/* 32: number of lv_vals that one slot fits */
#  define SLOTS_NEEDED_FOR_SET(parms)	(parms / 2 + 2)			/* 32: slots needed for params, frame, and mask_and_cnt.
									 * Note that parms / 2 is used instead of (parms + 1) / 2
									 * because on 32-bit platforms if the number of params is
									 * even, then we need two additional slots for frame and
									 * mask_and_cnt; if odd, then the parameter not counted due
									 * to integer division will share the same slot with frame
									 * field, making this macro still correct */
#endif

/* The first vacant slot */
#define PARM_CURR_SLOT			((TREF(parm_pool_ptr))->parms + (TREF(parm_pool_ptr))->start_idx)

/* Frame field of the previous set */
#ifdef GTM64
#  define PARM_ACT_FRAME(curr_slot, count)	(*(curr_slot - 2)).frame
#else
#  define PARM_ACT_FRAME(curr_slot, count)	(*((&(*(curr_slot - 2)).frame) + ((count % 2) ? 1 : 0)))
#endif

/* This value has to be even, so that when count is increased by this value, it retains its divisibility by two. */
#define SAFE_TO_OVWRT                   (((MAX_ACTUALS + 1) % 2 == 0) ? (MAX_ACTUALS + 1) : (MAX_ACTUALS + 2))

/* round capacity up to the next power of 2 above or equal to min */
#define CAPACITY_ROUND_UP2(cap, min)	\
{					\
	while (cap < min)		\
		cap <<= 1;		\
}

/* Reclaim the space used by the parameter set stored last, because the corresponding frame is being rewound. */
#define PARM_ACT_UNSTACK_IF_NEEDED									\
{													\
	int 		count;										\
	parm_slot	*curr_slot;									\
	if (TREF(parm_pool_ptr) && (0 < (TREF(parm_pool_ptr))->start_idx))				\
	{												\
		curr_slot = PARM_CURR_SLOT;								\
		count = (*(curr_slot - 1)).mask_and_cnt.actualcnt; 					\
		if (PARM_ACT_FRAME(curr_slot, count) == frame_pointer)					\
			(TREF(parm_pool_ptr))->start_idx 						\
				-= (SLOTS_NEEDED_FOR_SET(((MAX_ACTUALS < count) 			\
				? (count - SAFE_TO_OVWRT) : count)));					\
	}												\
}

typedef union
{
	struct
	{
		uint4			mask;		/* Mask for the param list (alias/non-alias) of up to 32 params */
		unsigned int		actualcnt;	/* Number of parameters in the parameter set */
	} mask_and_cnt;
	struct stack_frame_struct 	*frame;		/* Pointer to the frame that the parameter set corresponds to */
	lv_val				*actuallist;	/* On 32-bit platforms this field is 4 bytes, so when the number of
							 * parameters is odd, a frame value is stored in the second half of
							 * this union; otherwise, frame is stored in a separate slot, thus
							 * leaving 4 bytes unused. For details, refer to parm_pool.c.
							 */
} parm_slot;

typedef struct
{
	uint4		capacity;
	uint4		start_idx;
	parm_slot	parms[1];
} parm_pool;

STATICFNDCL void parm_pool_init(unsigned int init_capacity);
STATICFNDCL void parm_pool_expand(int slots_needed, int slots_copied);

#endif	/* PARM_POOL_H_INCLUDED */
