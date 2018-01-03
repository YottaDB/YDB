/****************************************************************
 *								*
 * Copyright (c) 2010-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "op.h"			/* for op_tstart etc. */
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
#include "hashtab_mname.h"
#include "tp_frame.h"
#include "tp_restart.h"
#include "repl_msg.h"			/* for gtmsource.h */
#include "gtmsource.h"			/* for jnlpool_addrs_ptr_t */

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
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	int			(*op_open_ptr)(mval *v, mval *p, mval *t, mval *mspace);
GBLREF	uint4			dollar_tlevel;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
#ifdef DEBUG
GBLREF	boolean_t		donot_INVOKE_MUMTSTART;
#endif

LITREF	mval			literal_zero;
LITREF	mval			literal_ten;
LITREF	char 			*trigger_subs[];

#define TRIGGER_NAME_COMMENT	"trigger name: "
#define TRIGGER_CYCLE_COMMENT	"  cycle: "

#define NAM_LEN(PTR, LEN)	MIN(STRLEN((PTR)), (LEN))

#define COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(OUT_BUFF, OUT_PTR, VAL, VAL_LEN)				\
{													\
	if ((INTCAST(OUT_PTR - OUT_BUFF) + VAL_LEN) > MAX_BUFF_SIZE)					\
	{												\
		util_out_print_gtmio("!AD", NOFLUSH, (unsigned int)(OUT_PTR - OUT_BUFF), OUT_BUFF);	\
		OUT_PTR = OUT_BUFF;									\
	}												\
	if (VAL_LEN > MAX_BUFF_SIZE)									\
	{												\
		util_out_print_gtmio("!AD", NOFLUSH, VAL_LEN, VAL);					\
		OUT_PTR = OUT_BUFF;									\
	} else												\
	{												\
		memcpy(OUT_PTR, (const void *)VAL, VAL_LEN);						\
		OUT_PTR += VAL_LEN;									\
	}												\
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

error_def(ERR_DBROLLEDBACK);
error_def(ERR_MUNOACTION);
error_def(ERR_MUNOACTION);
error_def(ERR_MUPCLIERR);
error_def(ERR_MUPCLIERR);
error_def(ERR_NEEDTRIGUPGRD);
error_def(ERR_TRIGDEFBAD);

STATICDEF char *triggerfile_quals[] = {
#define TRIGGER_SUBSDEF(SUBSTYPE, SUBSNAME, LITMVALNAME, TRIGFILEQUAL, PARTOFHASH)	TRIGFILEQUAL,
#include "trigger_subs_def.h"
#undef TRIGGER_SUBSDEF
};

STATICFNDEF void write_subscripts(char *out_rec, char **out_ptr, char **sub_ptr, int *sub_len)
{
	char			*out_p, *ptr;
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

STATICFNDEF void write_out_trigger(char *gbl_name, uint4 gbl_name_len, int nam_indx)
{
	mval			data_val;
	char			out_rec[MAX_BUFF_SIZE];
	char			*out_rec_ptr;
	char			*ptr1, *ptr2, *ptrtop;
	mval			mi, trigger_count, trigger_value, *protect_trig_mval;
	mval			*mv_trig_cnt_ptr;
	boolean_t		multi_line;
	boolean_t		have_value, multi_record;
	int			count;
	int			indx, sub_indx;
	int4			skip_chars;
	int			sub_len;
	char			*sub_ptr;
	char			*tmp_str_ptr;
	uint4			tmp_str_len;
	char			cycle[MAX_DIGITS_IN_INT + 1];
	int			cycle_len;
	char			*xecute_buff;
	int4			xecute_len;
	int			trig_protected_mval_push_count;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(!cs_addrs->hdr->hasht_upgrade_needed);	/* all callers should have errored out otherwise */
	BUILD_HASHT_SUB_SUB_CURRKEY(gbl_name, gbl_name_len, LITERAL_HASHCOUNT, LITERAL_HASHCOUNT_LEN);
	if (gvcst_get(&trigger_count))
	{
		mv_trig_cnt_ptr = &trigger_count;
		count = MV_FORCE_INT(mv_trig_cnt_ptr);
		BUILD_HASHT_SUB_SUB_CURRKEY(gbl_name, gbl_name_len, LITERAL_HASHLABEL, STRLEN(LITERAL_HASHLABEL));
		if (!gvcst_get(&trigger_value))
		{	/* There has to be a #LABEL */
			if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
				t_retry(cdb_sc_triggermod);
			assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
			rts_error_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(8) ERR_TRIGDEFBAD, 6, gbl_name_len,
					gbl_name, gbl_name_len, gbl_name, LEN_AND_LIT("\"#LABEL\""));
		}
		skip_chars = 1;
		if ((trigger_value.str.len != STRLEN(HASHT_GBL_CURLABEL))
				|| (0 != memcmp(trigger_value.str.addr, HASHT_GBL_CURLABEL, trigger_value.str.len)))
		{
			if ((1 == trigger_value.str.len) && ('1' == *trigger_value.str.addr))
				/* 1 == #LABEL - No leading blank in xecute string */
				skip_chars = 0;
		}
		BUILD_HASHT_SUB_SUB_CURRKEY(gbl_name, gbl_name_len, LITERAL_HASHCYCLE, LITERAL_HASHCYCLE_LEN);
		if (!gvcst_get(&trigger_value))
		{	/* There has to be a #CYCLE */
			if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
				t_retry(cdb_sc_triggermod);
			assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
			rts_error_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(8) ERR_TRIGDEFBAD, 6, gbl_name_len,
					gbl_name, gbl_name_len, gbl_name, LEN_AND_LIT("\"#CYCLE\""));
		}
		cycle_len = MIN(trigger_value.str.len, MAX_DIGITS_IN_INT);
		memcpy(cycle, trigger_value.str.addr, cycle_len);
		cycle[cycle_len] = '\0';
		trig_protected_mval_push_count = 0;
		xecute_buff = NULL;
		INCR_AND_PUSH_MV_STENT(protect_trig_mval); /* Protect protect_trig_mval from garbage collection */
		for (indx = 1; indx <= count; indx++)
		{
			if ((0 != nam_indx) && (indx != nam_indx))
				continue;
			MV_FORCE_MVAL(&mi, indx);
			BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(gbl_name, gbl_name_len, mi, trigger_subs[TRIGNAME_SUB],
				STRLEN(trigger_subs[TRIGNAME_SUB]));
			out_rec_ptr = out_rec;
			if (!gvcst_get(&trigger_value))
			{	/* There has to be a #NAME */
				if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
					t_retry(cdb_sc_triggermod);
				assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
				rts_error_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(8) ERR_TRIGDEFBAD, 6, gbl_name_len,
						gbl_name, gbl_name_len, gbl_name, LEN_AND_LIT("\"#NAME\""));

			}
			*out_rec_ptr++ = COMMENT_LITERAL;
			COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_rec_ptr, TRIGGER_NAME_COMMENT,
				STR_LIT_LEN(TRIGGER_NAME_COMMENT));
			COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_rec_ptr, trigger_value.str.addr,
				trigger_value.str.len - 1);	/* remove last # in name */
			COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_rec_ptr, SPANREG_REGION_LIT,
				SPANREG_REGION_LITLEN);
			COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_rec_ptr, gv_cur_region->rname,
				gv_cur_region->rname_len);
			COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_rec_ptr, ")", 1);
			COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_rec_ptr, TRIGGER_CYCLE_COMMENT,
				STR_LIT_LEN(TRIGGER_CYCLE_COMMENT));
			COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_rec_ptr, cycle, STRLEN(cycle));
			util_out_print_gtmio("!AD", FLUSH, (unsigned int)(out_rec_ptr - out_rec), out_rec);
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
			/* Assert that BHASH and LHASH are not part of NUM_SUBS calculation
			 * (confirms the -2 done in the #define of NUM_SUBS).
			 */
			assert(BHASH_SUB == NUM_SUBS);
			assert(LHASH_SUB == (NUM_SUBS + 1));
			for (sub_indx = 0; sub_indx < NUM_SUBS; sub_indx++)
			{
				if ((GVSUBS_SUB == sub_indx) || (CHSET_SUB == sub_indx))
					continue;
				BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(gbl_name, gbl_name_len, mi, trigger_subs[sub_indx],
					STRLEN(trigger_subs[sub_indx]));
				have_value = gvcst_get(protect_trig_mval);
				have_value = have_value && (protect_trig_mval->str.len);
				multi_record = FALSE;
				if (!have_value && (XECUTE_SUB == sub_indx))
				{
					op_gvdata(&data_val);
					multi_record = (literal_ten.m[0] == data_val.m[0]) && (literal_ten.m[1] == data_val.m[1]);
				}
				if (have_value || multi_record)
				{
					if (TRIGNAME_SUB == sub_indx)
					{	/* Output "-name=XYZ" only if it is user defined */
						BUILD_HASHT_SUB_MSUB_SUB_MSUB_CURRKEY(gbl_name, gbl_name_len, mi,
							trigger_subs[sub_indx], STRLEN(trigger_subs[sub_indx]), mi);
						if (!trigger_user_name(protect_trig_mval->str.addr, protect_trig_mval->str.len))
							continue;
						protect_trig_mval->str.len--;	/* Don't include trailing # */
					}
					COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_rec_ptr, " ", 1);
					COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_rec_ptr, triggerfile_quals[sub_indx],
						STRLEN(triggerfile_quals[sub_indx]));
					switch (sub_indx)
					{
						case DELIM_SUB:
						case ZDELIM_SUB:
							MAKE_ZWR_STR(protect_trig_mval->str.addr, protect_trig_mval->str.len,
									out_rec, out_rec_ptr);
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
								multi_record ? NULL : protect_trig_mval, &xecute_len);
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
								protect_trig_mval->str.addr,protect_trig_mval->str.len);
					}
				}
				protect_trig_mval->mvtype = 0; /* can now be garbage collected in the next iteration */
			}
			/* we had better have an XECUTE STRING, if not it is a restartable situation */
			DEBUG_ONLY(if (NULL == xecute_buff) TREF(donot_commit) |= DONOTCOMMIT_TRIGGER_SELECT_XECUTE;)
			util_out_print_gtmio("!AD", FLUSH, (unsigned int)(out_rec_ptr - out_rec), out_rec);
			if (multi_line)
			{
				ptr1 = xecute_buff;
				ptrtop = xecute_buff + xecute_len;
				do
				{
					xecute_len = ptrtop - ptr1;
					ptr2 = memchr(ptr1, '\n', xecute_len);
					if (NULL == ptr2)
						ptr2 = ptrtop; /* if last line is not terminated by newline, make it so */
					out_rec_ptr = out_rec;
					COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_rec_ptr, ptr1, UINTCAST(ptr2 - ptr1));
					util_out_print_gtmio("!AD", FLUSH, (unsigned int)(out_rec_ptr - out_rec), out_rec);
					ptr1 = ptr2 + 1;
				} while (ptr1 < ptrtop);
				out_rec_ptr = out_rec;
				COPY_TO_OUTPUT_AND_WRITE_IF_NEEDED(out_rec, out_rec_ptr, XTENDED_STOP, XTENDED_STOP_LEN);
				util_out_print_gtmio("!AD", FLUSH, (unsigned int)(out_rec_ptr - out_rec), out_rec);
			}
			if (NULL != xecute_buff)
			{
				free(xecute_buff);
				xecute_buff = NULL;
			}
		}
		DECR_AND_POP_MV_STENT();
	}
}

