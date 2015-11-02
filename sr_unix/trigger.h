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

#ifndef MUPIP_TRIGGER_INCLUDED
#define MUPIP_TRIGGER_INCLUDED

typedef enum
{
	TRIGNAME_SUB = 0,
	GVSUBS_SUB,
	CMD_SUB,
	OPTIONS_SUB,
	DELIM_SUB,
	ZDELIM_SUB,
	PIECES_SUB,
	XECUTE_SUB,
	CHSET_SUB,
	LHASH_SUB,
	BHASH_SUB,
	NUM_TOTAL_SUBS
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
#define MAX_BUFF_SIZE		65536			/* Size of input and output buffers */
#define COMMENT_LITERAL		';'
#define TRIGNAME_SEQ_DELIM	'#'
#define MAX_GVSUBS_LEN		8192			/* Maximum length of the gvsubs string */
#define	MAX_HASH_INDEX_LEN	1024			/* Max length of value for hash index entries */

#define	NUM_TRIGNAME_SEQ_CHARS	6
#define	MAX_TRIGNAME_LEN	(MAX_MIDENT_LEN - 2)				/* 2 for runtime characters */
#define MAX_USER_TRIGNAME_LEN	MAX_MIDENT_LEN - 3	/* 3 -- 2 for run time chars and 1 for delimiter */
#define	MAX_AUTO_TRIGNAME_LEN	(MAX_MIDENT_LEN - 4 - NUM_TRIGNAME_SEQ_CHARS)	/* 4 -- 2 for runtime characters, 2 for delims */

#define LITERAL_BHASH		"BHASH"
#define LITERAL_LHASH		"LHASH"
#define LITERAL_MAXHASHVAL	"#ZZZZZZZ"
#define LITERAL_HASHSEQNUM	"#SEQNUM"
#define	LITERAL_HASHTNAME	"#TNAME"
#define	LITERAL_HASHTNCOUNT	"#TNCOUNT"
#define	LITERAL_HASHTRHASH	"#TRHASH"

#define	INITIAL_CYCLE		"1"

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

static char *trigger_subs[]	= {LITERAL_TRIGNAME, LITERAL_GVSUBS, LITERAL_CMD, LITERAL_OPTIONS, LITERAL_DELIM, LITERAL_ZDELIM,
				   LITERAL_PIECES, LITERAL_XECUTE, LITERAL_CHSET, LITERAL_LHASH, LITERAL_BHASH};

/* Build up a comma delimited string */
#define ADD_COMMA_IF_NEEDED(COUNT, PTR)						\
{										\
	if (0 != COUNT)								\
	{									\
		MEMCPY_LIT(PTR, ",");						\
		PTR++;								\
	}									\
}
#define ADD_STRING(COUNT, PTR, LEN, COMMAND)					\
{										\
	memcpy(PTR, COMMAND, LEN);						\
	PTR += LEN;								\
	COUNT++;								\
}

#define STR2MVAL(MVAL, STR, LEN)						\
{										\
	MVAL.mvtype = MV_STR;							\
	MVAL.str.addr = STR;							\
	MVAL.str.len = LEN;							\
}
#define BUILD_HASHT_CURRKEY_NAME						\
{										\
	memcpy(gv_currkey->base, HASHT_GBLNAME, HASHT_GBLNAME_LEN);		\
	gv_currkey->base[HASHT_GBLNAME_LEN] = '\0';				\
	gv_currkey->base[HASHT_GBLNAME_FULL_LEN] = '\0';			\
	gv_currkey->end = HASHT_GBLNAME_FULL_LEN;				\
	gv_currkey->prev = 0;							\
}
#define BUILD_HASHT_SUB_CURRKEY(SUB, LEN)						\
{											\
	short int		max_key;						\
	boolean_t		was_null, is_null;					\
	mval			trig_val, *subsc_ptr;					\
											\
	max_key = gv_cur_region->max_key_size;						\
	was_null = is_null = FALSE;							\
	BUILD_HASHT_CURRKEY_NAME;							\
	subsc_ptr = &trig_val;								\
	STR2MVAL(trig_val, SUB, LEN);							\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);	\
}
#define BUILD_HASHT_SUB_SUB_CURRKEY(SUB1, LEN1, SUB2, LEN2)				\
{											\
	short int		max_key;						\
	boolean_t		was_null, is_null;					\
	mval			trig_val, *subsc_ptr;					\
											\
	max_key = gv_cur_region->max_key_size;						\
	was_null = is_null = FALSE;							\
	BUILD_HASHT_CURRKEY_NAME;							\
	subsc_ptr = &trig_val;								\
	STR2MVAL(trig_val, SUB1, LEN1);							\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);	\
	subsc_ptr = &trig_val;								\
	STR2MVAL(trig_val, SUB2, LEN2);							\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);	\
}
#define BUILD_HASHT_SUB_MSUB_SUB_CURRKEY(SUB1, LEN1, SUB2, SUB3, LEN3)			\
{											\
	short int		max_key;						\
	boolean_t		was_null, is_null;					\
	mval			trig_val, *subsc_ptr;					\
											\
	max_key = gv_cur_region->max_key_size;						\
	was_null = is_null = FALSE;							\
	BUILD_HASHT_CURRKEY_NAME;							\
	subsc_ptr = &trig_val;								\
	STR2MVAL(trig_val, SUB1, LEN1);							\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);	\
	subsc_ptr = &SUB2;								\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);	\
	subsc_ptr = &trig_val;								\
	STR2MVAL(trig_val, SUB3, LEN3);							\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);	\
}

