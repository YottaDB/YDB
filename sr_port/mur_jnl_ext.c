/****************************************************************
 *								*
 *	Copyright 2003, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "jnl_typedef.h"
#include "copy.h"
#include "init_root_gv.h"
#include "min_max.h"          	/* needed for init_root_gv.h */
#include "format_targ_key.h"   	/* needed for init_root_gv.h */
#include "mlkdef.h"
#include "zshow.h"
#include "mur_jnl_ext.h"
#include "op.h"
#include "real_len.h"		/* for real_len() prototype */
#include "gtmmsg.h"

GBLREF	gv_key		*gv_currkey;
GBLREF 	mur_gbls_t	murgbl;
GBLREF	jnl_ctl_list	*mur_jctl;
GBLREF 	mur_rab_t	mur_rab;
GBLREF	mur_opt_struct	mur_options;
GBLREF	boolean_t	is_updproc;
GBLREF  mval            curr_gbl_root;
GBLREF	char		muext_code[][2];
LITREF	int		jrt_update[JRT_RECTYPES];
LITREF	char		*jrt_label[JRT_RECTYPES];

#define LAB_LEN 	7
#define LAB_TERM	"\\"
#define LAB_TERM_SZ	(sizeof(LAB_TERM) - 1)

/* This routine formats and outputs journal extract records
   corresponding to M SET, KILL, ZKILL, TSTART, and ZTSTART commands */
