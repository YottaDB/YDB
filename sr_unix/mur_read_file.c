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

#include "gtm_fcntl.h"
#include <unistd.h>
#include "gtm_stat.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "muprec.h"
#include "iosp.h"
#include "copy.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "util.h"
#include "gtmmsg.h"

#define NEAR_SWTCH2_LINEAR	7

#define JNL_FILE_FIRST_RECORD	ROUND_UP(sizeof(jnl_file_header), DISK_BLOCK_SIZE)

error_def(ERR_JNLALIGN);
error_def(ERR_JNLRECFMT);
error_def(ERR_JNLREADEOF);
error_def(ERR_JNLBADLABEL);

GBLREF	char		*log_rollback;
GBLREF	mur_opt_struct	mur_options;

unsigned char	*mur_read_copy;
unsigned char	*jnl_curr_rec_copy; 	/* to be used by mur_get_pini_jpv, otherwise can be static */

static	boolean_t	tail_analysis = FALSE;


static	int4	file_read (struct mur_buffer_desc *b)
{
	struct mur_file_control	*fc;
	off_t			diff;
	int			n, status;

	fc = b->backptr;
	diff = (off_t)fc->eof_addr - b->txtlen;
	if ((off_t)b->dskaddr > diff)
		b->txtlen = fc->eof_addr > b->dskaddr ?
				fc->eof_addr - b->dskaddr :
				-((int4)(b->dskaddr - fc->eof_addr));
	if (b->txtlen <= 0)
		return SS_NORMAL;
	n = (b->txtlen + 1) & ~1;	/* only even length transfers allowed */
	LSEEKREAD(fc->fd, b->dskaddr, b->txtbase, n, status);
	if (0 != status)
	{
		if (-1 == status)	/* Length was wrong */
			status = (int4)ERR_JNLREADEOF;
	}
	if (status == SS_NORMAL  &&  b->dskaddr >= b->backptr->eof_addr)
		status = (int4)ERR_JNLREADEOF;
	return status;
}

static	int4	blkzeros (struct mur_file_control *fc, off_t offset)
{
	struct mur_buffer_desc	*b;
	unsigned char		*cp;
	int4			status;

	b = &fc->random_buff;
	b->txtbase = b->bufbase;
	b->txtlen = fc->blocksize;
	b->dskaddr = (uint4)offset;
	if ((status = file_read(b)) != SS_NORMAL)
		return status;
	for (cp = fc->random_buff.txtbase + b->txtlen - 1;  cp >= fc->random_buff.txtbase;  --cp)
	{
		if (*cp != 0)
			return SS_NORMAL;
	}
	return -1;
}

static	int4	blktrailer (mur_rab *r, uint4 da, unsigned char *base, int4 len)
{
	jrec_suffix	suffix;
	unsigned char	*cp;
	int4		status;

	for (cp = base + len - 1, status = -1;  cp >= base  &&  status != SS_NORMAL;  --cp)
	{
		if (*cp == JNL_REC_TRAILER)
		{
			memcpy(&suffix, cp - JREC_SUFFIX_SIZE + 1, JREC_SUFFIX_SIZE);
			if ((status = mur_read(r, da + (cp - JREC_SUFFIX_SIZE + 1 - base) - suffix.backptr))
					== (int4)ERR_JNLREADEOF)
				return status;
		}
	}
	if (status == SS_NORMAL)
		return -1;
	if (cp < base)
		return SS_NORMAL;
	return status;
}
/*
 * Note: This routine assumes that:
 *	1.  Successive calls are for reading the file sequentially forward -- not skipping any records;
 *	2.  Buffer size is greater than maximum record size.
 */
