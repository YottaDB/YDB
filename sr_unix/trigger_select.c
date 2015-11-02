/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
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
#include "rtnhdr.h"
#include "gv_trigger.h"
#include "targ_alloc.h"
#include "filestruct.h"
#include "mv_stent.h"		/* for COPY_SUBS_TO_GVCURRKEY macro */
#include "gvsub2str.h"		/* for COPY_SUBS_TO_GVCURRKEY macro */
#include "format_targ_key.h"	/* for COPY_SUBS_TO_GVCURRKEY macro */
#include "hashtab.h"		/* for STR_HASH (in COMPUTE_HASH_MNAME)*/
#include "trigger.h"
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
#include "io_params.h"
#include "min_max.h"			/* Needed for MIN */

GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_namehead		*gv_target;
GBLREF	gv_key			*gv_currkey;
GBLREF	gd_addr			*gd_header;
GBLREF	gv_key			*gv_altkey;
GBLREF	io_log_name		*dollar_principal;	/* pointer to log name GTM$PRINCIPAL if defined */
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	int			(*op_open_ptr)(mval *v, mval *p, int t, mval *mspace);

LITREF	mval		    	literal_hasht;
LITREF	mval			literal_zero;
LITREF	mval			literal_ten;

#define TRIGGER_NAME_COMMENT	"trigger name: "
#define TRIGGER_CYCLE_COMMENT	"  cycle: "

#define NAM_LEN(PTR, LEN)	MIN(STRLEN((PTR)), (LEN))

#define MAKE_ZWR_STR(STR, STR_LEN, OUT_STR)						\
{											\
	int		lcl_len;				       			\
											\
	format2zwr((sm_uc_ptr_t)STR, STR_LEN, (unsigned char *)OUT_STR, &lcl_len);	\
	OUT_STR += lcl_len;								\
}

STATICDEF char *triggerfile_quals[] = {"-name=", "", "-commands=", "-options=", "-delim=", "-zdelim=", "-pieces=", "-xecute="};

STATICFNDEF void write_subscripts(char **out_ptr, char **sub_ptr, int *sub_len)
{
	char			*out_p, *ptr, *dst_ptr;
	int			str_len;
	unsigned short		len_left, dst_len, len;
	char			dst[MAX_GVSUBS_LEN];

	len_left = *sub_len;
	ptr = *sub_ptr;
	out_p = *out_ptr;
	while (0 < len_left)
	{
		if (ISDIGIT(*ptr) || ('-' == *ptr))
		{
			*out_p++ = *ptr++;
			len_left--;
			while ((0 < len_left) && ISDIGIT(*ptr))
			{
				*out_p++ = *ptr++;
				len_left--;
			}
		}
		else if (ISALPHA(*ptr) || ('%' == *ptr))
		{
			*out_p++ = *ptr++;
			len_left--;
			while ((0 < len_left) && ISALNUM(*ptr))
			{
				*out_p++ = *ptr++;
				len_left--;
			}
		} else if ('"' == *ptr)
		{
			len = len_left;
			trigger_scan_string(ptr, &len_left, dst, &dst_len);
			MAKE_ZWR_STR(dst, dst_len, out_p);
			len_left--;		/* Need to skip the " */
			ptr += (len - len_left);
		} else
		{
			*out_p++ = *ptr++;
			len_left--;
		}
	}
	*sub_len = len_left;
	*sub_ptr = ptr;
	*out_ptr = out_p;
}

