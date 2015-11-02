/****************************************************************
 *								*
 *	Copyright 2010, 2011 Fidelity Information Services, Inc	*
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
#define TRIGGER_SUBDEF(SUBNAME) SUBNAME##_SUB
typedef enum
{
#include "trigger_subs_def.h"
#undef TRIGGER_SUBDEF
	,NUM_TOTAL_SUBS
} trig_subs_t;
#define NUM_SUBS		NUM_TOTAL_SUBS - 2	/* Number of subscripts users deal with - hash values not included */

typedef enum
{
	STATS_ADDED = 0,
	STATS_DELETED,
	STATS_UNCHANGED,
	STATS_MODIFIED,
	STATS_ERROR,
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

#define LITERAL_BHASH		"BHASH"
#define LITERAL_LHASH		"LHASH"
#define LITERAL_MAXHASHVAL	"$"				/* '$' collates between '#' and '%' */
#define LITERAL_HASHSEQNUM	"#SEQNUM"
#define	LITERAL_HASHTNAME	"#TNAME"
#define	LITERAL_HASHTNCOUNT	"#TNCOUNT"
#define	LITERAL_HASHTRHASH	"#TRHASH"

#define LITERAL_BHASH_LEN	STR_LIT_LEN(LITERAL_BHASH)
#define LITERAL_LHASH_LEN	STR_LIT_LEN(LITERAL_LHASH)

#define	INITIAL_CYCLE		"1"

#define XTENDED_START		"<<"
#define XTENDED_STOP		">>"
#define XTENDED_START_LEN	STR_LIT_LEN(XTENDED_START)
#define XTENDED_STOP_LEN	STR_LIT_LEN(XTENDED_STOP)

#define	CMDS_PRESENT		0
#define	OPTIONS_PRESENT		0
#define	OPTION_CONFLICT		0
#define	NO_NAME_CHANGE		0
#define	PUT_SUCCESS		0
#define	SEQ_SUCCESS		0
#define	ADD_NEW_TRIGGER		-1
#define	INVALID_LABEL		-2
#define	K_ZTK_CONFLICT		-3
#define	VAL_TOO_LONG		-4
#define	KEY_TOO_LONG		-5
#define	TOO_MANY_TRIGGERS	-6

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

#define BUILD_HASHT_SUB_CURRKEY_T(TRIG_VAL, SUB, LEN)							\
{													\
	short int		max_key;								\
	boolean_t		was_null = FALSE, is_null = FALSE;					\
	mval			*subsc_ptr;								\
	DCL_THREADGBL_ACCESS;										\
													\
	SETUP_THREADGBL_ACCESS;										\
	max_key = gv_cur_region->max_key_size;								\
	BUILD_HASHT_CURRKEY_NAME;									\
	subsc_ptr = &TRIG_VAL;										\
	STR2MVAL(TRIG_VAL, SUB, LEN);									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);			\
	TREF(gv_last_subsc_null) = is_null;								\
	TREF(gv_some_subsc_null) = was_null;								\
}

#define	BUILD_HASHT_SUB_MSUB_CURRKEY_T(TRIG_VAL, SUB1, LEN1, SUB2)					\
{													\
	short int		max_key;								\
	boolean_t		was_null = FALSE, is_null = FALSE;					\
	mval			*subsc_ptr;								\
	DCL_THREADGBL_ACCESS;										\
													\
	SETUP_THREADGBL_ACCESS;										\
	max_key = gv_cur_region->max_key_size;								\
	BUILD_HASHT_CURRKEY_NAME;									\
	subsc_ptr = &TRIG_VAL;										\
	STR2MVAL(TRIG_VAL, SUB1, LEN1);									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);			\
	subsc_ptr = &SUB2;										\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);			\
	TREF(gv_last_subsc_null) = is_null;								\
	TREF(gv_some_subsc_null) = was_null;								\
}

#define BUILD_HASHT_SUB_SUB_CURRKEY_T(TRIG_VAL, SUB1, LEN1, SUB2, LEN2)					\
{													\
	short int		max_key;								\
	boolean_t		was_null = FALSE, is_null = FALSE;					\
	mval			*subsc_ptr;								\
	DCL_THREADGBL_ACCESS;										\
													\
	SETUP_THREADGBL_ACCESS;										\
	max_key = gv_cur_region->max_key_size;								\
	BUILD_HASHT_CURRKEY_NAME;									\
	subsc_ptr = &TRIG_VAL;										\
	STR2MVAL(TRIG_VAL, SUB1, LEN1);									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);			\
	subsc_ptr = &TRIG_VAL;										\
	STR2MVAL(TRIG_VAL, SUB2, LEN2);									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);			\
	TREF(gv_last_subsc_null) = is_null;								\
	TREF(gv_some_subsc_null) = was_null;								\
}