static	int4	set_recpos_fwd (mur_rab *r, uint4 da, int4 ln)
{
	struct mur_file_control	*fc;
	struct mur_buffer_desc	*b, *b1;
	unsigned char		*cp;
	int4			n, m, rem, status;

	fc = r->pvt;
	if (da > fc->eof_addr - ln)
		return (int4)ERR_JNLREADEOF;
	b = &fc->seq_buff[fc->bufindex];
	if ((n = da - b->dskaddr) > b->txtlen)
	{
		b1 = b;
		b = &fc->seq_buff[fc->bufindex = !fc->bufindex];	/* Caution: embedded asignment */
		b1->dskaddr = b->dskaddr + b->txtlen;
		b1->txtlen = b1->buflen;

		if ((status = file_read(b1)) != SS_NORMAL)
			return status;
		n = da - b->dskaddr;
	}
	if (n < 0  ||  n > b->txtlen)
		GTMASSERT;
	fc->seq_recaddr = b->txtbase + n;
	fc->seq_reclen = ln;
	r->dskaddr = da;
	if ((rem = b->txtlen - n - ln) < 0)
	{
		b = &fc->seq_buff[!fc->bufindex];
		rem += b->txtlen;
		if (fc->bufindex)
		{
			m = fc->seq_buff[1].txtlen - n;
			cp = fc->seq_buff[0].bufbase - m;
			memcpy(cp, fc->seq_recaddr, m);
			fc->seq_recaddr = cp;
		}
	}
	fc->txt_remaining = rem;
	return SS_NORMAL;
}

mur_rab	*mur_rab_create (int buffer_size)
{
	mur_rab			*r;
	struct mur_file_control	*fc;
	int4			n;

	r = (mur_rab *)malloc(sizeof(*r));
	memset(r, 0, sizeof(*r));
	fc = r->pvt
	   = (struct mur_file_control *)malloc(sizeof(*fc));
	memset(fc, 0, sizeof(*fc));
	n = fc->blocksize
	  = buffer_size;
	fc->alloc_len = n * 3;
	fc->alloc_base = (unsigned char *)malloc(fc->alloc_len);
	fc->random_buff.bufbase = (unsigned char *)malloc(n);
	fc->seq_buff[0].bufbase = fc->alloc_base + n;
	fc->seq_buff[1].bufbase = fc->seq_buff[0].bufbase + n;
	fc->random_buff.buflen = fc->seq_buff[0].buflen
			       = fc->seq_buff[1].buflen
			       = n;
	fc->random_buff.backptr = fc->seq_buff[0].backptr
				= fc->seq_buff[1].backptr
				= fc;
	return r;
}

