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
#include <errno.h>
#include "gtm_ctype.h"
#include "gtm_string.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include <rtnhdr.h>
#include "gv_trigger.h"
#include "targ_alloc.h"
#include "filestruct.h"
#include "mv_stent.h"		/* for COPY_SUBS_TO_GVCURRKEY macro */
#include "gvsub2str.h"		/* for COPY_SUBS_TO_GVCURRKEY macro */
#include "format_targ_key.h"	/* for COPY_SUBS_TO_GVCURRKEY macro */
#include "trigger.h"
#include "trigger_gbl_fill_xecute_buffer.h"
#include "trigger_scan_string.h"
#include "trigger_select_protos.h"
#include "trigger_user_name.h"
#include "change_reg.h"
#include "gvcst_protos.h"
#include "op.h"
#include "mupip_exit.h"
#include "zshow.h"
#include "util.h"
#include "compiler.h"
#include "mvalconv.h"
#include "op_tcommit.h"
#include "gdscc.h"			/* needed for tp.h */
#include "gdskill.h"			/* needed for tp.h */
#include "buddy_list.h"			/* needed for tp.h */
#include "hashtab_int4.h"		/* needed for tp.h */
#include "jnl.h"			/* needed for tp.h */
#include "tp.h"
#include "t_retry.h"
#include "io_params.h"
#include "min_max.h"			/* Needed for MIN */
#include "gtmimagename.h"
#include "gtmio.h"
#include "have_crit.h"

GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_namehead		*gv_target;
GBLREF	gv_key			*gv_currkey;
GBLREF	gd_addr			*gd_header;
GBLREF	gv_key			*gv_altkey;
GBLREF	io_pair			io_curr_device;
GBLREF	io_pair			io_std_device;			/* standard device */
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	int			(*op_open_ptr)(mval *v, mval *p, int t, mval *mspace);

LITREF	mval		    	literal_hasht;
LITREF	mval			literal_zero;
LITREF	mval			literal_ten;
LITREF	char 			*trigger_subs[];

#define TRIGGER_NAME_COMMENT	"trigger name: "
#define TRIGGER_CYCLE_COMMENT	"  cycle: "

#define NAM_LEN(PTR, LEN)	MIN(STRLEN((PTR)), (LEN))

#define COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(OUT_BUFF, OUT_PTR, VAL, VAL_LEN)		\
{											\
	mval		op_val;								\
											\
	assert(MAX_BUFF_SIZE >= VAL_LEN);						\
	if ((INTCAST(OUT_PTR - OUT_BUFF) + VAL_LEN) > MAX_BUFF_SIZE)			\
	{										\
		STR2MVAL(op_val, OUT_BUFF, (unsigned int)(OUT_PTR - OUT_BUFF));		\
		op_write(&op_val);							\
		io_curr_device.out->dollar.x = 0;					\
		OUT_PTR = OUT_BUFF;							\
	}										\
	memcpy(OUT_PTR, (const void *)VAL, VAL_LEN);					\
	OUT_PTR += VAL_LEN;								\
}

#define COPY_SUBSCRIPT(OUT_BUFF, OUT_PTR, CHPTR, LEN_LEFT, IS_TYPE)			\
{											\
	COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(OUT_BUFF, OUT_PTR, CHPTR, 1);		\
	CHPTR++;									\
	LEN_LEFT--;									\
	while ((0 < LEN_LEFT) && IS_TYPE(*CHPTR))					\
	{										\
		COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(OUT_BUFF, OUT_PTR, CHPTR, 1);	\
		CHPTR++;								\
		LEN_LEFT--;								\
	}										\
}

#define MAKE_ZWR_STR(STR, STR_LEN, OUT_START, OUT_STR)					\
{											\
	int		lcl_len;				       			\
	unsigned char	tmp_buff[MAX_ZWR_EXP_RATIO * MAX_BUFF_SIZE];			\
											\
	format2zwr((sm_uc_ptr_t)STR, STR_LEN, tmp_buff, &lcl_len);			\
	COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(OUT_START, OUT_STR, tmp_buff, lcl_len);	\
}