void	mur_extract_set(fi_type *fi, jnl_record *rec, pini_list_struct *plst)
{
	enum jnl_record_type	rectype;
	int			max_blen, actual, extract_len, val_extr_len, val_len;
	char			*val_ptr, *ptr, *buff;
	jnl_string		*keystr;

	error_def		(ERR_MUINFOUINT4);
	error_def		(ERR_JNLBADRECFMT);
	error_def		(ERR_TEXT);

	if (mur_options.detail)
		SPRINTF(murgbl.extr_buff, "0x%08x [0x%04x] :: ", mur_jctl->rec_offset, mur_rab.jreclen);
	extract_len = (mur_options.detail ? STRLEN(murgbl.extr_buff) : 0);
	rectype = (enum jnl_record_type)rec->prefix.jrec_type;
	if (IS_FUPD_TUPD(rectype))
	{
		if (!mur_options.detail)
		{
			if (IS_TUPD(rectype))
			{
				EXT2BYTES(&muext_code[MUEXT_TSTART][0]); /* TSTART */
			} else /* if (IS_FUPD(rectype)) */
			{
				EXT2BYTES(&muext_code[MUEXT_ZTSTART][0]); /* ZTSTART */
			}
		} else
		{
			if (IS_TUPD(rectype))
				strcpy(murgbl.extr_buff + extract_len, "TSTART \\");
			else /* if (IS_FUPD(rectype)) */
				strcpy(murgbl.extr_buff + extract_len, "ZTSTART\\");
			extract_len = STRLEN(murgbl.extr_buff);
		}
		EXTTIME(rec->prefix.time);
		EXTQW(rec->prefix.tn);
		if (mur_options.detail)
			EXTINT(rec->prefix.checksum);
		EXTPID(plst);
		if (IS_ZTP(rectype))
		{
			EXTQW(rec->jrec_gset.token);
		}
		EXTQW(rec->jrec_set.token_seq.jnl_seqno);
		jnlext_write(fi, murgbl.extr_buff, extract_len);
	}
	/* Output the SET or KILL or ZKILL record */
	if (!mur_options.detail)
	{
		extract_len = 0;
		if (IS_SET(rectype))
		{
			EXT2BYTES(&muext_code[MUEXT_SET][0]);
		} else if (IS_KILL(rectype))
		{
			EXT2BYTES(&muext_code[MUEXT_KILL][0]);
		} else if (IS_ZKILL(rectype))
		{
			EXT2BYTES(&muext_code[MUEXT_ZKILL][0]);
		}
	}
	else
	{
		if (IS_FUPD_TUPD(rectype))
		{
			memcpy(murgbl.extr_buff, "                       ", 23);
			extract_len = 23;
		} else
			extract_len = STRLEN(murgbl.extr_buff);
		strcpy(murgbl.extr_buff + extract_len, "       \\");
		memcpy(murgbl.extr_buff + extract_len, jrt_label[rectype], LAB_LEN);
		extract_len += LAB_LEN;
		memcpy(murgbl.extr_buff + extract_len, LAB_TERM, LAB_TERM_SZ);
		extract_len += LAB_TERM_SZ;
	}
	EXTTIME(rec->prefix.time);
	EXTQW(rec->prefix.tn);
	if (mur_options.detail)
		EXTINT(rec->prefix.checksum);
	EXTPID(plst);
	if (IS_ZTP(rectype))
	{
		EXTQW(rec->jrec_gset.token);
	} else
	{
		EXTQW(rec->jrec_kill.token_seq.jnl_seqno);
	}
	assert(IS_SET_KILL_ZKILL(rectype));
	buff = &murgbl.extr_buff[extract_len];
	max_blen = MIN(MAX_ZWR_KEY_SZ, murgbl.max_extr_record_length - extract_len);
	assert(MAX_ZWR_KEY_SZ == max_blen);	/* We allocated enough for key and data expansion for ZWR format */
	ptr = (char *)format_targ_key((uchar_ptr_t)buff, max_blen, gv_currkey, TRUE);
	assert(NULL != ptr);
	if (NULL != ptr)
	{
		extract_len += (int)(ptr - &murgbl.extr_buff[extract_len]);
		if (IS_SET(rectype))
		{
			murgbl.extr_buff[extract_len++] = '=';
			if (IS_ZTP(rectype))
				/* ZTP has different record format than TP or non-TP */
				keystr = (jnl_string *)&rec->jrec_fkill.mumps_node;
			else
				/* TP and non-TP has same format */
				keystr = (jnl_string *)&rec->jrec_kill.mumps_node;
			val_ptr = &keystr->text[keystr->length];
			GET_MSTR_LEN(val_len, val_ptr);
			if (ZWR_EXP_RATIO(val_len) <= murgbl.max_extr_record_length - extract_len)
			{
				val_ptr += sizeof(mstr_len_t);
				ptr = &murgbl.extr_buff[extract_len];
				format2zwr((sm_uc_ptr_t)val_ptr, val_len, (unsigned char *)ptr, &val_extr_len);
				extract_len += val_extr_len;
			} else
			{
				gtm_putmsg(VARLSTCNT(9) ERR_JNLBADRECFMT,
					3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn, mur_jctl->rec_offset,
					ERR_TEXT, 2, LEN_AND_LIT("Length of the record is too high for zwr format"));
				if (mur_options.verbose || mur_options.detail)
				{
					gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT4, 4,
						LEN_AND_LIT("After max expansion record length"),
						ZWR_EXP_RATIO(val_len), ZWR_EXP_RATIO(val_len));
					gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("Buffer size"),
						murgbl.max_extr_record_length - extract_len,
						murgbl.max_extr_record_length - extract_len);
				}
				assert(FALSE);
			}
		}
	}
	jnlext_write(fi, murgbl.extr_buff, extract_len + 1);
}

void	mur_extract_null(fi_type *fi, jnl_record *rec, pini_list_struct *plst)
{
	int			extract_len;
	char			*ptr;

	extract_len = 0;
	if (!mur_options.detail)
	{
		EXT2BYTES(&muext_code[MUEXT_NULL][0]);
	} else
	{
		EXT_DET_PREFIX();
	}
	EXTTIME(rec->prefix.time);
	EXTQW(rec->prefix.tn);
	if (mur_options.detail)
		EXTINT(rec->prefix.checksum);
	EXTPID(plst);
	EXTQW(rec->jrec_null.jnl_seqno);
	jnlext_write(fi, murgbl.extr_buff, extract_len);
}