STATICFNDEF void write_gbls_or_names(char *gbl_name, uint4 gbl_name_len, boolean_t trig_name)
{
	char			save_name[MAX_MIDENT_LEN], curr_name[MAX_MIDENT_LEN], curr_gbl[MAX_MIDENT_LEN];
	boolean_t		wildcard;
	mval			mv_curr_nam;
        mval                    trig_gbl;
        mval                    mv_trigger_val;
	int			indx;
	char			*ptr;
	uint4			curr_name_len;
	int			trigvn_len;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* If ^#t needs to be upgraded, issue error. Cannot read older ^#t format that newer version does not always understand */
	if (cs_addrs->hdr->hasht_upgrade_needed)
		rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_NEEDTRIGUPGRD, 2, DB_LEN_STR(gv_cur_region));
	memcpy(save_name, gbl_name, gbl_name_len);
	wildcard = (NULL != (ptr = memchr(gbl_name, '*', gbl_name_len)));
	if (wildcard)
	{
		gbl_name_len--;
		assert(INTCAST(ptr - gbl_name) == gbl_name_len);
	}
	memcpy(curr_name, gbl_name, gbl_name_len);
	curr_name_len = gbl_name_len;
	while (TRUE)
	{
		if (trig_name)
		{
			/* $get(^#t("#TNAME",trigger_name)) */
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
					if (0 != memcmp(curr_name, save_name, gbl_name_len))
						break;
					continue;
				}
				break;
			}
			ptr = mv_trigger_val.str.addr;
			trigvn_len = MIN(mv_trigger_val.str.len, MAX_MIDENT_LEN);
			STRNLEN(ptr, trigvn_len, trigvn_len);
			ptr += trigvn_len;
			if ((mv_trigger_val.str.len == trigvn_len) || ('\0' != *ptr))
			{	/* We expect $c(0) in the middle of addr. If we dont find it, this is a restartable situation */
				if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
					t_retry(cdb_sc_triggermod);
				assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
				rts_error_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(8) ERR_TRIGDEFBAD, 6,
						LEN_AND_LIT("\"#TNAME\""), curr_name_len, curr_name,
						mv_trigger_val.str.len, mv_trigger_val.str.addr);
			}
			ptr++;
			A2I(ptr, mv_trigger_val.str.addr + mv_trigger_val.str.len, indx);
			if (1 > indx)
			{	/* We expect a valid index */
				if (UPDATE_CAN_RETRY(t_tries, t_fail_hist[t_tries]))
					t_retry(cdb_sc_triggermod);
				assert(WBTEST_HELPOUT_TRIGDEFBAD == gtm_white_box_test_case_number);
				rts_error_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(8) ERR_TRIGDEFBAD, 6,
						LEN_AND_LIT("\"#TNAME\""), curr_name_len, curr_name,
						mv_trigger_val.str.len, mv_trigger_val.str.addr);
			}
			/* Use a local buffer to avoid possible garbage collection issues from write_out_trigger below */
			memcpy(curr_gbl, mv_trigger_val.str.addr, trigvn_len);
			STR2MVAL(trig_gbl, curr_gbl, trigvn_len);
		} else
		{
			STR2MVAL(trig_gbl, curr_name, curr_name_len);
			indx = 0;
		}
		write_out_trigger(trig_gbl.str.addr, trig_gbl.str.len, indx);
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
			if (0 != memcmp(curr_name, save_name, gbl_name_len))
				break;
			curr_name_len = mv_curr_nam.str.len;
		} else
			break;
	}
}

