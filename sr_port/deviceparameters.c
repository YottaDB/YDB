/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
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
#include "namelook.h"
#include "cvtparm.h"
#include "deviceparameters.h"

GBLREF char 	window_token;
GBLREF mident 	window_ident;

LITREF unsigned char io_params_size[];
LITREF dev_ctl_struct dev_param_control[];
LITDEF nametabent dev_param_names[] =
{
	 {2,"AF*"}
	,{2,"AL*"}	,{4,"ALLO"}
	,{2,"AP*"}
	,{2,"AT*"}

	,{4,"BIGR*"}
	,{2,"BL*"}	,{4,"BLOC"}
	,{4,"BU*"}

	,{2,"CA"}, {4,"CANT*"}
	,{4,"CANO*"}
	,{2,"CE*"}
	,{3,"CHA*"}
	,{3,"CHS*"}
	,{3,"CLE*"}
	,{3,"CLI"}
	,{4,"CONN*"}
	,{4,"CONT*"}
	,{4,"CONV*"}
	,{3,"COP*"}
	,{2,"CP*"}
	,{2,"CT*"}	,{4,"CTRA"}

	,{4,"DELE*"}    ,{4,"DELE"}
	,{4,"DELI*"}
	,{3,"DET*"}
	,{3,"DOU*"}
	,{3,"DOW*"}

	,{2,"EB*"}	,{4,"EBCD"}
	,{2,"EC*"}
	,{2,"ED*"}	,{4,"EDIT"}
	,{6,"ERASEL*"}
	,{6,"ERASET*"}
	,{2,"ES*"}
	,{3,"EXC*"}	,{4,"EXCE"}
	,{3,"EXT*"}	,{4,"EXTE"}
	,{4,"EXTG*"}

	,{1,"F"}	,{3,"FIE*"}	,{5,"FIELD"}
	,{3,"FIF*"}	,{4,"FIFO"}
	,{3,"FIL*"}
	,{3,"FIR*"}
	,{3,"FIX*"}	,{5,"FIXED"}
	,{3,"FLA*"}
	,{3,"FLU*"}
	,{2,"FO*"}

	,{1,"G"}	,{2,"GR*"}

	,{2,"HE*"}
	,{3,"HOL*"}
	,{3,"HOS*"}	,{4,"HOST"}

	,{6,"ICHSET"}
	,{2,"IN*"}	,{4,"INSE"}
	,{3,"IOE*"}

	,{3,"LAB*"}
	,{3,"LAS*"}
	,{2,"LE*"}	,{4,"LENG"}
	,{2,"LI*"}
	,{4,"LOGF*"}
	,{4,"LOGQ*"}
	,{3,"LOW*"}

	,{1,"M"}
	,{3,"MOU*"}

	,{2,"NA*"}
	,{3,"NEW*"}
	,{3,"NEX*"}
	,{6,"NOBIGR*"}
	,{4,"NOBU*"}
	,{4,"NOCA*"}
	,{4,"NOCE*"}	,{6,"NOCENE"}
	,{6,"NOCONV*"}
	,{6,"NODELI*"}
	,{5,"NODOU*"}
	,{4,"NOEB*"}
	,{4,"NOEC*"}	,{6,"NOECHO"}
	,{4,"NOED*"}	,{6,"NOEDIT"}
	,{4,"NOES*"}	,{6,"NOESCA"}
	,{5,"NOEXT*"}
	,{5,"NOFIL*"}
	,{5,"NOFIX*"}
	,{5,"NOFLA*"}
	,{4,"NOHE*"}
	,{5,"NOHOL*"}
	,{5,"NOHOS*"}	,{6,"NOHOST"}
	,{4,"NOIN*"}	,{6,"NOINSE"}
	,{5,"NOLAB*"}
	,{5,"NOLOW*"}
	,{6,"NONOTI*"}
	,{5,"NOPAG*"}
	,{6,"NOPASS*"}
	,{6,"NOPAST*"}
	,{6,"NOPRIN*"}
	,{4,"NORC*"}
	,{6,"NOREAD"}	,{7,"NOREADO*"}
	,{7,"NOREADS*"}
	,{5,"NORES*"}
	,{5,"NORET*"}
	,{4,"NOSE*"}
	,{4,"NOST*"}
	,{4,"NOTE"}
	,{5,"NOTER*"}
	,{4,"NOTI*"}
	,{5,"NOTRA*"}
	,{5,"NOTRU*"}
	,{4,"NOTT*"}	,{6,"NOTTSY"}
	,{4,"NOTY*"}	,{6,"NOTYPE"}
	,{4,"NOUR*"}
	,{4,"NOWA*"}
	,{4,"NOWC*"}
	,{4,"NOWR"}	,{5,"NOWRA*"}	,{6,"NOWRAP"}
	,{7,"NOWRITE*"}
	,{2,"NU*"}

	,{1,"O"}
	,{6,"OCHSET"}
	,{2,"OP*"}
	,{2,"OV*"}
	,{2,"OW*"}

	,{2,"P1"}
	,{2,"P2"}
	,{2,"P3"}
	,{2,"P4"}
	,{2,"P5"}
	,{2,"P6"}
	,{2,"P7"}
	,{2,"P8"}
        ,{3,"PAD"}
	,{3,"PAG*"}
	,{4,"PASS*"}
	,{4,"PAST*"}
	,{4,"PRIO*"}
	,{4,"PRIN*"}
	,{3,"PRM*"}	,{6,"PRMMBX"}
	,{3,"PRO*"}

	,{3,"QUE*"}

	,{2,"RC*"}	,{4,"RCHK"}
	,{4,"READ"}	,{5,"READO*"}
	,{5,"READS*"}
	,{3,"REC*"}
	,{3,"REM*"}
	,{3,"REN*"}
	,{3,"RES*"}
	,{3,"RET*"}
	,{3,"REW*"}
	,{3,"RFA"}
	,{3,"RFM"}

	,{1,"S"}
	,{3,"SEQ*"}
	,{3,"SET*"}
	,{2,"SH*"}	,{4,"SHAR"}
	,{2,"SK*"}
	,{2,"SO*"}
	,{3,"SPA*"}
	,{3,"SPO*"}
	,{2,"ST*"}
	,{2,"SU*"}
	,{2,"SY*"}

	,{2,"TE*"}	,{4,"TERM"}
	,{2,"TM*"}
	,{3,"TRA*"}
	,{3,"TRU*"}
	,{2,"TT*"}	,{4,"TTSY"}
	,{2,"TY*"}	,{4,"TYPE"}

	,{2,"UI*"}	,{3,"UIC"}
	,{2,"UN*"}
	,{2,"UP*"}
	,{2,"UR*"}
	,{2,"US*"}

	,{2,"VA*"}

	,{1,"W"}
	,{2,"WA*"}	,{4,"WAIT"}
	,{2,"WC*"}	,{4,"WCHK"}
	,{2,"WI*"}	,{5,"WIDTH"}
	,{2,"WO*"}
	,{2,"WR"}	,{3,"WRA*"}
	,{5,"WRITE"}
	,{7,"WRITELB"}
	,{7,"WRITEOF"}
	,{7,"WRITEON*"}
	,{7,"WRITETM"}

	,{1,"X"}

	,{1,"Y"}

	,{2,"ZB*"}
	,{2,"ZD*"}
	,{3,"ZEX*"}
	,{4,"ZFIL*"}
	,{3,"ZFF"}
	,{2,"ZI*"}
	,{4,"ZLEN*"}
	,{4,"ZLIS*"}

	,{8,"ZNODELAY"} /* ZNO* have to be spelled out fully */
	,{9,"ZNOFILTER"}
	,{5,"ZNOFF"}
	,{7,"ZNOWRAP"}

	,{4,"ZWID*"}
	,{4,"ZWRA*"}
};
/* Offset of letter with dev_param_names */
LITDEF	unsigned char dev_param_index[27] =
{
/*	A    B    C    D    E    F    G    H    I    J    K    L    M    N   */
	0,   5,   9,   24,  30,  43,  55,  57,  61,  65,  65,  65,  73,  75,
/*	O    P    Q    R    S    T    U    V    W    X    Y    Z    end	     */
	135, 140, 157, 158, 171, 183, 192, 198, 199, 214, 215, 216, 230
};
/* Offset of string within letter in dev_param_names */
/* maintained in conjunction with zshow_params.h   = offset in letter, letter  */
LITDEF zshow_index zshow_param_index[] =
{
/*	ALLO     BLOC     CONV     CTRA     DELE     EBCD     EDIT    EXCE     EXTE     FIELD   FIL     FIXED   HOST */
	{2,0},   {2,1},   {10,2},  {14,2},  {1,3},   {1,4},   {4,4},  {9,4},   {11,4},  {2,5},  {5,5},  {8,5},  {3,7},
/*      ICHSET   INSE     LAB */
	{0,8},   {1,8},   {1,11},
/*	LENG     NOCENE   NOECHO   NOEDIT   NOESCA   NOHOST   NOINSE     */
	{3,11},  {7,13},  {13,13}, {15,13}, {17,13}, {25,13}, {27,13},
/*	NOPAST   NOREADS  NOTTSY   NOTYPE   NOWRAP   OCHSET   PAD     PAST     PRMMBX   RCHK    */
	{33,13}, {38,13}, {49,13}, {51,13}, {57,13}, {1,14},  {9,15}, {11,15}, {15,15}, {1,17},
/*      READ     READS	  REC      SHAR     TERM     TTSY     TYPE    UIC      WAIT     WCHK    WIDTH   WRITE  */
	{2,17},  {4,17},  {5,17},  {4,18},  {1,19},  {6,19},  {8,19}, {1,20},  {2,22},  {4,22}, {6,22}, {10,22}
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
	char		parstr[MAXDEVPARLEN];
	char		*parptr;
	boolean_t	is_parm_list;

	error_def(ERR_RPARENMISSING);
	error_def(ERR_DEVPARUNK);
	error_def(ERR_DEVPARINAP);
	error_def(ERR_DEVPARVALREQ);

	static readonly unsigned char dev_param_data[] =
	{
		 iop_after
		,iop_allocation	,iop_allocation
		,iop_append
		,iop_attach

		,iop_bigrecord
		,iop_blocksize ,iop_blocksize
		,iop_burst

		,iop_canctlo, iop_canctlo
		,iop_canonical
		,iop_cenable
		,iop_characteristic
		,iop_chset
		,iop_clearscreen
		,iop_cli
		,iop_connect
		,iop_contiguous
		,iop_convert
		,iop_copies
		,iop_cpulimit
		,iop_ctrap ,iop_ctrap

		,iop_delete ,iop_delete
		,iop_delimiter
		,iop_detach
		,iop_doublespace
		,iop_downscroll

		,iop_ebcdic ,iop_ebcdic
		,iop_echo
		,iop_editing ,iop_editing
		,iop_eraseline
		,iop_erasetape
		,iop_escape
		,iop_exception ,iop_exception
		,iop_extension ,iop_extension
		,iop_extgap

		,iop_field ,iop_field ,iop_field
		,iop_fifo, iop_fifo
		,iop_filter
		,iop_firstpage
		,iop_fixed ,iop_fixed
		,iop_flag
		,iop_flush
		,iop_form

		,iop_g_protection, iop_g_protection

		,iop_header
		,iop_hold
		,iop_hostsync, iop_hostsync

		,iop_ipchset
		,iop_insert ,iop_insert
		,iop_ioerror

		,iop_label
		,iop_lastpage
		,iop_length ,iop_length
		,iop_listen
		,iop_logfile
		,iop_logqueue
		,iop_lowercase

		,iop_m
		,iop_mount

		,iop_name
		,iop_newversion
		,iop_next
		,iop_nobigrecord
		,iop_noburst
		,iop_nocanonical
		,iop_nocenable ,iop_nocenable
		,iop_noconvert
		,iop_nodelimiter
		,iop_nodoublespace
		,iop_noebcdic
		,iop_noecho ,iop_noecho
		,iop_noediting ,iop_noediting
		,iop_noescape ,iop_noescape
		,iop_inhextgap
		,iop_nofilter
		,iop_nofixed
		,iop_noflag
		,iop_noheader
		,iop_nohold
		,iop_nohostsync ,iop_nohostsync
		,iop_noinsert ,iop_noinsert
		,iop_nolabel
		,iop_nolowercase
		,iop_nonotify
		,iop_page
		,iop_nopassall
		,iop_nopasthru
		,iop_noprint
		,iop_nordcheckdata
		,iop_noreadonly ,iop_noreadonly
		,iop_noreadsync
		,iop_norestart
		,iop_inhretry
		,iop_nosequential
		,iop_nostream
		,iop_note
		,iop_noterminator
		,iop_notify
		,iop_notrailer
		,iop_notruncate
		,iop_nottsync ,iop_nottsync
		,iop_notypeahead ,iop_notypeahead
		,iop_nourgent
		,iop_nowait
		,iop_nowtcheckdata
		,iop_nowrap ,iop_nowrap ,iop_nowrap
		,iop_nowriteonly
		,iop_nl

		,iop_o_protection
		,iop_opchset
		,iop_operator
		,iop_noinsert
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
		,iop_page
		,iop_passall
		,iop_pasthru
		,iop_priority
		,iop_print
		,iop_prmmbx ,iop_prmmbx
		,iop_o_protection

		,iop_queue

		,iop_rdcheckdata ,iop_rdcheckdata
		,iop_readonly ,iop_readonly
		,iop_readsync
		,iop_recordsize
		,iop_remote
		,iop_rename
		,iop_restart
		,iop_retry
		,iop_rewind
		,iop_rfa
		,iop_rfm

		,iop_s_protection
		,iop_sequential
		,iop_setup
		,iop_shared ,iop_shared
		,iop_skipfile
		,iop_socket
		,iop_space
		,iop_spool
		,iop_stream
		,iop_submit
		,iop_s_protection

		,iop_terminator, iop_terminator
		,iop_tmpmbx
		,iop_trailer
		,iop_truncate
		,iop_ttsync ,iop_ttsync
		,iop_typeahead ,iop_typeahead

		,iop_uic ,iop_uic
		,iop_unload
		,iop_upscroll
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

		,iop_zbfsize
		,iop_zdelay
		,iop_exception /* for ZEXCEPTION; ZEXC* is a synonym for EXC* */
		,iop_filter /* for ZFILTER; ZFIL* is a synonym for FIL* */
		,iop_zff
		,iop_zibfsize
		,iop_length /* for ZLENGTH; ZLEN* is a synonym for LE* and LENG */
		,iop_zlisten

		,iop_znodelay
		,iop_nofilter /* for ZNOFILTER; ZNOFILTER is a synonym for NOFIL* */
		,iop_znoff
		,iop_nowrap /* for ZNOWRAP; ZNOWRAP is a synonym for NOWR, NOWRA*, NOWRAP */
		,iop_width /* for ZWIDTH; ZWID* is a synonym for WI*, WIDTH */
		,iop_wrap /* for ZWRAP; ZWRA* is a synonym for WR, and WRA* */
	} ;

	assert(dev_param_index[26] == (sizeof(dev_param_names)/sizeof(nametabent)));
	assert(dev_param_index[26] == (sizeof(dev_param_data)/sizeof(unsigned char)));
	is_parm_list = (window_token == TK_LPAREN);
	if (is_parm_list)
		advancewindow();
	cat_cnt = 0;
	parptr = parstr;
	for (;;)
	{
		if((window_token != TK_IDENT)
			|| ((n = namelook(dev_param_index, dev_param_names, window_ident.addr, window_ident.len)) < 0))
		{
			stx_error(ERR_DEVPARUNK);
			return FALSE;
		}
		n = dev_param_data[n];
		if(!(dev_param_control[n].valid_with & who_calls)) {
			stx_error(ERR_DEVPARINAP);
			return FALSE;
		}
		advancewindow();
		*parptr++ = n;
		if (io_params_size[n])
		{
			if(window_token != TK_EQUAL)
			{
				stx_error(ERR_DEVPARVALREQ);
				return FALSE;
			}
			advancewindow();
			if (!expr(&x))
				return FALSE;
			assert(x.oprclass == TRIP_REF);
			if (x.oprval.tref->opcode == OC_LIT)
			{
				/* check to see if this string could overflow (5 is a int4 word plus a parameter code for
				   safety)  Must check before cvtparm, due to the fact that tmpmval could otherwise
				   be garbage collected by a later putstr
				*/
				if (parptr - parstr + x.oprval.tref->operand[0].oprval.mlit->v.str.len + 5 > sizeof(parstr))
				{
					cat_list[cat_cnt++] = put_str(parstr, parptr - parstr);
					parptr = parstr;
				}
				assert(x.oprval.tref->operand[0].oprclass == MLIT_REF);
				status = cvtparm(n, &x.oprval.tref->operand[0].oprval.mlit->v, &tmpmval);
				if (status)
				{
					stx_error(status);
					return FALSE;
				}
				memcpy(parptr, tmpmval.str.addr, tmpmval.str.len);
				parptr += tmpmval.str.len;
			} else
			{
				if (parptr > parstr)
				{
					cat_list[cat_cnt++] = put_str(parstr, parptr - parstr);
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
		if (window_token == TK_COLON)
		{
			advancewindow();
			continue;
		}
		else if (window_token == TK_RPAREN)
		{
			advancewindow();
			break;
		}
		stx_error(ERR_RPARENMISSING);
		return FALSE;
	}
	*parptr++ = iop_eol;
	cat_list[cat_cnt++] = put_str(parstr,parptr - parstr);
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