STATICFNDEF void write_out_trigger(char *gbl_name, unsigned short gbl_name_len, unsigned short file_name_len, mval *op_val,
				   int nam_indx)
{
	char			out_rec[MAX_BUFF_SIZE];
	char			*out_rec_ptr;
	char			*ptr1, *ptr2;
	mname_entry		gvent;
	mval			mi, trigger_count, trigger_value;
	mval			*mv_trig_cnt_ptr;
	int			count;
	int			indx, sub_indx;
	int			sub_len;
	char			*sub_ptr;
	char			*tmp_str_ptr, tmp_string[MAX_SRCLINE];
	unsigned short		tmp_str_len;
	char			cycle[MAX_DIGITS_IN_INT + 1];

	BUILD_HASHT_SUB_SUB_CURRKEY(gbl_name, gbl_name_len, LITERAL_HASHCOUNT, STRLEN(LITERAL_HASHCOUNT));
	if (gvcst_get(&trigger_count))
	{
		mv_trig_cnt_ptr = &trigger_count;
		count = MV_FORCE_INT(mv_trig_cnt_ptr);
		BUILD_HASHT_SUB_SUB_CURRKEY(gbl_name, gbl_name_len, LITERAL_HASHCYCLE, STRLEN(LITERAL_HASHCYCLE));
		if (!gvcst_get(&trigger_value))
			assert(FALSE);
		assert(MAX_DIGITS_IN_INT >= trigger_value.str.len);
		memcpy(cycle, trigger_value.str.addr, trigger_value.str.len);
		cycle[trigger_value.str.len] = '\0';
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
				memcpy(out_rec_ptr, TRIGGER_NAME_COMMENT, STRLEN(TRIGGER_NAME_COMMENT));
				out_rec_ptr += STR_LIT_LEN(TRIGGER_NAME_COMMENT);
				memcpy(out_rec_ptr, trigger_value.str.addr, trigger_value.str.len);
				out_rec_ptr += trigger_value.str.len;
				memcpy(out_rec_ptr, TRIGGER_CYCLE_COMMENT, STRLEN(TRIGGER_CYCLE_COMMENT));
				out_rec_ptr += STR_LIT_LEN(TRIGGER_CYCLE_COMMENT);
				memcpy(out_rec_ptr, cycle, STRLEN(cycle));
				out_rec_ptr += STRLEN(cycle);
				if (0 != file_name_len)
				{
					*out_rec_ptr++ = '\n';
					op_val->str.addr = (char *)out_rec;
					op_val->str.len = (unsigned int)(out_rec_ptr - out_rec);
					op_write(op_val);
				} else
					util_out_print_gtmio("!AD", TRUE, (unsigned int)(out_rec_ptr - out_rec),
						       (char *)out_rec);
			}
			out_rec_ptr = out_rec;
			*out_rec_ptr++ = '+';
			*out_rec_ptr++ = '^';
			memcpy(out_rec_ptr, gbl_name, gbl_name_len);
			out_rec_ptr += gbl_name_len;
			BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(gbl_name, gbl_name_len, mi, trigger_subs[GVSUBS_SUB],
				STRLEN(trigger_subs[GVSUBS_SUB]));
			if (gvcst_get(&trigger_value))
			{
				*out_rec_ptr++ = '(';
				sub_ptr = trigger_value.str.addr;
				sub_len = trigger_value.str.len;
				write_subscripts(&out_rec_ptr, &sub_ptr, &sub_len);
				*out_rec_ptr++ = ')';
			}
			for (sub_indx = 0; sub_indx < NUM_SUBS; sub_indx++)
			{
				if ((GVSUBS_SUB == sub_indx) || (CHSET_SUB == sub_indx))
					continue;
				BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(gbl_name, gbl_name_len, mi, trigger_subs[sub_indx],
					STRLEN(trigger_subs[sub_indx]));
				if (gvcst_get(&trigger_value))
				{
					if (TRIGNAME_SUB == sub_indx)
					{	/* Output name only if it is user defined */
						if (!trigger_user_name(trigger_value.str.addr, trigger_value.str.len))
							continue;
						trigger_value.str.len--;	/* Don't include trailing # */
					}
					*out_rec_ptr++ = ' ';
					memcpy(out_rec_ptr, triggerfile_quals[sub_indx], STRLEN(triggerfile_quals[sub_indx]));
					out_rec_ptr += STRLEN(triggerfile_quals[sub_indx]);
					switch (sub_indx)
					{
						case DELIM_SUB:
						case ZDELIM_SUB:
							MAKE_ZWR_STR(trigger_value.str.addr, trigger_value.str.len, out_rec_ptr);
							break;
						case XECUTE_SUB:
							assert(MAX_SRCLINE > trigger_value.str.len);
							if ('"' == trigger_value.str.addr[0])
							{
								tmp_str_ptr = &tmp_string[0];
								trigger_scan_string(trigger_value.str.addr,
									(unsigned short *)(&trigger_value.str.len), tmp_str_ptr,
									&tmp_str_len);
							} else
							{
								tmp_str_ptr = trigger_value.str.addr;
								tmp_str_len = trigger_value.str.len;
							}
							MAKE_ZWR_STR(tmp_str_ptr, tmp_str_len, out_rec_ptr);
							break;
						default:
							memcpy(out_rec_ptr, trigger_value.str.addr, trigger_value.str.len);
							out_rec_ptr += trigger_value.str.len;
							break;
					}
				}
			}
			if (0 != file_name_len)
			{
				*out_rec_ptr++ = '\n';
				op_val->str.addr = (char *)out_rec;
				op_val->str.len = (unsigned int)(out_rec_ptr - out_rec);
				op_write(op_val);
			} else
				util_out_print_gtmio("!AD", TRUE, (unsigned int)(out_rec_ptr - out_rec), (char *)out_rec);
		}
	}
}

