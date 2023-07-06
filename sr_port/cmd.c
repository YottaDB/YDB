/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"
#include "compiler.h"
#include "mdq.h"
#include "opcode.h"
#include "toktyp.h"
#include "nametabtyp.h"
#include "mmemory.h"
#include "advancewindow.h"
#include "cmd.h"
#include "namelook.h"
#include "error.h"

#define VMS_OS  01
#define UNIX_OS 02
#define ALL_SYS (VMS_OS | UNIX_OS)
#ifdef UNIX		/* command validation is a function of the OS */
#	 define VALID_CMD(i) (cmd_data[i].os_syst & UNIX_OS)
#	  ifdef __hppa
#	 	 define TRIGGER_OS 0
#	 	else
#	 define TRIGGER_OS UNIX_OS
#  endif
#elif defined VMS
#	 define VALID_CMD(i) (cmd_data[i].os_syst & VMS_OS)
#	 define TRIGGER_OS 0
#else
#	 error UNSUPPORTED PLATFORM
#endif

error_def(ERR_CMD);
error_def(ERR_CNOTONSYS);
error_def(ERR_EXPR);
error_def(ERR_INVCMD);
error_def(ERR_PCONDEXPECTED);
error_def(ERR_SPOREOL);

LITDEF nametabent cmd_names[] =
{	/* Must be in alpha order and in sync with cmd_index[], cmd_data[]; when inerting beware of legacy implications */
	{1, "B"}, {5, "BREAK"}
	,{1, "C"}, {5, "CLOSE"}
	,{1, "D"}, {2, "DO"}
	,{1, "E"}, {4, "ELSE"}
	,{1, "F"}, {3, "FOR"}
	,{1, "G"}, {4, "GOTO"}
	,{1, "H"}, {4, "HALT"}, {4, "HANG"}
	,{1, "I"}, {2, "IF"}
	,{1, "J"}, {3, "JOB"}
	,{1, "K"}, {4, "KILL"}
	,{1, "L"}, {4, "LOCK"}
	,{1, "M"}, {5, "MERGE"}
	,{1, "N"}, {3, "NEW"}
	,{1, "O"}, {4, "OPEN"}
	,{1, "Q"}, {4, "QUIT"}
	,{1, "R"}, {4, "READ"}
	,{1, "S"}, {3, "SET"}
	,{2, "TC"}, {7, "TCOMMIT"}
	,{3, "TRE"}, {8, "TRESTART"}
	,{3, "TRO"}, {9, "TROLLBACK"}
	,{2, "TS"}, {6, "TSTART"}
	,{1, "U"}, {3, "USE"}
	,{1, "V"}, {4, "VIEW"}
	,{1, "W"}, {5, "WRITE"}
	,{1, "X"}, {6, "XECUTE"}
	,{9, "ZALLOCATE"}
	,{7, "ZATTACH"}
	,{6, "ZBREAK"}
	,{2, "ZC"}	/* legacy abbreviation for ZCONTINUE */
	,{4, "ZCOM"}
	,{9, "ZCONTINUE"}
	,{8, "ZCOMPILE"}
	,{11, "ZDEALLOCATE"}
	,{5, "ZEDIT"}
	,{5, "ZGOTO"}
	,{2, "ZH"}	/* legacy abbreviation for ZHELP */
	,{5, "ZHALT"}
	,{5, "ZHELP"}
	,{7, "ZINVCMD"}
	,{5, "ZKILL"}
	,{5, "ZLINK"}
	,{8, "ZMESSAGE"}
	,{6, "ZPRINT"}
	#		ifdef AUTORELINK_SUPPORTED
	,{8, "ZRUPDATE"}
	#		endif
	,{5, "ZSHOW"}
	,{5, "ZSTEP"}
	,{7, "ZSYSTEM"}
	,{8, "ZTCOMMIT"}
	#		ifdef GTM_TRIGGER
	,{8, "ZTRIGGER"}
	#		endif
	,{7, "ZTSTART"}
	,{9, "ZWITHDRAW"}
	,{6, "ZWRITE"}
};
/*
 * cmd_index is an array indexed by the first alphabet of the command-name
 * cmd_index[0] is for commands beginning with 'A' ... cmd_index[26] element for commands beginning with 'Z'
 * The cmd_index[n] holds the index of the first element in cmd_names
 * 	whose command-name begins with the same 'n'th letter of the alphabet - A is 1st letter but in C terms get the 0 index.
 * if there are no cmds_starting with the letter the index does not change
 * 	e.g. with no cmd starting with 'A' cmd0] and cmd[1] both have values of 0
 * Example:
 * Say, [B]REAK is the command.
 * 'B'-'A' = 1. and cmd_index[1] = 0, and cmd_index[1+1]=2, so names for BREAK are within cmd_names[0] and cmd_names[1]
 * Say, [C]LOSE is the command.
 * 'C'-'A' = 2. and cmd_index[2] = 2 and cmd_index[2+1]=4, so names for CLOSE are within cmd_names[2] and cmd_names[3]
 * Say, [D]O is the command.
 * 'D'-'A' = 3. and cmd_index[3] = 4 and cmd_index[3+1]=6, so names for DO are within cmd_names[4] and cmd_names[5]
 * Say, [I]F is the command.
 * 'I'-'A' = 8. and cmd_index[8] = 15 and cmd_index[8+1]=17, so names for IF are within cmd_names[15] and cmd_names[16]
 * Say, [M]ERGE the command.
 * 'M'-'A' = 12. and cmd_index[12] = 23 and cmd_index[12+1]=25, so name for MERGE are with in cmd_names[23] and cmd_names[24]
 */