int4	mur_fopen (mur_rab *r, char *fna, int fnl)
{
	struct mur_file_control	*fc;
	jnl_file_header		*cp;
	int4			status, n;

	fc = r->pvt;
	/* We have to have write permission for recover and rollback */
	/* We do not need to write in journal file for mupip journal extract/show/verify.  So open it as read-only */
	if ((status = fc->fd = OPEN(fna, (mur_options.update ? O_RDWR : O_RDONLY))) == -1)
	{
		util_out_print("Error opening journal file !AD", TRUE, fnl, fna);
		return errno;
	}
	n = ROUND_UP(sizeof(jnl_file_header), 8);
	cp = fc->jfh
	   = (jnl_file_header *)malloc(n);
	LSEEKREAD(fc->fd, 0, cp, n, status);
	if (0 != status)
	{
		if (-1 == status)
			status = (int4)ERR_JNLREADEOF;
	} else if (0 != memcmp(cp->label, LIT_AND_LEN(JNL_LABEL_TEXT)))
			status = (int4)ERR_JNLBADLABEL;
	if (status != SS_NORMAL)
	{
		util_out_print("Error opening journal file !AD", TRUE, fnl, fna);
		gtm_putmsg(status);
		close(fc->fd);
		return status;
	}
	return SS_NORMAL;
}
int4	mur_fread_eof (mur_rab *r, char *fna, int fnl)
{
	struct mur_buffer_desc  *b;
	struct mur_file_control	*fc;
	struct stat		stat_buf;
	int4			status;
	int			fstat_res;
	off_t			dskaddr, last_rec_addr,
				eof_addr, start_offset;
	unsigned char		*text;
	jrec_suffix		suffix;
	boolean_t		proceed;
	uint4			check_time;
	jnl_record		*jrec;

	fc = r->pvt;
	FSTAT_FILE(fc->fd, &stat_buf, fstat_res);
	if (-1 == fstat_res)
		return errno;
	fc->eof_addr = stat_buf.st_size;
	tail_analysis = TRUE;
	b = fc->seq_buff;
	assert(0 == fc->blocksize % fc->jfh->alignsize);
	dskaddr = ROUND_DOWN(fc->eof_addr, fc->jfh->alignsize);
	for (; ;)
	{
		if (dskaddr >= fc->blocksize)
		{
			dskaddr -= fc->jfh->alignsize;
			b->txtlen = fc->blocksize;
			b->dskaddr = dskaddr;
			b->txtlen = fc->blocksize;
			b->txtbase = b->bufbase;
			if (SS_NORMAL == (status = file_read(b)))
			{
				text = b->txtbase + b->txtlen - JREC_SUFFIX_SIZE;
				memcpy(&suffix, text, JREC_SUFFIX_SIZE);
				if (JNL_REC_TRAILER != suffix.suffix_code  ||  ALIGN_KEY != suffix.filler_suffix
						||  suffix.backptr > (b->txtlen + JREC_SUFFIX_SIZE)
						||  JRT_ALIGN != (((jrec_prefix *)(text - suffix.backptr))->jrec_type)
						||  (JNL_REC_TRAILER != *(text - suffix.backptr - 1)
							&&  '\0' != *(text - suffix.backptr - 1)))
					continue;		/* Something wrong here, go back and look for something ok */
				start_offset = dskaddr + fc->jfh->alignsize - suffix.backptr - JREC_SUFFIX_SIZE;
				check_time = ((jnl_record *)(text - suffix.backptr))->val.jrec_align.short_time;
				break;
			}
		} else
		{
			start_offset = JNL_FILE_FIRST_RECORD;
			check_time = fc->jfh->bov_timestamp;
			break;
		}
	}
	if (log_rollback)
		util_out_print("MUR-I-DEBUG : Journal !AD  -->  Start_offset = 0x!XL --> Check_time = 0x!XL",
			TRUE, fnl, fna, start_offset, check_time);
	if (SS_NORMAL != mur_next(r, start_offset))
		GTMASSERT;
	last_rec_addr = r->dskaddr;
	eof_addr = r->dskaddr + r->reclen;
	for (proceed = TRUE; TRUE == proceed; )
	{
		if (SS_NORMAL != mur_next(r, 0))
			break;
		jrec = (jnl_record *)r->recbuff;
		switch(jrec->jrec_type)
		{
		case JRT_KILL:
		case JRT_ZTCOM:
		case JRT_FKILL:
		case JRT_GKILL:
		case JRT_SET:
		case JRT_FSET:
		case JRT_GSET:
		case JRT_PBLK:
		case JRT_AIMG:
		case JRT_EPOCH:
		case JRT_TKILL:
		case JRT_UKILL:
		case JRT_TSET:
		case JRT_USET:
		case JRT_TCOM:
		case JRT_NULL:
		case JRT_ZKILL:
		case JRT_FZKILL:
		case JRT_GZKILL:
		case JRT_TZKILL:
		case JRT_UZKILL:
		case JRT_INCTN:
			assert(&jrec->val.jrec_kill.short_time == &jrec->val.jrec_ztcom.tc_short_time);
			assert(&jrec->val.jrec_kill.short_time == &jrec->val.jrec_fkill.short_time);
			assert(&jrec->val.jrec_kill.short_time == &jrec->val.jrec_gkill.short_time);
			assert(&jrec->val.jrec_kill.short_time == &jrec->val.jrec_set.short_time);
			assert(&jrec->val.jrec_kill.short_time == &jrec->val.jrec_fset.short_time);
			assert(&jrec->val.jrec_kill.short_time == &jrec->val.jrec_gset.short_time);
			assert(&jrec->val.jrec_kill.short_time == &jrec->val.jrec_pblk.short_time);
			assert(&jrec->val.jrec_kill.short_time == &jrec->val.jrec_epoch.short_time);
			assert(&jrec->val.jrec_kill.short_time == &jrec->val.jrec_tkill.short_time);
			assert(&jrec->val.jrec_kill.short_time == &jrec->val.jrec_ukill.short_time);
			assert(&jrec->val.jrec_kill.short_time == &jrec->val.jrec_tset.short_time);
			assert(&jrec->val.jrec_kill.short_time == &jrec->val.jrec_uset.short_time);
			assert(&jrec->val.jrec_kill.short_time == &jrec->val.jrec_tcom.tc_short_time);
			assert(&jrec->val.jrec_kill.short_time == &jrec->val.jrec_null.short_time);
			assert(&jrec->val.jrec_kill.short_time == &jrec->val.jrec_zkill.short_time);
			assert(&jrec->val.jrec_kill.short_time == &jrec->val.jrec_fzkill.short_time);
			assert(&jrec->val.jrec_kill.short_time == &jrec->val.jrec_gzkill.short_time);
			assert(&jrec->val.jrec_kill.short_time == &jrec->val.jrec_tzkill.short_time);
			assert(&jrec->val.jrec_kill.short_time == &jrec->val.jrec_uzkill.short_time);
			assert(&jrec->val.jrec_kill.short_time == &jrec->val.jrec_inctn.short_time);
			assert(&jrec->val.jrec_kill.short_time == &jrec->val.jrec_aimg.short_time);
			if (check_time > jrec->val.jrec_kill.short_time)
			{
				assert(FALSE);
				proceed = FALSE;		/* how can we see an earlier-jnl-record */
			} else
				check_time = jrec->val.jrec_kill.short_time;
			break;
		case JRT_PINI:
		case JRT_PFIN:
		case JRT_EOF:
			assert(&jrec->val.jrec_pini.process_vector == &jrec->val.jrec_pfin.process_vector);
			assert(&jrec->val.jrec_pini.process_vector == &jrec->val.jrec_eof.process_vector);
			if (check_time > jrec->val.jrec_pini.process_vector.jpv_time)
			{
				assert(FALSE);
				proceed = FALSE;		/* how can we see an earlier-jnl-record */
			} else
				check_time = jrec->val.jrec_pini.process_vector.jpv_time;
			break;
		default:
			GTMASSERT;
		}
		if (FALSE == proceed)
			break;
		last_rec_addr = r->dskaddr;
		eof_addr = r->dskaddr + r->reclen;
	}
	fc->last_record = last_rec_addr;
	fc->eof_addr = eof_addr;
	if (log_rollback)
		util_out_print("MUR-I-DEBUG : Journal !AD  -->  Last Record = 0x!XL --> Eof_Addr = 0x!XL",
			TRUE, fnl, fna, fc->last_record, fc->eof_addr);
	tail_analysis = FALSE;
	return SS_NORMAL;
}

