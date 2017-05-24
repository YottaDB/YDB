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

#ifndef MUPIP_TRIGGER_INCLUDED
#define MUPIP_TRIGGER_INCLUDED

/* The order of these must match trigger_subs defined in mtables.c */
typedef enum
{
#define TRIGGER_SUBSDEF(SUBSTYPE, SUBSNAME, LITMVALNAME, TRIGFILEQUAL, PARTOFHASH)	SUBSTYPE,
#include "trigger_subs_def.h"
#undef TRIGGER_SUBSDEF
	NUM_TOTAL_SUBS
} trig_subs_t;
#define NUM_SUBS		NUM_TOTAL_SUBS - 2	/* Number of subscripts users deal with.
							 * Hash related subscripts BHASH/LHASH not included.
							 * This assumes BHASH and LHASH are last two entires and is asserted
							 * in "trigger_delete"/"write_out_trigger"/"trigger_update" function entry.
							 */
typedef enum
{
	STATS_ADDED = 0,
	STATS_DELETED,
	STATS_MODIFIED,
	STATS_ERROR_TRIGFILE,
	STATS_UNCHANGED_TRIGFILE,
	STATS_NOERROR_TRIGFILE,
	NUM_STATS
} trig_stats_t;

#define TRIG_SUCCESS		TRUE
#define TRIG_FAILURE		FALSE

#define MAX_COUNT_DIGITS	6			/* Max of 999,999 triggers for a global */
#define MAX_BUFF_SIZE		32768			/* Size of input and output buffers.  The cli routines can't handle lines
							 * longer than 32767 + 256 (MAX_LINE -- see cli.h).  Longer lines will
							 * be truncated by cli_str_setup().
							 */
#define MAX_XECUTE_LEN		MAX_STRLEN		/* Maximum length of the xecute string */

#define COMMENT_LITERAL		';'
#define TRIGNAME_SEQ_DELIM	'#'
#define MAX_GVSUBS_LEN		8192			/* Maximum length of the gvsubs string */
#define	MAX_HASH_INDEX_LEN	32768			/* Max length of value for hash index entries */

#define	NUM_TRIGNAME_SEQ_CHARS	6
#define	MAX_TRIGNAME_LEN	(MAX_MIDENT_LEN - 2)				/* 2 for runtime characters */
#define MAX_USER_TRIGNAME_LEN	MAX_MIDENT_LEN - 3	/* 3 -- 2 for run time chars and 1 for delimiter */
#define	MAX_AUTO_TRIGNAME_LEN	(MAX_MIDENT_LEN - 4 - NUM_TRIGNAME_SEQ_CHARS)	/* 4 -- 2 for runtime characters, 2 for delims */

#define LITERAL_MAXHASHVAL	"$"				/* '$' collates between '#' and '%' */
#define LITERAL_HASHSEQNUM	"#SEQNUM"
#define	LITERAL_HASHTNAME	"#TNAME"
#define	LITERAL_HASHTNCOUNT	"#TNCOUNT"

#define	TRSBS_IN_NONE		0
#define	TRSBS_IN_BHASH		1
#define	TRSBS_IN_LHASH		2

#define	INITIAL_CYCLE		"1"

#define XTENDED_START		"<<"
#define XTENDED_STOP		">>"
#define XTENDED_START_LEN	STR_LIT_LEN(XTENDED_START)
#define XTENDED_STOP_LEN	STR_LIT_LEN(XTENDED_STOP)

#define	PUT_SUCCESS		0

#define	INVALID_LABEL			-1
#define	K_ZTK_CONFLICT			-2
#define	VAL_TOO_LONG			-3
#define	KEY_TOO_LONG			-4
#define	TOO_MANY_TRIGGERS		-5
#define	ADD_SET_MODIFY_KILL_TRIG	-6
#define	ADD_SET_NOCHNG_KILL_TRIG	-7
#define	OPTIONS_CMDS_CONFLICT		-8
#define	NAME_CMDS_CONFLICT		-9

#define CONV_TO_ZWR(LEN, PTR, OUT_LEN, OUT_STR)						\
{											\
	LEN = MIN(OUT_BUFF_SIZE, LEN);							\
	format2zwr((sm_uc_ptr_t)PTR, LEN, OUT_STR, &OUT_LEN);				\
}

#define CONV_TO_ZWR_AND_PRINT(STR, LEN, PTR, OUT_LEN, OUT_STR)				\
{											\
	CONV_TO_ZWR(LEN, PTR, OUT_LEN, OUT_STR);					\
	util_out_print_gtmio(STR"!AD", FLUSH, OUT_LEN, OUT_STR);			\
}

