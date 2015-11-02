/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
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

GBLREF triple	*curr_fetch_trip;

error_def(ERR_CMD);
error_def(ERR_CNOTONSYS);
error_def(ERR_EXPR);
error_def(ERR_INVCMD);
error_def(ERR_PCONDEXPECTED);
error_def(ERR_SPOREOL);

int cmd(void)
{	/* All the commands are listed here. Two pairs of entries in general.
	 * One for full command and one for short-hand notation.
	 * For example, B and and BREAK.
	 */
LITDEF nametabent cmd_names[] =
	{
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
		,{3, "TRO"}, {8, "TROLLBAC*"}
		,{2, "TS"}, {6, "TSTART"}
		,{1, "U"}, {3, "USE"}
		,{1, "V"}, {4, "VIEW"}
		,{1, "W"}, {5, "WRITE"}
		,{1, "X"}, {6, "XECUTE"}
		,{2, "ZA"}, {8, "ZALLOCAT*"}
		,{3, "ZAT"}, {7, "ZATTACH"}
		,{2, "ZB"}, {6, "ZBREAK"}
		,{2, "ZC"}
		,{4, "ZCOM"}
		,{8, "ZCONTINU*"}
		,{8, "ZCOMPILE"}
		,{2, "ZD"}, {8, "ZDEALLOC*"}
		,{3, "ZED"}, {5, "ZEDIT"}
		,{2, "ZG"}, {5, "ZGOTO"}
		,{2, "ZH"}
		,{5, "ZHALT"}
		,{5, "ZHELP"}
		,{7, "ZINVCMD"}
		,{2, "ZK"}, {5, "ZKILL"}
		,{2, "ZL"}, {5, "ZLINK"}
		,{2, "ZM"}, {8, "ZMESSAGE"}
		,{2, "ZP"}, {6, "ZPRINT"}
		,{3, "ZSH"}, {5, "ZSHOW"}
		,{3, "ZST"}, {5, "ZSTEP"}
		,{3, "ZSY"}, {7, "ZSYSTEM"}
		,{3, "ZTC"}, {8, "ZTCOMMIT"}
#		ifdef GTM_TRIGGER
		,{3, "ZTR"}, {8, "ZTRIGGER"}
#		endif
		,{3, "ZTS"}, {7, "ZTSTART"}
		,{3, "ZWA"}, {6, "ZWATCH"}
		,{3, "ZWI*"}
		,{3, "ZWR"}, {6, "ZWRITE"}
	};
	/*
	 * cmd_index is an array indexed by the first alphabet of the command-name
	 * cmd_index[0] and cmd_index[1] elements are for a command beginning with 'A'.
	 * cmd_index[25] and cmd_index[26] element for a command beginning with 'Z'.
	 * The cmd_index[n] holds the index of the first element in cmd_names
	 * 	whose command-name begins with the same 'n'th letter of the alphabet (A is 1st letter).
         * Example:
	 * Say, [B]REAK is the command.
	 * 'B'-'A' = 1. and cmd_index[1] = 0 and cmd_index[1+1]=2. So it is in cmd_names[1] and cmd_names[2]
	 * Say, [C]LOSE is the command.
	 * 'C'-'A' = 2. and cmd_index[2] = 2 and cmd_index[2+1]=4. So it is in cmd_names[2] and cmd_names[4]
	 * Say, [D]O is the command.
	 * 'D'-'A' = 3. and cmd_index[3] = 4 and cmd_index[3+1]=6. So it is in cmd_names[4] and cmd_names[6]
	 * Say, [I]F is the command.
	 * 'I'-'A' = 8. and cmd_index[8] = 15 and cmd_index[8+1]=17. So it is in cmd_names[15] and cmd_names[17]
	 * Say, [M]ERGE the command.
	 * 'M'-'A' = 12. and cmd_index[12] = 23 and cmd_index[12+1]=25. So it is in cmd_names[23] and cmd_names[25]
	 */
LITDEF unsigned char cmd_index[27] =
	{
		0, 0, 2, 4, 6, 8, 10, 12, 15, 17, 19, 21, 23
		,25, 27, 29, 29, 31, 33, 35, 43, 45, 47, 49
		,51, 51, GTMTRIG_ONLY(96) NON_GTMTRIG_ONLY(94)
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
		,{m_zallocate, 0, 1, ALL_SYS}, {m_zallocate, 0, 1, ALL_SYS}
		,{m_zattach, 1, 1, ALL_SYS}, {m_zattach, 1, 1, ALL_SYS}
		,{m_zbreak, 0, 1, ALL_SYS}, {m_zbreak, 0, 1, ALL_SYS}
		,{m_zcontinue, 1, 1, ALL_SYS}
		,{m_zcompile, 0, 1, ALL_SYS}
		,{m_zcontinue, 1, 1, ALL_SYS}
		,{m_zcompile, 0, 1, ALL_SYS}
		,{m_zdeallocate, 1, 1, ALL_SYS}, {m_zdeallocate, 1, 1, ALL_SYS}
		,{m_zedit, 1, 1, ALL_SYS}, {m_zedit, 1, 1, ALL_SYS}
		,{m_zgoto, 1, 1, ALL_SYS}, {m_zgoto, 1, 1, ALL_SYS}
		,{m_zhelp, 1, 1, ALL_SYS}
		,{m_zhalt, 1, 1, ALL_SYS}
		,{m_zhelp, 1, 1, ALL_SYS}
		,{m_zinvcmd, 1, 1, ALL_SYS}
		,{m_zwithdraw, 0, 1, ALL_SYS}, {m_zwithdraw, 0, 1, ALL_SYS}
		,{m_zlink, 1, 1, ALL_SYS}, {m_zlink, 1, 1, ALL_SYS}
		,{m_zmessage, 0, 1, ALL_SYS}, {m_zmessage, 0, 1, ALL_SYS}
		,{m_zprint, 1, 1, ALL_SYS}, {m_zprint, 1, 1, ALL_SYS}
		,{m_zshow, 1, 1, ALL_SYS}, {m_zshow, 1, 1, ALL_SYS}
		,{m_zstep, 1, 1, ALL_SYS}, {m_zstep, 1, 1, ALL_SYS}
		,{m_zsystem, 1, 1, ALL_SYS}, {m_zsystem, 1, 1, ALL_SYS}
		,{m_ztcommit, 1, 1, ALL_SYS}, {m_ztcommit, 1, 1, ALL_SYS}
#		ifdef GTM_TRIGGER
		,{m_ztrigger, 0, 1, TRIGGER_OS}, {m_ztrigger, 0, 1, TRIGGER_OS}
#		endif
		,{m_ztstart, 1, 1, ALL_SYS}, {m_ztstart, 1, 1, ALL_SYS}
		,{m_zwatch, 0, 1, 0}, {m_zwatch, 0, 1, 0}
		,{m_zwithdraw, 0, 1, ALL_SYS}
		,{m_zwrite, 1, 1, ALL_SYS}, {m_zwrite, 1, 1, ALL_SYS}
	};

	triple		*temp_expr_start, *ref0, *ref1, *fetch0, *triptr;
	char		*c;
	int		rval, x;
	oprtype		*cr;
	boolean_t	shifting;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert((SIZEOF(cmd_names) / SIZEOF(nametabent)) == cmd_index[26]);
	while (TREF(expr_depth))
		DECREMENT_EXPR_DEPTH;				/* in case of prior errors */
	(TREF(side_effect_base))[0] = FALSE;
	TREF(temp_subs) = FALSE;
	CHKTCHAIN(TREF(curtchain));
	TREF(pos_in_chain) = *TREF(curtchain);
	if (TREF(window_token) != TK_IDENT)
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
	{
		if ((TK_COLON != TREF(director_token)) || (0 > (x = namelook(cmd_index, cmd_names, "ZINVCMD", 7))))
		{	/* the 2nd term of the above if should perform the assignment, but never be true - we're just paranoid */
			stx_error(MAKE_MSG_TYPE(ERR_INVCMD, ERROR));	/* force INVCMD to an error so stx_error sees it as hard */
			return FALSE;
		}
		stx_error(ERR_INVCMD);				/* the warning form so stx_error treats it as provisional */
	}
	if (!VALID_CMD(x) )
	{
	    	stx_error(ERR_CNOTONSYS);
		return FALSE;
	}
	advancewindow();
	if ((TK_COLON != TREF(window_token)) || !cmd_data[x].pcnd_ok)
	{
		assert((m_zinvcmd != cmd_data[x].fcn));
		cr = NULL;
		shifting = FALSE;
	} else
	{
		advancewindow();
		cr = (oprtype *)mcalloc(SIZEOF(oprtype));
		if (!bool_expr(FALSE, cr))
		{
			stx_error(ERR_PCONDEXPECTED);
			return FALSE;
		}
		if (shifting = ((TREF(expr_start) != TREF(expr_start_orig)) && (OC_NOOP != (TREF(expr_start))->opcode)))
		{	/* NOTE - assignent above */
			temp_expr_start = TREF(expr_start);
			triptr = newtriple(OC_GVRECTARG);
			triptr->operand[0] = put_tref(temp_expr_start);
		}
	}
	if (TK_SPACE == TREF(window_token))
		advancewindow();
	else if ((TK_EOL != TREF(window_token)) || !cmd_data[x].eol_ok)
	{
		stx_error(ERR_SPOREOL);
		return FALSE;
	}
	fetch0 = curr_fetch_trip;
	for (;;)
	{
		if ((EXPR_FAIL == (rval = (*cmd_data[x].fcn)())) || (TK_COMMA != TREF(window_token)))	/* NOTE assignment */
			break;
		else
		{	advancewindow();
			if ((TK_SPACE == TREF(window_token)) || (TK_EOL == TREF(window_token)))
			{
				stx_error(ERR_EXPR);
				return FALSE;
			}
		}
	}
	if ((EXPR_FAIL != rval) && cr)
	{
		if (fetch0 != curr_fetch_trip)
		{
			assert(OC_FETCH == curr_fetch_trip->opcode);
			*cr = put_tjmp((TREF(curtchain))->exorder.bl);
		} else
		{
			if (shifting)
			{
				ref0 = newtriple(OC_JMP);
				ref1 = newtriple(OC_GVRECTARG);
				ref1->operand[0] = put_tref(temp_expr_start);
				*cr = put_tjmp(ref1);
				tnxtarg(&ref0->operand[0]);
			} else
				tnxtarg(cr);
		}
	}
	return rval;
}