#define	BUILD_HASHT_SUB_MSUB_MSUB_CURRKEY(SUB1, LEN1, SUB2, SUB3)			\
{											\
	short int		max_key;						\
	boolean_t		was_null, is_null;					\
	mval			trig_val, *subsc_ptr;					\
											\
	max_key = gv_cur_region->max_key_size;						\
	was_null = is_null = FALSE;							\
	BUILD_HASHT_CURRKEY_NAME;							\
	subsc_ptr = &trig_val;								\
	STR2MVAL(trig_val, SUB1, LEN1);							\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);	\
	subsc_ptr = &SUB2;								\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);	\
	subsc_ptr = &SUB3;								\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);	\
}

#define	BUILD_HASHT_SUB_SUB_SUB_CURRKEY(SUB1, LEN1, SUB2, LEN2, SUB3, LEN3)		\
{											\
	short int		max_key;						\
	boolean_t		was_null, is_null;					\
	mval			trig_val, *subsc_ptr;					\
											\
	max_key = gv_cur_region->max_key_size;						\
	was_null = is_null = FALSE;							\
	BUILD_HASHT_CURRKEY_NAME;							\
	subsc_ptr = &trig_val;								\
	STR2MVAL(trig_val, SUB1, LEN1);							\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);	\
	subsc_ptr = &trig_val;								\
	STR2MVAL(trig_val, SUB2, LEN2);							\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);	\
	STR2MVAL(trig_val, SUB3, LEN3);							\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);	\
}

#define	BUILD_HASHT_SUB_MSUB_CURRKEY(SUB1, LEN1, SUB2)					\
{											\
	short int		max_key;						\
	boolean_t		was_null, is_null;					\
	mval			trig_val, *subsc_ptr;					\
											\
	max_key = gv_cur_region->max_key_size;						\
	was_null = is_null = FALSE;							\
	BUILD_HASHT_CURRKEY_NAME;							\
	subsc_ptr = &trig_val;								\
	STR2MVAL(trig_val, SUB1, LEN1);							\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);	\
	subsc_ptr = &SUB2;								\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);	\
}

#define	TRIGGER_GLOBAL_ASSIGNMENT_STR(VALUE, LEN, RES)							\
{													\
	STR2MVAL(trig_val, VALUE, LEN);									\
	if (gv_currkey->end + 1 + trig_val.str.len + SIZEOF(rec_hdr) > gv_cur_region->max_rec_size)	\
		RES = VAL_TOO_LONG;									\
	else if (gv_currkey->end + 1 > gv_cur_region->max_key_size)					\
		RES = KEY_TOO_LONG;									\
	else												\
	{												\
		gvcst_put(&trig_val);									\
		RES = PUT_SUCCESS;									\
	}												\
}