STATICFNDEF void dump_all_triggers(void)
{
	mval			curr_gbl_name, val;
	gd_region		*reg;
	gv_namehead		*save_gvtarget;
	int			reg_index;
	char			global[MAX_MIDENT_LEN];
	int			gbl_len;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(NULL != gd_header);
	save_gvtarget = gv_target;
	for (reg_index = 0, reg = gd_header->regions; reg_index < gd_header->n_regions; reg_index++, reg++)
	{
		if (IS_STATSDB_REGNAME(reg))
			continue;
		GVTR_SWITCH_REG_AND_HASHT_BIND_NAME(reg);
		if (NULL == cs_addrs)	/* not BG or MM access method */
			continue;
		/* gv_target now points to ^#t in region "reg" */
		if (0 != gv_target->root)
		{
			/* If ^#t needs to be upgraded, issue error. Cannot read older ^#t format */
			if (cs_addrs->hdr->hasht_upgrade_needed)
				rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_NEEDTRIGUPGRD, 2, DB_LEN_STR(gv_cur_region));
			op_gvdata(&val);
			if ((literal_ten.m[0] == val.m[0]) && (literal_ten.m[1] == val.m[1]))
			{	/* $DATA(^#t) is 10 - get first subscript (trigger's global) */
				BUILD_HASHT_SUB_CURRKEY(LITERAL_MAXHASHVAL, STR_LIT_LEN(LITERAL_MAXHASHVAL));
				while (TRUE)
				{
					op_gvorder(&curr_gbl_name);
					if (0 == curr_gbl_name.str.len)
						break;
					gbl_len = curr_gbl_name.str.len;
					memcpy(global, curr_gbl_name.str.addr, gbl_len);
					write_out_trigger(global, gbl_len, 0);
					BUILD_HASHT_SUB_CURRKEY(global, gbl_len);
				}
			} else
				assert((literal_zero.m[0] == val.m[0]) && (literal_zero.m[1] == val.m[1]));
		}
	}
}