LITDEF unsigned char cmd_index[27] =
{
	 0,  0,  2,  4,  6,  8, 10, 12, 15,	/* a b c d e f g h i */
	17, 19, 21, 23 ,25, 27, 29, 29, 31,	/* j k l m n o p q r */
	33, 35, 43, 45, 47, 49 ,51, 51, 76	/* s t u v w x y z ~ */
	GTMTRIG_ONLY(+ 1) ARLINK_ONLY(+ 1)	/* add ztrigger and zrupdate, respectively */
};
LITDEF struct
{
	int (*fcn)();
	unsigned int eol_ok:1;
	unsigned int pcnd_ok:1;
	char         os_syst;
} cmd_data[] =
{
	{m_break, 1, 1, ALL_SYS}, {m_break, 1, 1, ALL_SYS}
	,{m_close, 0, 1, ALL_SYS}, {m_close, 0, 1, ALL_SYS}
	,{m_do, 1, 1, ALL_SYS}, {m_do, 1, 1, ALL_SYS}
	,{m_else, 1, 0, ALL_SYS}, {m_else, 1, 0, ALL_SYS}
	,{m_for, 0, 0, ALL_SYS}, {m_for, 0, 0, ALL_SYS}
	,{m_goto, 0, 1, ALL_SYS}, {m_goto, 0, 1, ALL_SYS}
	,{m_hcmd, 1, 1, ALL_SYS}
	,{m_halt, 1, 1, ALL_SYS}
	,{m_hang, 0, 1, ALL_SYS}
	,{m_if, 1, 0, ALL_SYS}, {m_if, 1, 0, ALL_SYS}
	,{m_job, 0, 1, ALL_SYS}, {m_job, 0, 1, ALL_SYS}
	,{m_kill, 1, 1, ALL_SYS}, {m_kill, 1, 1, ALL_SYS}
	,{m_lock, 1, 1, ALL_SYS}, {m_lock, 1, 1, ALL_SYS}
	,{m_merge, 0, 1, ALL_SYS}, {m_merge, 0, 1, ALL_SYS}
	,{m_new, 1, 1, ALL_SYS}, {m_new, 1, 1, ALL_SYS}
	,{m_open, 0, 1, ALL_SYS}, {m_open, 0, 1, ALL_SYS}
	,{m_quit, 1, 1, ALL_SYS}, {m_quit, 1, 1, ALL_SYS}
	,{m_read, 0, 1, ALL_SYS}, {m_read, 0, 1, ALL_SYS}
	,{m_set, 0, 1, ALL_SYS}, {m_set, 0, 1, ALL_SYS}
	,{m_tcommit, 1, 1, ALL_SYS}, {m_tcommit, 1, 1, ALL_SYS}
	,{m_trestart, 1, 1, ALL_SYS}, {m_trestart, 1, 1, ALL_SYS}
	,{m_trollback, 1, 1, ALL_SYS}, {m_trollback, 1, 1, ALL_SYS}
	,{m_tstart, 1, 1, ALL_SYS}, {m_tstart, 1, 1, ALL_SYS}
	,{m_use, 0, 1, ALL_SYS}, {m_use, 0, 1, ALL_SYS}
	,{m_view, 0, 1, ALL_SYS}, {m_view, 0, 1, ALL_SYS}
	,{m_write, 0, 1, ALL_SYS}, {m_write, 0, 1, ALL_SYS}
	,{m_xecute, 0, 1, ALL_SYS}, {m_xecute, 0, 1, ALL_SYS}
	,{m_zallocate, 0, 1, ALL_SYS}
	,{m_zattach, 1, 1, ALL_SYS}
	,{m_zbreak, 0, 1, ALL_SYS}
	,{m_zcontinue, 1, 1, ALL_SYS}
	,{m_zcompile, 0, 1, ALL_SYS}
	,{m_zcontinue, 1, 1, ALL_SYS}
	,{m_zcompile, 0, 1, ALL_SYS}
	,{m_zdeallocate, 1, 1, ALL_SYS}
	,{m_zedit, 1, 1, ALL_SYS}
	,{m_zgoto, 1, 1, ALL_SYS}
	,{m_zhelp, 1, 1, ALL_SYS}
	,{m_zhalt, 1, 1, ALL_SYS}
	,{m_zhelp, 1, 1, ALL_SYS}
	,{m_zinvcmd, 1, 1, ALL_SYS}
	,{m_zwithdraw, 0, 1, ALL_SYS}
	,{m_zlink, 1, 1, ALL_SYS}
	,{m_zmessage, 0, 1, ALL_SYS}
	,{m_zprint, 1, 1, ALL_SYS}
	#		ifdef AUTORELINK_SUPPORTED
	,{m_zrupdate, 0, 1, ALL_SYS}
	#		endif
	,{m_zshow, 1, 1, ALL_SYS}
	,{m_zstep, 1, 1, ALL_SYS}
	,{m_zsystem, 1, 1, ALL_SYS}
	,{m_ztcommit, 1, 1, ALL_SYS}
	#		ifdef GTM_TRIGGER
	,{m_ztrigger, 0, 1, TRIGGER_OS}
	#		endif
	,{m_ztstart, 1, 1, ALL_SYS}
	,{m_zwithdraw, 0, 1, ALL_SYS}
	,{m_zwrite, 1, 1, ALL_SYS}
};

