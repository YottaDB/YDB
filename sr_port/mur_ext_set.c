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

#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "muprec.h"
#include "copy.h"
#include "init_root_gv.h"
#include "min_max.h"          /* needed for init_root_gv.h */
#include "format_targ_key.h"          /* needed for init_root_gv.h */
#include "mlkdef.h"
#include "zshow.h"
#include "mur_ext_set.h"
#include "op.h"


error_def(ERR_JNLRECFMT);

GBLREF  char		jn_tid[8];
GBLREF	int		mur_extract_bsize;
GBLREF	char		*mur_extract_buff;
GBLREF	mur_opt_struct	mur_options;
GBLREF	boolean_t	is_updproc;
GBLREF  mval            curr_gbl_root;
GBLREF	char		muext_code[][2];


GBLDEF	boolean_t	tstarted = FALSE;
static	char		*rec_buff;



/* This routine formats and outputs journal extract records
   corresponding to M SET, KILL, ZKILL, TSTART, and ZTSTART commands */

void	mur_extract_set(
			jnl_record	*rec,
			uint4   	pid,
			jnl_proc_time	*ref_time)
{
	enum jnl_record_type	rectype;
	short			temp_short;
	int			actual, extract_len, temp_int;
	char			*ptr, *ptr1, *ptr2, key_buff[512];
	char                    buf[MAX_KEY_SZ+1], *temp_ptr, *temp_key;
	int                     buf_len;
	gv_key			*key;
	mstr_len_t		mstr_len;

	if (rec_buff == NULL)
		rec_buff = (char *)malloc(mur_extract_bsize);

	/* Output a Transaction Start record, if necessary */
	extract_len = 0;

	switch (rectype = REF_CHAR(&rec->jrec_type))
	{
	case JRT_FKILL:
	case JRT_FSET:
	case JRT_FZKILL:
		EXT2BYTES(&muext_code[MUEXT_ZTSTART][0]); /* ZTSTART */
		goto common;

	case JRT_TKILL:
	case JRT_TSET:
	case JRT_TZKILL:
		EXT2BYTES(&muext_code[MUEXT_TSTART][0]); /* TSTART */

	common:
		extract_len = exttime(rec->val.jrec_fkill.short_time, ref_time, 3);
		EXTINT(pid);

		EXTQW(rec->val.jrec_tset.jnl_seqno);

		/* To have a tid field in TSET record for replication */
		if (JRT_TSET == rectype || JRT_TKILL == rectype || JRT_TZKILL == rectype)
		{
			EXTTXT(rec->val.jrec_tset.jnl_tid, sizeof(jn_tid));
		}

		jnlext_write(mur_extract_buff, extract_len + 1);
	}


	/* Output the SET or KILL or ZKILL record */

	extract_len = 0;
	switch (rectype)
	{
	default:
		assert(FALSE);
		break;

	case JRT_KILL:
	case JRT_FKILL:
	case JRT_GKILL:
	case JRT_TKILL:
	case JRT_UKILL:
		EXT2BYTES(&muext_code[MUEXT_KILL][0]); /* KILL */
		break;

	case JRT_ZKILL:
	case JRT_FZKILL:
	case JRT_GZKILL:
	case JRT_TZKILL:
	case JRT_UZKILL:
		EXT2BYTES(&muext_code[MUEXT_ZKILL][0]); /* ZKILL */
		break;

	case JRT_SET:
	case JRT_FSET:
	case JRT_GSET:
	case JRT_TSET:
	case JRT_USET:
		EXT2BYTES(&muext_code[MUEXT_SET][0]); /* SET */
	}

	extract_len = exttime(rec->val.jrec_kill.short_time, ref_time, 3);

	EXTINT(pid);

	EXTQW(rec->val.jrec_kill.jnl_seqno);
	EXTINT(rec->val.jrec_kill.tn);

	switch (rectype)
	{
	case JRT_KILL:
	case JRT_SET:
	case JRT_ZKILL:
		ptr1 = (char *)&rec->val.jrec_kill.mumps_node.length;
		ptr2 = rec->val.jrec_kill.mumps_node.text;
		break;

	case JRT_FKILL:
	case JRT_GKILL:
	case JRT_TKILL:
	case JRT_UKILL:
	case JRT_FSET:
	case JRT_GSET:
	case JRT_TSET:
	case JRT_USET:
	case JRT_FZKILL:
	case JRT_GZKILL:
	case JRT_TZKILL:
	case JRT_UZKILL:
		ptr1 = (char *)&rec->val.jrec_fkill.mumps_node.length;
		ptr2 = rec->val.jrec_fkill.mumps_node.text;
	}

	key = (gv_key *)key_buff;
	key->top = MAX_KEY_SZ;
	GET_SHORT(key->end, ptr1);
	if (key->end > key->top)
		rts_error(VARLSTCNT(1) ERR_JNLRECFMT);

	memcpy(key->base, ptr2, key->end);
	key->base[key->end] = '\0';
	ptr = &mur_extract_buff[extract_len];

	RETRIEVE_ROOT_VAL(ptr2, buf, temp_ptr, temp_key, buf_len);
        INIT_ROOT_GVT(buf, buf_len, curr_gbl_root);

	/* MAXKEY_SZ is incremented by 1 to make the call to format_targ_key
       same as runtime call to format_targ_key and to eliminate the core dump
       when jnl record with key size 0f 255 is extracted */

       ptr = (char *)format_targ_key((uchar_ptr_t)ptr, (MAX_KEY_SZ +1) + 5, key, TRUE);


	extract_len += (int)(ptr - &mur_extract_buff[extract_len]);

	switch (rectype)
	{
	case JRT_SET:
	case JRT_FSET:
	case JRT_GSET:
	case JRT_TSET:
	case JRT_USET:
		mur_extract_buff[extract_len++] = '=';
		GET_SHORT(temp_short, ptr1);
		ptr1 += temp_short + sizeof(short);
		GET_MSTR_LEN(mstr_len, ptr1);
		format2zwr((sm_uc_ptr_t)ptr1 + sizeof(mstr_len_t), mstr_len, (unsigned char*)rec_buff, &temp_int);
		if (extract_len + temp_int > mur_extract_bsize)
			rts_error(VARLSTCNT(1) ERR_JNLRECFMT);
		memcpy(&mur_extract_buff[extract_len], rec_buff, temp_int);
		extract_len += temp_int;
	}

	jnlext_write(mur_extract_buff, extract_len + 1);
}