void	mur_extract_align(fi_type *fi, jnl_record *rec, pini_list_struct *plst)
{
	int	extract_len;
	char	*ptr;

	if (!mur_options.detail)
		return;
	EXT_DET_PREFIX();
	EXTTIME(rec->prefix.time);
	EXTQW(rec->prefix.tn);
	if (mur_options.detail)
		EXTINT(rec->prefix.checksum);
	EXTPID(plst);
	jnlext_write(fi, murgbl.extr_buff, extract_len);
}

void	mur_extract_blk(fi_type *fi, jnl_record *rec, pini_list_struct *plst)
{
	int	extract_len;
	blk_hdr	pblk_head;
	char	*ptr;

	if (!mur_options.detail)
		return;
	EXT_DET_PREFIX();
	EXTTIME(rec->prefix.time);
	EXTQW(rec->prefix.tn);
	if (mur_options.detail)
		EXTINT(rec->prefix.checksum);
	EXTPID(plst);
	EXTINT(rec->jrec_pblk.blknum);
	EXTINT(rec->jrec_pblk.bsiz);
	memcpy((char*)&pblk_head, (char*)&rec->jrec_pblk.blk_contents[0], sizeof(blk_hdr));
	EXTQW(pblk_head.tn);
	EXTINT(rec->jrec_pblk.ondsk_blkver);
	jnlext_write(fi, murgbl.extr_buff, extract_len);
}

void	mur_extract_epoch(fi_type *fi, jnl_record *rec, pini_list_struct *plst)
{
	int	extract_len;
	char	*ptr;

	if (!mur_options.detail)
		return;
	EXT_DET_PREFIX();
	EXTTIME(rec->prefix.time);
	EXTQW(rec->prefix.tn);
	if (mur_options.detail)
		EXTINT(rec->prefix.checksum);
	EXTPID(plst);
	EXTQW(rec->jrec_epoch.jnl_seqno);
	EXTINT(rec->jrec_epoch.blks_to_upgrd);
	EXTINT(rec->jrec_epoch.free_blocks);
	EXTINT(rec->jrec_epoch.total_blks);
	jnlext_write(fi, murgbl.extr_buff, extract_len);
}

void    mur_extract_inctn(fi_type *fi, jnl_record *rec, pini_list_struct *plst)
{
        int     extract_len;
        char    *ptr;

	if (!mur_options.detail)
		return;
	EXT_DET_PREFIX();
        EXTTIME(rec->prefix.time);
	EXTQW(rec->prefix.tn);
	if (mur_options.detail)
		EXTINT(rec->prefix.checksum);
	EXTPID(plst);
	EXTINT(rec->jrec_inctn.opcode);
	EXTINT(rec->jrec_inctn.detail.blks_to_upgrd_delta);
	/* currently "detail" member in the inctn journal record is a union (type inctn_detail_t) of two 4-byte fields.
	 * hence extracting an int of one of the fields is enough to extract either.
	 * but this will no longer hold true if the size of one of the union's fields changes.
	 * add an assert here in case inctn_detail_t type changes.
	 */
	assert(sizeof(rec->jrec_inctn.detail) == sizeof(rec->jrec_inctn.detail.blks_to_upgrd_delta));
	jnlext_write(fi, murgbl.extr_buff, extract_len);
}

void	mur_extract_eof(fi_type *fi, jnl_record *rec, pini_list_struct *plst)
{
	int			extract_len = 0;
	char			*ptr;

	if (!mur_options.detail)
	{
		EXT2BYTES(&muext_code[MUEXT_EOF][0]);
	} else
	{
		EXT_DET_PREFIX();
	}
	EXTTIME(rec->jrec_eof.prefix.time);
	EXTQW(rec->jrec_eof.prefix.tn);
	if (mur_options.detail)
		EXTINT(rec->prefix.checksum);
	EXTPID(plst);
	EXTQW(rec->jrec_eof.jnl_seqno);
	jnlext_write(fi, murgbl.extr_buff, extract_len);
}