/* returns TRUE (not TRIG_FAILURE) in case of failure */
boolean_t trigger_select_tpwrap(char *select_list, uint4 select_list_len, char *file_name, uint4 file_name_len)
{
	mval			op_val, op_pars;
	boolean_t		select_status;
	io_pair			save_io_curr_device;
	mval			ts_mv;
	int			loopcnt;
	DEBUG_ONLY(unsigned int	lcl_t_tries;)
	enum cdb_sc		failure;

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
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* make a local copy of the select list and use it to avoid string-pool problems */
	if (MAX_BUFF_SIZE <= select_list_len)
		return TRUE;	/* failure return */
	gvinit();
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
		(*op_open_ptr)(&op_val, &op_pars, (mval *)&literal_zero, NULL);
	}
	save_io_curr_device = io_curr_device;
	op_use(&op_val, &op_pars);
	TREF(ztrig_use_io_curr_device) = TRUE;
	select_status = TRIG_SUCCESS;
	ts_mv.mvtype = MV_STR;
	ts_mv.str.len = 0;
	ts_mv.str.addr = NULL;
	if (0 == dollar_tlevel)
	{	/* If not already wrapped in TP, wrap it now implicitly */
		assert(!donot_INVOKE_MUMTSTART);
		DEBUG_ONLY(donot_INVOKE_MUMTSTART = TRUE);
		/* Note down dollar_tlevel before op_tstart. This is needed to determine if we need to break from the for-loop
		 * below after a successful op_tcommit of the trigger select . We cannot check that dollar_tlevel is zero
		 * since the op_tstart done below can be a nested sub-transaction
		 */
		op_tstart((IMPLICIT_TSTART + IMPLICIT_TRIGGER_TSTART), TRUE, &ts_mv, 0); /* 0 ==> save no locals but RESTART OK */
		/* The following for loop structure is similar to that in function "trigger_trgfile_tpwrap" and various
		 * other places so any changes here might need to be reflected there as well.
		 */
		for (loopcnt = 0; ; loopcnt++)
		{
			assert(donot_INVOKE_MUMTSTART);	/* Make sure still set */
			DEBUG_ONLY(lcl_t_tries = t_tries);
			select_status = trigger_select_tpwrap_helper(select_list, select_list_len);
			if (0 == dollar_tlevel)
				break;
			assert(0 < t_tries);
			assert((CDB_STAGNATE == t_tries) || (lcl_t_tries == t_tries - 1));
			failure = LAST_RESTART_CODE;
			assert(((cdb_sc_onln_rlbk1 != failure) && (cdb_sc_onln_rlbk2 != failure))
				|| !gv_target || !gv_target->root);
			assert((cdb_sc_onln_rlbk2 != failure) || !IS_GTM_IMAGE || TREF(dollar_zonlnrlbk));
			if (cdb_sc_onln_rlbk2 == failure)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DBROLLEDBACK);
			/* else if (cdb_sc_onln_rlbk1 == status) we don't need to do anything other than trying again. Since this
			 * is ^#t global, we don't need to GVCST_ROOT_SEARCH before continuing with the next restart because the
			 * trigger load logic already takes care of doing INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED before doing the
			 * actual trigger load
			 */
			/* We expect the above function to return with either op_tcommit or a tp_restart invoked.
			 * In the case of op_tcommit, we expect dollar_tlevel to be 0 and if so we break out of the loop.
			 * In the tp_restart case, we expect a maximum of 4 tries/retries and much lesser usually.
			 * Additionally we also want to avoid an infinite loop so limit the loop to what is considered
			 * a huge iteration count and assertpro if that is reached as it suggests an out-of-design situation.
			 */
			assertpro(TPWRAP_HELPER_MAX_ATTEMPTS >= loopcnt);
		}
	} else
	{
		select_status = trigger_select(select_list, select_list_len);
		assert(0 < dollar_tlevel);
	}
	TREF(ztrig_use_io_curr_device) = FALSE;
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
	return (TRIG_FAILURE == select_status);
}

