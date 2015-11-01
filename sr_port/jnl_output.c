/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"

#include <netinet/in.h> /* Required for gtmsource.h */
#include <arpa/inet.h>

#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ccp.h"
#include "iosp.h"
#include "jnl.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "min_max.h"
#include "sleep_cnt.h"
#include "jnl_write.h"

static	jnlpool_ctl_struct	temp_jnlpool_ctl_struct;
static	char			zeroes[JNL_REC_START_BNDRY] = "\0\0\0\0\0\0\0\0";

GBLDEF	jnlpool_ctl_ptr_t	temp_jnlpool_ctl = &temp_jnlpool_ctl_struct;
GBLDEF	uint4			cumul_jnl_rec_len;
DEBUG_ONLY(GBLDEF uint4		cumul_index;
	   GBLDEF uint4		cu_jnl_index;
	  )

GBLREF	uint4			gbl_jrec_time;	/* assigned by t_end,tp_tend & used by jnl_write* routines */
GBLREF	uint4			process_id;
GBLREF	sm_uc_ptr_t		jnldata_base;
GBLREF	bool			run_time;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	boolean_t		forw_phase_recovery;

LITREF	int			jnl_fixed_size[];

#ifdef DEBUG
/* The fancy ordering of operators/operands in the JNL_SPACE_AVAILABLE calculation is to avoid overflows. */
#define	JNL_SPACE_AVAILABLE(jb, lcl_dskaddr)							\
(												\
	assert((jb)->dskaddr <= (jb)->freeaddr),						\
	/* the following assert is an || to take care of 4G value overflows or 0 underflows */  \
	assert(((jb)->freeaddr <= (jb)->size) || (jb)->dskaddr >= (jb)->freeaddr - (jb)->size),	\
	((jb)->size - ((jb)->freeaddr - ((lcl_dskaddr = (jb)->dskaddr) & JNL_WRT_START_MASK)))	\
)
#else
#define	JNL_SPACE_AVAILABLE(jb, dummy)	 ((jb)->size - ((jb)->freeaddr - ((jb)->dskaddr & JNL_WRT_START_MASK)))
#endif

void	jnl_putchar(jnl_private_control *jpc, unsigned char c);
void	jnl_putstr(jnl_private_control *jpc, uchar_ptr_t s, int n);

void	jnl_putchar(jnl_private_control *jpc, unsigned char c)
{
	assert(jpc->temp_free >= 0  &&  jpc->temp_free < jpc->jnl_buff->size);
	jpc->jnl_buff->buff[jpc->temp_free++] = c;
	if (jpc->temp_free >= jpc->jnl_buff->size)
		jpc->temp_free = 0;
}

void	jnl_putstr(jnl_private_control *jpc, uchar_ptr_t s, int n)
{
	int		m, w;
	DEBUG_ONLY(
		uint4	lcl_dskaddr;
		uint4	avail_space;
	)

	assert(jpc->temp_free >= 0  &&  jpc->temp_free < jpc->jnl_buff->size);
	assert(n >= 0);
	assert((avail_space = JNL_SPACE_AVAILABLE(jpc->jnl_buff, lcl_dskaddr)) > n
			|| ((avail_space == n) && (lcl_dskaddr != (lcl_dskaddr & JNL_WRT_START_MASK))));
	assert(n < jpc->jnl_buff->size - 1);
	m = jpc->jnl_buff->size - jpc->temp_free;
	w = MIN(m, n);
	memcpy(jpc->jnl_buff->buff + jpc->temp_free, s, w);
	if (w != m)
		jpc->temp_free += n;
	else
	{
		n -= w;
		assert(n >= 0);
		memcpy(jpc->jnl_buff->buff, s + w, n);
		jpc->temp_free = n;
	}
}

void	jnl_write(jnl_private_control *jpc,
		  enum jnl_record_type rectype,
		  jrec_union	*fixed_section,
		  blk_hdr_ptr_t blk_ptr,
		  jnl_format_buffer *fjlist_header)