#define CHECK_FOR_M_NAME(START, PTR, LEN, STR, ERR, MAX_LEN)				\
{											\
	int		lcl_len;							\
	int		cur_len;							\
											\
	assert(0 < MAX_LEN);								\
	if (!ISALPHA_ASCII(*PTR) && ('%' != *PTR))					\
	{										\
		lcl_len = STRLEN(START);						\
		CONV_STR_AND_PRINT(STR, lcl_len, START);				\
		ERR = TRIG_FAILURE;							\
		continue;								\
	}										\
	PTR++;										\
	LEN--;										\
	cur_len = 1;									\
	while ((0 < LEN) && ISALNUM_ASCII(*PTR))					\
	{										\
		if (MAX_LEN < ++cur_len)						\
			break;								\
		PTR++;									\
		LEN--;									\
	}										\
}

#define INVALID_NAME_ERROR(ERR_STR, PTR, ERR_VAR, POS)					\
{											\
	int		lcl_len;							\
											\
	lcl_len = STRLEN(PTR);								\
	CONV_STR_AND_PRINT(ERR_STR, lcl_len, PTR);					\
	select_status = TRIG_FAILURE;							\
}

error_def(ERR_MUNOACTION);
error_def(ERR_MUPCLIERR);
error_def(ERR_TRIGDEFBAD);
error_def(ERR_MUPCLIERR);
error_def(ERR_MUNOACTION);


STATICDEF char *triggerfile_quals[] = {"-name=", "", "-commands=", "-options=", "-delim=", "-zdelim=", "-pieces=", "-xecute="};

STATICFNDEF void write_subscripts(char *out_rec, char **out_ptr, char **sub_ptr, int *sub_len)
{
	char			*out_p, *ptr, *dst_ptr;
	int			str_len;
	uint4			len_left, dst_len, len;
	char			dst[MAX_GVSUBS_LEN];

	len_left = *sub_len;
	ptr = *sub_ptr;
	out_p = *out_ptr;
	while (0 < len_left)
	{
		if (ISDIGIT_ASCII(*ptr) || ('-' == *ptr))
		{
			COPY_SUBSCRIPT(out_rec, out_p, ptr, len_left, ISDIGIT_ASCII);
		} else if (ISALPHA_ASCII(*ptr) || ('%' == *ptr))
		{
			COPY_SUBSCRIPT(out_rec, out_p, ptr, len_left, ISALNUM_ASCII);
		} else if ('"' == *ptr)
		{
			len = len_left;
			trigger_scan_string(ptr, &len_left, dst, &dst_len);
			MAKE_ZWR_STR(dst, dst_len, out_rec, out_p);
			len_left--;		/* Need to skip the trailing " */
			ptr += (len - len_left);
		} else
		{
			COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_p, ptr, 1);
			ptr++;
			len_left--;
		}
	}
	*sub_len = len_left;
	*sub_ptr = ptr;
	*out_ptr = out_p;
}

