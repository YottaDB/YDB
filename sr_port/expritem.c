/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "compiler.h"
#include "svnames.h"
#include "opcode.h"
#include "toktyp.h"
#include "nametabtyp.h"
#include "funsvn.h"
#include "advancewindow.h"
#include "stringpool.h"
#include "namelook.h"

#define VMS_OS  01
#define UNIX_OS 02
#define ALL_SYS (VMS_OS | UNIX_OS)
#ifdef UNIX			/* function and svn validation are a function of the OS */
#define VALID_FUN(i) (fun_data[i].os_syst & UNIX_OS)
#define VALID_SVN(i) (svn_data[i].os_syst & UNIX_OS)
#elif defined VMS
#define VALID_FUN(i) (fun_data[i].os_syst & VMS_OS)
#define VALID_SVN(i) (svn_data[i].os_syst & VMS_OS)
#else
#error UNSUPPORTED PLATFORM
#endif

GBLREF char window_token;
GBLREF char director_token;
GBLREF mval window_mval;
GBLREF mident window_ident;
GBLREF bool temp_subs;
GBLREF bool devctlexp;

LITREF toktabtype tokentable[];

/* note that svn_index array provides indexes into this array for each letter of the
   alphabet so changes here should be reflected there.
*/
LITDEF nametabent svn_names[] = {
	{ 1,"D" }, { 6,"DEVICE" }
	,{ 2,"EC" }, { 5,"ECODE" }
	,{ 2,"ES" }, { 6,"ESTACK" }
	,{ 2,"ET" }, { 5,"ETRAP" }
	,{ 1,"H" }, { 7,"HOROLOG" }
	,{ 1,"I" }, { 2,"IO" }
	,{ 1,"J" }, { 3,"JOB" }
	,{ 1,"K" }, { 3,"KEY" }
	,{ 1,"P" }, { 8,"PRINCIPA*" }
	,{ 1,"Q" }, { 4,"QUIT" }
	,{ 1,"R" }, { 8,"REFERENC*" }
	,{ 1,"S" }, { 7,"STORAGE" }
	,{ 2,"ST" }, { 5,"STACK" }
	,{ 2,"SY" }, { 6,"SYSTEM" }
	,{ 1,"T" }, { 4,"TEST" }
	,{ 2, "TL"}, { 6, "TLEVEL"}
	,{ 2, "TR"}, { 8, "TRESTART"}
	,{ 1,"X" }
	,{ 1,"Y" }
	,{ 2,"ZA" }
	,{ 2,"ZB" }
	,{ 2,"ZC" }
	,{ 3,"ZCM*" }
	,{ 3,"ZCO*" }
	,{ 3,"ZCS*" }
	,{ 3,"ZDA*" }
	,{ 2,"ZD*" }
	,{ 2,"ZE" }
	,{ 3,"ZED*" }
	,{ 3,"ZEO*" }
	,{ 3,"ZER*" }
	,{ 2,"ZG*" }
	,{ 4,"ZINI*"}
	,{ 4,"ZINT*"}
	,{ 3,"ZIO" }
	,{ 2,"ZJ" }, { 4,"ZJOB" }
	,{ 2,"ZL*" }
	,{ 8,"ZMAXTPTI" }
	,{ 3,"ZMO*" }
	,{ 4,"ZPOS*" }
	,{ 5,"ZPROC*" }
	,{ 5,"ZPROM*" }
	,{ 3,"ZRO*" }
	,{ 3,"ZSO*" }
	,{ 2,"ZS" }, { 4,"ZSTA*" }
	,{ 5,"ZSTEP"}
	,{ 3,"ZSY*"}
	,{ 2,"ZT*" }
	,{ 2,"ZV*" }
	,{ 4,"ZYER*" }
};
/* Indexes into svn_names array for each letter of the alphabet */
LITDEF unsigned char svn_index[27] = {
	 0,  0,  0,  0,  2,  8,  8,  8, 10,	/* a b c d e f g h i */
	12, 14 ,16, 16, 16, 16, 16, 18, 20,	/* j k l m n o p q r */
	22, 28, 34 ,34, 34, 34, 35, 36, 69	/* s t u v w x y z ~ */
};
/* These entries correspond to the entries in the svn_names array */
LITDEF svn_data_type svn_data[] =
{
	{ SV_DEVICE, FALSE, ALL_SYS }, { SV_DEVICE, FALSE, ALL_SYS }
	,{ SV_ECODE, TRUE, ALL_SYS }, { SV_ECODE, TRUE, ALL_SYS }
	,{ SV_ESTACK, FALSE, ALL_SYS }, { SV_ESTACK, FALSE, ALL_SYS }
	,{ SV_ETRAP, TRUE, ALL_SYS }, { SV_ETRAP, TRUE, ALL_SYS }
	,{ SV_HOROLOG, FALSE, ALL_SYS }, { SV_HOROLOG, FALSE, ALL_SYS }
	,{ SV_IO, FALSE, ALL_SYS }, { SV_IO, FALSE, ALL_SYS }
	,{ SV_JOB, FALSE, ALL_SYS }, { SV_JOB, FALSE, ALL_SYS }
	,{ SV_KEY, FALSE, ALL_SYS }, { SV_KEY, FALSE, ALL_SYS }
	,{ SV_PRINCIPAL, FALSE, ALL_SYS }, { SV_PRINCIPAL, FALSE, ALL_SYS }
	,{ SV_QUIT, FALSE, ALL_SYS }, { SV_QUIT, FALSE, ALL_SYS }
	,{ SV_REFERENCE, FALSE, ALL_SYS }, { SV_REFERENCE, FALSE, ALL_SYS }
	,{ SV_STORAGE, FALSE, ALL_SYS }, { SV_STORAGE, FALSE, ALL_SYS }
	,{ SV_STACK, FALSE, ALL_SYS }, { SV_STACK, FALSE, ALL_SYS }
	,{ SV_SYSTEM, FALSE, ALL_SYS }, { SV_SYSTEM, FALSE, ALL_SYS }
	,{ SV_TEST, FALSE, ALL_SYS }, { SV_TEST, FALSE, ALL_SYS }
	,{ SV_TLEVEL, FALSE, ALL_SYS }, { SV_TLEVEL, FALSE, ALL_SYS }
	,{ SV_TRESTART, FALSE, ALL_SYS }, { SV_TRESTART, FALSE, ALL_SYS }
	,{ SV_X, TRUE, ALL_SYS }
	,{ SV_Y, TRUE, ALL_SYS }
	,{ SV_ZA, FALSE, ALL_SYS }
	,{ SV_ZB, FALSE, ALL_SYS }
	,{ SV_ZC, FALSE, ALL_SYS }
	,{ SV_ZCMDLINE, FALSE, ALL_SYS }
	,{ SV_ZCOMPILE, TRUE, ALL_SYS }
	,{ SV_ZCSTATUS, FALSE, ALL_SYS}
	,{ SV_ZDATE_FORM, TRUE, ALL_SYS }
	,{ SV_ZDIR, TRUE, ALL_SYS }
	,{ SV_ZERROR, TRUE, ALL_SYS }
	,{ SV_ZEDITOR, FALSE, ALL_SYS }
	,{ SV_ZEOF, FALSE, ALL_SYS }
	,{ SV_ZERROR, TRUE, ALL_SYS }
	,{ SV_ZGBLDIR, TRUE, ALL_SYS }
	,{ SV_ZININTERRUPT, FALSE, ALL_SYS}
	,{ SV_ZINTERRUPT, TRUE, ALL_SYS}
	,{ SV_ZIO, FALSE, ALL_SYS }
	,{ SV_ZJOB, FALSE, ALL_SYS }, { SV_ZJOB, FALSE, ALL_SYS }
	,{ SV_ZLEVEL, FALSE, ALL_SYS }
	,{ SV_ZMAXTPTIME, TRUE, ALL_SYS }
	,{ SV_ZMODE, FALSE, ALL_SYS }
	,{ SV_ZPOS, FALSE, ALL_SYS }
	,{ SV_ZPROC, FALSE, ALL_SYS }
	,{ SV_PROMPT, TRUE, ALL_SYS }
	,{ SV_ZROUTINES, TRUE, ALL_SYS }
	,{ SV_ZSOURCE, TRUE, ALL_SYS }
	,{ SV_ZSTATUS, TRUE, ALL_SYS },{ SV_ZSTATUS, TRUE, ALL_SYS }
	,{ SV_ZSTEP, TRUE, ALL_SYS }
	,{ SV_ZSYSTEM, FALSE, ALL_SYS }
	,{ SV_ZTRAP, TRUE, ALL_SYS }
	,{ SV_ZVERSION, FALSE, ALL_SYS }
	,{ SV_ZYERROR, TRUE, ALL_SYS }
};