/* 3rd arg: Fixed-length section of the record. The union element and actual length are a function of the rectype if this is not */
/*	    a JRT_SET, JRT_KILL, or JRT_ZKILL. */
/* 4th arg: NULL, unless JRT_SET or JRT_KILL, in which case this points to the mval which is set at the node */
/* 5th arg: Pointer to data block for JRT_PBLK records; otherwise NULL */
{
	int4			m, n, offset, align_rec_filler_length, n_with_align, p, b;
	jnl_buffer_ptr_t	jb;
	sgmnt_addrs		*csa;
	jrec_union		align_rec;
	uint4 			status;
	DEBUG_ONLY(uint4	lcl_dskaddr;)

	error_def(ERR_JNLWRTNOWWRTR);
	error_def(ERR_JNLWRTDEFER);

	csa = &FILE_INFO(jpc->region)->s_addrs;
	assert(csa->now_crit  ||  (csa->hdr->clustered  &&  csa->nl->ccp_state == CCST_CLOSED));
	assert(rectype > JRT_BAD  &&  rectype < JRT_RECTYPES);
	jb = jpc->jnl_buff;
	assert(jb->freeaddr >= jb->dskaddr);
	++jb->reccnt[rectype - 1];
	/* Compute actual record length */
	if (fixed_section != NULL)
		n = JREC_PREFIX_SIZE + jnl_fixed_size[rectype] + JREC_SUFFIX_SIZE;
	else
		n = fjlist_header->record_size;
	if (blk_ptr != NULL)
		n += blk_ptr->bsiz;
	jb->bytcnt += n;
	n = ROUND_UP(n, JNL_REC_START_BNDRY);
	n_with_align = n + JREC_PREFIX_SIZE + jnl_fixed_size[JRT_ALIGN] + JREC_SUFFIX_SIZE;
	assert(0 == n_with_align % JNL_REC_START_BNDRY);
	assert(n_with_align < jb->alignsize);
	if (JRT_EOF == rectype  ||  jb->freeaddr / jb->alignsize == (jb->freeaddr + n_with_align - 1) / jb->alignsize)
		n_with_align = n;	/* don't write JRT_ALIGN for EOF record */
	else
	{
		align_rec_filler_length = ROUND_UP(jb->freeaddr, jb->alignsize) - jb->freeaddr - n_with_align + n;
		n_with_align += align_rec_filler_length;
	}
	UNIX_ONLY(assert(!jb->blocked));
	VMS_ONLY(assert(!jb->blocked || jb->blocked == process_id && lib$ast_in_prog())); /* wcs_wipchk_ast can set jb->blocked */
	jb->blocked = process_id;
	/* We should differentiate between a full and an empty journal buffer, hence the pessimism reflected in the <= check below.
	 * Hence also the -1 in jb->freeaddr - (jb->size - n_with_align - 1).
	 * This means that although we have space we might still be invoking jnl_write_attempt (very unlikely).
	 */
	if (JNL_SPACE_AVAILABLE(jb, lcl_dskaddr) <= n_with_align)
	{	/* The fancy ordering of operators/operands in the calculation done below is to avoid overflows. */
		if (SS_NORMAL != jnl_write_attempt(jpc,
					ROUND_UP2(jb->freeaddr - (jb->size - n_with_align - 1), JNL_WRT_START_MODULUS)))
		{
			assert(NOJNL == jpc->channel); /* jnl file lost */
			return; /* let the caller handle the error */
		}
	}
	jb->blocked = 0;
	if (jb->filesize < (jb->freeaddr + n_with_align)) /* not enough room in jnl file, extend it. */
	{	/* We should never reach here if we are called from t_end/tp_tend */
		assert(!run_time || csa->ti->early_tn == csa->ti->curr_tn);
		jnl_flush(jpc->region);
		assert(jb->freeaddr == jb->dskaddr);
		if (-1 == jnl_file_extend(jpc, n_with_align))	/* if extension fails, not much we can do */
		{
			assert(FALSE);
			return;
		}
		if (0 == csa->jnl->pini_addr && JRT_PINI != rectype)
		{	/* This can happen only if jnl got switched in jnl_file_extend above.
			 * We can't proceed now since the jnl record that we are writing now contains pini_addr	information
			 * 	pointing to the older journal which is inappropriate if written into the new journal.
			 */
			GTMASSERT;
		}
	}
	if (n_with_align != n)
	{	/* Write a JRT_ALIGN record. This code can be combined with the one below this if ().
		 * But to speed up normal record processing, this is written separately.
		 */
		jpc->temp_free = jb->free;
		jnl_putchar(jpc, JRT_ALIGN);
		offset = jb->freeaddr - jb->lastaddr;
		jnl_putstr(jpc, (uchar_ptr_t)THREE_LOW_BYTES(offset), 3);
		offset = 0;
		jnl_putstr(jpc, (uchar_ptr_t)&offset, 4);	/* write the filler_prefix */
		assert(0 != jb->freeaddr % jb->alignsize);	/* we can use the ROUND_UP macro below because we know
								 * that freeaddr should not be an exact multiple of alignsize */
		align_rec.jrec_align.align_str.align_string.length = align_rec_filler_length;
		switch(rectype)
		{
		case JRT_KILL:
		case JRT_FKILL:
		case JRT_GKILL:
		case JRT_SET:
		case JRT_FSET:
		case JRT_GSET:
		case JRT_TKILL:
		case JRT_UKILL:
		case JRT_TSET:
		case JRT_USET:
		case JRT_ZKILL:
		case JRT_FZKILL:
		case JRT_GZKILL:
		case JRT_TZKILL:
		case JRT_UZKILL:
			assert(forw_phase_recovery || (gbl_jrec_time ==
				((fixed_jrec_tp_kill_set *)(fjlist_header->buff + JREC_PREFIX_SIZE))->short_time));
			align_rec.jrec_align.short_time = (forw_phase_recovery ? ((fixed_jrec_tp_kill_set *)
							(fjlist_header->buff + JREC_PREFIX_SIZE))->short_time : gbl_jrec_time);
			break;
		case JRT_ZTCOM:
		case JRT_PBLK:
		case JRT_TCOM:
			assert(&fixed_section->jrec_kill.short_time == &fixed_section->jrec_pblk.short_time);
			assert(&fixed_section->jrec_kill.short_time == &fixed_section->jrec_tcom.tc_short_time);
			assert(&fixed_section->jrec_kill.short_time == &fixed_section->jrec_ztcom.tc_short_time);
			align_rec.jrec_align.short_time = (forw_phase_recovery ? fixed_section->jrec_kill.short_time
									: gbl_jrec_time);
			break;
		case JRT_EPOCH:
		case JRT_NULL:
		case JRT_INCTN:
		case JRT_AIMG:
			align_rec.jrec_align.short_time = fixed_section->jrec_fkill.short_time;
			break;
		case JRT_PFIN:
		case JRT_EOF:
			align_rec.jrec_align.short_time = MID_TIME(fixed_section->jrec_pini.process_vector[CURR_JPV].jpv_time);
			break;
		case JRT_PINI:
			if (!forw_phase_recovery)
			{
				align_rec.jrec_align.short_time =
					MID_TIME(fixed_section->jrec_pini.process_vector[CURR_JPV].jpv_time);
			} else
			{
				if (0 == fixed_section->jrec_pini.process_vector[SRVR_JPV].jpv_pid
					&& 0 ==  fixed_section->jrec_pini.process_vector[SRVR_JPV].jpv_image_count)
				{
					align_rec.jrec_align.short_time =
						MID_TIME(fixed_section->jrec_pini.process_vector[ORIG_JPV].jpv_time);
				} else
				{
					align_rec.jrec_align.short_time =
						MID_TIME(fixed_section->jrec_pini.process_vector[SRVR_JPV].jpv_time);
				}
			}
			break;
		default:
			GTMASSERT;
			break;
		}
		jnl_putstr(jpc, (uchar_ptr_t)&align_rec, jnl_fixed_size[JRT_ALIGN]);
		jnl_putstr(jpc, (uchar_ptr_t)jb, align_rec_filler_length);
		offset = ALIGN_KEY;
		jnl_putstr(jpc, (uchar_ptr_t)&offset, 4);	/* write the filler_suffix */
		offset = n_with_align - n - JREC_SUFFIX_SIZE;	/* don't count the suffix */
		jnl_putstr(jpc, (uchar_ptr_t)THREE_LOW_BYTES(offset), 3);
		jnl_putchar(jpc, JNL_REC_TRAILER);
		assert(0 == ((jb->freeaddr + offset + JREC_SUFFIX_SIZE) % jb->alignsize));
		assert(jb->buff[jb->free] == JRT_ALIGN);
		assert(jb->free < jpc->temp_free  ||  jpc->temp_free < jb->dsk);
		assert(jb->freeaddr >= jb->dskaddr);
		m = ((jpc->temp_free + ~JNL_WRT_END_MASK) & JNL_WRT_END_MASK) - jpc->temp_free;
		if (m != 0)
			memset(jb->buff + jpc->temp_free, 0, m);
		jpc->lastwrite = jb->freeaddr;
		jpc->new_freeaddr = jb->freeaddr + n_with_align - n;
		assert(jpc->temp_free == jpc->new_freeaddr % jb->size);
		/* Note that freeaddr should be updated ahead of free since jnl_output_sp.c does computation of wrtsize
		 * based on free and asserts follow later there which use freeaddr.
		 */
		jpc->free_update_inprog = TRUE; /* The following assignments must be protected against termination */
		jb->lastaddr = jpc->lastwrite;
		jb->freeaddr = jpc->new_freeaddr;
		jb->free = jpc->temp_free;
		jpc->free_update_inprog = FALSE;
	}
	jpc->temp_free = jb->free;
	if (fixed_section != NULL)
	{
		jnl_putchar(jpc, rectype);
		offset = jb->freeaddr - jb->lastaddr;
		jnl_putstr(jpc, (uchar_ptr_t)THREE_LOW_BYTES(offset), 3);
		offset = 0;
		jnl_putstr(jpc, (uchar_ptr_t)&offset, 4);	/* write the filler_prefix */
		jnl_putstr(jpc, (uchar_ptr_t)fixed_section, jnl_fixed_size[rectype]);
		if (blk_ptr != NULL)
			jnl_putstr(jpc, (uchar_ptr_t)blk_ptr, blk_ptr->bsiz);
		m = ROUND_UP(jpc->temp_free, JNL_REC_START_BNDRY) - jpc->temp_free;
		if (0 != m)
			jnl_putstr(jpc, (uchar_ptr_t)zeroes, m);
		offset = 0;
		jnl_putstr(jpc, (uchar_ptr_t)&offset, 4);	/* write the filler_suffix */

		offset = n - JREC_SUFFIX_SIZE;	/* don't count the suffix */
		jnl_putstr(jpc, (uchar_ptr_t)THREE_LOW_BYTES(offset), 3);
		jnl_putchar(jpc, JNL_REC_TRAILER);
	} else
	{
		offset = jb->freeaddr - jb->lastaddr;
		memcpy(fjlist_header->buff + 1, (uchar_ptr_t)THREE_LOW_BYTES(offset), 3);
		jnl_putstr(jpc, (uchar_ptr_t)fjlist_header->buff, n);
	}
	assert((jpc->temp_free - jb->free + jb->size) % jb->size == n);
	assert(jb->buff[jb->free] == rectype);
	assert(jb->free < jpc->temp_free  ||  jpc->temp_free < jb->dsk);
	assert(jb->freeaddr >= jb->dskaddr);
	m = ((jpc->temp_free + ~JNL_WRT_END_MASK) & JNL_WRT_END_MASK) - jpc->temp_free;
	assert(0 == m);
	if (m != 0)
		memset(jb->buff + jpc->temp_free, 0, m);
	jpc->lastwrite = jb->freeaddr;
	jpc->new_freeaddr = jb->freeaddr + n;
	assert(jpc->temp_free == jpc->new_freeaddr % jb->size);
	if (REPL_ENABLED(csa->hdr)  &&  (NULL != fjlist_header || JRT_TCOM == rectype))
	{
		assert(NULL != jnlpool.jnlpool_ctl && NULL != jnlpool_ctl); /* ensure we haven't yet detached from the jnlpool */
		assert((&FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs)->now_crit);	/* ensure we have the jnl pool lock */
		DEBUG_ONLY(cu_jnl_index++;)
		b = jpc->jnl_buff->size - jb->free;
		p = temp_jnlpool_ctl->jnlpool_size - temp_jnlpool_ctl->write;
		if (n <= b)
		{
			if (n <= p)	/* p & b >= n  (most frequent case) */
				memcpy(jnldata_base + temp_jnlpool_ctl->write,	   jb->buff + jb->free,	    n	 );
			else		/* p < n <= b */
			{
				memcpy(jnldata_base + temp_jnlpool_ctl->write,	   jb->buff + jb->free,	    p	 );
				memcpy(jnldata_base,				   jb->buff + jb->free + p, n - p);
			}
		} else
		{
			if (n <= p)		/* b < n <= p */
			{
				memcpy(jnldata_base + temp_jnlpool_ctl->write,     jb->buff + jb->free,     b    );
				memcpy(jnldata_base + temp_jnlpool_ctl->write + b, jb->buff,                n - b);
			} else if (p == b)	/* p = b < n */
			{
				memcpy(jnldata_base + temp_jnlpool_ctl->write,     jb->buff + jb->free,     p    );
				memcpy(jnldata_base,                               jb->buff,                n - p);
			} else if (p < b)	/* p < b < n */
			{
				memcpy(jnldata_base + temp_jnlpool_ctl->write,     jb->buff + jb->free,     p    );
				memcpy(jnldata_base,                               jb->buff + jb->free + p, b - p);
				memcpy(jnldata_base + b - p,                       jb->buff,                n - b);
			} else			/* b < p < n */
			{
				memcpy(jnldata_base + temp_jnlpool_ctl->write,     jb->buff + jb->free,     b    );
				memcpy(jnldata_base + temp_jnlpool_ctl->write + b, jb->buff,                p - b);
				memcpy(jnldata_base,                               jb->buff + p - b,        n - p);
			}
		}
		temp_jnlpool_ctl->write = temp_jnlpool_ctl->write + n;
		if (temp_jnlpool_ctl->write >= temp_jnlpool_ctl->jnlpool_size)
			temp_jnlpool_ctl->write -= temp_jnlpool_ctl->jnlpool_size;
	}
	/* Note that freeaddr should be updated ahead of free since jnl_output_sp.c does computation of wrtsize
	 * based on free and asserts follow later there which use freeaddr.
	 */
	jpc->free_update_inprog = TRUE; /* The following assignments must be protected against termination */
	jb->lastaddr = jpc->lastwrite;
	jb->freeaddr = jpc->new_freeaddr;
	jb->free = jpc->temp_free;
	jpc->free_update_inprog = FALSE;
	VMS_ONLY(
		if (((jb->freeaddr - jb->dskaddr) > jb->min_write_size)
		    && (SS_NORMAL != (status = jnl_qio_start(jpc))) && (ERR_JNLWRTNOWWRTR != status) && (ERR_JNLWRTDEFER != status))
	        {
			jb->blocked = 0;
			jnl_file_lost(jpc, status);
			return;
		}
	)
	if (dba_mm == jpc->region->dyn.addr->acc_meth)
		jnl_mm_timer(csa, jpc->region);
}