STATICFNDEF void write_out_trigger(char *gbl_name, uint4 gbl_name_len, uint4 file_name_len, mval *op_val, int nam_indx)
{
	mval			data_val;
	char			out_rec[MAX_BUFF_SIZE];
	char			*out_rec_ptr;
	char			*ptr1, *ptr2;
	mname_entry		gvent;
	mval			mi, trigger_count, trigger_value;
	mval			*mv_trig_cnt_ptr;
	boolean_t		multi_line;
	boolean_t		have_value, multi_record;
	int			count;
	int			indx, sub_indx;
	int4			skip_chars;
	int			sub_len;
	char			*sub_ptr;
	char			*tmp_str_ptr, tmp_string[MAX_SRCLINE];
	uint4			tmp_str_len;
	char			cycle[MAX_DIGITS_IN_INT + 1];
	unsigned char		util_buff[MAX_TRIG_UTIL_LEN];
	int4			util_len;
	char			*xecute_buff;
	int4			xecute_len;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	BUILD_HASHT_SUB_SUB_CURRKEY(gbl_name, gbl_name_len, LITERAL_HASHCOUNT, LITERAL_HASHCOUNT_LEN);
	if (gvcst_get(&trigger_count))
	{
		BUILD_HASHT_SUB_SUB_CURRKEY(gbl_name, gbl_name_len, LITERAL_HASHLABEL, STRLEN(LITERAL_HASHLABEL));
		if (!gvcst_get(&trigger_value))
		{	/* There has to be a #LABEL */
			if (CDB_STAGNATE > t_tries)
				t_retry(cdb_sc_triggermod);
			else
			{
				assert(FALSE);
				rts_error_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(8) ERR_TRIGDEFBAD, 6, gbl_name_len,
						gbl_name, gbl_name_len, gbl_name, LEN_AND_LIT("\"#LABEL\""));
			}
		}
		skip_chars = 1;
		if ((trigger_value.str.len != STRLEN(HASHT_GBL_CURLABEL))
				|| (0 != memcmp(trigger_value.str.addr, HASHT_GBL_CURLABEL, trigger_value.str.len)))
		{
			if ((1 == trigger_value.str.len) && ('1' == *trigger_value.str.addr))
				/* 1 == #LABEL - No leading blank in xecute string */
				skip_chars = 0;
		}
		mv_trig_cnt_ptr = &trigger_count;
		count = MV_FORCE_INT(mv_trig_cnt_ptr);
		BUILD_HASHT_SUB_SUB_CURRKEY(gbl_name, gbl_name_len, LITERAL_HASHCYCLE, LITERAL_HASHCYCLE_LEN);
		if (!gvcst_get(&trigger_value))
		{	/* There has to be a #CYCLE */
			if (CDB_STAGNATE > t_tries)
				t_retry(cdb_sc_triggermod);
			else
			{
				assert(FALSE);
				rts_error_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(8) ERR_TRIGDEFBAD, 6, gbl_name_len,
						gbl_name, gbl_name_len, gbl_name, LEN_AND_LIT("\"#CYCLE\""));
			}
		}
		assert(MAX_DIGITS_IN_INT >= trigger_value.str.len);
		memcpy(cycle, trigger_value.str.addr, trigger_value.str.len);
		cycle[trigger_value.str.len] = '\0';
		xecute_buff = NULL;
		for (indx = 1; indx <= count; indx++)
		{
			if ((0 != nam_indx) && (indx != nam_indx))
				continue;
			MV_FORCE_MVAL(&mi, indx);
			BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(gbl_name, gbl_name_len, mi, trigger_subs[TRIGNAME_SUB],
				STRLEN(trigger_subs[TRIGNAME_SUB]));
			out_rec_ptr = out_rec;
			if (gvcst_get(&trigger_value))
			{
				*out_rec_ptr++ = COMMENT_LITERAL;

				COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_rec_ptr, TRIGGER_NAME_COMMENT,
					STR_LIT_LEN(TRIGGER_NAME_COMMENT));
				COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_rec_ptr, trigger_value.str.addr,
					trigger_value.str.len);
				COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_rec_ptr, TRIGGER_CYCLE_COMMENT,
					STR_LIT_LEN(TRIGGER_CYCLE_COMMENT));
				COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_rec_ptr, cycle, STRLEN(cycle));
				STR2MVAL((*op_val), out_rec, (unsigned int)(out_rec_ptr - out_rec));
				op_write(op_val);
				op_wteol(1);
			}
			out_rec_ptr = out_rec;
			*out_rec_ptr++ = '+';
			*out_rec_ptr++ = '^';
			COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_rec_ptr, gbl_name, gbl_name_len);
			BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(gbl_name, gbl_name_len, mi, trigger_subs[GVSUBS_SUB],
				STRLEN(trigger_subs[GVSUBS_SUB]));
			if (gvcst_get(&trigger_value))
			{
				COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_rec_ptr, "(", 1);
				sub_ptr = trigger_value.str.addr;
				sub_len = trigger_value.str.len;
				write_subscripts(out_rec, &out_rec_ptr, &sub_ptr, &sub_len);
				COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_rec_ptr, ")", 1);
			}
			for (sub_indx = 0; sub_indx < NUM_SUBS; sub_indx++)
			{
				if ((GVSUBS_SUB == sub_indx) || (CHSET_SUB == sub_indx))
					continue;
				BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(gbl_name, gbl_name_len, mi, trigger_subs[sub_indx],
					STRLEN(trigger_subs[sub_indx]));
				have_value = gvcst_get(&trigger_value);
				multi_record = FALSE;
				if (!have_value && (XECUTE_SUB == sub_indx))
				{
					op_gvdata(&data_val);
					multi_record = (literal_ten.m[0] == data_val.m[0]) && (literal_ten.m[1] == data_val.m[1]);
				}
				if (have_value || multi_record)
				{
					if (TRIGNAME_SUB == sub_indx)
					{	/* Output name only if it is user defined */
						BUILD_HASHT_SUB_MSUB_SUB_MSUB_CURRKEY(gbl_name, gbl_name_len, mi,
							trigger_subs[sub_indx], STRLEN(trigger_subs[sub_indx]), mi);
						if (!trigger_user_name(trigger_value.str.addr, trigger_value.str.len))
							continue;
						trigger_value.str.len--;	/* Don't include trailing # */
					}
					COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_rec_ptr, " ", 1);
					COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_rec_ptr, triggerfile_quals[sub_indx],
						STRLEN(triggerfile_quals[sub_indx]));
					switch (sub_indx)
					{
						case DELIM_SUB:
						case ZDELIM_SUB:
							MAKE_ZWR_STR(trigger_value.str.addr, trigger_value.str.len, out_rec,
								     out_rec_ptr);
							break;
						case XECUTE_SUB:
							/* After the buffer is malloc-ed by trigger_gbl_fill_xecute_buffer(), the
							 * only exits from the inner loop have to go to the free() below.  Exits
							 * from the outer loop happen before the buffer is malloc-ed.  So no leaks
							 * by error returns.  But just to make sure, let's assert that the buffer
							 * pointer is NULL.
							 */
							assert(NULL == xecute_buff);
							xecute_buff = trigger_gbl_fill_xecute_buffer(gbl_name, gbl_name_len, &mi,
								multi_record ? NULL : &trigger_value, &xecute_len);
							multi_line = (NULL != memchr(xecute_buff, '\n', xecute_len));
							assert(NULL != xecute_buff);
							if (multi_line)
							{
								COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_rec_ptr,
									XTENDED_START, XTENDED_START_LEN);
							} else
							{
								tmp_str_ptr = xecute_buff + skip_chars;
								tmp_str_len = xecute_len - skip_chars;
								MAKE_ZWR_STR(tmp_str_ptr, tmp_str_len, out_rec, out_rec_ptr);
							}
							break;
						default:
							COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_rec_ptr,
								trigger_value.str.addr,trigger_value.str.len);
					}
				}
			}
			/* we had better have an XECUTE STRING, probably should check some buddies */
			assert(NULL != xecute_buff);
			STR2MVAL((*op_val), out_rec, (unsigned int)(out_rec_ptr - out_rec));
			op_write(op_val);
			op_wteol(1);
			if (multi_line)
			{
				ptr1 = xecute_buff;
				do
				{
					ptr2 = memchr(ptr1, '\n', xecute_len);
					out_rec_ptr = out_rec;
					COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_rec_ptr, ptr1, UINTCAST(ptr2 - ptr1));
					STR2MVAL((*op_val), out_rec, (unsigned int)(out_rec_ptr - out_rec));
					op_write(op_val);
					op_wteol(1);
					ptr1 = ptr2 + 1;
				} while (ptr1 < (xecute_buff + xecute_len));
				out_rec_ptr = out_rec;
				COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_rec_ptr, XTENDED_STOP, XTENDED_STOP_LEN);
				STR2MVAL((*op_val), out_rec, (unsigned int)(out_rec_ptr - out_rec));
				op_write(op_val);
				op_wteol(1);
			}
			if (NULL != xecute_buff)
			{
				free(xecute_buff);
				xecute_buff = NULL;
			}
		}
	}
}