#define CONV_STR_AND_PRINT(STR, LEN, PTR)						\
{											\
	int		out_len;							\
	unsigned char	out_str[MAX_ZWR_EXP_RATIO * OUT_BUFF_SIZE];			\
											\
	CONV_TO_ZWR_AND_PRINT(STR, LEN, PTR, out_len, out_str);				\
}

#define CHECK_FOR_ROOM_IN_OUTPUT_AND_COPY(SRC, DST, LEN, MAX_LEN)		\
{										\
	if (MAX_LEN < LEN)							\
	{									\
		util_out_print_gtmio("Trigger definition too long", FLUSH);	\
		return FALSE;							\
	}									\
	MAX_LEN -= LEN;								\
	memcpy(DST, SRC, LEN);							\
}

/* Build up a comma delimited string */
#define ADD_COMMA_IF_NEEDED(COUNT, PTR, MAX_LEN)					\
{											\
	if (0 != COUNT)									\
	{										\
		if (0 > --MAX_LEN)							\
		{									\
			util_out_print_gtmio("Trigger definition too long", FLUSH);	\
			return FALSE;							\
		}									\
		MEMCPY_LIT(PTR, ",");							\
		PTR++;									\
	}										\
}
#define ADD_STRING(COUNT, PTR, LEN, COMMAND, MAX_LEN)				\
{										\
	CHECK_FOR_ROOM_IN_OUTPUT_AND_COPY(COMMAND, PTR, LEN, MAX_LEN)		\
	PTR += LEN;								\
	COUNT++;								\
}

#define STR2MVAL(MVAL, STR, LEN)						\
{										\
	MVAL.mvtype = MV_STR;							\
	MVAL.str.addr = (char *)STR;   /* Cast to override "const" attribute */ \
	MVAL.str.len = LEN;							\
}

/* Sets up gv_currkey with ^#t */
#define BUILD_HASHT_CURRKEY_NAME						\
{										\
	DCL_THREADGBL_ACCESS;							\
										\
	SETUP_THREADGBL_ACCESS;							\
	memcpy(gv_currkey->base, HASHT_GBLNAME, HASHT_GBLNAME_LEN);		\
	gv_currkey->base[HASHT_GBLNAME_LEN] = '\0';				\
	gv_currkey->base[HASHT_GBLNAME_FULL_LEN] = '\0';			\
	gv_currkey->end = HASHT_GBLNAME_FULL_LEN;				\
	gv_currkey->prev = 0;							\
	TREF(gv_last_subsc_null) = FALSE;					\
	TREF(gv_some_subsc_null) = FALSE;					\
}

#define BUILD_HASHT_SUB_CURRKEY_T(TRIG_VAL, SUB, LEN)						\
{												\
	boolean_t		was_null = FALSE, is_null = FALSE;				\
	mval			*subsc_ptr;							\
	DCL_THREADGBL_ACCESS;									\
												\
	SETUP_THREADGBL_ACCESS;									\
	BUILD_HASHT_CURRKEY_NAME;								\
	subsc_ptr = &TRIG_VAL;									\
	STR2MVAL(TRIG_VAL, SUB, LEN);								\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);	\
	TREF(gv_last_subsc_null) = is_null;							\
	TREF(gv_some_subsc_null) = was_null;							\
}

#define	BUILD_HASHT_SUB_MSUB_CURRKEY_T(TRIG_VAL, SUB1, LEN1, SUB2)				\
{												\
	boolean_t		was_null = FALSE, is_null = FALSE;				\
	mval			*subsc_ptr;							\
	DCL_THREADGBL_ACCESS;									\
												\
	SETUP_THREADGBL_ACCESS;									\
	BUILD_HASHT_CURRKEY_NAME;								\
	subsc_ptr = &TRIG_VAL;									\
	STR2MVAL(TRIG_VAL, SUB1, LEN1);								\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);	\
	subsc_ptr = &SUB2;									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);	\
	TREF(gv_last_subsc_null) = is_null;							\
	TREF(gv_some_subsc_null) = was_null;							\
}