/* note that fun_index array provides indexes into this array for each letter of the
   alphabet so changes here should be reflected there.
*/
LITDEF nametabent fun_names[] =
{
	{1, "A"}, {5, "ASCII"}
	,{1, "C"}, {4, "CHAR"}
	,{1, "D"}, {4, "DATA"}
	,{1, "E"}, {7, "EXTRACT"}
	,{1, "F"}, {4, "FIND"}
	,{2, "FN"}, {7, "FNUMBER"}
	,{1, "G"}, {3, "GET"}
	,{1, "J"}, {7, "JUSTIFY"}
	,{1, "L"}, {6, "LENGTH"}
	,{1, "N"}
	,{2, "NA"}, {4, "NAME"}
	,{4, "NEXT"}
	,{1, "O"}, {5, "ORDER"}
	,{1, "P"}, {5, "PIECE"}
	,{2, "QL"}, {7, "QLENGTH"}
	,{2, "QS"}, {8, "QSUBSCRI*"}
	,{1, "Q"}, {5, "QUERY"}
	,{1, "R"}, {6, "RANDOM"}
	,{2, "RE"}, {7, "REVERSE"}
	,{1, "S"}, {6, "SELECT"}
	,{2, "ST"}, {5, "STACK"}
	,{1, "T"}, {4, "TEXT"}
	,{2, "TR"}, {8, "TRANSLAT*"}
	,{1, "V*"}
	,{7, "ZBITAND"}
	,{8, "ZBITCOUN"}
	,{8, "ZBITFIND"}
	,{7, "ZBITGET"}
	,{7, "ZBITLEN"}
	,{7, "ZBITNOT"}
	,{6, "ZBITOR"}
	,{7, "ZBITSET"}
	,{7, "ZBITSTR"}
	,{7, "ZBITXOR"}
	,{2, "ZC"}, {5, "ZCALL"}
	,{2, "ZD"}, {5, "ZDATE"}
	,{6, "ZECHAR"}
	,{5, "ZFILE"}, {8, "ZFILEATT*"}
	,{7, "ZGETDVI"}
	,{7, "ZGETJPI"}
	,{7, "ZGETLKI"}
	,{7, "ZGETSYI"}
	,{8, "ZJOBEXAM"}
	,{5, "ZLKID"}
	,{2, "ZM"}, {8, "ZMESSAGE"}
	,{6, "ZPARSE"}
	,{4, "ZPID"}
	,{2, "ZP"}, {8, "ZPREVIOU*"}
	,{5, "ZPRIV"}, {8, "ZPRIVILE*"}
	,{2, "ZQ"}, {8, "ZQGBLMOD"}
	,{7, "ZSEARCH"}
	,{7, "ZSETPRV"}
	,{8, "ZSIGPROC"}
	,{7, "ZTRNLNM"}
};
/* Index into fun_names array where entries that start with each letter of
   the alphabet begin. */