int4	mur_close (mur_rab *r)
{
	struct mur_file_control	*fc;
	int4			status;

	fc = r->pvt;
	if ((status = close(fc->fd)) == -1)
		return errno;
	free(fc->random_buff.bufbase);
	free(fc->alloc_base);
	free(fc);
	free(r);
	return SS_NORMAL;
}

uint4	mur_read (mur_rab *r, uint4 dskaddr)
{
	struct mur_buffer_desc	*b;
	struct mur_file_control	*fc;
	unsigned char		*cp;
	int4			n, m, status;

	fc = r->pvt;
	b = &fc->random_buff;
	b->dskaddr = dskaddr & ~(DISK_BLOCK_SIZE - 1);
	b->txtlen = b->buflen;
	b->txtbase = b->bufbase;
	if ((status = file_read(b)) == SS_NORMAL)
	{
		cp = (dskaddr & (DISK_BLOCK_SIZE - 1)) + b->txtbase;
		m = cp - b->txtbase + b->txtlen;
		n = jnl_record_length((jnl_record *)cp, m);
		if (n < 0)
		{
			assert(tail_analysis);
			return (int4)ERR_JNLRECFMT;
		}
		if (n > m)
		{
			assert(tail_analysis);
			return (int4)ERR_JNLALIGN;
		}
		r->recaddr = cp;
		r->reclen = n;
		if (((int)r->recaddr & 7) == 0)
			r->recbuff = r->recaddr;
		else
		{
			assert(NULL != mur_read_copy);
			memcpy(mur_read_copy, r->recaddr, n);
			r->recbuff = mur_read_copy;
		}
	}
	return status;
}