void	mur_extract_null(jnl_record *rec)
{
	int			extract_len;
	char			*ptr;
	jnl_process_vector	*pv = &rec->val.jrec_pini.process_vector[CURR_JPV];
	jnl_proc_time		*ref_time = &pv->jpv_time;

	extract_len = 0;
	if (!mur_options.detail)
	{
		EXT2BYTES(&muext_code[MUEXT_NULL][0]);
	}
	else
	{
		extract_len = strlen(mur_extract_buff);
		strcpy(mur_extract_buff + extract_len, "NULL   \\");
		extract_len = strlen(mur_extract_buff);
	}

	extract_len = exttime(rec->val.jrec_null.short_time, ref_time, extract_len);
	extract_len = exttime(rec->val.jrec_null.recov_short_time, ref_time, extract_len);
	EXTINT(0);
	EXTQW(rec->val.jrec_null.jnl_seqno);
	EXTINT(rec->val.jrec_null.tn);
	jnlext_write(mur_extract_buff, extract_len);
}

void	detailed_extract_set(
			jnl_record	*rec,
			uint4   	pid,
			jnl_proc_time	*ref_time)
{
	enum jnl_record_type	rectype;
	short			temp_short;
	int			actual, extract_len, temp_int;
	char			*ptr, *ptr1, *ptr2, key_buff[MAX_KEY_SZ + 5];
	char                    buf[MAX_KEY_SZ+1], *temp_ptr, *temp_key;
	int                     buf_len;
	gv_key			*key;
	mstr_len_t		mstr_len;


	if (rec_buff == NULL)
		rec_buff = (char *)malloc(mur_extract_bsize);

	/* Output a Transaction Start record, if necessary */

	extract_len = 0;
	switch (rectype = REF_CHAR(&rec->jrec_type))
	{
	case JRT_FKILL:
	case JRT_FSET:
	case JRT_FZKILL:
		if (tstarted)
			break;
		extract_len = strlen(mur_extract_buff);
		strcpy(mur_extract_buff + extract_len, "ZTSTART\\");
		extract_len = strlen(mur_extract_buff);
		extract_len = exttime(rec->val.jrec_fkill.short_time, ref_time, extract_len);
		extract_len = exttime(rec->val.jrec_fkill.recov_short_time, ref_time, extract_len);
		goto common;

	case JRT_TKILL:
	case JRT_TSET:
	case JRT_TZKILL:
		if (tstarted)
			break;
		extract_len = strlen(mur_extract_buff);
		strcpy(mur_extract_buff + extract_len, "TSTART \\");
		extract_len = strlen(mur_extract_buff);
	/* To have a tid field in TSET record for replication */
		EXTTXT(rec->val.jrec_tset.jnl_tid, sizeof(jn_tid));

	common:
		extract_len = exttime(rec->val.jrec_fkill.short_time, ref_time, extract_len);
		extract_len = exttime(rec->val.jrec_fkill.recov_short_time, ref_time, extract_len);
		EXTINT(pid);
		jnlext_write(mur_extract_buff, extract_len + 1);
		if (is_updproc)
			tstarted = TRUE;
		if (mur_options.detail)
		{
			memcpy(mur_extract_buff, "                       ", 23);
			extract_len = 23;
		}
		else
			extract_len = 0;
	}

	/* Output the SET or KILL or ZKILL record */

	switch (rectype)
	{
	default:
		assert(FALSE);
		break;

	case JRT_KILL:
		extract_len = strlen(mur_extract_buff);
		strcpy(mur_extract_buff + extract_len, "KILL   \\");
		break;

	case JRT_ZKILL:
		extract_len = strlen(mur_extract_buff);
		strcpy(mur_extract_buff + extract_len, "ZKILL  \\");
		break;

	case JRT_FKILL:
	case JRT_TKILL:
		strcpy(mur_extract_buff + extract_len, "  KILL \\");
		break;
	case JRT_GKILL:
	case JRT_UKILL:
		extract_len = strlen(mur_extract_buff);
		strcpy(mur_extract_buff + extract_len, "  KILL \\");
		break;

	case JRT_FZKILL:
	case JRT_TZKILL:
		strcpy(mur_extract_buff + extract_len, "  ZKILL\\");
		break;
	case JRT_GZKILL:
	case JRT_UZKILL:
		extract_len = strlen(mur_extract_buff);
		strcpy(mur_extract_buff + extract_len, "  ZKILL\\");
		break;

	case JRT_SET:
		extract_len = strlen(mur_extract_buff);
		strcpy(mur_extract_buff + extract_len, "SET    \\");
		break;

	case JRT_FSET:
	case JRT_TSET:
		strcpy(mur_extract_buff + extract_len, "  SET  \\");
		break;
	case JRT_GSET:
	case JRT_USET:
		extract_len = strlen(mur_extract_buff);
		strcpy(mur_extract_buff + extract_len, "  SET  \\");
		break;
	}

	extract_len = exttime(rec->val.jrec_kill.short_time, ref_time, strlen(mur_extract_buff));
	extract_len = exttime(rec->val.jrec_kill.recov_short_time, ref_time, extract_len);

	EXTINT(pid);
	EXTQW(rec->val.jrec_kill.jnl_seqno);
	EXTINT(rec->val.jrec_kill.tn);

	switch (rectype)
	{
	case JRT_KILL:
	case JRT_SET:
	case JRT_ZKILL:
		ptr1 = (char *)&rec->val.jrec_kill.mumps_node.length;
		ptr2 = rec->val.jrec_kill.mumps_node.text;
		break;

	case JRT_FKILL:
	case JRT_GKILL:
	case JRT_TKILL:
	case JRT_UKILL:
	case JRT_FSET:
	case JRT_GSET:
	case JRT_TSET:
	case JRT_USET:
	case JRT_FZKILL:
	case JRT_GZKILL:
	case JRT_TZKILL:
	case JRT_UZKILL:
		ptr1 = (char *)&rec->val.jrec_fkill.mumps_node.length;
		ptr2 = rec->val.jrec_fkill.mumps_node.text;
	}

	key = (gv_key *)key_buff;
	key->top = MAX_KEY_SZ;
	GET_SHORT(key->end, ptr1);
	if (key->end > key->top)
		rts_error(VARLSTCNT(1) ERR_JNLRECFMT);

	memcpy(key->base, ptr2, key->end);
	key->base[key->end] = '\0';
	ptr = &mur_extract_buff[extract_len];

	RETRIEVE_ROOT_VAL(ptr2, buf, temp_ptr, temp_key, buf_len);
        INIT_ROOT_GVT(buf, buf_len, curr_gbl_root);

	/* MAXKEY_SZ is incremented by 1 to make the call to format_targ_key
       		same as runtime call to format_targ_key and to eliminate the core dump
       		when jnl record with key size 0f 255 is extracted */

       	ptr = (char *)format_targ_key((uchar_ptr_t)ptr, (MAX_KEY_SZ +1) + 5, key, TRUE);
	extract_len += (int)(ptr - &mur_extract_buff[extract_len]);

	switch (rectype)
	{
	case JRT_SET:
	case JRT_FSET:
	case JRT_GSET:
	case JRT_TSET:
	case JRT_USET:
		mur_extract_buff[extract_len++] = '=';
		GET_SHORT(temp_short, ptr1);
		ptr1 += temp_short + sizeof(short);
		GET_MSTR_LEN(mstr_len, ptr1);
		format2zwr((sm_uc_ptr_t)ptr1 + sizeof(mstr_len_t), mstr_len, (unsigned char*)rec_buff, &temp_int);
		if (extract_len + temp_int > mur_extract_bsize)
			rts_error(VARLSTCNT(1) ERR_JNLRECFMT);
		memcpy(&mur_extract_buff[extract_len], rec_buff, temp_int);
		extract_len += temp_int;
	}

	jnlext_write(mur_extract_buff, extract_len + 1);
}

