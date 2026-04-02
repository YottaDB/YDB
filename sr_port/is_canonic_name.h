/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Technically, MAX_LVSUBSCRIPTS and MAX_GVSUBSCRIPTS could be different, but we don't know
 * which one to use until we determine whether src is a global or local name.
 */
typedef union
{
	int lv[MAX_LVSUBSCRIPTS + 1 + 2]; /* 1 for the name, 2 for the environment components */
	int gv[MAX_GVSUBSCRIPTS + 1 + 2];
} gv_name_and_subscripts;

#define DETERMINE_BUFFER(SRC, START_BUFF, STOP_BUFF, START, STOP, RET)	\
MBSTART {								\
	if ((0 < (SRC)->str.len) && ('^' == (SRC)->str.addr[0]))	\
	{								\
		RET = FALSE;						\
		START = (START_BUFF).gv;				\
		STOP = (STOP_BUFF).gv;					\
	} else								\
	{								\
		RET = TRUE;						\
		START = (START_BUFF).lv;				\
		STOP = (STOP_BUFF).lv;					\
	}								\
} MBEND

#define NOCANONICNAME_ERROR(MVAL) 						\
MBSTART {									\
	int		error_len;						\
	unsigned char	*error_chr;						\
										\
	error_len = ZWR_EXP_RATIO((MVAL)->str.len);				\
	ENSURE_STP_FREE_SPACE(error_len);					\
	DBG_MARK_STRINGPOOL_UNEXPANDABLE;					\
	format2zwr((sm_uc_ptr_t)(MVAL)->str.addr, (MVAL)->str.len,		\
		   (uchar_ptr_t)stringpool.free, &error_len);			\
	DBG_MARK_STRINGPOOL_EXPANDABLE						\
	error_chr = stringpool.free;						\
	stringpool.free += error_len;						\
	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NOCANONICNAME, 2,		\
		      error_len, error_chr);					\
} MBEND

boolean_t is_canonic_name(mval *src, int *subscripts, int *start_off, int *stop_off);
boolean_t parse_glvn_and_subscripts(mval *src, int *subscripts, int *start, int *stop, int *contains_env, boolean_t no_mult_env);