int cmd(void)
{	/* module driving parsing of commands */
	/* All the commands are listed here. Two pairs of entries in general.
	 * One for full command and one for short-hand notation.
	 * For example, B and and BREAK.
	 */
	boolean_t	shifting;
	char		*c;
	int		rval, x;
	int4		fetch_cnt;
	oprtype		*cr;
<<<<<<< HEAD
	triple		*fetch0, *fetch1, *oldchain, *ref0, *ref1, *temp_expr_start, tmpchain, *triptr;
	triple		*boolexprfinish, *boolexprfinish2;
=======
	triple		*fetch0, *fetch1, *oldchain, *ref0, *ref1, *temp_expr_start = NULL, tmpchain, *triptr;
>>>>>>> 3c1c09f2 (GT.M V7.1-001)
	mval		*v;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(!TREF(discard));	/* make sure this global is not set from a previous cmd() call that had a parse error */
	assert((SIZEOF(cmd_names) / SIZEOF(nametabent)) == cmd_index[26]);
	while (TREF(expr_depth))
		DECREMENT_EXPR_DEPTH;				/* in case of prior errors */
	(TREF(side_effect_base))[0] = FALSE;
	TREF(temp_subs) = FALSE;
	CHKTCHAIN(TREF(curtchain), exorder, FALSE);
	TREF(pos_in_chain) = *TREF(curtchain);
	if (TK_IDENT != TREF(window_token))
	{
		stx_error(ERR_CMD);
		return FALSE;
	}
	assert(0 != (TREF(window_ident)).len);
	c = (TREF(window_ident)).addr;
	if ('%' == *c)
	{
		stx_error(ERR_CMD);
		return FALSE;
	}
	if (0 > (x = namelook(cmd_index, cmd_names, c, (TREF(window_ident)).len)))
	{	/* if there's a postconditional, use ZINVCMD to let us peal off any arguments and get to the rest of the line */
		if ((TK_COLON != TREF(director_token)) || (0 > (x = namelook(cmd_index, cmd_names, "ZINVCMD", 7))))
		{	/* the 2nd term of the above if should perform the assignment, but never be true - we're just paranoid */
			stx_error(MAKE_MSG_TYPE(ERR_INVCMD, ERROR));	/* force INVCMD to an error so stx_error sees it as hard */
			return FALSE;
		}
		stx_error(ERR_INVCMD);					/* use warning form so stx_error treats it as provisional */
	}
	if (!VALID_CMD(x))
	{
		stx_error(ERR_CNOTONSYS);
		return FALSE;
	}
	oldchain = NULL;
	advancewindow();
	fetch0 = (TREF(fetch_control)).curr_fetch_trip;
	fetch1 = (TREF(fetch_control)).curr_fetch_opr;
	if ((TK_COLON != TREF(window_token)) || !cmd_data[x].pcnd_ok)
	{
		//assert(m_zinvcmd != cmd_data[x].fcn);
		cr = NULL;
		shifting = FALSE;
		fetch_cnt = -1;
	} else
	{
		fetch_cnt = (TREF(fetch_control)).curr_fetch_count;
		advancewindow();
		cr = (oprtype *)mcalloc(SIZEOF(oprtype));
		if (!bool_expr(FALSE, cr, &boolexprfinish))
		{
			stx_error(ERR_PCONDEXPECTED);
			return FALSE;
		}
		/* the next block could be simpler if done earlier, but doing it here picks up any Boolean optimizations  */
		triptr = (TREF(curtchain))->exorder.bl;
		if (boolexprfinish == triptr)
			triptr = triptr->exorder.bl;
		while (OC_NOOP == triptr->opcode)
			triptr = triptr->exorder.bl;
		if (OC_LIT == triptr->opcode)
		{
			v = &triptr->operand[0].oprval.mlit->v;
			if (0 == MV_FORCE_BOOL(v))
			{	/* it's FALSE, so no need for this parse - get ready to discard it */
				exorder_init(&tmpchain);
				oldchain = setcurtchain(&tmpchain);
				TREF(discard) = (NULL != oldchain);
			}
			unuse_literal(v);
			dqdel(triptr, exorder);				/* if it's TRUE, so just pretend it never appeared */
			/* Remove OC_BOOLEXPRSTART and OC_BOOLEXPRFINISH opcodes too */
			REMOVE_BOOLEXPRSTART_AND_FINISH(boolexprfinish);	/* Note: Will set "boolexprfinish" to NULL */
		}
		if (shifting = ((TREF(expr_start) != TREF(expr_start_orig)) && (OC_NOOP != (TREF(expr_start))->opcode)))
		{	/* NOTE - assignment above */
			temp_expr_start = TREF(expr_start);
			triptr = newtriple(OC_GVRECTARG);
			triptr->operand[0] = put_tref(temp_expr_start);
		}
	}
	if (TK_SPACE == TREF(window_token))
		advancewindow();
	else if ((TK_EOL != TREF(window_token)) || !cmd_data[x].eol_ok)
	{
		if (NULL != oldchain)
		{
			setcurtchain(oldchain);
			TREF(discard) = FALSE;
		}
		stx_error(ERR_SPOREOL);
		return FALSE;
	}
	for (;;)
	{
		if ((EXPR_FAIL == (rval = (*cmd_data[x].fcn)())) || (TK_COMMA != TREF(window_token)))	/* NOTE assignment */
			break;
		else
		{	advancewindow();
			if ((TK_SPACE == TREF(window_token)) || (TK_EOL == TREF(window_token)))
			{
				if (NULL != oldchain)
				{
					setcurtchain(oldchain);
					TREF(discard) = FALSE;
				}
				stx_error(ERR_EXPR);
				return FALSE;
			}
		}
	}
	if (NULL != oldchain)
	{	/* for a literal 0 postconditional, we just throw the command & args away and return happiness */
<<<<<<< HEAD
=======
		assert(0 <= fetch_cnt);
		(TREF(fetch_control)).curr_fetch_trip = fetch0;
		(TREF(fetch_control)).curr_fetch_opr = fetch1;
		(TREF(fetch_control)).curr_fetch_count = fetch_cnt;
>>>>>>> 3c1c09f2 (GT.M V7.1-001)
		setcurtchain(oldchain);
		if (fetch0 != (TREF(fetch_control)).curr_fetch_trip)
		{
			assert(OC_FETCH == (TREF(fetch_control)).curr_fetch_trip->opcode);
			ins_triple((TREF(fetch_control)).curr_fetch_trip);
		} else
		{
			(TREF(fetch_control)).curr_fetch_trip = fetch0;
			(TREF(fetch_control)).curr_fetch_opr = fetch1;
			(TREF(fetch_control)).curr_fetch_count = fetch_cnt;
		}
		TREF(discard) = FALSE;
		return TRUE;
	}
	if ((EXPR_FAIL != rval) && (NULL != cr))
	{
		if (fetch0 != (TREF(fetch_control)).curr_fetch_trip)
		{
			assert(OC_FETCH == (TREF(fetch_control)).curr_fetch_trip->opcode);
			if (NULL != boolexprfinish)
			{
				INSERT_BOOLEXPRFINISH_AFTER_JUMP(boolexprfinish, boolexprfinish2);
				dqdel(boolexprfinish2, exorder);
				dqins((TREF(fetch_control)).curr_fetch_trip->exorder.bl, exorder, boolexprfinish2);
				*cr = put_tjmp(boolexprfinish2);
			} else
			{
				*cr = put_tjmp((TREF(fetch_control)).curr_fetch_trip);
				boolexprfinish2 = NULL;
			}
		} else
		{
			if (shifting)
			{	/* the following appears to be a hack ensuring emit_code doesn't find any unmatched OC_GVRECTARG */
				assert(temp_expr_start);
				ref0 = newtriple(OC_JMP);
				ref1 = newtriple(OC_GVRECTARG);
				ref1->operand[0] = put_tref(temp_expr_start);
				if (NULL != boolexprfinish)
				{
					INSERT_BOOLEXPRFINISH_AFTER_JUMP(boolexprfinish, boolexprfinish2);
					dqdel(boolexprfinish2, exorder);
					dqins(ref0, exorder, boolexprfinish2);
					*cr = put_tjmp(boolexprfinish2);
				} else
				{
					*cr = put_tjmp(ref1);
					boolexprfinish2 = NULL;
				}
				tnxtarg(&ref0->operand[0]);
			} else if (NULL != boolexprfinish)
			{
				INSERT_BOOLEXPRFINISH_AFTER_JUMP(boolexprfinish, boolexprfinish2);
				*cr = put_tjmp(boolexprfinish2);
			} else
				boolexprfinish2 = NULL;
		}
		INSERT_OC_JMP_BEFORE_OC_BOOLEXPRFINISH(boolexprfinish2);
	}
	return rval;
}