void	mur_extract_align(jnl_record *rec)
{
	int			extract_len;
	jnl_process_vector	*pv = &rec->val.jrec_pini.process_vector[CURR_JPV];
	jnl_proc_time		*ref_time = &pv->jpv_time;

	assert (mur_options.detail);
	extract_len = strlen(mur_extract_buff);
	strcpy(mur_extract_buff + extract_len, "ALIGN  \\");
	extract_len = exttime(rec->val.jrec_align.short_time, ref_time, strlen(mur_extract_buff));
	jnlext_write(mur_extract_buff, extract_len);
}

void	mur_extract_pblk(jnl_record *rec, uint4 pid, jnl_proc_time *ref_time)
{
	int	extract_len;
	blk_hdr	pblk_head;
	char	*ptr;

	assert (mur_options.detail);
	extract_len = strlen(mur_extract_buff);
	strcpy(mur_extract_buff + extract_len, "PBLK   \\");
	extract_len = exttime(rec->val.jrec_pblk.short_time, ref_time, strlen(mur_extract_buff));
	EXTINT(pid);
	EXTINT(rec->val.jrec_pblk.blknum);
	EXTINT(rec->val.jrec_pblk.bsiz);
	memcpy((uchar_ptr_t)&pblk_head, (uchar_ptr_t)&rec->val.jrec_pblk.blk_contents[0], sizeof(blk_hdr));
	EXTINT(pblk_head.tn);
	jnlext_write(mur_extract_buff, extract_len);
}