#define BUILD_HASHT_SUB_SUB_CURRKEY_T(TRIG_VAL, SUB1, LEN1, SUB2, LEN2)				\
{												\
	boolean_t		was_null = FALSE, is_null = FALSE;				\
	mval			*subsc_ptr;							\
	DCL_THREADGBL_ACCESS;									\
												\
	SETUP_THREADGBL_ACCESS;									\
	BUILD_HASHT_CURRKEY_NAME;								\
	subsc_ptr = &TRIG_VAL;									\
	STR2MVAL(TRIG_VAL, SUB1, LEN1);								\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);	\
	subsc_ptr = &TRIG_VAL;									\
	STR2MVAL(TRIG_VAL, SUB2, LEN2);								\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);	\
	TREF(gv_last_subsc_null) = is_null;							\
	TREF(gv_some_subsc_null) = was_null;							\
}

#define	BUILD_HASHT_SUB_MSUB_MSUB_CURRKEY_T(TRIG_VAL, SUB1, LEN1, SUB2, SUB3) 			\
{												\
	boolean_t		was_null = FALSE, is_null = FALSE;				\
	mval			*subsc_ptr;							\
	DCL_THREADGBL_ACCESS;									\
												\
	SETUP_THREADGBL_ACCESS;									\
	BUILD_HASHT_CURRKEY_NAME;								\
	subsc_ptr = &TRIG_VAL;									\
	STR2MVAL(TRIG_VAL, SUB1, LEN1);								\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);	\
	subsc_ptr = &SUB2;									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);	\
	subsc_ptr = &SUB3;									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);	\
	TREF(gv_last_subsc_null) = is_null;							\
	TREF(gv_some_subsc_null) = was_null;							\
}

#define	BUILD_HASHT_SUB_SUB_MSUB_MSUB_CURRKEY_T(TRIG_VAL, SUB0, LEN0, SUB1, LEN1, SUB2, SUB3) 	\
{												\
	boolean_t		was_null = FALSE, is_null = FALSE;				\
	mval			*subsc_ptr;							\
	DCL_THREADGBL_ACCESS;									\
												\
	SETUP_THREADGBL_ACCESS;									\
	BUILD_HASHT_CURRKEY_NAME;								\
	subsc_ptr = &TRIG_VAL;									\
	STR2MVAL(TRIG_VAL, SUB0, LEN0);								\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);	\
	STR2MVAL(TRIG_VAL, SUB1, LEN1);								\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);	\
	subsc_ptr = &SUB2;									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);	\
	subsc_ptr = &SUB3;									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);	\
	TREF(gv_last_subsc_null) = is_null;							\
	TREF(gv_some_subsc_null) = was_null;							\
}

#define BUILD_HASHT_SUB_MSUB_SUB_CURRKEY_T(TRIG_VAL, SUB1, LEN1, SUB2, SUB3, LEN3)		\
{												\
	boolean_t		was_null = FALSE, is_null = FALSE;				\
	mval			*subsc_ptr;							\
	DCL_THREADGBL_ACCESS;									\
												\
	SETUP_THREADGBL_ACCESS;									\
	BUILD_HASHT_CURRKEY_NAME;								\
	subsc_ptr = &TRIG_VAL;									\
	STR2MVAL(TRIG_VAL, SUB1, LEN1);								\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);	\
	subsc_ptr = &SUB2;									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);	\
	subsc_ptr = &TRIG_VAL;									\
	STR2MVAL(TRIG_VAL, SUB3, LEN3);								\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);	\
	TREF(gv_last_subsc_null) = is_null;							\
	TREF(gv_some_subsc_null) = was_null;							\
}

#define BUILD_HASHT_SUB_SUB_MSUB_SUB_CURRKEY_T(TRIG_VAL, SUB0, LEN0, SUB1, LEN1, SUB2, SUB3, LEN3)	\
{													\
	boolean_t		was_null = FALSE, is_null = FALSE;					\
	mval			*subsc_ptr;								\
	DCL_THREADGBL_ACCESS;										\
													\
	SETUP_THREADGBL_ACCESS;										\
	BUILD_HASHT_CURRKEY_NAME;									\
	subsc_ptr = &TRIG_VAL;										\
	STR2MVAL(TRIG_VAL, SUB0, LEN0);									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);		\
	STR2MVAL(TRIG_VAL, SUB1, LEN1);									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);		\
	subsc_ptr = &SUB2;										\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);		\
	subsc_ptr = &TRIG_VAL;										\
	STR2MVAL(TRIG_VAL, SUB3, LEN3);									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);		\
	TREF(gv_last_subsc_null) = is_null;								\
	TREF(gv_some_subsc_null) = was_null;								\
}