LITDEF unsigned char fun_index[27] = {
	 0,  2,  2,  4,  6,  8, 12, 14, 14,	/* a b c d e f g h i */
	14, 16, 16, 18, 18, 22, 24, 26, 32,	/* j k l m n o p q r */
	36, 40, 44, 44, 45, 45, 45, 45, 82	/* s t u v w x y z ~ */
};
/* Each entry corresponds to an entry in fun_names */
LITDEF fun_data_type fun_data[] =
{
	 { OC_FNASCII, ALL_SYS }, { OC_FNASCII, ALL_SYS }
        ,{ OC_FNCHAR, ALL_SYS }, { OC_FNCHAR, ALL_SYS }
	,{ OC_FNDATA, ALL_SYS }, { OC_FNDATA, ALL_SYS }
	,{ OC_FNEXTRACT, ALL_SYS }, { OC_FNEXTRACT, ALL_SYS }
	,{ OC_FNFIND, ALL_SYS }, { OC_FNFIND, ALL_SYS }
	,{ OC_FNFNUMBER, ALL_SYS }, { OC_FNFNUMBER, ALL_SYS }
	,{ OC_FNGET, ALL_SYS }, { OC_FNGET, ALL_SYS }
	,{ OC_FNJ2, ALL_SYS }, { OC_FNJ2, ALL_SYS }
	,{ OC_FNLENGTH, ALL_SYS }, { OC_FNLENGTH, ALL_SYS }
	,{ OC_FNNEXT, ALL_SYS }
	,{ OC_FNNAME, ALL_SYS }, { OC_FNNAME, ALL_SYS }
	,{ OC_FNNEXT, ALL_SYS }
	,{ OC_FNORDER, ALL_SYS }, {OC_FNORDER, ALL_SYS }
	,{ OC_FNPIECE, ALL_SYS }, { OC_FNPIECE, ALL_SYS }
	,{ OC_FNQLENGTH, ALL_SYS }, { OC_FNQLENGTH, ALL_SYS }
	,{ OC_FNQSUBSCR, ALL_SYS }, { OC_FNQSUBSCR, ALL_SYS }
	,{ OC_FNQUERY, ALL_SYS }, { OC_FNQUERY, ALL_SYS }
	,{ OC_FNRANDOM, ALL_SYS }, { OC_FNRANDOM, ALL_SYS }
	,{ OC_FNREVERSE, ALL_SYS }, { OC_FNREVERSE, ALL_SYS }
	,{ OC_PASSTHRU, ALL_SYS }, { OC_PASSTHRU, ALL_SYS }
	,{ OC_FNSTACK1, ALL_SYS }, { OC_FNSTACK1, ALL_SYS }
	,{ OC_FNTEXT, ALL_SYS }, { OC_FNTEXT, ALL_SYS }
	,{ OC_FNTRANSLATE, ALL_SYS }, { OC_FNTRANSLATE, ALL_SYS }
	,{ OC_FNVIEW, ALL_SYS }
	,{ OC_FNZBITAND, ALL_SYS }
	,{ OC_FNZBITCOUN, ALL_SYS }
	,{ OC_FNZBITFIND, ALL_SYS }
	,{ OC_FNZBITGET, ALL_SYS }
	,{ OC_FNZBITLEN, ALL_SYS }
	,{ OC_FNZBITNOT, ALL_SYS }
	,{ OC_FNZBITOR, ALL_SYS }
	,{ OC_FNZBITSET, ALL_SYS }
	,{ OC_FNZBITSTR, ALL_SYS }
	,{ OC_FNZBITXOR, ALL_SYS }
#ifdef __sun
	,{ OC_FNZCALL,UNIX_OS}, { OC_FNZCALL,UNIX_OS}
#else
	,{ OC_FNZCALL, VMS_OS }, { OC_FNZCALL, VMS_OS }
#endif
	,{ OC_FNZDATE, ALL_SYS }, { OC_FNZDATE, ALL_SYS }
        ,{ OC_FNCHAR, ALL_SYS }
	,{ OC_FNZFILE, VMS_OS }, { OC_FNZFILE, VMS_OS }
	,{ OC_FNZGETDVI, VMS_OS }
	,{ OC_FNZGETJPI, ALL_SYS }
	,{ OC_FNZGETLKI, VMS_OS }
	,{ OC_FNZGETSYI, VMS_OS }
	,{ OC_FNZJOBEXAM, ALL_SYS }
	,{ OC_FNZLKID, VMS_OS}
	,{ OC_FNZM, ALL_SYS }, { OC_FNZM, ALL_SYS }
	,{ OC_FNZPARSE, ALL_SYS }
	,{ OC_FNZPID, VMS_OS }
	,{ OC_FNZPREVIOUS, ALL_SYS }, { OC_FNZPREVIOUS, ALL_SYS }
	,{ OC_FNZPRIV, VMS_OS }, { OC_FNZPRIV, VMS_OS }
	,{ OC_FNZQGBLMOD, ALL_SYS }, { OC_FNZQGBLMOD, ALL_SYS }
	,{ OC_FNZSEA, ALL_SYS }
	,{ OC_FNZSETPRV, VMS_OS }
	,{ OC_FNZSIGPROC, ALL_SYS }
	,{ OC_FNZTRNLNM, ALL_SYS }
};

