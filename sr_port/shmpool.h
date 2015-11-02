/****************************************************************
 *								*
 *	Copyright 2005, 2010 Fidelity Information Services, Inc	*
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

#define		SHMPOOL_BUFFER_SIZE	0x00100000	/* 1MB pool for shared memory buffers (backup and downgrade) */

/* Block types for shmpool_blk_hdr */
enum shmblk_type
{
	SHMBLK_FREE = 1,	/* Block is not in use */
	SHMBLK_REFORMAT,	/* Block contains reformat information */
	SHMBLK_BACKUP		/* Block in use by online backup */
};

/* These shared memory blocks are in what was called the "backup buffer" in shared memory. It is a 1MB area
   of shared mem storage with (now) multiple uses. It is used by both backup and the online downgrade process
   (the latter on VMS only). Free blocks are queued in shared memory.
*/

typedef struct shmpool_blk_hdr_struct
{	/* Block header for each block in shmpool buffer area. The data portion of the block immediately follows this header and
	   in the case of backup, is written out to the temporary file with the data block appended to it. Same holds true for
	   writing out incremental backup files so any change to this structure warrants consideration of changing the format
	   version for the incremental backup (INC_HEADER_LABEL in murest.h).
	 */
	que_ent		sm_que;			/* Main queue fields */
	volatile enum shmblk_type blktype;	/* free, backup or reformat? */
	block_id	blkid;			/* block number */
	union
	{
		struct
		{	/* Use for backup */
			enum db_ver	ondsk_blkver;	/* Version of block from cache_rec */
			VMS_ONLY(int4	filler;)	/* If VMS, this structure will be 2 words since rfrmt struct is */
		} bkup;
#ifdef VMS
		struct
		{	/* Use in downgrade mode (as reformat buffer) */
			volatile sm_off_t	cr_off;	/* Offset to cache rec associated with this reformat buffer */
			volatile int4		cycle;	/* cycle of given cache record (to validate we have same CR */
		} rfrmt;
#endif
	} use;
	pid_t		holder_pid;		/* PID holding/using this buffer */
	boolean_t	valid_data;		/* This buffer holds valid data (else not initialized) */
	int4		image_count;		/* VMS only */
	VMS_ONLY(int4	filler;)		/* 8 byte alignment. Only necessary for VMS since bkup struct will only
						   be 4 bytes for UNIX and this filler is not then necessary for alignment.
						 */
} shmpool_blk_hdr;

typedef struct muinc_blk_hdr_struct
{	/* This is a mirror structure of shmpool_blk_hdr_struct to maintain compatibility with the current incremental
	 * backup format. The sm_que structure is replaced with a 8-byte filler.
	 */

	char		filler_8byte[8];	/* Main queue fields */
	volatile enum shmblk_type blktype;	/* free, backup or reformat? */
	block_id	blkid;			/* block number */
	union
	{
		struct
		{	/* Use for backup */
			enum db_ver	ondsk_blkver;	/* Version of block from cache_rec */
			VMS_ONLY(int4	filler;)	/* If VMS, this structure will be 2 words since rfrmt struct is */
		} bkup;
#ifdef VMS
		struct
		{	/* Use in downgrade mode (as reformat buffer) */
			volatile sm_off_t	cr_off;	/* Offset to cache rec associated with this reformat buffer */
			volatile int4		cycle;	/* cycle of given cache record (to validate we have same CR */
		} rfrmt;
#endif
	} use;
	pid_t		holder_pid;		/* PID holding/using this buffer */
	boolean_t	valid_data;		/* This buffer holds valid data (else not initialized) */
	int4		image_count;		/* VMS only */
	VMS_ONLY(int4	filler;)		/* 8 byte alignment. Only necessary for VMS since bkup struct will only
						   be 4 bytes for UNIX and this filler is not then necessary for alignment.
						 */
} muinc_blk_hdr;

/* Header of the shmpool buffer area. Describes contents */
typedef struct  shmpool_buff_hdr_struct
{
	global_latch_t  shmpool_crit_latch;	/* Latch to update header fields */
	off_t           dskaddr;		/* Highest disk address used (backup only) */
	trans_num       backup_tn;		/* TN at start of full backup (backup only) */
	trans_num       inc_backup_tn;		/* TN to start from for incremental backup (backup only) */
	char            tempfilename[256];	/* Name of temporary file we are using (backup only) */
	que_ent		que_free;		/* Queue header for all free elements */
	que_ent		que_backup;		/* Queue header for all (allocated) backup elements */
	VMS_ONLY(que_ent que_reformat;)		/* Queue header for all (allocated) reformat elements */
	volatile int4	free_cnt;		/* Elements on free queue */
	volatile int4   backup_cnt;		/* Elements used for backup */
	volatile int4	reformat_cnt;		/* Elements used for reformat */
	volatile int4	allocs_since_chk;	/* Allocations since last lost block check */
	uint4		total_blks;		/* Total shmpool block buffers in 1MB buffer area */
	uint4		blk_size;		/* Size of the created buffers (excluding header - convenient blk_size field) */
	pid_t           failed;			/* Process id that failed to write to temp file causing failure (backup only) */
	int4            backup_errno;		/* errno value when "failed" is set (backup only) */
	uint4           backup_pid;		/* Process id performing online backup (backup only) */
	uint4           backup_image_count;	/* Image count of process running online backup (VMS & backup only) */
	boolean_t	shmpool_blocked;	/* secshr_db_clnup() detected a problem on shutdown .. force recovery */
	uint4		filler;			/* 8 byte alignment */
} shmpool_buff_hdr;

typedef	shmpool_buff_hdr	*shmpool_buff_hdr_ptr_t;
typedef muinc_blk_hdr	muinc_blk_hdr_t;
typedef muinc_blk_hdr	*muinc_blk_hdr_ptr_t;
typedef	shmpool_blk_hdr		*shmpool_blk_hdr_ptr_t;

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