#define	BUILD_HASHT_SUB_SUB_SUB_CURRKEY_T(TRIG_VAL, SUB1, LEN1, SUB2, LEN2, SUB3, LEN3) 	\
{												\
	boolean_t		was_null = FALSE, is_null = FALSE;				\
	mval			*subsc_ptr;							\
	DCL_THREADGBL_ACCESS;									\
												\
	SETUP_THREADGBL_ACCESS;									\
	BUILD_HASHT_CURRKEY_NAME;								\
	subsc_ptr = &TRIG_VAL;									\
	STR2MVAL(TRIG_VAL, SUB1, LEN1);								\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);	\
	subsc_ptr = &TRIG_VAL;									\
	STR2MVAL(TRIG_VAL, SUB2, LEN2);								\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);	\
	STR2MVAL(TRIG_VAL, SUB3, LEN3);								\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);	\
	TREF(gv_last_subsc_null) = is_null;							\
	TREF(gv_some_subsc_null) = was_null;							\
}

#define BUILD_HASHT_SUB_MSUB_SUB_MSUB_CURRKEY_T(TRIG_VAL, SUB1, LEN1, SUB2, SUB3, LEN3, SUB4)	\
{												\
	boolean_t		was_null = FALSE, is_null = FALSE;				\
	mval			*subsc_ptr;							\
	DCL_THREADGBL_ACCESS;									\
												\
	SETUP_THREADGBL_ACCESS;									\
	BUILD_HASHT_CURRKEY_NAME;								\
	subsc_ptr = &TRIG_VAL;									\
	STR2MVAL(TRIG_VAL, SUB1, LEN1);								\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);	\
	subsc_ptr = &SUB2;									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);	\
	subsc_ptr = &TRIG_VAL;									\
	STR2MVAL(TRIG_VAL, SUB3, LEN3);								\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);	\
	subsc_ptr = &SUB4;									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, gv_cur_region, gv_currkey, was_null, is_null);	\
	TREF(gv_last_subsc_null) = is_null;							\
	TREF(gv_some_subsc_null) = was_null;							\
}

#define	TRIGGER_GLOBAL_ASSIGNMENT_STR(TRIG_VAL, VALUE, LEN, RES)	\
{									\
	STR2MVAL(TRIG_VAL, VALUE, LEN);					\
	if (LEN > gv_cur_region->max_rec_size)				\
		RES = VAL_TOO_LONG;					\
	else if (gv_currkey->end + 1 > gv_cur_region->max_key_size)	\
		RES = KEY_TOO_LONG;					\
	else								\
	{								\
		gvcst_put(&TRIG_VAL);					\
		RES = PUT_SUCCESS;					\
	}								\
}

#define	TRIGGER_GLOBAL_ASSIGNMENT_MVAL(VALUE, RES)			\
{									\
	mval		*lcl_mv_ptr;					\
									\
	lcl_mv_ptr = &VALUE;						\
	MV_FORCE_STR(lcl_mv_ptr);					\
	if (lcl_mv_ptr->str.len > gv_cur_region->max_rec_size)		\
		RES = VAL_TOO_LONG;					\
	else if (gv_currkey->end + 1 > gv_cur_region->max_key_size)	\
		RES = KEY_TOO_LONG;					\
	else								\
	{								\
		gvcst_put(lcl_mv_ptr);					\
		RES = PUT_SUCCESS;					\
	}								\
}

#define BUILD_HASHT_SUB_CURRKEY(SUB, LEN)								\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_CURRKEY_T(trig_val, SUB, LEN);							\
}

#define	BUILD_HASHT_SUB_MSUB_CURRKEY(SUB1, LEN1, SUB2)							\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_MSUB_CURRKEY_T(trig_val, SUB1, LEN1, SUB2);					\
}

#define BUILD_HASHT_SUB_SUB_CURRKEY(SUB1, LEN1, SUB2, LEN2)						\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_SUB_CURRKEY_T(trig_val, SUB1, LEN1, SUB2, LEN2);				\
}

#define	BUILD_HASHT_SUB_MSUB_MSUB_CURRKEY(SUB1, LEN1, SUB2, SUB3)					\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_MSUB_MSUB_CURRKEY_T(trig_val, SUB1, LEN1, SUB2, SUB3);				\
}

#define BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(SUB1, LEN1, SUB2, SUB3, LEN3)					\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_MSUB_SUB_CURRKEY_T(trig_val, SUB1, LEN1, SUB2, SUB3, LEN3);			\
}