LITDEF int (*fun_parse[])(oprtype *, opctype) =
{
	f_ascii, f_ascii, f_char, f_char, f_data, f_data, f_extract
	, f_extract, f_find, f_find, f_fnumber, f_fnumber, f_get, f_get
	, f_justify, f_justify, f_length, f_length, f_next, f_name, f_name, f_next, f_order
	, f_order, f_piece, f_piece, f_qlength, f_qlength, f_qsubscript, f_qsubscript
	, f_query, f_query, f_mint, f_mint, f_reverse, f_reverse, f_select, f_select
	, f_stack, f_stack, f_text, f_text, f_translate, f_translate, f_view
	, f_two_mval, f_one_mval, f_fnzbitfind, f_fnzbitget, f_one_mval
	, f_one_mval, f_two_mval, f_fnzbitset, f_fnzbitstr, f_two_mval
	, f_zcall, f_zcall, f_zdate, f_zdate, f_zechar
	, f_two_mstrs, f_two_mstrs, f_two_mstrs, f_mint_mstr, f_two_mstrs, f_zgetsyi
	, f_zjobexam, f_mint, f_mint, f_mint, f_zparse, f_mint, f_zprevious, f_zprevious, f_mstr, f_mstr
	, f_zqgblmod, f_zqgblmod, f_zsearch, f_mstr, f_zsigproc, f_ztrnlnm
};