STATICFNDEF void write_gbls_or_names(char *gbl_name, uint4 gbl_name_len, uint4 file_name_len, mval *op_val, boolean_t trig_name)
{
	char			save_name[MAX_MIDENT_LEN], curr_name[MAX_MIDENT_LEN];
	boolean_t		wildcard;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gd_region		*save_gv_cur_region;
	gv_key			*save_gv_currkey;
	gv_namehead		*hasht_tree, *save_gv_target;
	sgm_info		*save_sgm_info_ptr;
	sgmnt_addrs		*csa;
	mname_entry		gvent;
	mstr			ms_gbl_nam;
	mval			mv_curr_nam;
        mval                    mi, trigger_count, trig_gbl;
        mval                    *mv_trig_cnt_ptr, mv_trigger_val;
	int			indx, count;
	char			*ptr;
	uint4			curr_name_len;
	int			trigvn_len;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	memcpy(save_name, gbl_name, gbl_name_len);
	wildcard = (NULL != (ptr = memchr(gbl_name, '*', gbl_name_len)));
	if (wildcard)
	{
		*ptr = '\0';
		gbl_name_len--;
		assert(INTCAST(ptr - gbl_name) == gbl_name_len);
	}
	mv_trig_cnt_ptr = &trigger_count;
	memcpy(curr_name, gbl_name, gbl_name_len);
	curr_name_len = gbl_name_len;
	STR2MVAL(mv_curr_nam, gbl_name, gbl_name_len);
	while (TRUE)
	{
		if (0 != memcmp(curr_name, save_name, gbl_name_len))
			break;
		if (trig_name)
		{
			BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STR_LIT_LEN(LITERAL_HASHTNAME), curr_name, curr_name_len);
			if (!gvcst_get(&mv_trigger_val))
			{
				if (wildcard)
				{
					op_gvorder(&mv_curr_nam);
					if (0 == mv_curr_nam.str.len)
						break;
					memcpy(curr_name, mv_curr_nam.str.addr, mv_curr_nam.str.len);
					curr_name_len = mv_curr_nam.str.len;
					continue;
				}
				break;
			}
			ptr = mv_trigger_val.str.addr;
			trigvn_len = STRLEN(mv_trigger_val.str.addr);
			ptr += trigvn_len + 1;
			A2I(ptr, mv_trigger_val.str.addr + mv_trigger_val.str.len, indx);
			STR2MVAL(trig_gbl, mv_trigger_val.str.addr, trigvn_len);
			SAVE_TRIGGER_REGION_INFO;
			ms_gbl_nam.addr = mv_trigger_val.str.addr;
			ms_gbl_nam.len = trigvn_len;
			GV_BIND_NAME_ONLY(gd_header, &ms_gbl_nam);
			csa = gv_target->gd_csa;
			SETUP_TRIGGER_GLOBAL;
			INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
		} else
		{
			STR2MVAL(trig_gbl, curr_name, curr_name_len);
			indx = 0;
		}
		write_out_trigger(trig_gbl.str.addr, trig_gbl.str.len, file_name_len, op_val, indx);
		if (trig_name)
		{
			RESTORE_TRIGGER_REGION_INFO;
		}
		if (wildcard)
		{
			if (trig_name)
			{
				BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STR_LIT_LEN(LITERAL_HASHTNAME), curr_name,
					curr_name_len);
			} else
			{
				BUILD_HASHT_SUB_CURRKEY(curr_name, curr_name_len);
			}
			op_gvorder(&mv_curr_nam);
			if (0 == mv_curr_nam.str.len)
				break;
			memcpy(curr_name, mv_curr_nam.str.addr, mv_curr_nam.str.len);
			curr_name_len = mv_curr_nam.str.len;
		} else
			break;
	}
}

