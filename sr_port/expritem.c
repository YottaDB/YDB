/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2025 YottaDB LLC and/or its subsidiaries.	*
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
#include "svnames.h"
#include "nametabtyp.h"
#include "funsvn.h"
#include "advancewindow.h"
#include "stringpool.h"
#include "namelook.h"
#include "fullbool.h"
#include "show_source_line.h"
#include "op.h"
#include "mmemory.h"

GBLREF	bool		devctlexp;
GBLREF	boolean_t	run_time;

error_def(ERR_EXPR);
error_def(ERR_FCNSVNEXPECTED);
error_def(ERR_FNOTONSYS);
error_def(ERR_INVFCN);
error_def(ERR_INVSVN);
error_def(ERR_NUMOFLOW);
error_def(ERR_RPARENMISSING);
error_def(ERR_SIDEEFFECTEVAL);
error_def(ERR_VAREXPECTED);

LITREF	toktabtype	tokentable[];
LITREF	mval		literal_null;
LITREF	mval		literal_numoflow;
LITREF	octabstruct	oc_tab[];

/* note that svn_index array provides indexes into this array for each letter of the
 * alphabet so changes here should be reflected there.
 */
LITDEF nametabent svn_names[] =
{	/* Must be in alpha order and in sync with svn_index[], svn_data[]; when inerting beware of legacy implications */
	 { 1, "D" }, { 6, "DEVICE" }
	,{ 2, "EC" }, { 5, "ECODE" }
	,{ 2, "ES" }, { 6, "ESTACK" }
	,{ 2, "ET" }, { 5, "ETRAP" }
	,{ 1, "H" }, { 7, "HOROLOG" }
	,{ 1, "I" }, { 2, "IO" }
	,{ 1, "J" }, { 3, "JOB" }
	,{ 1, "K" }, { 3, "KEY" }
	,{ 1, "P" }, { 9, "PRINCIPAL" }
	,{ 1, "Q" }, { 4, "QUIT" }
	,{ 1, "R" }, { 9, "REFERENCE" }
	,{ 2, "ST" }, { 5, "STACK" }
	,{ 1, "S" }, { 7, "STORAGE" }
	,{ 2, "SY" }, { 6, "SYSTEM" }
	,{ 1, "T" }, { 4, "TEST" }
	,{ 2, "TL"}, { 6, "TLEVEL"}
	,{ 2, "TR"}, { 8, "TRESTART"}
	,{ 1, "X" }
	,{ 1, "Y" }
	,{ 2, "ZA" }
	,{10, "ZALLOCSTOR" }
	,{ 6, "ZAUDIT" }
	,{ 2, "ZB" }
	,{ 2, "ZC" }
	,{ 6, "ZCHSET" }
	,{ 6, "ZCLOSE" }
	,{ 8, "ZCMDLINE" }
	,{ 8, "ZCOMPILE" }
	,{ 8, "ZCSTATUS" }
	,{ 2, "ZD" }	/* legacy abbreviation for $ZDIRECTORY */
	,{ 9, "ZDATEFORM" }
	,{10, "ZDIRECTORY" }
	,{ 2, "ZE" }	/* legacy abbreviation for $ZERROR */
	,{ 7, "ZEDITOR" }
	,{ 4, "ZEOF" }
	,{ 6, "ZERROR" }
	,{ 7, "ZGBLDIR" }
	,{ 4, "ZGLD" }  /* legacy abbreviation for $ZGBLDIR */
	,{ 8, "ZHOROLOG" }
	,{ 2, "ZI" }	/* legacy abbreviation for $ZININTERRUPT */
	,{ 7, "ZICUVER" }
	,{12, "ZININTERRUPT" }
	,{10, "ZINTERRUPT"}
	,{ 3, "ZIO" }
	,{ 4, "ZJOB" }
	,{ 4, "ZKEY" }
	,{ 6, "ZLEVEL" }
	,{10, "ZMALLOCLIM" }
	,{13, "ZMAXTPTIMEOUT" }
	,{ 8, "ZMLKHASH" }
	,{ 5, "ZMODE" }
	,{ 9, "ZONLNRLBK" }
	,{ 2, "ZP" }	/* legacy abbreviation for $ZPOSITION */
	,{11, "ZPATNUMERIC" }
	,{ 4, "ZPIN" }
	,{ 9, "ZPOSITION" }
	,{ 5, "ZPOUT" }
	,{ 7, "ZPROMPT" }
	,{ 5, "ZQUIT" }
	,{10, "ZREALSTORE" }
	,{ 8, "ZRELDATE" }
	,{ 9, "ZROUTINES" }
	,{ 2, "ZS" }	/* legacy abbreviation for $ZSTATUS */
	,{ 7, "ZSOURCE" }
	,{ 7, "ZSTATUS" }
	,{ 5, "ZSTEP"}
	,{ 9, "ZSTRPLLIM" }
	,{ 7, "ZSYSTEM" }
	,{ 2, "ZT" }	/* legacy abbreviation for $ZTRAP */
	,{ 6, "ZTCODE" }
	,{ 6, "ZTDATA" }
	,{ 7, "ZTDELIM"}
	,{ 6, "ZTEXIT" }
	,{ 8, "ZTIMEOUT" }
	,{ 7, "ZTLEVEL" }
	,{ 6, "ZTNAME" }
	,{10, "ZTOLDVALUE" }
	,{ 5, "ZTRAP" }
	,{10, "ZTRIGGEROP" }
	,{ 7, "ZTSLATE" }
	,{ 8, "ZTUPDATE" }
	,{ 7, "ZTVALUE" }
	,{10, "ZTWORMHOLE" }
	,{ 9, "ZUSEDSTOR" }
	,{ 3, "ZUT" }
	,{ 8, "ZVERSION" }
	,{ 7, "ZYERROR" }, { 4, "ZYER" }, { 5, "ZYERR" }
	,{ 9, "ZYINTRSIG" }
	,{ 9, "ZYRELEASE" }, { 4, "ZYRE" }
	,{ 9, "ZYSQLNULL" }
};