STATICFNDEF void write_gbls_or_names(char *gbl_name, unsigned short gbl_name_len, unsigned short file_name_len, mval *op_val,
				     boolean_t trig_name)
{
	char			save_name[MAX_MIDENT_LEN + 1], curr_name[MAX_MIDENT_LEN + 1];
	boolean_t		wildcard;
	mval			mv_curr_nam;
        mval                    mi, trigger_count, trig_gbl;
        mval                    *mv_trig_cnt_ptr, mv_trigger_val;
	int			indx, count;
	char			*ptr;
	boolean_t		name_found;
	uint4			curr_name_len;
	int			trigvn_len;

	memcpy(save_name, gbl_name, gbl_name_len);
	save_name[gbl_name_len] = '\0';
	wildcard = (NULL != (ptr = strchr(gbl_name, '*')));
	if (wildcard)
	{
		*ptr = '\0';
		gbl_name_len--;
	}
	if (trig_name)
	{
		BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), gbl_name, gbl_name_len);
	} else
	{
		BUILD_HASHT_SUB_CURRKEY(gbl_name, gbl_name_len);
	}
	mv_trig_cnt_ptr = &trigger_count;
	name_found = FALSE;
	memcpy(curr_name, gbl_name, gbl_name_len);
	curr_name_len = gbl_name_len;
	STR2MVAL(mv_curr_nam, gbl_name, gbl_name_len);
	while (TRUE)
	{
		if (0 != memcmp(curr_name, save_name, gbl_name_len))
			break;
		if (trig_name)
		{
			BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), curr_name, curr_name_len);
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
			assert(MAX_MIDENT_LEN >= trigvn_len);
			ptr += trigvn_len + 1;
			A2I(ptr, mv_trigger_val.str.addr + mv_trigger_val.str.len, indx);
			STR2MVAL(trig_gbl, mv_trigger_val.str.addr, trigvn_len);
		} else
		{
			STR2MVAL(trig_gbl, curr_name, curr_name_len);
			name_found = TRUE;
			indx = 0;
		}
		if (name_found)
			write_out_trigger(trig_gbl.str.addr, trig_gbl.str.len, file_name_len, op_val, indx);
		if (wildcard)
		{
			if (trig_name)
			{
				BUILD_HASHT_SUB_SUB_CURRKEY(LITERAL_HASHTNAME, STRLEN(LITERAL_HASHTNAME), curr_name, curr_name_len);
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

STATICFNDEF void dump_all_triggers(unsigned short file_name_len, mval *op_val)
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
				BUILD_HASHT_SUB_CURRKEY(LITERAL_MAXHASHVAL, STRLEN(LITERAL_MAXHASHVAL));
				while (TRUE)
				{
					op_gvorder(&curr_gbl_name);
					if (0 == curr_gbl_name.str.len)
						break;
					gbl_len = curr_gbl_name.str.len;
					memcpy(global, curr_gbl_name.str.addr, gbl_len);
					write_out_trigger(global, gbl_len, file_name_len, op_val, 0);
					BUILD_HASHT_SUB_CURRKEY(curr_gbl_name.str.addr, curr_gbl_name.str.len);
				}
			} else
				assert((literal_zero.m[0] == val.m[0]) && (literal_zero.m[1] == val.m[1]));
		}
	}
}