#define	TRIGGER_GLOBAL_ASSIGNMENT_MVAL(VALUE, RES)							\
{													\
	mval		*lcl_mv_ptr;									\
													\
	lcl_mv_ptr = &VALUE;										\
	MV_FORCE_STR(lcl_mv_ptr);									\
	if (gv_currkey->end + 1 + trig_val.str.len + SIZEOF(rec_hdr) > gv_cur_region->max_rec_size)	\
		RES = VAL_TOO_LONG;									\
	else if (gv_currkey->end + 1 > gv_cur_region->max_key_size)					\
		RES = KEY_TOO_LONG;									\
	else												\
	{												\
		gvcst_put(lcl_mv_ptr);									\
		RES = PUT_SUCCESS;									\
	}												\
}

#define	SET_TRIGGER_GLOBAL_SUB_STR(SUB1, LEN1, VALUE, LEN, RES)				\
{											\
	short int		max_key;						\
	boolean_t		was_null, is_null;					\
	mval			trig_val, *subsc_ptr;					\
											\
	max_key = gv_cur_region->max_key_size;						\
	was_null = is_null = FALSE;							\
	BUILD_HASHT_CURRKEY_NAME;							\
	subsc_ptr = &trig_val;								\
	STR2MVAL(trig_val, SUB1, LEN1);							\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);	\
	TRIGGER_GLOBAL_ASSIGNMENT_STR(VALUE, LEN, RES);					\
}

#define	SET_TRIGGER_GLOBAL_SUB_SUB_STR(SUB1, LEN1, SUB2, LEN2, VALUE, LEN, RES)		\
{											\
	short int		max_key;						\
	boolean_t		was_null, is_null;					\
	mval			trig_val, *subsc_ptr;					\
											\
	max_key = gv_cur_region->max_key_size;						\
	was_null = is_null = FALSE;							\
	BUILD_HASHT_CURRKEY_NAME;							\
	subsc_ptr = &trig_val;								\
	STR2MVAL(trig_val, SUB1, LEN1);							\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);	\
	subsc_ptr = &trig_val;								\
	STR2MVAL(trig_val, SUB2, LEN2);							\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);	\
	TRIGGER_GLOBAL_ASSIGNMENT_STR(VALUE, LEN, RES);					\
}

#define	SET_TRIGGER_GLOBAL_SUB_SUB_MVAL(SUB1, LEN1, SUB2, LEN2, VALUE, RES)		\
{											\
	short int		max_key;						\
	boolean_t		was_null, is_null;					\
	mval			trig_val, *subsc_ptr;					\
											\
	max_key = gv_cur_region->max_key_size;						\
	was_null = is_null = FALSE;							\
	BUILD_HASHT_CURRKEY_NAME;							\
	subsc_ptr = &trig_val;								\
	STR2MVAL(trig_val, SUB1, LEN1);							\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);	\
	subsc_ptr = &trig_val;								\
	STR2MVAL(trig_val, SUB2, LEN2);							\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);	\
	TRIGGER_GLOBAL_ASSIGNMENT_MVAL(VALUE, RES);					\
}

#define	SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_STR(SUB1, LEN1, SUB2, SUB3, LEN3, VALUE, LEN, RES)	\
{												\
	short int		max_key;							\
	boolean_t		was_null, is_null;						\
	mval			trig_val, *subsc_ptr;						\
												\
	max_key = gv_cur_region->max_key_size;							\
	was_null = is_null = FALSE;								\
	BUILD_HASHT_CURRKEY_NAME;							\
	subsc_ptr = &trig_val;								\
	STR2MVAL(trig_val, SUB1, LEN1);							\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);	\
	subsc_ptr = &SUB2;									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);		\
	subsc_ptr = &trig_val;									\
	STR2MVAL(trig_val, SUB3, LEN3);								\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);		\
	TRIGGER_GLOBAL_ASSIGNMENT_STR(VALUE, LEN, RES);						\
}