int expritem(oprtype *a)
{

	int 		index, sv_opcode;
	unsigned char 	type;
	short int 	i;
	triple 		*ref;
	oprtype 	x1;
	error_def(ERR_EXPR);
	error_def(ERR_FCNSVNEXPECTED);
	error_def(ERR_FNOTONSYS);
	error_def(ERR_INVFCN);
	error_def(ERR_INVSVN);
	error_def(ERR_RPARENMISSING);
	error_def(ERR_VAREXPECTED);

	assert(svn_index[26] == (sizeof(svn_names)/sizeof(nametabent)));
	assert(sizeof(svn_names)/sizeof(nametabent) == sizeof(svn_data)/sizeof(svn_data_type)); /* are all SVNs covered? */
	assert(fun_index[26] == (sizeof(fun_names)/sizeof(nametabent)));
	assert(sizeof(fun_names)/sizeof(nametabent) == sizeof(fun_data)/sizeof(fun_data_type)); /* are all functions covered? */
	if (i = tokentable[window_token].uo_type)
	{
		type = tokentable[window_token].opr_type;
		advancewindow();
		if (i == OC_NEG && (window_token == TK_NUMLIT || window_token == TK_INTLIT))
		{
			assert(MV_IS_NUMERIC(&window_mval));
			if (window_mval.mvtype & MV_INT)
				window_mval.m[1] = -window_mval.m[1];
			else
				window_mval.sgn = 1;
			if (window_token == TK_NUMLIT)
				n2s(&window_mval);
		}
		else
		{
			if (!expratom(&x1))
				return FALSE;
			coerce(&x1,type);
			ref = newtriple((opctype) i);
			ref->operand[0] = x1;
			*a = put_tref(ref);
			return TRUE;
		}
	}
	switch(i = window_token)
	{
	case TK_INTLIT:
		n2s(&window_mval);
	case TK_NUMLIT:
	case TK_STRLIT:
		*a = put_lit(&window_mval);
		advancewindow();
		return TRUE;
	case TK_LPAREN:
		advancewindow();
		if (eval_expr(a))
			if (window_token == TK_RPAREN)
			{
				advancewindow();
				return TRUE;
			}
		stx_error(ERR_RPARENMISSING);
		return FALSE;
	case TK_DOLLAR:
		if (director_token == TK_DOLLAR)
		{
			temp_subs = TRUE;
			if (!exfunc (a))
				return FALSE;
		}
		else if (director_token == TK_AMPERSAND)
		{	advancewindow();
			temp_subs = TRUE;
			if (!extern_func(a))
				return FALSE;
		}
		else
		{
			advancewindow();
			if (window_token != TK_IDENT)
			{
				stx_error(ERR_FCNSVNEXPECTED);
				return FALSE;
			}
			if (director_token == TK_LPAREN)
			{
				index = namelook(fun_index,fun_names,window_ident.c);
				if (index < 0)
				{
					stx_error(ERR_INVFCN);
					return FALSE;
				}
				assert(sizeof(fun_names)/sizeof(fun_data_type) > index);
				if (! VALID_FUN(index) )
				{
				    	stx_error(ERR_FNOTONSYS);
					return FALSE;
		                }
				advancewindow();
				advancewindow();
				assert(OPCODE_COUNT > fun_data[index].opcode);
				if (!(bool)((*fun_parse[index])(a, fun_data[index].opcode)))
					return FALSE;
				if (window_token != TK_RPAREN)
				{
					stx_error(ERR_RPARENMISSING);
					return FALSE;
				}
				advancewindow();
			} else
			{
				index = namelook(svn_index,svn_names,window_ident.c);
				if (index < 0)
				{
					stx_error(ERR_INVSVN);
					return FALSE;
				}
				assert(sizeof(svn_names)/sizeof(svn_data_type) > index);
				if (!VALID_SVN(index))
				{
				        stx_error(ERR_FNOTONSYS);
					return FALSE;
		                }
				advancewindow();
				sv_opcode = svn_data[index].opcode;
				assert(SV_NUM_SV > sv_opcode);
				if (sv_opcode == SV_TEST)
					*a = put_tref(newtriple(OC_GETTRUTH));
				else
				{
					if (sv_opcode == SV_X || sv_opcode == SV_Y)
						devctlexp = TRUE;
					ref = newtriple(OC_SVGET);
					ref->operand[0] = put_ilit(sv_opcode);
					*a = put_tref(ref);
				}
			}
		}
		return TRUE;
	case TK_COLON:
		stx_error (ERR_EXPR);
		return FALSE;
	}
	return FALSE;
}