/* Indexes into svn_names array for each letter of the alphabet */
LITDEF unsigned char svn_index[27] = {
	 0,  0,  0,  0,  2,  8,  8,  8, 10,	/* a b c d e f g h i */
	12, 14 ,16, 16, 16, 16, 16, 18, 20,	/* j k l m n o p q r */
	22, 28, 34 ,34, 34, 34, 35, 36, 110	/* s t u v w x y z ~ */
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
	,{ SV_STACK, FALSE, ALL_SYS }, { SV_STACK, FALSE, ALL_SYS }
	,{ SV_STORAGE, FALSE, ALL_SYS }, { SV_STORAGE, FALSE, ALL_SYS }
	,{ SV_SYSTEM, TRUE, ALL_SYS }, { SV_SYSTEM, TRUE, ALL_SYS }
	,{ SV_TEST, FALSE, ALL_SYS }, { SV_TEST, FALSE, ALL_SYS }
	,{ SV_TLEVEL, FALSE, ALL_SYS }, { SV_TLEVEL, FALSE, ALL_SYS }
	,{ SV_TRESTART, FALSE, ALL_SYS }, { SV_TRESTART, FALSE, ALL_SYS }
	,{ SV_X, TRUE, ALL_SYS }
	,{ SV_Y, TRUE, ALL_SYS }
	,{ SV_ZA, FALSE, ALL_SYS }
	,{ SV_ZALLOCSTOR, FALSE, ALL_SYS }
	,{ SV_ZAUDIT, FALSE, ALL_SYS }
	,{ SV_ZB, FALSE, ALL_SYS }
	,{ SV_ZC, FALSE, ALL_SYS }
	,{ SV_ZCHSET, FALSE, ALL_SYS }
	,{ SV_ZCLOSE, FALSE, UNIX_OS }
	,{ SV_ZCMDLINE, TRUE, ALL_SYS }
	,{ SV_ZCOMPILE, TRUE, ALL_SYS }
	,{ SV_ZCSTATUS, FALSE, ALL_SYS}
	,{ SV_ZDIR, TRUE, ALL_SYS }
	,{ SV_ZDATE_FORM, TRUE, ALL_SYS }
	,{ SV_ZDIR, TRUE, ALL_SYS }
	,{ SV_ZERROR, TRUE, ALL_SYS }
	,{ SV_ZEDITOR, FALSE, ALL_SYS }
	,{ SV_ZEOF, FALSE, ALL_SYS }
	,{ SV_ZERROR, TRUE, ALL_SYS }
	,{ SV_ZGBLDIR, TRUE, ALL_SYS }
	,{ SV_ZGBLDIR, TRUE, ALL_SYS }
	,{ SV_ZHOROLOG, FALSE, ALL_SYS }
	,{ SV_ZININTERRUPT, FALSE, ALL_SYS}
	,{ SV_ZICUVER, FALSE, ALL_SYS}
	,{ SV_ZININTERRUPT, FALSE, ALL_SYS}
	,{ SV_ZINTERRUPT, TRUE, ALL_SYS}
	,{ SV_ZIO, FALSE, ALL_SYS }
	,{ SV_ZJOB, FALSE, ALL_SYS }
	,{ SV_ZKEY, FALSE , ALL_SYS }
	,{ SV_ZLEVEL, FALSE, ALL_SYS }
	,{ SV_ZMALLOCLIM, TRUE, ALL_SYS}
	,{ SV_ZMAXTPTIME, TRUE, ALL_SYS }
	,{ SV_ZMLKHASH, FALSE, ALL_SYS }
	,{ SV_ZMODE, FALSE, ALL_SYS }
	,{ SV_ZONLNRLBK, FALSE, UNIX_OS }
	,{ SV_ZPOS, FALSE, ALL_SYS }
	,{ SV_ZPATNUMERIC, FALSE, ALL_SYS }
	,{ SV_ZPIN, FALSE, ALL_SYS }
	,{ SV_ZPOS, FALSE, ALL_SYS }
	,{ SV_ZPOUT, FALSE, ALL_SYS }
	,{ SV_PROMPT, TRUE, ALL_SYS }
	,{ SV_ZQUIT, TRUE, ALL_SYS }
	,{ SV_ZREALSTOR, FALSE, ALL_SYS }
	,{ SV_ZRELDATE, FALSE, ALL_SYS }
	,{ SV_ZROUTINES, TRUE, ALL_SYS }
	,{ SV_ZSTATUS, TRUE, ALL_SYS }
	,{ SV_ZSOURCE, TRUE, ALL_SYS }
	,{ SV_ZSTATUS, TRUE, ALL_SYS }
	,{ SV_ZSTEP, TRUE, ALL_SYS }
	,{ SV_ZSTRPLLIM, TRUE, ALL_SYS }
	,{ SV_ZSYSTEM, FALSE, ALL_SYS }
	,{ SV_ZTRAP, TRUE, ALL_SYS }
	,{ SV_ZTCODE, FALSE, TRIGGER_OS }
	,{ SV_ZTDATA, FALSE, TRIGGER_OS }
	,{ SV_ZTDELIM, FALSE, TRIGGER_OS }
	,{ SV_ZTEXIT, TRUE, ALL_SYS }
	,{ SV_ZTIMEOUT, TRUE, UNIX_OS}
	,{ SV_ZTLEVEL, FALSE, TRIGGER_OS}
	,{ SV_ZTNAME, FALSE, TRIGGER_OS }
	,{ SV_ZTOLDVAL, FALSE, TRIGGER_OS }
	,{ SV_ZTRAP, TRUE, ALL_SYS }
	,{ SV_ZTRIGGEROP, FALSE, TRIGGER_OS}
	,{ SV_ZTSLATE, TRUE, TRIGGER_OS}
	,{ SV_ZTUPDATE, FALSE, TRIGGER_OS }
	,{ SV_ZTVALUE, TRUE, TRIGGER_OS }
	,{ SV_ZTWORMHOLE, TRUE, TRIGGER_OS }
	,{ SV_ZUSEDSTOR, FALSE, ALL_SYS }
	,{ SV_ZUT, FALSE, ALL_SYS }
	,{ SV_ZVERSION, FALSE, ALL_SYS }
	,{ SV_ZYERROR, TRUE, ALL_SYS } ,{ SV_ZYERROR, TRUE, ALL_SYS }, { SV_ZYERROR, TRUE, ALL_SYS }
	,{ SV_ZYINTRSIG, FALSE, ALL_SYS }
	,{ SV_ZYRELEASE, FALSE, ALL_SYS } ,{ SV_ZYRELEASE, FALSE, ALL_SYS }
	,{ SV_ZYSQLNULL, FALSE, ALL_SYS }
};