STATICFNDEF boolean_t trigger_select_tpwrap_helper(char *select_list, uint4 select_list_len)
{
	enum cdb_sc		cdb_status;
	boolean_t		select_status;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ESTABLISH_RET(trigger_tpwrap_ch, TRIG_FAILURE);
	select_status = trigger_select(select_list, select_list_len);
	if (TRIG_SUCCESS == select_status)
	{
		GVTR_OP_TCOMMIT(cdb_status);
		if (cdb_sc_normal != cdb_status)
			t_retry(cdb_status);	/* won't return */
	} else
	{	/* Record cannot be committed - undo everything */
		assert(donot_INVOKE_MUMTSTART);
		DEBUG_ONLY(donot_INVOKE_MUMTSTART = FALSE);
		/* Print $ztrigger/mupip-trigger output before rolling back TP */
		TP_ZTRIGBUFF_PRINT;
		OP_TROLLBACK(-1);		/* returns but kills implicit transaction */
	}
	REVERT;
	return select_status;
}

STATICFNDEF boolean_t trigger_select(char *select_list, uint4 select_list_len)
{
	boolean_t		dump_all;
	char			save_select_list[MAX_BUFF_SIZE];
	char			*sel_ptr, *strtok_ptr, *prev_ptr, *ptr1, *ptr2;
	int			gbl_len, prev_len;
	mname_entry		gvname;
	int			len, len1, badpos;
	boolean_t		trig_name;
	gv_key			save_currkey[DBKEYALLOC(MAX_KEY_SZ)];
	gd_region		*save_gv_cur_region;
	gv_namehead		*save_gv_target;
	sgm_info		*save_sgm_info_ptr;
	jnlpool_addrs_ptr_t	save_jnlpool;
	boolean_t		select_status;
	gvnh_reg_t		*gvnh_reg;
	gd_binding		*map, *start_map, *end_map;
	gd_region		*reg, *reg_start, *reg_top;
	int			*reg_done, reg_array_size, reg_index;

	assert(dollar_tlevel);
	select_status = TRIG_SUCCESS;
	memcpy(save_select_list, select_list, select_list_len);
	save_select_list[select_list_len] = '\0';
	dump_all = FALSE;
	prev_len = 0;
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
	if (dump_all)
		dump_all_triggers();
	else
	{
		len = select_list_len;
		sel_ptr = STRTOK_R(save_select_list, ",", &strtok_ptr);
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
				gbl_len = NAM_LEN(sel_ptr + 1, (int)(ptr1 - sel_ptr) - 1);
				ptr1 = sel_ptr + 1;
				/* Skip only if the previous global is the same as the current */
				if ((prev_len == gbl_len) && (0 == memcmp(prev_ptr, ptr1, gbl_len)))
					continue;
				SAVE_REGION_INFO(save_currkey, save_gv_target, save_gv_cur_region, save_sgm_info_ptr, save_jnlpool);
				prev_ptr = ptr1;
				prev_len = gbl_len;
				start_map = gv_srch_map(gd_header, ptr1, gbl_len, SKIP_BASEDB_OPEN_FALSE);
				ptr1[gbl_len - 1]++;
				end_map = gv_srch_map(gd_header, ptr1, gbl_len, SKIP_BASEDB_OPEN_FALSE);
				ptr1[gbl_len - 1]--;
				gvname.var_name.addr = ptr1;
				gvname.var_name.len = gbl_len;
				if (start_map != end_map)
				{	/* Global specification involves multiple regions */
					reg_start = &gd_header->regions[0];
					reg_array_size = gd_header->n_regions;
					reg_done = malloc(reg_array_size * SIZEOF(*reg_done));
					memset(reg_done, 0, reg_array_size * SIZEOF(*reg_done));
					if ('*' == ptr1[gbl_len])
						gvname.var_name.len++;
					for (map = start_map; map <= end_map; map++)
					{
						OPEN_BASEREG_IF_STATSREG(map);
						reg = map->reg.addr;
						GET_REG_INDEX(gd_header, reg_start, reg, reg_index);	/* sets "reg_index" */
						assert(reg_array_size > reg_index);
						if (!reg_done[reg_index])
						{	/* this region first encountered now */
							if (IS_STATSDB_REGNAME(reg))
								continue;
							GVTR_SWITCH_REG_AND_HASHT_BIND_NAME(reg);
							if (NULL == cs_addrs)	/* not BG or MM access method */
								continue;
							/* gv_target now points to ^#t in region "reg" */
							if (0 != gv_target->root)
								write_gbls_or_names(gvname.var_name.addr, gvname.var_name.len,
												trig_name);
							reg_done[reg_index] = TRUE;
						}
					}
					free(reg_done);
				} else
				{	/* Global specification involves only one region */
					COMPUTE_HASH_MNAME(&gvname);
					GV_BIND_NAME_ONLY(gd_header, &gvname, gvnh_reg);
					/* skip selecting/dumping triggers if not BG or MM access method */
					if (NULL != cs_addrs)
					{
						if ('*' == ptr1[gbl_len])
							gvname.var_name.len++;
						SET_GVTARGET_TO_HASHT_GBL(gv_target->gd_csa);
						INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
						if (0 != gv_target->root)
							write_gbls_or_names(gvname.var_name.addr, gvname.var_name.len,
											trig_name);
					}
				}
			} else
			{
				if (len1 != (badpos = validate_input_trigger_name(ptr1, len1, NULL))) /* assignment is intended */
				{ /* is the input name valid */
					INVALID_NAME_ERROR("Invalid name entry in SELECT list: ", sel_ptr, select_status, badpos);
					continue;
				}
				if (TRIGNAME_SEQ_DELIM == *(sel_ptr + (len1 - 1)))
					len1--;	/* drop the trailing # sign */
				gvname.var_name.addr = sel_ptr;
				gvname.var_name.len = len1;
				SAVE_REGION_INFO(save_currkey, save_gv_target, save_gv_cur_region, save_sgm_info_ptr, save_jnlpool);
				for (reg = gd_header->regions, reg_top = reg + gd_header->n_regions; reg < reg_top; reg++)
				{
					if (IS_STATSDB_REGNAME(reg))
						continue;
					GVTR_SWITCH_REG_AND_HASHT_BIND_NAME(reg);
					if (NULL == cs_addrs)	/* not BG or MM access method */
						continue;
					/* gv_target now points to ^#t in region "reg" */
					if (0 == gv_target->root)
						continue;
					write_gbls_or_names(gvname.var_name.addr, gvname.var_name.len, trig_name);
				}
			}
			RESTORE_REGION_INFO(save_currkey, save_gv_target, save_gv_cur_region, save_sgm_info_ptr, save_jnlpool);
		} while (NULL != (sel_ptr = STRTOK_R(NULL, ",", &strtok_ptr)));	/* Embedded assignment is intended */
	}
	return select_status;
}
#endif