#define BUILD_HASHT_SUB_SUB_MSUB_SUB_CURRKEY(SUB0, LEN0, SUB1, LEN1, SUB2, SUB3, LEN3)			\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_SUB_MSUB_SUB_CURRKEY_T(trig_val, SUB0, LEN0, SUB1, LEN1, SUB2, SUB3, LEN3);	\
}

#define BUILD_HASHT_SUB_SUB_MSUB_MSUB_CURRKEY(SUB0, LEN0, SUB1, LEN1, SUB2, SUB3)			\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_SUB_MSUB_MSUB_CURRKEY_T(trig_val, SUB0, LEN0, SUB1, LEN1, SUB2, SUB3);		\
}

#define	BUILD_HASHT_SUB_SUB_SUB_CURRKEY(SUB1, LEN1, SUB2, LEN2, SUB3, LEN3)				\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_SUB_SUB_CURRKEY_T(trig_val, SUB1, LEN1, SUB2, LEN2, SUB3, LEN3);		\
}

#define BUILD_HASHT_SUB_MSUB_SUB_MSUB_CURRKEY(SUB1, LEN1, SUB2, SUB3, LEN3, SUB4)			\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_MSUB_SUB_MSUB_CURRKEY_T(trig_val, SUB1, LEN1, SUB2, SUB3, LEN3, SUB4);		\
}

#define	SET_TRIGGER_GLOBAL_SUB_STR(SUB1, LEN1, VALUE, LEN, RES)						\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_CURRKEY_T(trig_val, SUB, LEN);							\
	TRIGGER_GLOBAL_ASSIGNMENT_STR(trig_val, VALUE, LEN, RES);					\
}

#define	SET_TRIGGER_GLOBAL_SUB_SUB_STR(SUB1, LEN1, SUB2, LEN2, VALUE, LEN, RES)				\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_SUB_CURRKEY_T(trig_val, SUB1, LEN1, SUB2, LEN2);				\
	TRIGGER_GLOBAL_ASSIGNMENT_STR(trig_val, VALUE, LEN, RES);					\
}

#define	SET_TRIGGER_GLOBAL_SUB_MVAL(SUB1, LEN1, VALUE, RES)						\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_CURRKEY_T(trig_val, SUB1, LEN1);						\
	TRIGGER_GLOBAL_ASSIGNMENT_MVAL(VALUE, RES);							\
}

#define	SET_TRIGGER_GLOBAL_SUB_SUB_MVAL(SUB1, LEN1, SUB2, LEN2, VALUE, RES)				\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_SUB_CURRKEY_T(trig_val, SUB1, LEN1, SUB2, LEN2);				\
	TRIGGER_GLOBAL_ASSIGNMENT_MVAL(VALUE, RES);							\
}

#define	SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_STR(SUB1, LEN1, SUB2, SUB3, LEN3, VALUE, LEN, RES)		\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_MSUB_SUB_CURRKEY_T(trig_val, SUB1, LEN1, SUB2, SUB3, LEN3);			\
	TRIGGER_GLOBAL_ASSIGNMENT_STR(trig_val, VALUE, LEN, RES);					\
}

#define	SET_TRIGGER_GLOBAL_SUB_MSUB_MSUB_STR(SUB1, LEN1, SUB2, SUB3, VALUE, LEN, RES)			\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_MSUB_MSUB_CURRKEY_T(trig_val, SUB1, LEN1, SUB2, SUB3);				\
	TRIGGER_GLOBAL_ASSIGNMENT_STR(trig_val, VALUE, LEN, RES);					\
}

#define	SET_TRIGGER_GLOBAL_SUB_SUB_MSUB_MSUB_STR(SUB0, LEN0, SUB1, LEN1, SUB2, SUB3, VALUE, LEN, RES)	\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_SUB_MSUB_MSUB_CURRKEY_T(trig_val, SUB0, LEN0, SUB1, LEN1, SUB2, SUB3);		\
	TRIGGER_GLOBAL_ASSIGNMENT_STR(trig_val, VALUE, LEN, RES);					\
}

#define	SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_MVAL(SUB1, LEN1, SUB2, SUB3, LEN3, VALUE, RES)			\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_MSUB_SUB_CURRKEY_T(trig_val, SUB1, LEN1, SUB2, SUB3, LEN3);			\
	TRIGGER_GLOBAL_ASSIGNMENT_MVAL(VALUE, RES);							\
}