/* note that fun_index array provides indexes into this array for each letter of the
 * alphabet so changes here should be reflected there.
 * "*" is used below only after 8 characters.
 */
LITDEF nametabent fun_names[] =
{	/* Must be in alpha order & in sync with fun_index[], fun_data[], fun_parse; when inerting beware of legacy implications */
	 {1, "A"}, {5, "ASCII"}
	,{1, "C"}, {4, "CHAR"}
	,{1, "D"}, {4, "DATA"}
	,{1, "E"}, {7, "EXTRACT"}
	,{1, "F"}, {4, "FIND"}
	,{2, "FN"}, {7, "FNUMBER"}
	,{1, "G"}, {3, "GET"}
	,{1, "I"}, {4, "INCR"}, {9, "INCREMENT"}
	,{1, "J"}, {7, "JUSTIFY"}
	,{1, "L"}, {6, "LENGTH"}
	,{1, "N"}	/* legacy abbreviation for $NEXT() */
	,{2, "NA"}, {4, "NAME"}
	,{4, "NEXT"}
	,{1, "O"}, {5, "ORDER"}
	,{1, "P"}, {5, "PIECE"}
	,{1, "Q"}	/* legacy abbreviation for $QUERY() */
	,{2, "QL"}, {7, "QLENGTH"}
	,{2, "QS"}, {10, "QSUBSCRIPT"}
	,{5, "QUERY"}
	,{1, "R"}, {6, "RANDOM"}
	,{2, "RE"}, {7, "REVERSE"}
	,{1, "S"}, {6, "SELECT"}
	,{2, "ST"}, {5, "STACK"}
	,{1, "T"}, {4, "TEXT"}
	,{2, "TR"}, {9, "TRANSLATE"}
	,{1, "V"} ,{4, "VIEW"}
	,{2, "ZA"}	/* legacy abbreviation for $ZASCII() */
	,{8, "ZAHANDLE"}
	,{6, "ZASCII"}
	,{11,"ZATRANSFORM"}
	,{9, "ZAUDITLOG"}
	,{7, "ZBITAND"}
	,{9, "ZBITCOUNT"}
	,{8, "ZBITFIND"}
	,{7, "ZBITGET"}
	,{10, "ZBITLENGTH"}
	,{7, "ZBITNOT"}
	,{6, "ZBITOR"}
	,{7, "ZBITSET"}
	,{7, "ZBITSTR"}
	,{7, "ZBITXOR"}
	,{5, "ZCHAR"}
	,{3, "ZCO"}
	,{8, "ZCOLLATE"}
	,{8, "ZCONVERT"}
	,{2, "ZD"}	/* legacy abbreviation for $ZDATE() */
	,{5, "ZDATA"}
	,{5, "ZDATE"}
	,{8, "ZEXTRACT"}
	,{5, "ZFIND"}
	,{7, "ZGETJPI"}
	,{10,"ZINCREMENT"}
	,{2, "ZJ"}	/* legacy abbreviation for $ZJUSTIFY() */
	,{8, "ZJOBEXAM"}
	,{8, "ZJUSTIFY"}
	,{7, "ZLENGTH"}
	,{8, "ZMESSAGE"}
	,{2, "ZP"}	/* legacy abbreviation for $ZPREVIOUS() */
	,{6, "ZPARSE"}
	,{5, "ZPEEK"}
	,{6, "ZPIECE"}
	,{9, "ZPREVIOUS"}
	,{8, "ZQGBLMOD"}
	,{7, "ZSEARCH"}
	,{8, "ZSIGPROC"}
	,{7, "ZSOCKET"}
	,{7, "ZSUBSTR"}
	,{7, "ZSYSLOG"}
	,{10,"ZTRANSLATE"}
	,{8, "ZTRIGGER"}
	,{7, "ZTRNLNM"}
	,{6, "ZWIDTH"}
	,{6, "ZWRITE"}
	,{4, "ZYCO"}, {9, "ZYCOMPILE"}
	,{6, "ZYHASH"}
	,{11, "ZYISSQLNULL"}
	,{4, "ZYSU"}, {8, "ZYSUFFIX"}
};

