/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.	*
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
#include "opcode.h"
#include "toktyp.h"
#include "nametabtyp.h"
#include "io_params.h"
#include "zshow_params.h"
#include "advancewindow.h"
#include "int_namelook.h"
#include "cvtparm.h"
#include "deviceparameters.h"

error_def(ERR_DEVPARINAP);
error_def(ERR_DEVPARUNK);
error_def(ERR_DEVPARVALREQ);
error_def(ERR_RPARENMISSING);

LITREF unsigned char io_params_size[];
LITREF dev_ctl_struct dev_param_control[];
LITDEF nametabent dev_param_names[] =
{	/* Must be in alpha order and in sync with dev_param_index[], zshow_param_index[] and dev_param_data[] */
	 {4,"ALLO*"}				/* in zshow_param_index */
	,{2,"AP*"}	,{6,"APPEND"}
	,{2,"AT*"}	,{6,"ATTACH"}

	,{9,"BIGRECORD"}			/* dead VMS placeholder, used in the test system */
	,{2,"BL*"}	,{4,"BLOC*"}	,{9,"BLOCKSIZE"}

	,{4,"CANO*"}	,{9,"CANONICAL"}
	,{2,"CE*"}	,{7,"CENABLE"}
	,{3,"CHS*"}	,{5,"CHSET"}
	,{3,"CLE*"}	,{11,"CLEARSCREEN"}
	,{4,"COMM*"}	,{7,"COMMAND"}
	,{4,"CONN*"}	,{7,"CONNECT"}
	,{4,"CONT*"}				/*,{10,"CONTIGUOUS"}*/
	,{4,"CONV*"}	,{7,"CONVERT"}
	,{2,"CT*"}	,{4,"CTRA"}	,{5,"CTRAP"}	/*,{5,"CTRAP"}*/

	,{4,"DELE*"}    ,{6,"DELETE"}
	,{4,"DELI*"}	,{9,"DELIMITER"}
	,{4,"DEST*" }	,{7,"DESTROY"}
	,{3,"DET*"}				/*,{6, "DETACH}*/
	,{3,"DOW*"}				/*,{10,"DOWNSCROLL"}*/

	,{2,"EB*"}	,{6,"EBCDIC"}
	,{2,"EC*"}	,{4,"ECHO"}
	,{2,"ED*"}	,{4,"EDIT"}
	,{4,"EMPT*"}	,{7,"EMPTERM"}
	,{6,"ERASEL*"}	,{9,"ERASELINE*"}
	,{2,"ES*"}				/*,{6,"ESCAPE*"}*/
	,{3,"EXC*"}	,{4,"EXCE*"}		/*,{9,"EXCEPTION"}*/
	,{3,"EXT*"}	,{4,"EXTE*"}		/*,{9,"EXTENSION"}*/

	,{1,"F"}				/* legacy abreviation for FIELD */
	,{4,"FFLF"}
	,{3,"FIE*"}	,{3,"FIE*"}	,{5,"FIELD"}
	,{3,"FIF*"}	,{4,"FIFO"}
	,{3,"FIL*"}				/*,{6,"FILTER"}*/
	,{3,"FIX*"}	,{5,"FIXED"}
	,{3,"FLA*"}	,{4,"FLAG"}
	,{3,"FLU*"}	,{5,"FLUSH"}
	,{3,"FOL*"}	,{6,"FOLLOW"}

	,{1,"G"}	,{5,"GROUP"}

	,{3,"HOL*"}				/* dead VMS placeholder */
	,{3,"HOS*"}	,{8,"HOSTSYNC"}
	,{2,"HU*"}	,{9,"HUPENABLE"}

	,{6,"ICHSET"}
	,{4,"IKEY"}
	,{4,"INDE*"}	,{11,"INDEPENDENT"}
	,{8,"INREWIND"}
	,{6,"INSEEK"}
	,{2,"IN*"}	,{4,"INSE*"}		/*,{6,"INSERT"}*/
	,{3,"IOE*"}				/*,{7,"IOERROR*"}*/

	,{3,"KEY"}

	,{3,"LAB*"}				/* in zshow_param_index */
	,{2,"LE*"}	,{4,"LENG*"}		/*,{6,"LENGTH"}*/
	,{2,"LI*"}	,{6,"LISTEN"}
	,{4,"LOGF*"}				/* dead VMS placeholder */
	,{4,"LOGQ*"}				/* dead VMS placeholder */
	,{3,"LOW*"}				/* dead VMS placeholder */

	,{1,"M"}
	,{4,"MORE*"}	,{12,"MOREREADTIME"}

	,{3,"NEW*"}	,{10,"NEWVERSION"}
	,{4,"NOCA*"}	,{11,"NOCANONICAL"}
	,{4,"NOCE*"}	,{6,"NOCENA*"}		/*,{9,"NOCENABLE"}*/
	,{6,"NOCONV*"}	,{9,"NOCONVERT"}
	,{6,"NODELI*"}	,{11,"NODELIMITER"}
	,{6,"NODEST*"}	,{9,"NODESTROY"}
	,{4,"NOEB*"}	,{8,"NOEBCDIC"}
	,{4,"NOEC*"}	,{6,"NOECHO"}
	,{4,"NOED*"}	,{6,"NOEDIT"}
	,{6,"NOEMPT*"}	,{9,"NOEMPTERM"}
	,{4,"NOES*"}	,{6,"NOESCA*"}		/*,{8,"NOESCAPE"}*/
	,{6,"NOFFLF"}
	,{5,"NOFIL*"}	,{8,"NOFILTER"}
	,{5,"NOFIX*"}	,{7,"NOFIXED"}
	,{4,"NOFLA*"}	,{5,"NOFLAG"}
	,{5,"NOFOL*"}	,{8,"NOFOLLOW"}
	,{5,"NOHOS*"}	,{6,"NOHOST*"}		/*,{10,"NOHOSTSYNC"}*/
	,{4,"NOHU*"}	,{11,"NOHUPENABLE"}
	,{4,"NOIN*"}	,{6,"NOINSE*"}		/*,{8,"NOINSERT"}*/
	,{4,"NOPAG*"}	,{5,"NOPAGE"}
	,{6,"NOPAST*"}	,{9,"NOPASTHRU"}
	,{6,"NOREAD"}	,{10,"NOREADONLY"}
	,{7,"NOREADS*"}	,{10,"NOREADSYNC"}
	,{4,"NOSE*"}	,{12,"NOSEQUENTAIL"}
	,{4,"NOST*"}	,{8,"NOSTREAM*"}
	,{4,"NOTE"}
	,{5,"NOTER*"}	,{12,"NOTERMINATOR"}
	,{4,"NOTI*"}				/* dead VMS placeholder */
	,{5,"NOTRA*"}				/* dead VMS placeholder */
	,{5,"NOTRU*"}	,{10,"NOTRUNCATE"}
	,{4,"NOTT*"}	,{6,"NOTTSY*"}		,{8,"NOTTSYNC"}
	,{4,"NOTY*"}	,{6,"NOTYPE"}
	,{4,"NOWA*"}	,{6,"NOWAIT"}
	,{4,"NOWR"}	,{6,"NOWRAP"}
	,{7,"NOWRITE*"}
	,{2,"NU*"}	,{4,"NULL"}		/* suspect this is only for internal use */

	,{1,"O"}				/* legacy abreviation for OWNER */
	,{6,"OCHSET"}
	,{4,"OKEY"}
	,{7,"OPTIONS"}
	,{9,"OUTREWIND"}
	,{7,"OUTSEEK"}
	,{2,"OV*"}	,{9,"OVERWRITE"}
	,{2,"OW*"}				/*,{5,"OWNER"}*/

	,{2,"P1"}
	,{2,"P2"}
	,{2,"P3"}
	,{2,"P4"}
	,{2,"P5"}
	,{2,"P6"}
	,{2,"P7"}
	,{2,"P8"}
	,{3,"PAD"}
	,{3,"PAG*"}	,{4,"PAGE"}
	,{4,"PARS*"}	,{5,"PARSE"}
	,{4,"PAST*"}	,{7,"PASTHRU*"}
	,{3,"PRM*"}	,{6,"PRMMBX"}		/* in zshow_param_index */
	,{3,"PRO*"}	,{10,"PROTECTION"}

	,{3,"QUE*"}				/* dead VMS placeholder */

	,{4,"RCHK*"}				/* in zshow_param_index */
	,{4,"READ"}	,{8,"READONLY"}
	,{5,"READS*"}	,{8,"READSYNC"}
	,{3,"REC*"}	,{10,"RECORDSIZE"}
	,{3,"REN*"}	,{6,"RENAME"}
	,{3,"REP*"}	,{7,"REPLACE"}
	,{3,"REW*"}	,{6,"REWIND"}
	,{3,"RFA"}				/* dead VMS placeholder */
	,{3,"RFM"}				/* dead VMS placeholder */

	,{1,"S"}				/* legacy abreviation for WORLD */
	,{3,"SEE*"}	,{4,"SEEK"}
	,{3,"SEQ*"}	,{10,"SEQUENTIAL"}
	,{2,"SH"}	,{4,"SHAR*"}		/*{6,"SHARED"}*/
	,{4,"SHEL*"}	,{5,"SHELL"}
	,{2,"SO*"}	,{6,"SOCKET"}
	,{3,"SPA*"}				/* dead VMS placeholder */
	,{3,"SPO*"}				/* dead VMS placeholder */
	,{2,"ST"}	,{3,"STR*"}		/* legacy abreviations for STREAM*/
	,{4,"STDE*"}	,{6,"STDERR"}
	,{2,"SU*"}				/* dead VMS placeholder */
	,{6,"STREAM"}
	,{2,"SY*"}

	,{2,"TE*"}	,{4,"TERM*"}	,{10,"TERMINATOR"}
	,{3,"TIM*"}				/*,{7,"TIMEOUT"}*/
	,{3,"TRU*"}	,{8,"TRUNCATE"}
	,{4,"TTSY*"}	,{6,"TTSYNC"}		/* in zshow_param_index */
	,{4,"TYPE*"}	,{9,"TYPEAHEAD"}	/* in zshow_param_index */

	,{2,"UI*"}	,{3,"UIC"}
	,{2,"UP*"}	,{8,"UPSCROLL"}
	,{2,"UR*"}				/* dead VMS placeholder */
	,{2,"US*"}				/* dead VMS placeholder */

	,{2,"VA*"}				/*,{8,"VARIABLE"}*/

	,{1,"W"}				/* legacy abreviation for WORLD */
	,{2,"WA*"}	,{4,"WAIT"}		/* in zshow_param_index */
	,{2,"WC*"}	,{4,"WCHK"}		/* in zshow_param_index */
	,{2,"WI*"}	,{5,"WIDTH"}
	,{2,"WO*"}				/*,{5,"WORLD"}*/
	,{2,"WR"}	,{4,"WRAP"}
	,{5,"WRITE"}				/* legacy abreviation for WRITEONLY */
	,{7,"WRITELB"}				/* dead VMS placeholder */
	,{7,"WRITEOF"}				/* dead VMS placeholder */
	,{9,"WRITEONLY*"}
	,{7,"WRITETM"}				/* dead VMS placeholder */

	,{1,"X"}

	,{1,"Y"}

	,{2,"ZB*"}	,{7,"ZBFSIZE"}
	,{2,"ZD*"}	,{6,"ZDELAY"}
	,{3,"ZEX*"}	,{10,"ZEXCEPTION"}
	,{4,"ZFIL*"}	,{7,"ZFILTER"}
	,{3,"ZFF"}
	,{2,"ZI*"}	,{8,"ZIBFSIZE"}
	,{4,"ZLEN*"}	,{7,"ZLENGTH"}
	,{4,"ZLIS*"}	,{7,"ZLISTEN"}

	,{8,"ZNODELAY"} /* ZNO* have to be spelled out fully */
	,{9,"ZNOFILTER"}
	,{5,"ZNOFF"}
	,{7,"ZNOWRAP"}

	,{4,"ZWID*"}	,{6,"ZWIDTH"}
	,{4,"ZWRA*"}	,{5,"ZWRAP"}
};