STATICFNDEF void dump_all_triggers(uint4 file_name_len, mval *op_val)
{
	mval			curr_gbl_name, val;
	gd_region		*reg;
	sgmnt_addrs		*csa;
	mname_entry		gvent;
	gv_namehead		*hasht_tree, *save_gvtarget;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	char			save_altkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key			*save_gv_currkey;
	gv_key			*save_gv_altkey;
	unsigned char		*key;
	mval			trigger_value;
	int			reg_index;
	mstr			gbl_name;
	char			global[MAX_MIDENT_LEN];
	int			gbl_len;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(NULL != gd_header);
	save_gvtarget = gv_target;
	for (reg_index = 0, reg = gd_header->regions; reg_index < gd_header->n_regions; reg_index++, reg++)
	{
		if (!reg->open)
			gv_init_reg(reg);
		gv_cur_region = reg;
		change_reg();
		csa = cs_addrs;
		SETUP_TRIGGER_GLOBAL;
		INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
		if (0 != gv_target->root)
		{
			op_gvdata(&val);
			if ((literal_ten.m[0] == val.m[0]) && (literal_ten.m[1] == val.m[1]))
			{ /* $DATA(^#t) is 10 - get first subscript (trigger's global) */
				BUILD_HASHT_SUB_CURRKEY(LITERAL_MAXHASHVAL, STR_LIT_LEN(LITERAL_MAXHASHVAL));
				while (TRUE)
				{
					op_gvorder(&curr_gbl_name);
					if (0 == curr_gbl_name.str.len)
						break;
					gbl_len = curr_gbl_name.str.len;
					memcpy(global, curr_gbl_name.str.addr, gbl_len);
					write_out_trigger(global, gbl_len, file_name_len, op_val, 0);
					BUILD_HASHT_SUB_CURRKEY(global, gbl_len);
				}
			} else
				assert((literal_zero.m[0] == val.m[0]) && (literal_zero.m[1] == val.m[1]));
		}
	}
}