#define	BUILD_HASHT_SUB_MSUB_MSUB_CURRKEY_T(TRIG_VAL, SUB1, LEN1, SUB2, SUB3) 				\
{													\
	short int		max_key;								\
	boolean_t		was_null = FALSE, is_null = FALSE;					\
	mval			*subsc_ptr;								\
	DCL_THREADGBL_ACCESS;										\
													\
	SETUP_THREADGBL_ACCESS;										\
	max_key = gv_cur_region->max_key_size;								\
	BUILD_HASHT_CURRKEY_NAME;									\
	subsc_ptr = &TRIG_VAL;										\
	STR2MVAL(TRIG_VAL, SUB1, LEN1);									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);			\
	subsc_ptr = &SUB2;										\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);			\
	subsc_ptr = &SUB3;										\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);			\
	TREF(gv_last_subsc_null) = is_null;								\
	TREF(gv_some_subsc_null) = was_null;								\
}

#define BUILD_HASHT_SUB_MSUB_SUB_CURRKEY_T(TRIG_VAL, SUB1, LEN1, SUB2, SUB3, LEN3)			\
{													\
	short int		max_key;								\
	boolean_t		was_null = FALSE, is_null = FALSE;					\
	mval			*subsc_ptr;								\
	DCL_THREADGBL_ACCESS;										\
													\
	SETUP_THREADGBL_ACCESS;										\
	max_key = gv_cur_region->max_key_size;								\
	BUILD_HASHT_CURRKEY_NAME;									\
	subsc_ptr = &TRIG_VAL;										\
	STR2MVAL(TRIG_VAL, SUB1, LEN1);									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);			\
	subsc_ptr = &SUB2;										\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);			\
	subsc_ptr = &TRIG_VAL;										\
	STR2MVAL(TRIG_VAL, SUB3, LEN3);									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);			\
	TREF(gv_last_subsc_null) = is_null;								\
	TREF(gv_some_subsc_null) = was_null;								\
}

#define	BUILD_HASHT_SUB_SUB_SUB_CURRKEY_T(TRIG_VAL, SUB1, LEN1, SUB2, LEN2, SUB3, LEN3) 		\
{													\
	short int		max_key;								\
	boolean_t		was_null = FALSE, is_null = FALSE;					\
	mval			*subsc_ptr;								\
	DCL_THREADGBL_ACCESS;										\
													\
	SETUP_THREADGBL_ACCESS;										\
	max_key = gv_cur_region->max_key_size;								\
	BUILD_HASHT_CURRKEY_NAME;									\
	subsc_ptr = &TRIG_VAL;										\
	STR2MVAL(TRIG_VAL, SUB1, LEN1);									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);			\
	subsc_ptr = &TRIG_VAL;										\
	STR2MVAL(TRIG_VAL, SUB2, LEN2);									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);			\
	STR2MVAL(TRIG_VAL, SUB3, LEN3);									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);			\
	TREF(gv_last_subsc_null) = is_null;								\
	TREF(gv_some_subsc_null) = was_null;								\
}

#define BUILD_HASHT_SUB_MSUB_SUB_MSUB_CURRKEY_T(TRIG_VAL, SUB1, LEN1, SUB2, SUB3, LEN3, SUB4)		\
{													\
	short int		max_key;								\
	boolean_t		was_null = FALSE, is_null = FALSE;					\
	mval			*subsc_ptr;								\
	DCL_THREADGBL_ACCESS;										\
													\
	SETUP_THREADGBL_ACCESS;										\
	max_key = gv_cur_region->max_key_size;								\
	BUILD_HASHT_CURRKEY_NAME;									\
	subsc_ptr = &TRIG_VAL;										\
	STR2MVAL(TRIG_VAL, SUB1, LEN1);									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);			\
	subsc_ptr = &SUB2;										\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);			\
	subsc_ptr = &TRIG_VAL;										\
	STR2MVAL(TRIG_VAL, SUB3, LEN3);									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);			\
	subsc_ptr = &SUB4;										\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);			\
	TREF(gv_last_subsc_null) = is_null;								\
	TREF(gv_some_subsc_null) = was_null;								\
}

#define	TRIGGER_GLOBAL_ASSIGNMENT_STR(TRIG_VAL, VALUE, LEN, RES)					\
{													\
	STR2MVAL(trig_val, VALUE, LEN);									\
	if (gv_currkey->end + 1 + TRIG_VAL.str.len + SIZEOF(rec_hdr) > gv_cur_region->max_rec_size)	\
		RES = VAL_TOO_LONG;									\
	else if (gv_currkey->end + 1 > gv_cur_region->max_key_size)					\
		RES = KEY_TOO_LONG;									\
	else												\
	{												\
		gvcst_put(&TRIG_VAL);									\
		RES = PUT_SUCCESS;									\
	}												\
}

