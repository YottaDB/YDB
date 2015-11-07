/****************************************************************
 *								*
 *	Copyright 2010, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef GTM_TRIGGER
#include "gtm_string.h"
#include "gdsroot.h"			/* for gdsfhead.h */
#include "gdsbt.h"			/* for gdsfhead.h */
#include "gdsfhead.h"			/* For gvcst_protos.h */
#include "gvcst_protos.h"
#include "hashtab_str.h"
#include "mv_stent.h"			/* for COPY_SUBS_TO_GVCURRKEY macro */
#include "gvsub2str.h"			/* for COPY_SUBS_TO_GVCURRKEY */
#include "format_targ_key.h"		/* for COPY_SUBS_TO_GVCURRKEY */
#include <rtnhdr.h>
#include "gv_trigger.h"
#include "trigger.h"
#include "trigger_compare_protos.h"
#include "trigger_gbl_fill_xecute_buffer.h"
#include "mvalconv.h"			/* Needed for MV_FORCE_MVAL and MV_FORCE_UMVAL */
#include "op.h"
#include "nametabtyp.h"
#include "targ_alloc.h"			/* for SETUP_TRIGGER_GLOBAL & SWITCH_TO_DEFAULT_REGION */
#include "gdscc.h"			/* needed for tp.h */
#include "gdskill.h"			/* needed for tp.h */
#include "buddy_list.h"			/* needed for tp.h */
#include "hashtab_int4.h"		/* needed for tp.h */
#include "filestruct.h"			/* needed for jnl.h */
#include "jnl.h"			/* needed for tp.h */
#include "tp.h"

GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gd_addr			*gd_header;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey;
GBLREF	gv_namehead		*gv_target;

LITREF	mval			literal_hasht;
LITREF	mval			literal_one;
LITREF	mval			literal_ten;
LITREF	char 			*trigger_subs[];

#define NON_HASH	-1
LITDEF int trig_subsc_xlate_tbl[11] =
{
	NON_HASH, 1, NON_HASH, NON_HASH, 4, 5, 6, 7, NON_HASH, NON_HASH, NON_HASH
};

#define COPY_VAL_TO_INDEX_STR(SUB, PTR)						\
{										\
	memcpy(PTR, values[SUB], value_len[SUB]);				\
	PTR += value_len[SUB];							\
	*PTR++ = '\0';								\
}

error_def(ERR_TRIGDEFBAD);

STATICFNDEF boolean_t compare_vals(char *trigger_val, uint4 trigger_val_len, char *key_val, uint4 key_val_len)
{
	char		*end_ptr;
	char		*k_ptr;
	char		*t_ptr;

	if (key_val_len < trigger_val_len)
		return FALSE;
	end_ptr = trigger_val + trigger_val_len;
	t_ptr = trigger_val;
	k_ptr = key_val;
	while ((end_ptr > t_ptr) && (*t_ptr == *k_ptr))
	{
		t_ptr++;
		k_ptr++;
	}
	return ((end_ptr > t_ptr) && ('\0' == *k_ptr));
}

void build_kill_cmp_str(char *trigvn, int trigvn_len, char **values, uint4 *value_len, mstr *kill_key, boolean_t multi_line_xecute)
{
	uint4			lcl_len;
	char			*ptr;

	lcl_len = kill_key->len;
	assert(lcl_len >= (trigvn_len + 1 + value_len[GVSUBS_SUB] + 1 + value_len[XECUTE_SUB] + 1));
	ptr = kill_key->addr;
	memcpy(ptr, trigvn, trigvn_len);
	ptr += trigvn_len;
	*ptr++ = '\0';
	COPY_VAL_TO_INDEX_STR(GVSUBS_SUB, ptr);
	if (!multi_line_xecute)
		COPY_VAL_TO_INDEX_STR(XECUTE_SUB, ptr);
	kill_key->len = INTCAST(ptr - kill_key->addr) - 1;
}