boolean_t trigger_select(char *select_list, uint4 select_list_len, char *file_name, uint4 file_name_len)
{
	char			*sel_ptr, *strtok_ptr, *prev_ptr, *ptr1, *ptr2;
	int			gbl_len, prev_len;
	mstr			gbl_name;
	sgmnt_addrs		*csa;
	gv_namehead		*hasht_tree, *save_gvtarget;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	char			save_altkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key			*save_gv_currkey;
	gv_key			*save_gv_altkey;
	gd_region		*save_gv_cur_region;
	char			save_select_list[MAX_BUFF_SIZE];
	mname_entry		gvent;
	mval			trigger_value;
	int			len, len1, badpos;
	int			local_errno;
	boolean_t		trig_name;
	gv_namehead		*save_gv_target;
	sgm_info		*save_sgm_info_ptr;
	boolean_t		dump_all;
	mval			op_val, op_pars;
	boolean_t		select_status;
	io_pair			save_io_curr_device;

	static readonly unsigned char	open_params_list[] =
	{
		(unsigned char)iop_m,
		(unsigned char)iop_noreadonly,
		(unsigned char)iop_nowrap,
		(unsigned char)iop_stream,
		(unsigned char)iop_eol
	};
	static readonly unsigned char	use_params[] =
	{
		(unsigned char)iop_nowrap,
		(unsigned char)iop_eol
	};
	static readonly unsigned char	no_param = (unsigned char)iop_eol;

	/* make a local copy of the select list and use it to avoid string-pool problems */
	if (MAX_BUFF_SIZE <= select_list_len)
		return TRIG_FAILURE;
	memcpy(save_select_list, select_list, select_list_len);
	save_select_list[select_list_len] = '\0';
	gvinit();
	dump_all = FALSE;
	prev_len = 0;
	op_pars.mvtype = MV_STR;
	op_val.mvtype = MV_STR;
	if (0 == file_name_len)
	{
		op_pars.str.len = SIZEOF(use_params);
		op_pars.str.addr = (char *)use_params;
		if (IS_MUPIP_IMAGE)
		{
			PRINTF("\n");
			FFLUSH(NULL);
			op_val.str.len = io_std_device.out->trans_name->len;
			op_val.str.addr = io_std_device.out->trans_name->dollar_io;
		} else
		{
			op_val.str.len = io_curr_device.out->trans_name->len;
			op_val.str.addr = io_curr_device.out->trans_name->dollar_io;
		}
	} else
	{
		op_pars.str.len = SIZEOF(open_params_list);
		op_pars.str.addr = (char *)open_params_list;
		op_val.str.len = file_name_len;
		op_val.str.addr = (char *)file_name;
		(*op_open_ptr)(&op_val, &op_pars, 0, 0);
	}
	save_io_curr_device = io_curr_device;
	op_use(&op_val, &op_pars);
	if (0 == select_list_len)
		dump_all = TRUE;
	else
	{
		for (ptr1 = save_select_list, len = select_list_len;
		     (NULL != (ptr2 = strchr(ptr1, '*'))) && (len > (len1 = INTCAST(ptr2 - ptr1)));
		     ptr1 = ptr2 + 1)
		{	/* look for either a real "dump-it-all" *, an error *, or a wildcard * */
			/* A "*" anywhere in the select list (at a place where a global name would be) is the same as only a "*" */
			len -= (len1 + 1);		/* Length after the "*" -- len1 is length before the "*" */
			assert((0 <= len1) && (0 <= len));
			if (dump_all = ((0 == len1) && (0 == len) && (ptr1 == save_select_list))
					|| ((0 == len1) && (0 == len) && (',' == *(ptr2 - 1)))
					|| ((0 == len1) && (0 < len) && (',' == *(ptr2 + 1)))
					|| ((0 < len1) && (0 == len) && (',' == *(ptr2 - 1)))
					|| ((0 < len1) && (0 < len) && (',' == *(ptr2 - 1)) && (',' == *(ptr2 + 1))))
				break;
		}
	}
	select_status = TRIG_SUCCESS;
	if (dump_all)
		dump_all_triggers(file_name_len, &op_val);
	else
	{
		len = select_list_len;
		sel_ptr = strtok_r(save_select_list, ",", &strtok_ptr);
		do
		{
			trig_name = ('^' != *sel_ptr);
			ptr1 = sel_ptr;
			len1 = STRLEN(ptr1);
			if (!trig_name)
			{
				ptr1++;
				len1--;
				CHECK_FOR_M_NAME(sel_ptr, ptr1, len1, "Invalid global variable name in SELECT list: ",
						 select_status, MAX_MIDENT_LEN);
				if ((0 != len1) &&  ((1 != len1) || ('*' != *ptr1)))
				{
					badpos = len1;
					INVALID_NAME_ERROR("Invalid global variable name in SELECT list: ", sel_ptr,
						select_status, badpos);
					continue;
				}
				SAVE_TRIGGER_REGION_INFO;
				gbl_len = NAM_LEN(sel_ptr + 1, (int)(ptr1 - sel_ptr) - 1);
				ptr1 = sel_ptr + 1;
				if ((prev_len != gbl_len) || (0 != memcmp(prev_ptr, ptr1, gbl_len)))
				{
					gbl_name.addr = ptr1;
					gbl_name.len = gbl_len;
					prev_ptr = ptr1;
					prev_len = gbl_len;
					GV_BIND_NAME_ONLY(gd_header, &gbl_name);
					if ('*' == *(ptr1 + gbl_len))
						gbl_name.len++;
				}
				csa = gv_target->gd_csa;
				SETUP_TRIGGER_GLOBAL;
			} else
			{
				if (len1 != (badpos = validate_input_trigger_name(ptr1, len1, NULL))) /* assignment is intended */
				{ /* is the input name valid */
					INVALID_NAME_ERROR("Invalid name entry in SELECT list: ",
							   sel_ptr, select_status, badpos);
					continue;
				}
				if (TRIGNAME_SEQ_DELIM == *(sel_ptr + (len1 - 1)))
					/* drop the trailing # sign */
					len1--;
				gbl_name.addr = sel_ptr;
				gbl_name.len = len1;
				SAVE_TRIGGER_REGION_INFO;
				SWITCH_TO_DEFAULT_REGION;
			}
			INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
			if (0 != gv_target->root)
				write_gbls_or_names(gbl_name.addr, gbl_name.len, file_name_len, &op_val, trig_name);
			RESTORE_TRIGGER_REGION_INFO;
		} while (NULL != (sel_ptr = strtok_r(NULL, ",", &strtok_ptr)));	/* Embedded assignment is intended */
	}
	if (0 != file_name_len)
	{
		op_val.mvtype = op_pars.mvtype = MV_STR;
		op_val.str.addr = (char *)file_name;;
		op_val.str.len = file_name_len;
		op_pars.str.len = SIZEOF(no_param);
		op_pars.str.addr = (char *)&no_param;
		op_close(&op_val, &op_pars);
		/* Return back to the current device */
		io_curr_device = save_io_curr_device;
	}
	return select_status;
}

#endif