/* Index into fun_names array where entries that start with each letter of the alphabet begin. */
LITDEF unsigned char fun_index[27] =
{
	 0,  2,  2,  4,  6,  8, 12, 14,  14,	/* a b c d e f g h i */
	17, 19, 19, 21, 21, 25, 27, 29,  35,	/* j k l m n o p q r */
	39, 43, 47, 47, 49, 49, 49, 49, 102,	/* s t u v w x y z ~ */
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
	,{ OC_FNINCR, ALL_SYS }, { OC_FNINCR, ALL_SYS }, { OC_FNINCR, ALL_SYS }
	,{ OC_FNJ2, ALL_SYS }, { OC_FNJ2, ALL_SYS }
	,{ OC_FNLENGTH, ALL_SYS }, { OC_FNLENGTH, ALL_SYS }
	,{ OC_FNNEXT, ALL_SYS }
	,{ OC_FNNAME, ALL_SYS }, { OC_FNNAME, ALL_SYS }
	,{ OC_FNNEXT, ALL_SYS }
	,{ OC_FNORDER, ALL_SYS }, {OC_FNORDER, ALL_SYS }
	,{ OC_FNPIECE, ALL_SYS }, { OC_FNPIECE, ALL_SYS }
	,{ OC_FNQUERY, ALL_SYS }
	,{ OC_FNQLENGTH, ALL_SYS }, { OC_FNQLENGTH, ALL_SYS }
	,{ OC_FNQSUBSCR, ALL_SYS }, { OC_FNQSUBSCR, ALL_SYS }
	,{ OC_FNQUERY, ALL_SYS }
	,{ OC_FNRANDOM, ALL_SYS }, { OC_FNRANDOM, ALL_SYS }
	,{ OC_FNREVERSE, ALL_SYS }, { OC_FNREVERSE, ALL_SYS }
	,{ OC_PASSTHRU, ALL_SYS }, { OC_PASSTHRU, ALL_SYS }	/* f_select creates a STOTEMP and STOs anchored by a PASSTHRU */
	,{ OC_FNSTACK1, ALL_SYS }, { OC_FNSTACK1, ALL_SYS }
	,{ OC_FNTEXT, ALL_SYS }, { OC_FNTEXT, ALL_SYS }
	,{ OC_FNTRANSLATE, ALL_SYS }, { OC_FNTRANSLATE, ALL_SYS }
	,{ OC_FNVIEW, ALL_SYS } ,{ OC_FNVIEW, ALL_SYS }
	,{ OC_FNZASCII, ALL_SYS }
	,{ OC_FNZAHANDLE, ALL_SYS }
	,{ OC_FNZASCII, ALL_SYS }
	,{ OC_FNZATRANSFORM, ALL_SYS }
	,{ OC_FNZAUDITLOG, ALL_SYS }
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
	,{ OC_FNZCHAR, ALL_SYS }
	,{ OC_FNZCONVERT2, UNIX_OS }
	,{ OC_FNZCOLLATE, UNIX_OS }
	,{ OC_FNZCONVERT2, UNIX_OS }
	,{ OC_FNZDATE, ALL_SYS }
	,{ OC_FNZDATA, ALL_SYS }
	,{ OC_FNZDATE, ALL_SYS }
	,{ OC_FNZEXTRACT, ALL_SYS }
	,{ OC_FNZFIND, ALL_SYS }
	,{ OC_FNZGETJPI, ALL_SYS }
	,{ OC_FNINCR, ALL_SYS }
	,{ OC_FNZJ2, ALL_SYS }
	,{ OC_FNZJOBEXAM, ALL_SYS }
	,{ OC_FNZJ2, ALL_SYS }
	,{ OC_FNZLENGTH, ALL_SYS }
	,{ OC_FNZM, ALL_SYS }
	,{ OC_FNZPREVIOUS, ALL_SYS }
	,{ OC_FNZPARSE, ALL_SYS }
	,{ OC_FNZPEEK, UNIX_OS }
	,{ OC_FNZPIECE, ALL_SYS }
	,{ OC_FNZPREVIOUS, ALL_SYS }
	,{ OC_FNZQGBLMOD, ALL_SYS }
	,{ OC_FNZSEA, ALL_SYS }
	,{ OC_FNZSIGPROC, ALL_SYS }
	,{ OC_FNZSOCKET, ALL_SYS }
	,{ OC_FNZSUBSTR, ALL_SYS }
	,{ OC_FNZSYSLOG, UNIX_OS }
	,{ OC_FNZTRANSLATE, ALL_SYS }
	,{ OC_FNZTRIGGER, TRIGGER_OS }
	,{ OC_FNZTRNLNM, ALL_SYS }
	,{ OC_FNZWIDTH, ALL_SYS }
	,{ OC_FNZWRITE, ALL_SYS }
	,{ OC_FNZYCOMPILE, ALL_SYS } ,{ OC_FNZYCOMPILE, ALL_SYS }
	,{ OC_FNZYHASH, ALL_SYS }
	,{ OC_FNZYISSQLNULL, ALL_SYS }
	,{ OC_FNZYSUFFIX, ALL_SYS } ,{ OC_FNZYSUFFIX, ALL_SYS }
};