boolean_t trigger_select(char *select_list, uint4 select_list_len, char *file_name, uint4 file_name_len)
{
	boolean_t		found_blank;
	char			*sel_ptr, *prev_ptr, *ptr1, *ptr2;
	int			gbl_len, prev_len;
	mstr			gbl_name;
	sgmnt_addrs		*csa;
	gv_namehead		*hasht_tree, *save_gvtarget;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	char			save_altkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key			*save_gv_currkey;
	gv_key			*save_gv_altkey;
	gd_region		*save_gv_cur_region;
	mname_entry		gvent;
	mval			trigger_value;
	int			len, len1;
	int			local_errno;
	boolean_t		trig_name;
	gv_namehead		*save_gv_target;
	sgm_info		*save_sgm_info_ptr;
	boolean_t		dump_all;
	mval			op_val, op_pars;
	boolean_t		select_error;
	static readonly unsigned char	open_params_list[] =
	{
		(unsigned char)iop_m,
		(unsigned char)iop_noreadonly,
		(unsigned char)iop_nowrap,
		(unsigned char)iop_stream,
		(unsigned char)iop_eol
	};
	static readonly unsigned char	no_param = (unsigned char)iop_eol;

	error_def(ERR_MUPCLIERR);
	error_def(ERR_MUNOACTION);

	gvinit();
	dump_all = found_blank = FALSE;
	prev_len = 0;
	if (0 == select_list_len)
		dump_all = TRUE;
	else
	{
		for (ptr1 = select_list, len = select_list_len;
				(NULL != (ptr2 = strchr(ptr1, '*'))) && (len > (len1 = INTCAST(ptr2 - ptr1)));
				ptr1 = ptr2 + 1)
		{	/* look for either a real "dump-it-all" *, an error *, or a wildcard * */
			/* A "*" anywhere in the select list (at a place where a global name would be) is the same as only a "*" */
			len -= (len1 + 1);		/* Length after the "*" -- len1 is length before the "*" */
			assert((0 <= len1) && (0 <= len));
			if (dump_all = ((0 == len1) && (0 == len))
					|| ((0 == len1) && (0 < len) && (',' == *(ptr2 + 1)))
					|| ((0 < len1) && (0 == len) && (',' == *(ptr2 - 1)))
					|| ((0 < len1) && (0 < len) && (',' == *(ptr2 - 1)) && (',' == *(ptr2 + 1))))
				break;
		}
		if (0 != file_name_len)
		{
			op_pars.mvtype = MV_STR;
			op_pars.str.len = SIZEOF(open_params_list);
			op_pars.str.addr = (char *)open_params_list;
			op_val.mvtype = MV_STR;
			op_val.str.len = file_name_len;
			op_val.str.addr = (char *)file_name;
			(*op_open_ptr)(&op_val, &op_pars, 0, 0);
			op_use(&op_val, &op_pars);
		}
	}
	select_error = FALSE;
	if (dump_all)
		dump_all_triggers(file_name_len, &op_val);
	else
	{
		len = select_list_len;
		sel_ptr = strtok(select_list, ",");
		do
		{
			if (NULL != (ptr1 = strchr(sel_ptr, ' ')))
			{
				*ptr1 = '\0';
				found_blank = TRUE;
			}
			trig_name = ('^' != *sel_ptr);
			ptr1 = sel_ptr;
			if (!trig_name)
			{
				ptr1++;
				len--;
				if (!ISALPHA(*ptr1) && ('%' != *ptr1))
				{
					util_out_print_gtmio("Invalid global variable name in SELECT list- !AD", TRUE,
						NAM_LEN(sel_ptr, len), sel_ptr);
					select_error = TRUE;
					continue;
				}
				ptr1++;
				len--;
				while (ISALNUM(*ptr1) && (0 < len))
				{
					ptr1++;
					len--;
				}
				if ('*' == *ptr1)
				{
					ptr1++;
					len--;
				}
				if (('\0' != *ptr1) && (0 != len))
				{
					util_out_print_gtmio("Invalid entry in SELECT list - !AD", TRUE, NAM_LEN(sel_ptr, len),
						sel_ptr);
					select_error = TRUE;
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
					gv_bind_name(gd_header, &gbl_name);
				}
				csa = gv_target->gd_csa;
				SETUP_TRIGGER_GLOBAL;
			} else
			{	/* It should be a trigger name */
				while (ISGRAPH(*ptr1) && (0 < len))
				{
				       ptr1++;
				       len--;
				}
				if ('\0' != *ptr1 && (0 != len))
				{
					util_out_print_gtmio("Invalid entry in SELECT list - !AD", TRUE, NAM_LEN(sel_ptr, len),
						sel_ptr);
					select_error = TRUE;
					continue;
				}
				gbl_name.addr = sel_ptr;
				gbl_name.len = NAM_LEN(sel_ptr, (int)(ptr1 - sel_ptr));
				SAVE_TRIGGER_REGION_INFO;
				SWITCH_TO_DEFAULT_REGION;
			}
			INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
			if (0 != gv_target->root)
				write_gbls_or_names(gbl_name.addr, gbl_name.len, file_name_len, &op_val, trig_name);
			TP_CHANGE_REG_IF_NEEDED(save_gv_cur_region);
			RESTORE_TRIGGER_REGION_INFO;
			len--;
		} while ((NULL != (sel_ptr = strtok(NULL, ","))) && !found_blank);
	}
	if (0 != file_name_len)
	{
		op_val.mvtype = op_pars.mvtype = MV_STR;
		op_val.str.addr = (char *)file_name;;
		op_val.str.len = file_name_len;
		op_pars.str.len = SIZEOF(no_param);
		op_pars.str.addr = (char *)&no_param;
		op_close(&op_val, &op_pars);
	}
	return select_error;
}
#endif