#define	SET_TRIGGER_GLOBAL_SUB_SUB_SUB_STR(SUB1, LEN1, SUB2, LEN2, SUB3, LEN3, VALUE, LEN, RES)		\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_SUB_SUB_CURRKEY_T(trig_val, SUB1, LEN1, SUB2, LEN2, SUB3, LEN3);		\
	TRIGGER_GLOBAL_ASSIGNMENT_STR(trig_val, VALUE, LEN, RES);					\
}

#define	SET_TRIGGER_GLOBAL_SUB_SUB_SUB_MVAL(SUB1, LEN1, SUB2, LEN2, SUB3, LEN3, VALUE, RES)		\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_SUB_SUB_CURRKEY_T(trig_val, SUB1, LEN1, SUB2, LEN2, SUB3, LEN3);		\
	TRIGGER_GLOBAL_ASSIGNMENT_MVAL(VALUE, RES);							\
}

#define	SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_MSUB_MVAL(SUB1, LEN1, SUB2, SUB3, LEN3, SUB4, VALUE, RES)	\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_MSUB_SUB_MSUB_CURRKEY_T(trig_val, SUB1, LEN1, SUB2, SUB3, LEN3, SUB4);		\
	TRIGGER_GLOBAL_ASSIGNMENT_MVAL(VALUE, RES);							\
}

#define	SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_MSUB_STR(SUB1, LEN1, SUB2, SUB3, LEN3, SUB4, VALUE, LEN, RES)	\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_MSUB_SUB_MSUB_CURRKEY_T(trig_val, SUB1, LEN1, SUB2, SUB3, LEN3, SUB4);		\
	TRIGGER_GLOBAL_ASSIGNMENT_STR(trig_val, VALUE, LEN, RES);					\
}

#define	INT2STR(INT, STR)			\
{						\
	char		*lcl_ptr;		\
	int		num_len;		\
						\
	lcl_ptr = STR;				\
	num_len = 0;				\
	I2A(lcl_ptr, num_len, INT);		\
	assert(MAX_DIGITS_IN_INT >= num_len);	\
	*(lcl_ptr + num_len) = '\0';		\
}

/* If this is the first call of this macro inside a function, then note down whatever is in the util_output buffer.
 * This is the prefix that is already printed for the first "util_out_print_gtmio" call. For the second and future
 * calls of the "util_out_print_gtmio" function inside the current function, print the noted down prefix first.
 * This gives more user-friendly output (e.g. deleting wildcard triggers by name prints same Line # in each trigger
 * that gets deleted).
 */
#define	UTIL_PRINT_PREFIX_IF_NEEDED(FIRST_GTMIO, UTILPREFIX, UTILPREFIXLEN)			\
{												\
	boolean_t	ret;									\
												\
	if (FIRST_GTMIO)									\
	{											\
		ret = util_out_save(UTILPREFIX, UTILPREFIXLEN);					\
		assert(ret);	/* assert we have space to save prefix in UTILPREFIX */		\
		/* If there was no space to save the prefix, print second and later calls of	\
		 * "util_out_print_gtmio" without the prefix in pro.				\
		 */										\
		if (ret)									\
			FIRST_GTMIO = FALSE;							\
	} else											\
		util_out_print_gtmio("!AD", NOFLUSH, *UTILPREFIXLEN, UTILPREFIX);		\
}

#define	SET_DISP_TRIGVN(REG, DISP_TRIGVN, DISP_TRIGVN_LEN, TRIGVN, TRIGVN_LEN)			\
{												\
	memcpy(DISP_TRIGVN, TRIGVN, TRIGVN_LEN);						\
	DISP_TRIGVN_LEN = TRIGVN_LEN;								\
	MEMCPY_LIT(&DISP_TRIGVN[DISP_TRIGVN_LEN], SPANREG_REGION_LIT);				\
	DISP_TRIGVN_LEN += SPANREG_REGION_LITLEN;						\
	memcpy(&DISP_TRIGVN[DISP_TRIGVN_LEN], REG->rname, REG->rname_len);			\
	DISP_TRIGVN_LEN += REG->rname_len;							\
	DISP_TRIGVN[DISP_TRIGVN_LEN++] = ')';							\
	assert(DISP_TRIGVN_LEN < ARRAYSIZE(DISP_TRIGVN));					\
	DISP_TRIGVN[DISP_TRIGVN_LEN] = '\0';	/* null terminate just in case */		\
}

#endif /* MUPIP_TRIGGER_INCLUDED */
