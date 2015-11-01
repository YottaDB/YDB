/****************************************************************
 *								*
 *	Copyright 2005, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef SHMPOOL_H_INCLUDED
#define SHMPOOL_H_INCLUDED

#define SBLKP_REL2ABS(base, queue)	((shmpool_blk_hdr_ptr_t)((char_ptr_t)(base) + (((que_ent_ptr_t)base)->queue)))
#define MAX_LOST_SHMPOOL_BLKS		5	/* When we have lost this many, reclaim */

#include "gdsbgtr.h"

/* Macro to allow release of a shmpool reformat block if the current cache record is pointing to it
   (and we can get the shmpool critical lock).
*/
#ifdef VMS
#  define SHMPOOL_FREE_CR_RFMT_BLOCK(reg, csa, cr)									\
{															\
	shmpool_blk_hdr_ptr_t	SFCRB_sblkh_p;										\
	int			SFCRB_sblkh_off;									\
        assert(csa->now_crit); 												\
	if (0 != (SFCRB_sblkh_off = cr->shmpool_blk_off))								\
	{	/* Break the link to the shmpool buffer for this cache record now that					\
		   the IO is complete. Also allow the block to be requeued if we can get				\
		   the shmpool lock (if not block will be harvested later).						\
		*/													\
		cr->shmpool_blk_off = 0;										\
		SFCRB_sblkh_p = (shmpool_blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, SFCRB_sblkh_off);				\
		if (GDS_ANY_REL2ABS(csa, SFCRB_sblkh_p->use.rfrmt.cr_off) == cr 					\
		    && SHMBLK_REFORMAT == SFCRB_sblkh_p->blktype							\
                    && cr->cycle == SFCRB_sblkh_p->use.rfrmt.cycle							\
		    && shmpool_lock_hdr_nowait(reg))									\
		{	/* If it is pointing at us and we are able to get lock (check of offset pointer			\
			   outside of lock is not definitive but allows us to avoid a lock operation if			\
			   it isn't true).										\
			   Recheck offset pointer/type now that we are under lock 					\
			*/												\
			if (GDS_ANY_REL2ABS(csa, SFCRB_sblkh_p->use.rfrmt.cr_off) == cr					\
			    && SHMBLK_REFORMAT == SFCRB_sblkh_p->blktype						\
			    && cr->cycle == SFCRB_sblkh_p->use.rfrmt.cycle)						\
			{	/* This reformat buffer is (still) pointing to us. Free it for reuse */			\
				shmpool_blk_free(reg, SFCRB_sblkh_p);							\
                                BG_TRACE_ANY(csa, refmt_blk_chk_blk_freed);						\
			} else												\
                        {												\
                                BG_TRACE_ANY(csa, refmt_blk_chk_blk_kept);						\
                        }												\
			shmpool_unlock_hdr(reg);									\
		} else													\
                {													\
                        BG_TRACE_ANY(csa, refmt_blk_chk_blk_kept);							\
                }													\
	}														\
}
#else
#  define SHMPOOL_FREE_CR_RFMT_BLOCK(reg, csa, cr)
#endif

void	  shmpool_buff_init(gd_region *reg);
shmpool_blk_hdr_ptr_t shmpool_blk_alloc(gd_region *reg, enum shmblk_type blktype);
void	  shmpool_blk_free(gd_region *reg, shmpool_blk_hdr_ptr_t sblkh_p);
VMS_ONLY(void shmpool_harvest_reformatq(gd_region *reg);)
void	  shmpool_abandoned_blk_chk(gd_region *reg, boolean_t force);
boolean_t shmpool_lock_hdr(gd_region *reg);
boolean_t shmpool_lock_hdr_nowait(gd_region *reg);
void	  shmpool_unlock_hdr(gd_region *reg);
boolean_t shmpool_lock_held_by_us(gd_region *reg);

#endif
