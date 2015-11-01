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

#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cdb_sc.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "jnl.h"
#include "gdscc.h"
#include "iosp.h"
#include "mdefsp.h"
#include "ccp.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"

GBLREF	gd_region		*gv_cur_region;
GBLREF	char			jn_tid[8];
GBLREF 	short  			dollar_tlevel;
GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	sgm_info		*sgm_info_ptr;

static	const	enum jnl_record_type	jnl_opcode[3][5] =
					{
						{ JRT_KILL, JRT_FKILL, JRT_TKILL, JRT_GKILL, JRT_UKILL },
						{ JRT_SET,  JRT_FSET,  JRT_TSET,  JRT_GSET,  JRT_USET  },
						{ JRT_ZKILL, JRT_FZKILL, JRT_TZKILL, JRT_GZKILL, JRT_UZKILL}
					};
static	char			zeroes[JNL_REC_START_BNDRY] = "\0\0\0\0\0\0\0\0";

LITREF	int			jnl_fixed_size[];

void	jnl_format(jnl_format_buffer *jfb)
{
	enum jnl_record_type	rectype;
	sgmnt_addrs		*csa;
	int4			align_fill_size, offset, jrec_size;
	int			subcode, temp_free;
	jnl_action		*ja;
	char			*local_buffer;

	csa = &FILE_INFO(gv_cur_region)->s_addrs;
	if (jnl_fence_ctl.level == 0 && dollar_tlevel == 0)
		subcode = 0;
	else
	{
		if (csa->next_fenced == NULL)
		{
			subcode = 1;
			csa->next_fenced = jnl_fence_ctl.fence_list;
			jnl_fence_ctl.fence_list = csa;
		} else
			subcode = 3;
		if (dollar_tlevel != 0)
			++subcode;
	}
	ja = &(jfb->ja);
	rectype = jnl_opcode[ja->operation][subcode];
	assert(rectype > JRT_BAD  &&  rectype < JRT_RECTYPES);
	/* Compute actual record length */
	jrec_size = JREC_PREFIX_SIZE + jnl_fixed_size[rectype] + JREC_SUFFIX_SIZE;
	if (NULL != ja->key)
		jrec_size += ja->key->end + sizeof(ja->key->end);
	switch (rectype)
	{
	case JRT_SET:
	case JRT_FSET:
	case JRT_GSET:
	case JRT_TSET:
	case JRT_USET:
		jrec_size += ja->val->str.len + sizeof(ja->val->str.len);
	}
	jrec_size = ROUND_UP(jrec_size, JNL_REC_START_BNDRY);
	if (0 == dollar_tlevel) /* jfb->buff already malloced in gvcst_init */
		local_buffer = (char *)jfb->buff;
	else
	{
		local_buffer = (char *)get_new_element(sgm_info_ptr->format_buff_list, DIVIDE_ROUND_UP(jrec_size, 8));
		jfb->buff = local_buffer;
		memcpy(((fixed_jrec_tp_kill_set *)local_buffer)->jnl_tid, (uchar_ptr_t)jn_tid, sizeof(jn_tid));
		/* assume an align record will be written while computing maximum jnl-rec size requirements */
		sgm_info_ptr->total_jnl_record_size += jrec_size + JREC_PREFIX_SIZE + jnl_fixed_size[JRT_ALIGN] + JREC_SUFFIX_SIZE;
	}
	jfb->record_size = jrec_size;
	jfb->rectype = rectype;

	/* PREFIX */
	temp_free = 0;
	local_buffer[temp_free++] = rectype;
	local_buffer[temp_free++] = '\0';
	local_buffer[temp_free++] = '\0';
	local_buffer[temp_free++] = '\0';

	offset = 0;
	assert(0 == (int)(local_buffer + temp_free) % 4);
	assert(4 == sizeof(offset));
	*(int *)(local_buffer + temp_free) = offset;
	temp_free += 4;
	/* Actual content */
	temp_free += jnl_fixed_size[rectype];
	if (NULL != ja)
	{
		memcpy(local_buffer + temp_free, (uchar_ptr_t)&ja->key->end, sizeof(ja->key->end));
		temp_free += sizeof(ja->key->end);
		memcpy(local_buffer + temp_free, (uchar_ptr_t)ja->key->base, ja->key->end);
		temp_free += ja->key->end;
	}
	switch (rectype)
	{
	case JRT_SET:
	case JRT_FSET:
	case JRT_GSET:
	case JRT_TSET:
	case JRT_USET:
		memcpy(local_buffer + temp_free, (uchar_ptr_t)&ja->val->str.len, sizeof(ja->val->str.len));
		temp_free += sizeof(ja->val->str.len);
		memcpy(local_buffer + temp_free, (uchar_ptr_t)ja->val->str.addr, ja->val->str.len);
		temp_free += ja->val->str.len;
	}
	if (align_fill_size = (ROUND_UP(temp_free, JNL_REC_START_BNDRY) - temp_free))
	{
		memcpy(local_buffer + temp_free, (uchar_ptr_t)zeroes, align_fill_size);
		temp_free += align_fill_size;
	}
	/* SUFFIX */
	assert(0 == offset);
	*(int *)(local_buffer + temp_free) = offset;
	temp_free += 4;
	offset = jrec_size - JREC_SUFFIX_SIZE;	/* don't count the suffix */
	memcpy(local_buffer + temp_free, (uchar_ptr_t)THREE_LOW_BYTES(offset), 3);
	temp_free += 3;
	local_buffer[temp_free++] = JNL_REC_TRAILER;
}