#define	SET_TRIGGER_GLOBAL_SUB_MSUB_SUB_MVAL(SUB1, LEN1, SUB2, SUB3, LEN3, VALUE, RES)		\
{												\
	short int		max_key;							\
	boolean_t		was_null, is_null;						\
	mval			trig_val, *subsc_ptr;						\
												\
	max_key = gv_cur_region->max_key_size;							\
	was_null = is_null = FALSE;								\
	BUILD_HASHT_CURRKEY_NAME;							\
	subsc_ptr = &trig_val;								\
	STR2MVAL(trig_val, SUB1, LEN1);							\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);	\
	subsc_ptr = &SUB2;									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);		\
	subsc_ptr = &trig_val;									\
	STR2MVAL(trig_val, SUB3, LEN3);								\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);		\
	TRIGGER_GLOBAL_ASSIGNMENT_MVAL(VALUE, RES);						\
}

#define	SET_TRIGGER_GLOBAL_SUB_SUB_SUB_STR(SUB1, LEN1, SUB2, LEN2, SUB3, LEN3, VALUE, LEN, RES)	\
{												\
	short int		max_key;							\
	boolean_t		was_null, is_null;						\
	mval			trig_val, *subsc_ptr;						\
												\
	max_key = gv_cur_region->max_key_size;							\
	was_null = is_null = FALSE;								\
	BUILD_HASHT_CURRKEY_NAME;							\
	subsc_ptr = &trig_val;								\
	STR2MVAL(trig_val, SUB1, LEN1);							\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);	\
	subsc_ptr = &trig_val;									\
	STR2MVAL(trig_val, SUB2, LEN2);								\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);		\
	STR2MVAL(trig_val, SUB3, LEN3);								\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);		\
	TRIGGER_GLOBAL_ASSIGNMENT_STR(VALUE, LEN, RES);						\
}

#define	SET_TRIGGER_GLOBAL_SUB_SUB_SUB_MVAL(SUB1, LEN1, SUB2, LEN2, SUB3, LEN3, VALUE, RES)	\
{												\
	short int		max_key;							\
	boolean_t		was_null, is_null;						\
	mval			trig_val, *subsc_ptr;						\
												\
	max_key = gv_cur_region->max_key_size;							\
	was_null = is_null = FALSE;								\
	BUILD_HASHT_CURRKEY_NAME;							\
	subsc_ptr = &trig_val;								\
	STR2MVAL(trig_val, SUB1, LEN1);							\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);	\
	subsc_ptr = &trig_val;									\
	STR2MVAL(trig_val, SUB2, LEN2);								\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);		\
	STR2MVAL(trig_val, SUB3, LEN3);								\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);		\
	TRIGGER_GLOBAL_ASSIGNMENT_MVAL(VALUE, RES);						\
}

#define	SET_TRIGGER_GLOBAL_SUB_MSUB_MSUB_STR(SUB1, LEN1, SUB2, SUB3, VALUE, LEN, RES)		\
{												\
	short int		max_key;							\
	boolean_t		was_null, is_null;						\
	mval			trig_val, *subsc_ptr;						\
												\
	max_key = gv_cur_region->max_key_size;							\
	was_null = is_null = FALSE;								\
	BUILD_HASHT_CURRKEY_NAME;							\
	subsc_ptr = &trig_val;								\
	STR2MVAL(trig_val, SUB1, LEN1);							\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);	\
	subsc_ptr = &SUB2;									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);		\
	subsc_ptr = &SUB3;									\
	COPY_SUBS_TO_GVCURRKEY(subsc_ptr, max_key, gv_currkey, was_null, is_null);		\
	TRIGGER_GLOBAL_ASSIGNMENT_STR(VALUE, LEN, RES);						\
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
}

#endif /* MUPIP_TRIGGER_INCLUDED */