/* Offset of letter in dev_param_names.  Adding e.g. 1 entry there will add 1 to every entry corresponding to subsequent letters */
LITDEF	uint4 dev_param_index[27] =
{
/*	A    B    C    D    E    F    G    H    I    J    K    L    M    N   */
	0,   5,   9,   27,  35,  50,  66,  68,  73,  82,  82,  83,  91,  94,

/*	O    P    Q    R    S    T    U    V    W    X    Y    Z    end	     */
	162, 171, 190, 191, 206, 226, 236, 242, 243, 258, 259, 260, 283
};

/* Offset of string within letter in dev_param_names */
/* maintained in conjunction with zshow_params.h   = offset in letter, letter */
/* Ie: a = 0 & z = 25, so (currently) BLOC is the third entry in the 'B' section of dev_param_names[] */
LITDEF zshow_index zshow_param_index[] =
{
/*	ALLO    BLOC     COMMAND CONV     CTRA     DELE    DEST    EBCDIC   EDIT     EMPTERM  EXCE     EXTE */
	{0,0},  {2,1},   {9,2},  {14,2},  {16,2},  {0,3},  {4,3},  {1,4},   {4,4},   {7,4},   {12,4},  {14,4},
/*	FIE     FIL     FIXED   FOLLOW   HOST    ICHSET  INDEPENDENT  INSE    LAB      LENG */
	{4,5},  {7,5},  {9,5},  {15,5},  {1,7},  {0,8},  {3,8},	      {7,8},  {0,11},  {2,11},
/*	NOCENE   NODEST    NOECHO    NOEDIT    NOEMPTERM NOESCA    NOFOLLOW  NOHOST     NOINSE */
	{4,13},  {10,13},  {15,13},  {17,13},  {19,13},  {21,13},  {30,13},  {32,13},  {36,13},
/*	NOPAST   NOREADS  NOTTSY    NOTYPE   NOWRAP */
	{39,13}, {42,13}, {57,13},  {59,13}, {64,13},
/*	OCHSET   PAD      PARSE     PAST      PRMMBX    RCHK     READ     READS    REC */
	{1,14},  {8,15},  {12,15},  {13,15},  {17,15},  {0,17},  {1,17},  {3,17},  {5,17},
/*      SHAR     SHELL    STDERR    STREAM    TERM      TTSY     TYPE     UIC */
	{6,18},  {8,18},  {16,18},  {18,18},  {1,19},  {7,19},  {8,19},  {1,20},
/*      WAIT     WCHK     WIDTH    WRITE */
        {2,22},  {4,22},  {6,22},  {10,22}
};