void build_set_cmp_str(char *trigvn, int trigvn_len, char **values, uint4 *value_len, mstr *set_key, boolean_t multi_line_xecute)
{
	uint4			lcl_len;
	char			*ptr;

	lcl_len = set_key->len;
	assert(lcl_len >= (trigvn_len + 1 + value_len[GVSUBS_SUB] + 1 + value_len[PIECES_SUB] + 1 + value_len[DELIM_SUB] + 1
			   + value_len[ZDELIM_SUB] + 1 + value_len[XECUTE_SUB] + 1));
	ptr = set_key->addr;
	memcpy(ptr, trigvn, trigvn_len);
	ptr += trigvn_len;
	*ptr++ = '\0';
	COPY_VAL_TO_INDEX_STR(GVSUBS_SUB, ptr);
	COPY_VAL_TO_INDEX_STR(PIECES_SUB, ptr);
	COPY_VAL_TO_INDEX_STR(DELIM_SUB, ptr);
	COPY_VAL_TO_INDEX_STR(ZDELIM_SUB, ptr);
	if (!multi_line_xecute)
		COPY_VAL_TO_INDEX_STR(XECUTE_SUB, ptr);
	set_key->len = INTCAST(ptr - set_key->addr) - 1;
}

boolean_t search_trigger_hash(char *trigvn, int trigvn_len, stringkey *trigger_hash, int match_index, int *hash_indx)
{
	mval			collision_indx;
	mval			*collision_indx_ptr;
	int			hash_index;
	mval			data_val, key_val;
	int4			len;
	mval			mv_hash;
	boolean_t		match, multi_record;
	char			*ptr;
	int			trig_index;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	collision_indx_ptr = &collision_indx;
	MV_FORCE_UMVAL(&mv_hash, trigger_hash->hash_code);
	BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(LITERAL_HASHTRHASH, STR_LIT_LEN(LITERAL_HASHTRHASH), mv_hash, "", 0);
	while (TRUE)
	{
		op_gvorder(collision_indx_ptr);
		if (0 == collision_indx_ptr->str.len)
		{
			hash_index = 0;
			match = FALSE;
			break;
		}
		BUILD_HASHT_SUB_MSUB_MSUB_CURRKEY(LITERAL_HASHTRHASH, STR_LIT_LEN(LITERAL_HASHTRHASH), mv_hash, collision_indx);
		if (!gvcst_get(&key_val))
		{	/* There has to be a #TRHASH entry */
			assert(FALSE);
			rts_error_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(8) ERR_TRIGDEFBAD, 6, trigvn_len, trigvn,
					LEN_AND_LIT("\"#TRHASH\""), mv_hash.str.len, mv_hash.str.addr);
		}
		ptr = key_val.str.addr;
		len = STRLEN(ptr);
		if ((len != trigvn_len) || (0 != memcmp(trigvn, ptr, len)))
		{
			hash_index = 0;
			match = FALSE;
			break;
		}
		ptr += len;
		assert(('\0' == *ptr) && (key_val.str.len > len));
		ptr++;
		A2I(ptr, key_val.str.addr + key_val.str.len, trig_index);
		assert(-1 != trig_index);
		if ((0 == match_index) || (match_index == trig_index))
		{
			hash_index = MV_FORCE_INT(collision_indx_ptr);
			match = TRUE;
			break;
		}
	}
	*hash_indx = hash_index;
	return match;
}