uint4	mur_next (mur_rab *r, uint4 dskaddr)
{
	struct mur_buffer_desc	*b, *b1;
	struct mur_file_control	*fc;
	jrec_suffix		suffix;
	unsigned char		*cp;
	int4			n, m, status;
	uint4			da, datmp;

	fc = r->pvt;
	if (dskaddr != 0)
	{
		fc->bufindex = 0;
		b = fc->seq_buff;
		b->txtbase = b->bufbase;
		b->txtlen = b->buflen;
		b->dskaddr = dskaddr & ~(DISK_BLOCK_SIZE - 1);
		if ((status = file_read(b)) != SS_NORMAL)
			return status;
		r->dskaddr = b->dskaddr;
		fc->seq_reclen = dskaddr - b->dskaddr;
		fc->seq_recaddr = b->txtbase;
		b1 = b + 1;
		b1->txtbase = b1->bufbase;
		b1->dskaddr = b->dskaddr + b->txtlen;
		b1->txtlen = b1->buflen;
		if ((status = file_read(b1)) != SS_NORMAL)
			return status;
	}
	da = r->dskaddr + fc->seq_reclen;
	b = &fc->seq_buff[fc->bufindex];
	if ((status = set_recpos_fwd(r, da, 1)) != SS_NORMAL)
		return status;
	if (*fc->seq_recaddr == 0)
	{
		if ((datmp = (da & ~(DISK_BLOCK_SIZE - 1))) == da)
		{
			assert(tail_analysis);
			return (int4)ERR_JNLALIGN;
		}
		datmp += DISK_BLOCK_SIZE;
		if ((status = set_recpos_fwd(r, datmp, 1)) != SS_NORMAL)
			return status;
		da = r->dskaddr;
	}
	b = &fc->seq_buff[fc->bufindex];
	m = fc->txt_remaining + fc->seq_reclen;
	n = jnl_record_length((jnl_record *)fc->seq_recaddr, m);
	if (n > m)
	{
		if ((status = set_recpos_fwd(r, da, n)) != SS_NORMAL)
			return status;
		b = &fc->seq_buff[fc->bufindex];
		m = fc->txt_remaining + fc->seq_reclen;
		n = jnl_record_length((jnl_record *)fc->seq_recaddr, m);
		if (n > m)
		{
			if (tail_analysis)
				return ERR_JNLREADEOF;
			else
				GTMASSERT;
		}
	}
	if (n < 0)
	{
		assert(tail_analysis);
		return (int4)ERR_JNLRECFMT;
	}
	fc->seq_reclen = n;
	memcpy(&suffix, fc->seq_recaddr + n - JREC_SUFFIX_SIZE, JREC_SUFFIX_SIZE);
	if (suffix.suffix_code != JNL_REC_TRAILER)
	{
		assert(tail_analysis);
		return (int4)ERR_JNLRECFMT;
	}
	if (suffix.backptr + JREC_SUFFIX_SIZE != n)
	{
		assert(tail_analysis);
		return (int4)ERR_JNLALIGN;
	}
	r->recaddr = fc->seq_recaddr;
	r->reclen = fc->seq_reclen;
	if (((int)r->recaddr & 7) == 0)
		r->recbuff = r->recaddr;
	else
	{
		assert(NULL != mur_read_copy);
		memcpy(mur_read_copy, r->recaddr, n);
		r->recbuff = mur_read_copy;
	}
	return SS_NORMAL;
}