#define	TRIGGER_GLOBAL_ASSIGNMENT_MVAL(TRIG_VAL, VALUE, RES)						\
{													\
	mval		*lcl_mv_ptr;									\
													\
	lcl_mv_ptr = &VALUE;										\
	MV_FORCE_STR(lcl_mv_ptr);									\
	if (gv_currkey->end + 1 + TRIG_VAL.str.len + SIZEOF(rec_hdr) > gv_cur_region->max_rec_size)	\
		RES = VAL_TOO_LONG;									\
	else if (gv_currkey->end + 1 > gv_cur_region->max_key_size)					\
		RES = KEY_TOO_LONG;									\
	else												\
	{												\
		gvcst_put(lcl_mv_ptr);									\
		RES = PUT_SUCCESS;									\
	}												\
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

#define	SET_TRIGGER_GLOBAL_SUB_SUB_MVAL(SUB1, LEN1, SUB2, LEN2, VALUE, RES)				\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_SUB_CURRKEY_T(trig_val, SUB1, LEN1, SUB2, LEN2);				\
	TRIGGER_GLOBAL_ASSIGNMENT_MVAL(trig_val, VALUE, RES);						\
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
	BUILD_HASHT_SUB_MSUB_MSUB_CURRKEY_T(trig_val, SUB1, LEN1, SUB2, SUB3);			 	\
	TRIGGER_GLOBAL_ASSIGNMENT_STR(trig_val, VALUE, LEN, RES);					\
}

#define	SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_MVAL(SUB1, LEN1, SUB2, SUB3, LEN3, VALUE, RES)			\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_MSUB_SUB_CURRKEY_T(trig_val, SUB1, LEN1, SUB2, SUB3, LEN3);			\
	TRIGGER_GLOBAL_ASSIGNMENT_MVAL(trig_val, VALUE, RES);						\
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
	TRIGGER_GLOBAL_ASSIGNMENT_MVAL(trig_val, VALUE, RES);						\
}

#define	SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_MSUB_MVAL(SUB1, LEN1, SUB2, SUB3, LEN3, SUB4, VALUE, RES)	\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_MSUB_SUB_MSUB_CURRKEY_T(trig_val, SUB1, LEN1, SUB2, SUB3, LEN3, SUB4);		\
	TRIGGER_GLOBAL_ASSIGNMENT_MVAL(trig_val, VALUE, RES);						\
}

#define	SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_MSUB_STR(SUB1, LEN1, SUB2, SUB3, LEN3, SUB4, VALUE, LEN, RES)	\
{													\
	mval			trig_val;								\
													\
	BUILD_HASHT_SUB_MSUB_SUB_MSUB_CURRKEY_T(trig_val, SUB1, LEN1, SUB2, SUB3, LEN3, SUB4);		\
	TRIGGER_GLOBAL_ASSIGNMENT_STR(trig_val, VALUE, LEN, RES);					\
}

#define	INT2STR(INT, STR)									\
{												\
	char		*lcl_ptr;								\
	int		num_len;								\
												\
	lcl_ptr = STR;										\
	num_len = 0;										\
	I2A(lcl_ptr, num_len, INT);								\
	assert(MAX_DIGITS_IN_INT >= num_len);							\
	*(lcl_ptr + num_len) = '\0';								\
}

#define SAVE_TRIGGER_REGION_INFO							\
{											\
	save_gv_target = gv_target;							\
	save_gv_cur_region = gv_cur_region;						\
	save_sgm_info_ptr = sgm_info_ptr;						\
	assert(NULL != gv_currkey);							\
	assert((SIZEOF(gv_key) + gv_currkey->end) <= SIZEOF(save_currkey));		\
	save_gv_currkey = (gv_key *)&save_currkey[0];					\
	memcpy(save_gv_currkey, gv_currkey, SIZEOF(gv_key) + gv_currkey->end);		\
}
#define RESTORE_TRIGGER_REGION_INFO							\
{											\
	gv_target = save_gv_target;							\
	sgm_info_ptr = save_sgm_info_ptr;						\
	/* check no keysize expansion occurred inside gvcst_root_search */		\
	assert(gv_currkey->top == save_gv_currkey->top);				\
	memcpy(gv_currkey, save_gv_currkey, SIZEOF(gv_key) + save_gv_currkey->end);	\
	if (NULL != save_gv_cur_region)							\
	{										\
		TP_CHANGE_REG_IF_NEEDED(save_gv_cur_region);				\
	} else										\
	{										\
		gv_cur_region = NULL;							\
		cs_data = NULL;								\
		cs_addrs = NULL;							\
	}										\
}

#endif /* MUPIP_TRIGGER_INCLUDED */