/* Each entry corresponds to an entry in fun_names */
GBLDEF int (*fun_parse[])(oprtype *, opctype) =		/* contains addresses so can't be a LITDEF */
{
	f_ascii, f_ascii,
	f_char, f_char,
	f_data, f_data,
	f_extract, f_extract,
	f_find, f_find,
	f_fnumber, f_fnumber,
	f_get, f_get,
	f_incr, f_incr, f_incr,
	f_justify, f_justify,
	f_length, f_length,
	f_next,
	f_name, f_name,
	f_next,
	f_order, f_order,
	f_piece, f_piece,
	f_query,
	f_qlength, f_qlength,
	f_qsubscript, f_qsubscript,
	f_query,
	f_mint, f_mint,
	f_reverse, f_reverse,
	f_select, f_select,
	f_stack, f_stack,
	f_text, f_text,
	f_translate, f_translate,
	f_view, f_view,
	f_ascii,
	f_zahandle,
	f_ascii,
	f_zatransform,
	f_one_mval,
	f_two_mval,			/* $ZBITAND */
	f_one_mval,			/* $ZBITCOUNT */
	f_fnzbitfind,
	f_fnzbitget,
	f_one_mval,			/* $ZBITLENGTH*/
	f_one_mval,			/* $ZBITNOT */
	f_two_mval,			/* $ZBITOR */
	f_fnzbitset,
	f_fnzbitstr,
	f_two_mval,			/* $ZBITXOR */
	f_zchar,
	f_zconvert,
	f_zcollate,
	f_zconvert,
	f_zdate,
	f_data,				/* $ZDATA reuses parser for $DATA since only runtime execution differs */
	f_zdate,
	f_extract,
	f_find,
	f_mint_mstr,			/* $ZGETJPI */
	f_incr,
	f_justify,
	f_zjobexam,
	f_justify,
	f_length,
	f_mint,				/* $ZMESSAGE */
	f_zprevious,
	f_zparse,
	f_zpeek,
	f_piece,
	f_zprevious,
	f_zqgblmod,
	f_zsearch,
	f_zsigproc,
	f_zsocket,
	f_extract,			/* $ZSUBSTR */
	f_one_mval,			/* $ZSYSLOG */
	f_translate,
	f_ztrigger,
	f_ztrnlnm,
	f_zwidth,
	f_zwrite,
	f_zycompile, f_zycompile,
	f_zyhash,
	f_zyissqlnull,
	f_zysuffix, f_zysuffix
};