void	mur_extract_epoch(jnl_record *rec, uint4 pid, jnl_proc_time *ref_time)
{
	int	extract_len;
	char	*ptr;

	assert (mur_options.detail);
	if (ref_time == NULL)
		ref_time = (jnl_proc_time *)malloc(sizeof(jnl_proc_time));
	extract_len = strlen(mur_extract_buff);
	strcpy(mur_extract_buff + extract_len, "EPOCH  \\");
	extract_len = exttime(rec->val.jrec_epoch.short_time, ref_time, strlen(mur_extract_buff));
	EXTINT(pid);
	EXTQW(rec->val.jrec_epoch.jnl_seqno);
	EXTINT(rec->val.jrec_epoch.tn);
	jnlext_write(mur_extract_buff, extract_len);
}

void    mur_extract_inctn(jnl_record *rec, uint4 pid, jnl_proc_time *ref_time)
{
        int     extract_len;
        char    *ptr;

	if (!mur_options.detail)
		return;
	if (ref_time == NULL)
		ref_time = (jnl_proc_time *)malloc(sizeof(jnl_proc_time));
	extract_len = strlen(mur_extract_buff);
       	strcpy(mur_extract_buff + extract_len, "INCTN  \\");
        extract_len = exttime(rec->val.jrec_inctn.short_time, ref_time, strlen(mur_extract_buff));
	EXTINT(pid);
	EXTINT(rec->val.jrec_inctn.tn);
	EXTINT(rec->val.jrec_inctn.opcode);
	jnlext_write(mur_extract_buff, extract_len);
}

void	mur_extract_aimg(jnl_record *rec, uint4 pid, jnl_proc_time *ref_time)
{
	int	extract_len;
	char	*ptr;

	if (!mur_options.detail)
		return;
	if (ref_time == NULL)
		ref_time = (jnl_proc_time *)malloc(sizeof(jnl_proc_time));
	extract_len = strlen(mur_extract_buff);
	strcpy(mur_extract_buff + extract_len, "AIMG   \\");
	extract_len = exttime(rec->val.jrec_aimg.short_time, ref_time, strlen(mur_extract_buff));
	EXTINT(pid);
	EXTINT(rec->val.jrec_aimg.tn);
	EXTINT(rec->val.jrec_aimg.blknum);
	EXTINT(rec->val.jrec_aimg.bsiz);
	jnlext_write(mur_extract_buff, extract_len);
}