int deviceparameters(oprtype *c, char who_calls)
{
	oprtype 	x;
	oprtype 	cat_list[n_iops];
	int		cat_cnt;
	mval		tmpmval;
	triple		*ref, *parm;
	int		n;
	int		status;
	char		parstr[MAX_COMPILETIME_DEVPARLEN * 4];	/* Allocated space must be more than MAX_COMPILETIME_DEVPARLEN
								 * is the only need. We randomly choose 4x space.
								 */
	char		*parptr;
	boolean_t	is_parm_list;
	boolean_t	parse_warn;

	static readonly unsigned char dev_param_data[] =
	{							/* must be in sync with dev_param_names[] */
		iop_allocation
		,iop_append ,iop_append
		,iop_attach ,iop_attach

		,iop_bigrecord
		,iop_blocksize ,iop_blocksize ,iop_blocksize

		,iop_canonical ,iop_canonical
		,iop_cenable ,iop_cenable
		,iop_chset ,iop_chset
		,iop_clearscreen ,iop_clearscreen
		,iop_command ,iop_command
		,iop_connect ,iop_connect
		,iop_contiguous
		,iop_convert ,iop_convert
		,iop_ctrap ,iop_ctrap ,iop_ctrap

		,iop_delete ,iop_delete
		,iop_delimiter ,iop_delimiter
		,iop_destroy ,iop_destroy
		,iop_detach
		,iop_downscroll

		,iop_ebcdic ,iop_ebcdic
		,iop_echo ,iop_echo
		,iop_editing ,iop_editing
		,iop_empterm ,iop_empterm
		,iop_eraseline ,iop_eraseline
		,iop_escape
		,iop_exception ,iop_exception
		,iop_extension ,iop_extension

		,iop_field
		,iop_fflf
		,iop_field ,iop_field ,iop_field
		,iop_fifo, iop_fifo
		,iop_filter
		,iop_fixed ,iop_fixed
		,iop_flag ,iop_flag
		,iop_flush ,iop_flush
		,iop_follow ,iop_follow

		,iop_g_protection, iop_g_protection

		,iop_hold
		,iop_hostsync, iop_hostsync
		,iop_hupenable ,iop_hupenable

		,iop_ipchset
		,iop_input_key
		,iop_independent ,iop_independent
		,iop_inrewind
		,iop_inseek
		,iop_insert ,iop_insert
		,iop_ioerror

		,iop_key

		,iop_label
		,iop_length ,iop_length
		,iop_zlisten ,iop_zlisten			/* Replaces iop_listen. LISTEN is now aliased to ZLISTEN. */
		,iop_logfile
		,iop_logqueue
		,iop_lowercase

		,iop_m
		,iop_morereadtime ,iop_morereadtime

		,iop_newversion ,iop_newversion
		,iop_nocanonical ,iop_nocanonical
		,iop_nocenable ,iop_nocenable
		,iop_noconvert ,iop_noconvert
		,iop_nodelimiter ,iop_nodelimiter
		,iop_nodestroy ,iop_nodestroy
		,iop_noebcdic ,iop_noebcdic
		,iop_noecho ,iop_noecho
		,iop_noediting ,iop_noediting
		,iop_noempterm ,iop_noempterm
		,iop_noescape ,iop_noescape
		,iop_nofflf
		,iop_nofilter ,iop_nofilter
		,iop_nofixed ,iop_nofixed
		,iop_noflag ,iop_noflag
		,iop_nofollow ,iop_nofollow
		,iop_nohostsync ,iop_nohostsync
		,iop_nohupenable ,iop_nohupenable
		,iop_noinsert ,iop_noinsert
		,iop_page ,iop_page
		,iop_nopasthru ,iop_nopasthru
		,iop_noreadonly ,iop_noreadonly
		,iop_noreadsync ,iop_noreadsync
		,iop_nosequential ,iop_nosequential
		,iop_nostream ,iop_nostream
		,iop_note
		,iop_noterminator, iop_noterminator
		,iop_notify
		,iop_notrailer
		,iop_notruncate ,iop_notruncate
		,iop_nottsync ,iop_nottsync ,iop_nottsync
		,iop_notypeahead ,iop_notypeahead
		,iop_nowait ,iop_nowait
		,iop_nowrap ,iop_nowrap
		,iop_nowriteonly
		,iop_nl ,iop_nl

		,iop_o_protection
		,iop_opchset
		,iop_output_key
		,iop_options
		,iop_outrewind
		,iop_outseek
		,iop_noinsert ,iop_noinsert
		,iop_o_protection

		,iop_p1
		,iop_p2
		,iop_p3
		,iop_p4
		,iop_p5
		,iop_p6
		,iop_p7
		,iop_p8
		,iop_pad
		,iop_page ,iop_page
		,iop_parse ,iop_parse
		,iop_pasthru ,iop_pasthru
		,iop_prmmbx ,iop_prmmbx
		,iop_o_protection ,iop_o_protection

		,iop_queue

		,iop_rdcheckdata
		,iop_readonly ,iop_readonly
		,iop_readsync ,iop_readsync
		,iop_recordsize ,iop_recordsize
		,iop_rename ,iop_rename
		,iop_replace ,iop_replace
		,iop_rewind ,iop_rewind
		,iop_rfa
		,iop_rfm

		,iop_s_protection
		,iop_seek ,iop_seek
		,iop_sequential ,iop_sequential
		,iop_shared ,iop_shared
		,iop_shell ,iop_shell
		,iop_socket ,iop_socket
		,iop_space
		,iop_spool
		,iop_stream, iop_stream
		,iop_stderr, iop_stderr
		,iop_stream
		,iop_submit
		,iop_s_protection

		,iop_terminator, iop_terminator, iop_terminator
		,iop_timeout
		,iop_truncate ,iop_truncate
		,iop_ttsync ,iop_ttsync
		,iop_typeahead ,iop_typeahead

		,iop_uic ,iop_uic
		,iop_upscroll ,iop_upscroll
	        ,iop_urgent
		,iop_user

		,iop_nofixed

		,iop_w_protection
		,iop_wait ,iop_wait
		,iop_wtcheckdata ,iop_wtcheckdata
		,iop_width ,iop_width
		,iop_w_protection
		,iop_wrap ,iop_wrap
		,iop_writeonly
		,iop_writelb
		,iop_writeof
		,iop_writeonly
		,iop_writetm

		,iop_x

		,iop_y

		,iop_zbfsize, iop_zbfsize
		,iop_zdelay, iop_zdelay
		,iop_exception, iop_exception	/* for ZEXCEPTION which is a synonym for EXCEPTION */
		,iop_filter, iop_filter		/* for ZFILTER which is a synonym for FILTER */
		,iop_zff
		,iop_zibfsize, iop_zibfsize
		,iop_length, iop_length		/* for ZLENGTH which is a synonym for LENGTH */
		,iop_zlisten, iop_zlisten

		,iop_znodelay
		,iop_nofilter			/* for ZNOFILTER which is a synonym for NOFILTER */
		,iop_znoff
		,iop_nowrap			/* for ZNOWRAP which is a synonym for NOWRAP */

		,iop_width, iop_width		/* for ZWIDTH which is a synonym for WIDTH */
		,iop_wrap, iop_wrap		/* for ZWRAP which is a synonym for WRAP */
	} ;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;

	assert((SIZEOF(dev_param_names) / SIZEOF(nametabent) == dev_param_index[26]));
	assert((SIZEOF(dev_param_data) / SIZEOF(unsigned char)) == dev_param_index[26]);
	is_parm_list = (TK_LPAREN == TREF(window_token));
	if (is_parm_list)
		advancewindow();
	cat_cnt = 0;
	parptr = parstr;
	parse_warn = FALSE;
	for (;;)
	{
		if (TK_IDENT != TREF(window_token))
		{
			stx_error(ERR_DEVPARPARSE);
			return FALSE;
		}
		n = int_namelook(dev_param_index, dev_param_names, (TREF(window_ident)).addr, (TREF(window_ident)).len);
		if (0 > n)
		{
			STX_ERROR_WARN(ERR_DEVPARUNK);	/* sets "parse_warn" to TRUE */
			break;
		}
		n = dev_param_data[n];
		if (!(dev_param_control[n].valid_with & who_calls))
		{
			STX_ERROR_WARN(ERR_DEVPARINAP);	/* sets "parse_warn" to TRUE */
			break;
		}
		advancewindow();
		*parptr++ = n;
		if (io_params_size[n])
		{
			if (TK_EQUAL != TREF(window_token))
			{
				STX_ERROR_WARN(ERR_DEVPARVALREQ);	/* sets "parse_warn" to TRUE */
				break;
			}
			advancewindow();
			if (EXPR_FAIL == expr(&x, MUMPS_EXPR))
				return FALSE;
			assert(TRIP_REF == x.oprclass);
			if (OC_LIT == x.oprval.tref->opcode)
			{
				/* Check to see if this string could overflow (5 is a int4 word plus a parameter code for
				 * safety)  Must check before cvtparm, due to the fact that tmpmval could otherwise
				 * be garbage collected by a later putstr
				 */
				assert(SIZEOF(parstr) > MAX_COMPILETIME_DEVPARLEN);
				if (parptr - parstr + x.oprval.tref->operand[0].oprval.mlit->v.str.len + 5 > SIZEOF(parstr))
				{
					cat_list[cat_cnt++] = put_str(parstr, INTCAST(parptr - parstr));
					parptr = parstr;
				}
				assert(MLIT_REF == x.oprval.tref->operand[0].oprclass);
				status = cvtparm(n, &x.oprval.tref->operand[0].oprval.mlit->v, &tmpmval);
				if (status)
				{
					stx_error(status);
					return FALSE;
				} else if ((MAX_COMPILETIME_DEVPARLEN + IOP_VAR_SIZE_4BYTE_LEN) < tmpmval.str.len)
				{	/* We have one string literal that is longer than MAX_COMPILETIME_DEVPARLEN, the maximum
					 * allowed length for string literals seen at compile time. Issue error.
					 * Note: We support a much higher limit (MAX_RUNTIME_DEVPARLEN) for dynamic strings
					 * (e.g. device parameters that point to say local variable names instead of literals).
					 * See IOP_VAR_SIZE_4BYTE for such device parameters.
					 */
					assert(IOP_VAR_SIZE_4BYTE == io_params_size[n]);
					stx_error(ERR_DEVPARTOOBIG);
					return FALSE;
				}
				memcpy(parptr, tmpmval.str.addr, tmpmval.str.len);
				parptr += tmpmval.str.len;
			} else
			{
				if (parptr > parstr)
				{
					cat_list[cat_cnt++] = put_str(parstr, INTCAST(parptr - parstr));
					parptr = parstr;
				}
				ref = newtriple(OC_CVTPARM);
				ref->operand[0] = put_ilit(n);
				ref->operand[1] = x;
				cat_list[cat_cnt++] = put_tref(ref);
			}
		}
		if (!is_parm_list)
			break;
		if (TK_COLON == TREF(window_token))
		{
			advancewindow();
			continue;
		}
		else if (TK_RPAREN == TREF(window_token))
		{
			advancewindow();
			break;
		}
		stx_error(ERR_RPARENMISSING);
		return FALSE;
	}
	if (parse_warn)
	{	/* Parse the remaining arguments until the corresponding RIGHT-PAREN or SPACE or EOL is reached */
		if (!parse_until_rparen_or_space())
		{
			stx_error(ERR_DEVPARPARSE);
			return FALSE;
		}
		if (TK_RPAREN == TREF(window_token))
			advancewindow();
	}
	*parptr++ = iop_eol;
	cat_list[cat_cnt++] = put_str(parstr,INTCAST(parptr - parstr));
	if (cat_cnt <= 1)
		*c = cat_list[0];
	else
	{
		ref = newtriple(OC_CAT);
		ref->operand[0] = put_ilit(cat_cnt + 1);
		*c = put_tref(ref);
		for (n = 0 ; n < cat_cnt ; n++)
		{
			parm = newtriple(OC_PARAMETER);
			ref->operand[1] = put_tref(parm);
			ref = parm;
			ref->operand[0] = cat_list[n];
		}
	}
	return TRUE;
}