boolean_t search_triggers(char *trigvn, int trigvn_len, char **values, uint4 *value_len, stringkey *trigger_hash, int *hash_indx,
			  int *trig_indx, int match_index, boolean_t doing_set)
{
	mval			collision_indx;
	mval			*collision_indx_ptr;
	sgmnt_addrs		*csa;
	mval			data_val;
	mstr			gbl_name;
	mname_entry		gvent;
	gv_namehead		*hasht_tree;
	boolean_t		have_value;
	mval			key_val;
	int4			len;
	boolean_t		multi_record;
	mval			mv_hash;
	mval			mv_trig_indx;
	boolean_t		match;
	int4			num;
	char			*ptr;
	int4			rec_len;
	int			sub_indx;
	mval			sub_val;
	uint4			trig_hash;
	int			trig_hash_index;
	int			trig_index;
	char			*xecute_buff;
	int4			xecute_len;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	collision_indx_ptr = &collision_indx;
	MV_FORCE_UMVAL(&mv_hash, trigger_hash->hash_code);
	BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(LITERAL_HASHTRHASH, STR_LIT_LEN(LITERAL_HASHTRHASH), mv_hash, "", 0);
	xecute_buff = NULL;
	while (TRUE)
	{
		match = TRUE;
		op_gvorder(collision_indx_ptr);
		if (0 == collision_indx_ptr->str.len)
		{
			trig_hash_index = trig_index = 0;
			match = FALSE;
			break;
		}
		BUILD_HASHT_SUB_MSUB_MSUB_CURRKEY(LITERAL_HASHTRHASH, STR_LIT_LEN(LITERAL_HASHTRHASH), mv_hash, collision_indx);
		if (!gvcst_get(&key_val))
		{	/* There has to be a #TRHASH entry */
			assert(FALSE);
			rts_error_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(8) ERR_TRIGDEFBAD, 6, trigvn_len, trigvn,
					LEN_AND_LIT("\"#TRHASH\""), mv_hash.str.len, mv_hash.str.addr);
		}
		ptr = key_val.str.addr;
		len = STRLEN(ptr);
		if ((len != trigvn_len) || (0 != memcmp(trigvn, ptr, len)))
		{
			trig_hash_index = trig_index = 0;
			match = FALSE;
			break;
		}
		ptr += len;
		assert(('\0' == *ptr) && (key_val.str.len > len));
		ptr++;
		A2I(ptr, key_val.str.addr + key_val.str.len, trig_index);
		assert(-1 != trig_index);
		gbl_name.addr = trigvn;
		gbl_name.len = trigvn_len;
		GV_BIND_NAME_ONLY(gd_header, &gbl_name);
		csa = gv_target->gd_csa;
		SETUP_TRIGGER_GLOBAL;
		INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
		MV_FORCE_MVAL(&mv_trig_indx, trig_index);
		for (sub_indx = 0; sub_indx < NUM_TOTAL_SUBS; sub_indx++)
		{
			if (NON_HASH == trig_subsc_xlate_tbl[sub_indx])
				continue;
			if (!doing_set && ((DELIM_SUB == sub_indx) || (ZDELIM_SUB == sub_indx) || (PIECES_SUB == sub_indx)))
				continue;
			BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(trigvn, trigvn_len, mv_trig_indx, trigger_subs[sub_indx],
							 STRLEN(trigger_subs[sub_indx]));
			multi_record = FALSE;
			have_value = gvcst_get(&key_val);
			if (!have_value && (XECUTE_SUB == sub_indx))
			{
				op_gvdata(&data_val);
				multi_record = (literal_ten.m[0] == data_val.m[0]) && (literal_ten.m[1] == data_val.m[1]);
			}
			if ((0 == value_len[sub_indx]) && !have_value && !multi_record)
				continue;
			if (XECUTE_SUB == sub_indx)
			{
				assert(NULL == xecute_buff);	/* We don't want a memory leak */
				xecute_buff = trigger_gbl_fill_xecute_buffer(trigvn, trigvn_len, &mv_trig_indx,
									     multi_record ? NULL : &key_val, &xecute_len);
				if ((value_len[sub_indx] == xecute_len)
				    && (0 == memcmp(values[sub_indx], xecute_buff, value_len[sub_indx])))
				{
					free(xecute_buff);
					xecute_buff = NULL;
					continue;
				} else
				{
					free(xecute_buff);
					xecute_buff = NULL;
					trig_hash_index = trig_index = 0;
					match = FALSE;
					break;
				}
			} else
			{
				if (have_value && (value_len[sub_indx] == key_val.str.len)
				    && (0 == memcmp(values[sub_indx], key_val.str.addr, value_len[sub_indx])))
					continue;
				else
				{
					trig_hash_index = trig_index = 0;
					match = FALSE;
					break;
				}
			}
		}
		SWITCH_TO_DEFAULT_REGION;
		if (!match || ((0 != match_index) && (match_index != trig_index)))
		{
			BUILD_HASHT_SUB_MSUB_MSUB_CURRKEY(LITERAL_HASHTRHASH, STR_LIT_LEN(LITERAL_HASHTRHASH), mv_hash,
							  collision_indx);
			continue;
		}
		trig_hash_index = MV_FORCE_INT(collision_indx_ptr);
		match = TRUE;
		break;
	}
	*trig_indx = trig_index;
	*hash_indx = trig_hash_index;
	return match;
}
#endif /* GTM_TRIGGER */
