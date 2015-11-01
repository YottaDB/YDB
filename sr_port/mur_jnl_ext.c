/****************************************************************
 *								*
 *	Copyright 2003 Sanchez Computer Associates, Inc.	*
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
#include "hashdef.h"
#include "buddy_list.h"
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

	if (mur_options.detail)
		SPRINTF(murgbl.extr_buff, "0x%08x [0x%04x] :: ", mur_jctl->rec_offset, mur_rab.jreclen);
	extract_len = (mur_options.detail ? strlen(murgbl.extr_buff) : 0);
	rectype = rec->prefix.jrec_type;
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
			extract_len = strlen(murgbl.extr_buff);
		}
		EXTTIME(rec->prefix.time);
		EXTINT(rec->prefix.tn);
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
			extract_len = strlen(murgbl.extr_buff);
		strcpy(murgbl.extr_buff + extract_len, "       \\");
		memcpy(murgbl.extr_buff + extract_len, jrt_label[rectype], LAB_LEN);
		extract_len += LAB_LEN;
		memcpy(murgbl.extr_buff + extract_len, LAB_TERM, LAB_TERM_SZ);
		extract_len += LAB_TERM_SZ;
	}
	EXTTIME(rec->prefix.time);
	EXTINT(rec->prefix.tn);
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
				assert(FALSE);
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
	EXTINT(rec->prefix.tn);
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
	EXTINT(rec->prefix.tn);
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
	EXTINT(rec->prefix.tn);
	EXTPID(plst);
	EXTINT(rec->jrec_pblk.blknum);
	EXTINT(rec->jrec_pblk.bsiz);
	memcpy((char*)&pblk_head, (char*)&rec->jrec_pblk.blk_contents[0], sizeof(blk_hdr));
	EXTINT(pblk_head.tn);
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
	EXTINT(rec->prefix.tn);
	EXTPID(plst);
	EXTQW(rec->jrec_epoch.jnl_seqno);
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
	EXTINT(rec->prefix.tn);
	EXTPID(plst);
	EXTINT(rec->jrec_inctn.opcode);
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
	EXTINT(rec->jrec_eof.prefix.tn);
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
	EXTINT(rec->jrec_pfin.prefix.tn);
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
	EXTINT(rec->prefix.tn);
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
	EXTINT(rec->jrec_tcom.prefix.tn);
	EXTPID(plst);
	if (JRT_ZTCOM == rec->prefix.jrec_type)
	{
		EXTQW(rec->jrec_ztcom.token);
	}
	EXTQW(rec->jrec_tcom.token_seq.jnl_seqno);
	EXTINT(rec->jrec_tcom.participants);
	if (JRT_TCOM == rec->prefix.jrec_type)
	{
		EXTTXT(rec->jrec_tcom.jnl_tid, sizeof(rec->jrec_tcom.jnl_tid));
	}
	jnlext_write(fi, murgbl.extr_buff, extract_len);
}

int extract_process_vector(jnl_process_vector *pv, int extract_len)
{
	int			actual;
	char			*ptr;

	/* EXTTIME(MID_TIME(pv->jpv_time)); */
	EXTINT(pv->jpv_pid);
	EXTTXT(pv->jpv_node, JPV_LEN_NODE);
	EXTTXT(pv->jpv_user, JPV_LEN_USER);
	EXTTXT(pv->jpv_terminal, JPV_LEN_TERMINAL);
	EXTINTVMS(pv->jpv_mode);
	EXTTIMEVMS(MID_TIME(pv->jpv_login_time));
	EXTINTVMS(pv->jpv_image_count);
	EXTTXTVMS(pv->jpv_prcnam, JPV_LEN_PRCNAM);
	return extract_len;
}

/* A utility routine to compute the length of a string, exclusive of trailing blanks or nuls
   (NOTE:  this routine is also called from mur_output_show() and the mur_extract_*() routines) */
int real_len(int length, char *str)
{
	int	clen;	/* current length */
	for (clen = length - 1;  clen >= 0  &&  (str[clen] == ' '  ||  str[clen] == '\0');  --clen)
		;
	return clen + 1;
}