uint4	mur_previous (mur_rab *r, uint4 dskaddr)
{	/* THIS COULD BE IMPROVED BY DOUBLE BUFFERING */
	struct mur_buffer_desc	*b;
	struct mur_file_control	*fc;
	jrec_suffix		suffix;
	int4			n, jnlreclenret, status;
	uint4			da, datmp;

	fc = r->pvt;
	b = &fc->seq_buff[fc->bufindex = 1];
	if ((da = dskaddr) == 0)
	{
		GET_LONG(n, fc->seq_recaddr);
#ifdef BIGENDIAN
		n &= 0x00FFFFFF;
#else
		n = ((jnl_record *)&n)->jrec_backpointer;
#endif
		if (n == 0)
			return (int4)ERR_JNLREADEOF;
		da = r->dskaddr < n ? 0 : r->dskaddr - n;
	}
	if (da < JNL_FILE_FIRST_RECORD)
		return (int4)ERR_JNLREADEOF;
	if (dskaddr != 0  ||  da < b->dskaddr)
	{
		if ((datmp = dskaddr) == 0)
		{
			datmp = (r->dskaddr + (DISK_BLOCK_SIZE - 1)) & ~(DISK_BLOCK_SIZE - 1);
			datmp = datmp < b->buflen ? JNL_FILE_FIRST_RECORD : datmp - b->buflen;
			assert((b->buflen & (DISK_BLOCK_SIZE - 1)) == 0);
			if (datmp < JNL_FILE_FIRST_RECORD)
				datmp = JNL_FILE_FIRST_RECORD;
		} else
			datmp &= ~(DISK_BLOCK_SIZE - 1);
		b->dskaddr = datmp;
		b->txtbase = b->bufbase;
		b->txtlen = b->buflen;
		if ((status = file_read(b)) != SS_NORMAL)
			return status;
	}
	r->dskaddr = da;
	fc->seq_recaddr = r->recaddr
			= da - b->dskaddr + b->txtbase;
	n = fc->seq_recaddr - b->txtbase + b->txtlen;
	jnlreclenret = jnl_record_length((jnl_record *)fc->seq_recaddr, n);
	if (0 > jnlreclenret)
	{
		assert(tail_analysis);
		return (int4)ERR_JNLRECFMT;
	}
	if (jnlreclenret > n)
	{
		assert(tail_analysis);
		return (int4)ERR_JNLALIGN;
	}
	fc->seq_reclen = r->reclen = (unsigned int)jnlreclenret;
	memcpy(&suffix, fc->seq_recaddr + fc->seq_reclen - JREC_SUFFIX_SIZE, JREC_SUFFIX_SIZE);
        if (suffix.suffix_code != JNL_REC_TRAILER)
	{
		assert(tail_analysis);
                return (int4)ERR_JNLRECFMT;
	}
        if (suffix.backptr + JREC_SUFFIX_SIZE != fc->seq_reclen)
	{
		assert(tail_analysis);
                return (int4)ERR_JNLALIGN;
	}
	if (((int)r->recaddr & 7) == 0)
		r->recbuff = r->recaddr;
	else
	{
		assert(NULL != mur_read_copy);
		memcpy(mur_read_copy, r->recaddr, r->reclen);
		r->recbuff = mur_read_copy;
	}
	return SS_NORMAL;
}

int4	mur_get_first (mur_rab *r)
{
	return mur_next(r, JNL_FILE_FIRST_RECORD);
}


int4	mur_get_last (mur_rab *r)
{
	return mur_previous(r, r->pvt->last_record);
}


jnl_file_header	*mur_get_file_header (mur_rab *r)
{
	return r->pvt->jfh;
}