void	mur_extract_pfin(fi_type *fi, jnl_record *rec, pini_list_struct *plst)
{
	int	extract_len;
	char	*ptr;

	extract_len = 0;
	if (!mur_options.detail)
	{
		EXT2BYTES(&muext_code[MUEXT_PFIN][0]);
	}
	else
	{
		EXT_DET_PREFIX();
	}
	EXTTIME(rec->jrec_pfin.prefix.time);
	EXTQW(rec->jrec_pfin.prefix.tn);
	if (mur_options.detail)
		EXTINT(rec->prefix.checksum);
	EXTPID(plst);
	jnlext_write(fi, murgbl.extr_buff, extract_len);
}

void	mur_extract_pini(fi_type *fi, jnl_record *rec, pini_list_struct *plst)
{
	int	extract_len = 0;
	char			*ptr;

	if (!mur_options.detail)
	{
		EXT2BYTES(&muext_code[MUEXT_PINI][0]);
	} else
	{
		EXT_DET_PREFIX();
	}
	EXTTIME(rec->prefix.time);
	EXTQW(rec->prefix.tn);
	if (mur_options.detail)
		EXTINT(rec->prefix.checksum);
	extract_len = extract_process_vector((jnl_process_vector *)&rec->jrec_pini.process_vector[CURR_JPV], extract_len);
	extract_len = extract_process_vector((jnl_process_vector *)&rec->jrec_pini.process_vector[ORIG_JPV], extract_len);
	jnlext_write(fi, murgbl.extr_buff, extract_len);
}



/* This routine formats and outputs journal extract records corresponding to M TCOMMIT and ZTCOMMIT commands */
void	mur_extract_tcom(fi_type *fi, jnl_record *rec, pini_list_struct *plst)
{
	int	actual, extract_len = 0;
	char	*ptr;

	if (!mur_options.detail)
	{
		if (rec->prefix.jrec_type == JRT_ZTCOM)
		{
			EXT2BYTES(&muext_code[MUEXT_ZTCOMMIT][0]);
		} else
		{
			EXT2BYTES(&muext_code[MUEXT_TCOMMIT][0]);
		}
	} else
	{
		EXT_DET_PREFIX();
	}
	EXTTIME(rec->jrec_tcom.prefix.time);
	EXTQW(rec->jrec_tcom.prefix.tn);
	if (mur_options.detail)
		EXTINT(rec->prefix.checksum);
	EXTPID(plst);
	if (JRT_ZTCOM == rec->prefix.jrec_type)
	{
		EXTQW(rec->jrec_ztcom.token);
	}
	EXTQW(rec->jrec_tcom.token_seq.jnl_seqno);
	EXTINT(rec->jrec_tcom.participants);
	if (JRT_TCOM == rec->prefix.jrec_type)
	{
		EXTTXT((unsigned char *)&rec->jrec_tcom.jnl_tid[0], sizeof(rec->jrec_tcom.jnl_tid));
	}
	jnlext_write(fi, murgbl.extr_buff, extract_len);
}

int extract_process_vector(jnl_process_vector *pv, int extract_len)
{
	int			actual;
	char			*ptr;

	/* EXTTIME(MID_TIME(pv->jpv_time)); */
	EXTINT(pv->jpv_pid);
	EXTTXT((unsigned char *)&pv->jpv_node[0], JPV_LEN_NODE);
	EXTTXT((unsigned char *)&pv->jpv_user[0], JPV_LEN_USER);
	EXTTXT((unsigned char *)&pv->jpv_terminal[0], JPV_LEN_TERMINAL);
	EXTINTVMS(pv->jpv_mode);
	EXTTIMEVMS(MID_TIME(pv->jpv_login_time));
	EXTINTVMS(pv->jpv_image_count);
	EXTTXTVMS(pv->jpv_prcnam, JPV_LEN_PRCNAM);
	return extract_len;
}