int expritem(oprtype *a)
/* process an expression item into the operand at *a */
{
	boolean_t	parse_warn, saw_local, saw_se, se_warn;
	int		i, index, sv_opcode;
	mval		v;
	oprtype		*j, *k, x1;
	tbp		*funcbp, *tripbp;
	triple		*argtrip, *functrip, *ref, *t1, *t2, *t3;
	unsigned char	type;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	CHKTCHAIN(TREF(curtchain), exorder, TRUE);	/* defined away in mdq.h except with DEBUG_TRIPLES */
	assert(svn_index[26] == (SIZEOF(svn_names)/SIZEOF(nametabent)));
	assert(SIZEOF(svn_names)/SIZEOF(nametabent) == SIZEOF(svn_data)/SIZEOF(svn_data_type)); /* are all SVNs covered? */
	assert(fun_index[26] == (SIZEOF(fun_names)/SIZEOF(nametabent)));
	assert(SIZEOF(fun_names)/SIZEOF(nametabent) == SIZEOF(fun_data)/SIZEOF(fun_data_type)); /* are all functions covered? */
	if ((i = tokentable[TREF(window_token)].uo_type))		/* NOTE assignment */
	{
		type = tokentable[TREF(window_token)].opr_type;
		advancewindow();
		if ((OC_NEG == i) && ((TK_NUMLIT == TREF(window_token)) || (TK_INTLIT == TREF(window_token))))
		{
			assert(MV_IS_NUMERIC(&(TREF(window_mval))));
			if ((TREF(window_mval)).mvtype & MV_INT)
				(TREF(window_mval)).m[1] = -(TREF(window_mval)).m[1];
			else
				(TREF(window_mval)).sgn = 1;
			if (TK_NUMLIT == TREF(window_token))
				n2s(&(TREF(window_mval)));
		} else
		{
			if (!expratom(&x1))
				return FALSE;
			coerce(&x1, type);
			ref = newtriple((opctype)i);
			ref->operand[0] = x1;
			*a = put_tref(ref);
			return TRUE;
		}
	}
	switch (i = TREF(window_token))				/* NOTE assignment */
	{
	case TK_INTLIT:
		n2s(&(TREF(window_mval)));
	case TK_NUMLIT:
	case TK_STRLIT:
		*a = put_lit(&(TREF(window_mval)));
		advancewindow();
		return TRUE;
	case TK_NUMOFLOWLIT:
		STX_ERROR_WARN(ERR_NUMOFLOW);	/* sets "parse_warn" to TRUE */
		advancewindow();
		*a = put_lit((mval *)&literal_numoflow);
		return TRUE;
	case TK_LPAREN:
		advancewindow();
		INCREMENT_EXPR_DEPTH;
		if (eval_expr(a) && TK_RPAREN == TREF(window_token))
		{
			DECREMENT_EXPR_DEPTH;
			advancewindow();
			return TRUE;
		}
		DECREMENT_EXPR_DEPTH;
		stx_error(ERR_RPARENMISSING);
		return FALSE;
	case TK_DOLLAR:
		parse_warn = saw_se = FALSE;
		if ((TK_DOLLAR == TREF(director_token)) || (TK_AMPERSAND == TREF(director_token)))
		{
			ENCOUNTERED_SIDE_EFFECT;
			TREF(temp_subs) = TRUE;
			saw_se = TRUE;
			advancewindow();
			if ((TK_DOLLAR == TREF(window_token)) ? (EXPR_FAIL == exfunc(a, FALSE))
							      : (EXPR_FAIL == extern_func(a)))
				return FALSE;
		} else
		{
			advancewindow();
			if (TK_IDENT != TREF(window_token))
			{
				stx_error(ERR_FCNSVNEXPECTED);
				return FALSE;
			}
			if (TK_LPAREN == TREF(director_token))
			{
				index = namelook(fun_index, fun_names, (TREF(window_ident)).addr, (TREF(window_ident)).len);
				if ((-1 != index) && !gtm_utf8_mode)
				{	/** When possible, update opcodes rather than mess with xfer table */
					switch (fun_data[index].opcode)
					{
					case OC_FNASCII:
						index = namelook(fun_index, fun_names, "zascii", 6);
						break;
					case OC_FNCHAR:
						index = namelook(fun_index, fun_names, "zchar", 5);
						break;
					case OC_FNEXTRACT:
						index = namelook(fun_index, fun_names, "zextract", 8);
						break;
					case OC_FNFIND:
						index = namelook(fun_index, fun_names, "zfind", 5);
						break;
					case OC_FNLENGTH:
						index = namelook(fun_index, fun_names, "zlength", 7);
						break;
					case OC_FNPIECE:
						index = namelook(fun_index, fun_names, "zpiece", 6);
						break;
					case OC_FNTRANSLATE:
						index = namelook(fun_index, fun_names, "ztranslate", 10);
						break;
					default: break;
					}
				}
				if (index < 0)
				{
					STX_ERROR_WARN(ERR_INVFCN);	/* sets "parse_warn" to TRUE */
				} else
				{
					assert(SIZEOF(fun_names) / SIZEOF(fun_data_type) > index);
					if (!VALID_FUN(index))
					{
						STX_ERROR_WARN(ERR_FNOTONSYS);	/* sets "parse_warn" to TRUE */
					} else if ((OC_FNINCR == fun_data[index].opcode) || (OC_FNZCALL == fun_data[index].opcode))
					{	/* $INCR is used. This can operate on undefined local variables
						 * and make them defined. If used in a SET where the left and right
						 * side of the = operator use this variable (as a subscript on the left
						 * and as input to the $INCR function on the right), we want an UNDEF
						 * error to show up which means we need to set "temp_subs" to TRUE.
						 */
						 ENCOUNTERED_SIDE_EFFECT;
						 TREF(temp_subs) = TRUE;
						 saw_se = TRUE;
					}
				}
				advancewindow();
				if ((0 <= index)
					&& ((OC_FNZCHAR == fun_data[index].opcode) || (OC_FNCHAR == fun_data[index].opcode))
					&& (MV_NUM_APPROX == ((MV_NM | MV_NUM_APPROX) & (TREF(director_mval)).mvtype)))
				{	/* [z]f_char need an error based s2n failing to create a valid numeric argument */
					assert((0 == (TREF(director_mval)).e) && !val_iscan(&(TREF(director_mval))));
					TREF(last_source_column) += (TK_EOL == TREF(director_token)) ? -2 : 2;  /* improve hints */
					stx_error(ERR_NUMOFLOW);
				}
				advancewindow();
				if (!parse_warn)
				{
					assert(OPCODE_COUNT > fun_data[index].opcode);
					if (!(boolean_t)((*fun_parse[index])(a, fun_data[index].opcode)))
						return FALSE;
				} else
				{
					*a = put_lit((mval *)&literal_null);
					/* Parse remaining arguments until reaching the corresponding RIGHT-PAREN/SPACE/EOL */
					if (!parse_until_rparen_or_space())
						return FALSE;
				}
				if (TK_RPAREN != TREF(window_token))
				{
					stx_error(ERR_RPARENMISSING);
					return FALSE;
				}
				advancewindow();
			} else
			{
				index = namelook(svn_index, svn_names, (TREF(window_ident)).addr, (TREF(window_ident)).len);
				if (0 > index)
				{
					STX_ERROR_WARN(ERR_INVSVN);	/* sets "parse_warn" to TRUE */
				} else
				{
					assert(SIZEOF(svn_names) / SIZEOF(svn_data_type) > index);
					if (!VALID_SVN(index))
					{
						STX_ERROR_WARN(ERR_FNOTONSYS);	/* sets "parse_warn" to TRUE */
					}
				}
				advancewindow();
				if (!parse_warn)
				{
					sv_opcode = svn_data[index].opcode;
					assert(SV_NUM_SV > sv_opcode);
					switch (sv_opcode)
					{
						case SV_TEST:
							*a = put_tref(newtriple(OC_GETTRUTH));
							break;
						case SV_ZCHSET:		/* making these literals at compile time assumes code is */
						case SV_ZRELDATE:	/* generated for/in the environment where it runs, which */
						case SV_ZVERSION:	/* relies on appropriate code management */
							op_svget(sv_opcode, &v);
							assert(MVTYPE_IS_NUMERIC(v.mvtype) && MVTYPE_IS_STRING(v.mvtype));
							ENSURE_STP_FREE_SPACE(v.str.len);
							memcpy((char *)stringpool.free, v.str.addr, v.str.len);
							v.str.addr = (char *)stringpool.free;
							stringpool.free += v.str.len;
							*a = put_lit(&v);
							break;
						case SV_X:
						case SV_Y:
							devctlexp = TRUE;		/* WARNING fallthrough */
						default:
							ref = newtriple(OC_SVGET);
							ref->operand[0] = put_ilit(sv_opcode);
							*a = put_tref(ref);
							break;
					}
				} else
					*a = put_lit((mval *)&literal_null);
				return TRUE;
			}
		}
		if (saw_se && (OLD_SE != TREF(side_effect_handling)))
		{
			assert(TREF(expr_depth) <= TREF(side_effect_depth));
			(TREF(side_effect_base))[TREF(expr_depth)] = TRUE;
		}
		functrip = t1 = a->oprval.tref;
		if (parse_warn || !(TREF(side_effect_base))[TREF(expr_depth)] || (NO_REF == functrip->operand[1].oprclass))
			return TRUE;	/* 1 argument gets a pass */
		switch (functrip->opcode)
		{
			case OC_EXFUN:		/* relies on protection from actuallist */
			case OC_EXTEXFUN:	/* relies on protection from actuallist */
			case OC_FNFGNCAL:	/* relies on protection from actuallist */
			case OC_EXFUNRET:	/* arguments are addresses/locations rather than values */
			case OC_FNGET:		/* $get() gets a pass because protects itself */
			case OC_FNINCR:		/* $increment() gets a pass because its ordering needs no protection */
			case OC_FNNEXT:		/* only has 1 arg, but uses 2 for lvn interface */
			case OC_FNORDER:	/* may have 1 or 2 args, internally uses 1 extra for lvn arg, but protects itself */
			case OC_FNZPREVIOUS:	/* only has 1 arg, but uses 2 for lvn interface */
			case OC_INDINCR:	/* $increment() gets a pass because its ordering needs no protection */
				return TRUE;
			default:
				break;
		}
		assert(0 < TREF(expr_depth));
		/* This block protects lvn evaluations in earlier arguments from changes caused by side effects in later
		 * arguments by capturing the prechange value in a temporary; coerce or preexisting temporary might already
		 * do the job and indirect local evaluations may already have shifted to occur earlier. This algorithm is similar
		 * to one in eval_expr for concatenation, but it must deal with possible arguments in both operands for
		 * both the initial triple and the last parameter triple, and the possibility of empty operand[0] in some
		 * functions so they have not been combined. We should have least one side effect (see compiler.h) and two
		 * arguments to bother - to know side effect, we have an array malloc'd and high water marked to avoid a limit
		 * on expression nesting depth, anchored by TREF(side_effect_base) and indexed by TREF(expr_depth) so
		 * ENCOUNTERED_SIDE_EFFECT can mark the prior level; f_select mallocs and free its own array
		  */
		assert(OLD_SE != TREF(side_effect_handling));
		funcbp = &functrip->backptr;		/* borrow backptr to track args */
		tripbp = (tbp *)mcalloc(SIZEOF(tbp));
		dqinit(tripbp, que);
		tripbp->bkptr = NULL;
		assert(NULL == funcbp->bkptr);
		assert((funcbp == funcbp->que.fl) && (funcbp == funcbp->que.bl));
		saw_se = saw_local = FALSE;
		for (argtrip = t1; ; argtrip = t1)
		{	/* work functrip,oprval.tref arguments forward */
			if (argtrip != functrip)
				tripbp = &argtrip->backptr;
			assert(NULL == tripbp->bkptr);
			for (j = argtrip->operand; j < ARRAYTOP(argtrip->operand); j++)
			{	/* process all (two) operands */
				t1 = j->oprval.tref;
				if (NO_REF == j->oprclass)
					continue;	/* some functions leave holes in their arguments */
				if (((ARRAYTOP(argtrip->operand) - 1) == j) && (TRIP_REF == j->oprclass)
						&& (OC_PARAMETER == t1->opcode))
					break;		/* only need to deal with last operand[1] */
				for (k = j; INDR_REF == k->oprclass; k = k->oprval.indr)
					;	/* INDR_REFs used by e.g. extrinsics finally end up at a TRIP_REF */
				if (TRIP_REF != k->oprclass)
					continue;			/* something else - not to worry */
				/* may need to step back past coerce of side effects */
				t3 = k->oprval.tref;
				t2 = (oc_tab[t3->opcode].octype & OCT_COERCE) ? t3->operand[0].oprval.tref : t3;
				if ((OC_VAR == t2->opcode) || (OC_GETINDX == t2->opcode))
				{	/* it's an lvn */
					if ((t3 != t2) || ((ARRAYTOP(argtrip->operand) - 1) == j))
						continue;		/* but if it's the last or there's a coerce */
					saw_local = TRUE;		/* left operand may need protection */
				}
				if (!saw_local)
					continue;			/* no local yet to worry about */
				saw_se = TRUE;
				if (NULL != tripbp->bkptr)
				{	/* this one's already flagged */
					assert((ARRAYTOP(argtrip->operand) - 1) == j);
					continue;
				}
				/* chain stores args to manage later insert of temps to hold left values */
				assert((tripbp == tripbp->que.fl) && (tripbp == tripbp->que.bl));
				tripbp->bkptr = argtrip;
				dqins(funcbp, que, tripbp);
			}
			if ((NULL == t1) || (OC_PARAMETER != t1->opcode))
				break;					/* end of arg list */
			assert(argtrip->operand[1].oprval.tref == t1);
		}
		if (!saw_se)						/* might have lucked out on ordering */
			saw_local = FALSE;				/* just clear the backptrs - shut off other processing */
		saw_se = FALSE;
		se_warn = SE_WARN_ON;
		dqloop(funcbp, que, tripbp)
		{	/* work chained arguments which are in reverse order */
			argtrip = tripbp->bkptr;
			assert(NULL != argtrip);
			dqdel(tripbp, que);
			tripbp->bkptr = NULL;
			if (!saw_local)
				continue;
			/* found some need to insert temps */
			for (j = &argtrip->operand[1]; j >= argtrip->operand; j--)
			{	/* match to the operand - usually 0 but have to cover 1 as well */
				for (k = j; INDR_REF == k->oprclass; k = k->oprval.indr)
					;	/* INDR_REFs used by e.g. extrinsics finally end up at a TRIP_REF */
				assert((TRIP_REF == k->oprclass) || (NO_REF == k->oprclass));
				t1 = k->oprval.tref;
				if ((NO_REF == k->oprclass) || (OC_PARAMETER == t1->opcode)
						|| (oc_tab[t1->opcode].octype & OCT_COERCE))
					continue;
				if ((OC_VAR == t1->opcode) || (OC_GETINDX == t1->opcode))
				{	/* have an operand that needs a temp because threat from some side effect */
					ref = maketriple(OC_STOTEMP);
					ref->operand[0] = put_tref(t1);
					dqins(t1, exorder, ref); 	/* NOTE:this violates infomation hiding */
					k->oprval.tref = ref;
					if (se_warn)
						ISSUE_SIDEEFFECTEVAL_WARNING(t1->src.column + 1);
				} else
					saw_se = TRUE;
			}
		}
		assert((funcbp == funcbp->que.fl) && (funcbp == funcbp->que.bl) && (NULL == funcbp->bkptr));
		return TRUE;	/* end of order of evaluation processing for functions*/
	case TK_COLON:
		stx_error(ERR_EXPR);
		return FALSE;
	}	/* case default: intentionally omitted as it simply uses the below return FALSE */
	return FALSE;
}
