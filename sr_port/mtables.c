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

#include "compiler.h"
#include "opcode.h"
#include "toktyp.h"
#include "rtnhdr.h"
#include "mv_stent.h"
#include "release_name.h"

LITDEF char ctypetab[NUM_CHARS] = {
	/* ASCII 0-127 */
	TK_EOL,    TK_ERROR,        TK_ERROR,    TK_ERROR,     TK_ERROR,     TK_ERROR,    TK_ERROR,      TK_ERROR,
	TK_ERROR,  TK_SPACE,        TK_ERROR,    TK_ERROR,     TK_EOR,       TK_ERROR,    TK_ERROR,      TK_ERROR,
	TK_ERROR,  TK_ERROR,        TK_ERROR,    TK_ERROR,     TK_ERROR,     TK_ERROR,    TK_ERROR,      TK_ERROR,
	TK_ERROR,  TK_ERROR,        TK_ERROR,    TK_ERROR,     TK_ERROR,     TK_ERROR,    TK_ERROR,      TK_ERROR,
	TK_SPACE,  TK_EXCLAIMATION, TK_QUOTE,    TK_HASH,      TK_DOLLAR,    TK_PERCENT,  TK_AMPERSAND,  TK_APOSTROPHE,
	TK_LPAREN, TK_RPAREN,       TK_ASTERISK, TK_PLUS,      TK_COMMA,     TK_MINUS,    TK_PERIOD,     TK_SLASH,
	TK_DIGIT,  TK_DIGIT,        TK_DIGIT,    TK_DIGIT,     TK_DIGIT,     TK_DIGIT,    TK_DIGIT,      TK_DIGIT,
	TK_DIGIT,  TK_DIGIT,        TK_COLON,    TK_SEMICOLON, TK_LESS,      TK_EQUAL,    TK_GREATER,    TK_QUESTION,
	TK_ATSIGN, TK_UPPER,        TK_UPPER,    TK_UPPER,     TK_UPPER,     TK_UPPER,    TK_UPPER,      TK_UPPER,
	TK_UPPER,  TK_UPPER,        TK_UPPER,    TK_UPPER,     TK_UPPER,     TK_UPPER,    TK_UPPER,      TK_UPPER,
	TK_UPPER,  TK_UPPER,        TK_UPPER,    TK_UPPER,     TK_UPPER,     TK_UPPER,    TK_UPPER,      TK_UPPER,
	TK_UPPER,  TK_UPPER,        TK_UPPER,    TK_LBRACKET,  TK_BACKSLASH, TK_RBRACKET, TK_CIRCUMFLEX, TK_UNDERSCORE,
	TK_ERROR,  TK_LOWER,        TK_LOWER,    TK_LOWER,     TK_LOWER,     TK_LOWER,    TK_LOWER,      TK_LOWER,
	TK_LOWER,  TK_LOWER,        TK_LOWER,    TK_LOWER,     TK_LOWER,     TK_LOWER,    TK_LOWER,      TK_LOWER,
	TK_LOWER,  TK_LOWER,        TK_LOWER,    TK_LOWER,     TK_LOWER,     TK_LOWER,    TK_LOWER,      TK_LOWER,
	TK_LOWER,  TK_LOWER,        TK_LOWER,    TK_ERROR,     TK_VBAR,      TK_ERROR,    TK_ERROR,      TK_ERROR,
	/* non-ASCII 128-255 */
	TK_ERROR,  TK_ERROR,        TK_ERROR,    TK_ERROR,     TK_ERROR,     TK_ERROR,    TK_ERROR,      TK_ERROR,
	TK_ERROR,  TK_ERROR,        TK_ERROR,    TK_ERROR,     TK_ERROR,     TK_ERROR,    TK_ERROR,      TK_ERROR,
	TK_ERROR,  TK_ERROR,        TK_ERROR,    TK_ERROR,     TK_ERROR,     TK_ERROR,    TK_ERROR,      TK_ERROR,
	TK_ERROR,  TK_ERROR,        TK_ERROR,    TK_ERROR,     TK_ERROR,     TK_ERROR,    TK_ERROR,      TK_ERROR,
	TK_ERROR,  TK_ERROR,        TK_ERROR,    TK_ERROR,     TK_ERROR,     TK_ERROR,    TK_ERROR,      TK_ERROR,
	TK_ERROR,  TK_ERROR,        TK_ERROR,    TK_ERROR,     TK_ERROR,     TK_ERROR,    TK_ERROR,      TK_ERROR,
	TK_ERROR,  TK_ERROR,        TK_ERROR,    TK_ERROR,     TK_ERROR,     TK_ERROR,    TK_ERROR,      TK_ERROR,
	TK_ERROR,  TK_ERROR,        TK_ERROR,    TK_ERROR,     TK_ERROR,     TK_ERROR,    TK_ERROR,      TK_ERROR,
	TK_ERROR,  TK_ERROR,        TK_ERROR,    TK_ERROR,     TK_ERROR,     TK_ERROR,    TK_ERROR,      TK_ERROR,
	TK_ERROR,  TK_ERROR,        TK_ERROR,    TK_ERROR,     TK_ERROR,     TK_ERROR,    TK_ERROR,      TK_ERROR,
	TK_ERROR,  TK_ERROR,        TK_ERROR,    TK_ERROR,     TK_ERROR,     TK_ERROR,    TK_ERROR,      TK_ERROR,
	TK_ERROR,  TK_ERROR,        TK_ERROR,    TK_ERROR,     TK_ERROR,     TK_ERROR,    TK_ERROR,      TK_ERROR,
	TK_ERROR,  TK_ERROR,        TK_ERROR,    TK_ERROR,     TK_ERROR,     TK_ERROR,    TK_ERROR,      TK_ERROR,
	TK_ERROR,  TK_ERROR,        TK_ERROR,    TK_ERROR,     TK_ERROR,     TK_ERROR,    TK_ERROR,      TK_ERROR,
	TK_ERROR,  TK_ERROR,        TK_ERROR,    TK_ERROR,     TK_ERROR,     TK_ERROR,    TK_ERROR,      TK_ERROR,
	TK_ERROR,  TK_ERROR,        TK_ERROR,    TK_ERROR,     TK_ERROR,     TK_ERROR,    TK_ERROR,      TK_ERROR
};

#define tokdef(n,b,u,t) { n , b , u , t }

LITDEF toktabtype tokentable[] =
{
	tokdef("NULL",0,0,0),
	tokdef("TK_ERROR",0,0,0),
	tokdef("TK_EOL",0,0,0),
	tokdef("TK_EOR",0,0,0),
	tokdef("TK_SPACE",0,0,0),
	tokdef("TK_ATSIGN",0,0,0),
	tokdef("TK_IDENT",0,0,0),
	tokdef("TK_NUMLIT",0,0,0),
	tokdef("TK_INTLIT",0,0,0),
	tokdef("TK_CIRCUMFLEX",0,0,0),
	tokdef("TK_COMMA",0,0,0),
	tokdef("TK_LPAREN",0,0,0),
	tokdef("TK_RPAREN",0,0,0),
	tokdef("TK_PLUS",OC_ADD,OC_FORCENUM,OCT_MVAL),
	tokdef("TK_MINUS",OC_SUB,OC_NEG,OCT_MVAL),
	tokdef("TK_ASTERISK",OC_MUL,0,OCT_MVAL),
	tokdef("TK_SLASH",OC_DIV,0,OCT_MVAL),
	tokdef("TK_BACKSLASH",OC_IDIV,0,OCT_MVAL),
	tokdef("TK_UNDERSCORE",OC_CAT,0,OCT_MVAL),
	tokdef("TK_HASH",OC_MOD,0,OCT_MVAL),
	tokdef("TK_APOSTROPHE",0,OC_COM,OCT_BOOL),
	tokdef("TK_COLON",0,0,0),
	tokdef("TK_QUOTE",0,0,0),
	tokdef("TK_EQUAL",OC_EQU,0,OCT_MVAL),
	tokdef("TK_GREATER",OC_GT,0,OCT_MVAL),
	tokdef("TK_LESS",OC_LT,0,OCT_MVAL),
	tokdef("TK_LBRACKET",OC_CONTAIN,0,OCT_MVAL),
	tokdef("TK_RBRACKET",OC_FOLLOW,0,OCT_MVAL),
	tokdef("TK_QUESTION",OC_PATTERN,0,OCT_MVAL),
	tokdef("TK_AMPERSAND",OC_AND,0,OCT_BOOL),
	tokdef("TK_EXCLAIMATION",OC_OR,0,OCT_BOOL),
	tokdef("TK_NEQUAL",OC_NEQU,0,OCT_MVAL),
	tokdef("TK_NGREATER",OC_NGT,0,OCT_MVAL),
	tokdef("TK_NLESS",OC_NLT,0,OCT_MVAL),
	tokdef("TK_NLBRACKET",OC_NCONTAIN,0,OCT_MVAL),
	tokdef("TK_NRBRACKET",OC_NFOLLOW,0,OCT_MVAL),
	tokdef("TK_NQUESTION",OC_NPATTERN,0,OCT_MVAL),
	tokdef("TK_NAMPERSAND",OC_NAND,0,OCT_BOOL),
	tokdef("TK_NEXCLAIMATION",OC_NOR,0,OCT_BOOL),
	tokdef("TK_PERCENT",0,0,0),
	tokdef("TK_UPPER",0,0,0),
	tokdef("TK_LOWER",0,0,0),
	tokdef("TK_PERIOD",0,0,0),
	tokdef("TK_DIGIT",0,0,0),
	tokdef("TK_DOLLAR",0,0,0),
	tokdef("TK_SEMICOLON",0,0,0),
	tokdef("TK_STRLIT",0,0,0),
	tokdef("TK_VBAR",0,0,0),
	tokdef("TK_EXPONENT",OC_EXP,0,OCT_MVAL),
	tokdef("TK_SORTS_AFTER",OC_SORTS_AFTER,0,OCT_MVAL),
	tokdef("TK_NSORTS_AFTER",OC_NSORTS_AFTER,0,OCT_MVAL)
};

GBLREF mv_stent *mv_chain;
LITDEF unsigned char mvs_size[] =
	{
		MV_SIZE(mvs_msav),
		MV_SIZE(mvs_mval),
		MV_SIZE(mvs_stab),
		MV_SIZE(mvs_iarr),
		MV_SIZE(mvs_ntab),
		MV_SIZE(mvs_parm),
		MV_SIZE(mvs_pval),
		MV_SIZE(mvs_stck),
		MV_SIZE(mvs_nval),
		MV_SIZE(mvs_tval),
		MV_SIZE(mvs_tp_holder),
		MV_SIZE(mvs_zintr)
	};

static readonly unsigned char localpool[3] = {'1' , '1' , '0'};
LITDEF mval literal_null 	= DEFINE_MVAL_LITERAL(MV_STR | MV_NM | MV_INT | MV_NUM_APPROX , 0 , 0 , 0 ,  0  , 0 , 0 );
LITDEF mval literal_zero 	= DEFINE_MVAL_LITERAL(MV_STR | MV_NM | MV_INT, 0 , 0 , 1 , (char *) &localpool[2] , 0 , 0 );
LITDEF mval literal_one 	= DEFINE_MVAL_LITERAL(MV_STR | MV_NM | MV_INT, 0 , 0 , 1 , (char *) &localpool[1] , 0 ,  1000 );
LITDEF mval literal_ten 	= DEFINE_MVAL_LITERAL(MV_STR | MV_NM | MV_INT, 0 , 0 , 2 , (char *) &localpool[1] , 0 , 10000 );
LITDEF mval literal_eleven 	= DEFINE_MVAL_LITERAL(MV_STR | MV_NM | MV_INT, 0 , 0 , 2 , (char *) &localpool[0] , 0 , 11000 );
LITDEF mval SBS_MVAL_INT_ELE 	= DEFINE_MVAL_LITERAL(MV_NM | MV_INT, 0 , 0 , 0, 0, 0 ,30*1000);

LITDEF mval *fndata_table[2][2] =
	{
		{&literal_zero, &literal_one},
		{&literal_ten, &literal_eleven}
	};

LITDEF mval *fnzqgblmod_table[2] =
	{
		&literal_zero, &literal_one,
	};

LITDEF char gtm_release_name[]   = GTM_RELEASE_NAME;
LITDEF int4 gtm_release_name_len = sizeof(GTM_RELEASE_NAME) - 1;
LITDEF char gtm_product[]        = GTM_PRODUCT;
LITDEF int4 gtm_product_len      = sizeof(GTM_PRODUCT) - 1;
LITDEF char gtm_version[]        = GTM_VERSION;
LITDEF int4 gtm_version_len      = sizeof(GTM_VERSION) - 1;

/* Indexed by enum db_ver in gdsdbver.h. Note that a db_ver value can be -1 but only in
   the internal context of incremental/stream backup so the value should never appear where
   this table is being indexed (suggest asserts for index being > 0 at usage points).
*/
LITDEF char *gtm_dbversion_table[] =
{
	"V4",
	"V5"
};

LITDEF int4 ten_pwr[10] = { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000 } ;
LITDEF unsigned char lower_to_upper_table[] =
{
0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,
31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,
61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,
91,92,93,94,95,96,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,
123,124,125,126,127,128,129,
130,131,132,133,134,135,136,137,138,139,
140,141,142,143,144,145,146,147,148,149,
150,151,152,153,154,155,156,157,158,159,
160,161,162,163,164,165,166,167,168,169,
170,171,172,173,174,175,176,177,178,179,
180,181,182,183,184,185,186,187,188,189,
190,191,192,193,194,195,196,197,198,199,
200,201,202,203,204,205,206,207,208,209,
210,211,212,213,214,215,216,217,218,219,
220,221,222,223,224,225,226,227,228,229,
230,231,232,233,234,235,236,237,238,239,
240,241,242,243,244,245,246,247,248,249,
250,251,252,253,254,255
};

#ifdef __MVS__
LITDEF unsigned char ebcdic_lower_to_upper_table[] =
{
/*	EBCDIC	*/
0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,
31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,
61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,
91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,
/*									129,130*/
111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,193,194,
/*131,132,133,134,135,136,137	*/
195,196,197,198,199,200,201,138,139,140,
141,142,143,144,209,210,211,212,213,214,
215,216,217,154,155,156,157,158,159,160,
161,226,227,228,229,230,231,232,233,
170,171,172,173,174,175,176,177,178,179,
180,181,182,183,184,185,186,187,188,189,
190,191,192,193,194,195,196,197,198,199,
200,201,202,203,204,205,206,207,208,209,
210,211,212,213,214,215,216,217,218,219,
220,221,222,223,224,225,226,227,228,229,
230,231,232,233,234,235,236,237,238,239,
240,241,242,243,244,245,246,247,248,249,
250,251,252,253,254,255
};
#endif

LITDEF unsigned char upper_to_lower_table[] =
{
0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,
31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,
61,62,63,64,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,
91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,
123,124,125,126,127,128,129,
130,131,132,133,134,135,136,137,138,139,
140,141,142,143,144,145,146,147,148,149,
150,151,152,153,154,155,156,157,158,159,
160,161,162,163,164,165,166,167,168,169,
170,171,172,173,174,175,176,177,178,179,
180,181,182,183,184,185,186,187,188,189,
190,191,192,193,194,195,196,197,198,199,
200,201,202,203,204,205,206,207,208,209,
210,211,212,213,214,215,216,217,218,219,
220,221,222,223,224,225,226,227,228,229,
230,231,232,233,234,235,236,237,238,239,
240,241,242,243,244,245,246,247,248,249,
250,251,252,253,254,255
};

#ifdef DEBUG
/* These instructions follow the definitions made
   in vxi.h and were generated from them. Where
   skips were made in the definitions, the string
   "*N/A*  " is placed in the table.
*/
LITDEF char vxi_opcode[][6] = {
	 "HALT  "
	,"NOP   "
	,"REI   "
	,"BPT   "
	,"RET   "
	,"RSB   "
	,"LDPCTX"
	,"SVPCTX"
	,"CVTPS "
	,"CVTSP "
	,"INDEX "
	,"CRC   "
	,"PROBER"
	,"PROBEW"
	,"INSQUE"
	,"REMQUE"
	,"BSBB  "
	,"BRB   "
	,"BNEQ  "
	,"BEQL  "
	,"BGTR  "
	,"BLEQ  "
	,"JSB   "
	,"JMP   "
	,"BGEQ  "
	,"BLSS  "
	,"BGTRU "
	,"BLEQU "
	,"BVC   "
	,"BVS   "
	,"BGEQU "
	,"BLSSU "
	,"ADDP4 "
	,"ADDP6 "
	,"SUBP4 "
	,"SUBP6 "
	,"CVTPT "
	,"MULP  "
	,"CVTTP "
	,"DIVP  "
	,"MOVC3 "
	,"CMPC3 "
	,"SCANC "
	,"SPANC "
	,"MOVC5 "
	,"CMPC5 "
	,"MOVTC "
	,"MOVTUC"
	,"BSBW  "
	,"BRW   "
	,"CVTWL "
	,"CVTWB "
	,"MOVP  "
	,"CMPP3 "
	,"CVTPL "
	,"CMPP4 "
	,"EDITPC"
	,"MATCHC"
	,"LOCC  "
	,"SKPC  "
	,"MOVZWL"
	,"ACBW  "
	,"MOVAW "
	,"PUSHAW"
	,"ADDF2 "
	,"ADDF3 "
	,"SUBF2 "
	,"SUBF3 "
	,"MULF2 "
	,"MULF3 "
	,"DIVF2 "
	,"DIVF3 "
	,"CVTFB "
	,"CVTFW "
	,"CVTFL "
	,"CVTRFL"
	,"CVTBF "
	,"CVTWF "
	,"CVTLF "
	,"ACBF  "
	,"MOVF  "
	,"CMPF  "
	,"MNEGF "
	,"TSTF  "
	,"EMODF "
	,"POLYF "
	,"CVTFD "
	,"*N/A* "
	,"ADAWI "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"INSQHI"
	,"INSQTI"
	,"REMQHI"
	,"REMQTI"
	,"ADDD2 "
	,"ADDD3 "
	,"SUBD2 "
	,"SUBD3 "
	,"MULD2 "
	,"MULD3 "
	,"DIVD2 "
	,"DIVD3 "
	,"CVTDB "
	,"CVTDW "
	,"CVTDL "
	,"CVTRDL"
	,"CVTBD "
	,"CVTWD "
	,"CVTLD "
	,"ACBD  "
	,"MOVD  "
	,"CMPD  "
	,"MNEGD "
	,"TSTD  "
	,"EMODD "
	,"POLYD "
	,"CVTDF "
	,"*N/A* "
	,"ASHL  "
	,"ASHQ  "
	,"EMUL  "
	,"EDIV  "
	,"CLRQ  "
	,"MOVQ  "
	,"MOVAQ "
	,"PUSHAQ"
	,"ADDB2 "
	,"ADDB3 "
	,"SUBB2 "
	,"SUBB3 "
	,"MULB2 "
	,"MULB3 "
	,"DIVB2 "
	,"DIVB3 "
	,"BISB2 "
	,"BISB3 "
	,"BICB2 "
	,"BICB3 "
	,"XORB2 "
	,"XORB3 "
	,"MNEGB "
	,"CASEB "
	,"MOVB  "
	,"CMPB  "
	,"MCOMB "
	,"BITB  "
	,"CLRB  "
	,"TSTB  "
	,"INCB  "
	,"DECB  "
	,"CVTBL "
	,"CVTBW"
	,"MOVZBL"
	,"MOVZBW"
	,"ROTL  "
	,"ACBB  "
	,"MOVAB "
	,"PUSHAB"
	,"ADDW2 "
	,"ADDW3 "
	,"SUBW2 "
	,"SUBW3 "
	,"MULW2 "
	,"MULW3 "
	,"DIVW2 "
	,"DIVW3 "
	,"BISW2 "
	,"BISW3 "
	,"BICW2 "
	,"BICW3 "
	,"XORW2 "
	,"XORW3 "
	,"MNEGW "
	,"CASEW "
	,"MOVW  "
	,"CMPW  "
	,"MCOMW "
	,"BITW  "
	,"CLRW  "
	,"TSTW  "
	,"INCW  "
	,"DECW  "
	,"BISPSW"
	,"BICPSW"
	,"POPR  "
	,"PUSHR "
	,"CHMK  "
	,"CHME  "
	,"CHMS  "
	,"CHMU  "
	,"ADDL2 "
	,"ADDL3 "
	,"SUBL2 "
	,"SUBL3 "
	,"MULL2 "
	,"MULL3 "
	,"DIVL2 "
	,"DIVL3 "
	,"BISL2 "
	,"BISL3 "
	,"BICL2 "
	,"BICL3 "
	,"XORL2 "
	,"XORL3 "
	,"MNEGL "
	,"CASEL "
	,"MOVL  "
	,"CMPL  "
	,"MCOML "
	,"BITL  "
	,"CLRL  "
	,"TSTL  "
	,"INCL  "
	,"DECL  "
	,"ADWC  "
	,"SBWC  "
	,"MTPR  "
	,"MFPR  "
	,"MOVPSL"
	,"PUSHL "
	,"MOVAL "
	,"PUSHAL"
	,"BBS   "
	,"BBC   "
	,"BBSS  "
	,"BBCS  "
	,"BBSC  "
	,"BBCC  "
	,"BBSSI "
	,"BBCCI "
	,"BLBS  "
	,"BLBC  "
	,"FFS   "
	,"FFC   "
	,"CMPV  "
	,"CMPZV "
	,"EXTV  "
	,"EXTZV "
	,"INSV  "
	,"ACBL  "
	,"AOBLSS"
	,"AOBLEQ"
	,"SOBGEQ"
	,"SOBGTR"
	,"CVTLB "
	,"CVTLW "
	,"ASHP  "
	,"CVTLP "
	,"CALLG "
	,"CALLS "
	,"XFC   "
	,"ESCD  "
	,"ESCE  "
	,"ESCF  "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"CVTDH "
	,"CVTGF "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"ADDG2 "
	,"ADDG3 "
	,"SUBG2 "
	,"SUBG3 "
	,"MULG2 "
	,"MULG3 "
	,"DIVG2 "
	,"DIVG3 "
	,"CVTGB "
	,"CVTGW "
	,"CVTGL "
	,"CVTRGL"
	,"CVTBG "
	,"CVTWG "
	,"CVTLG "
	,"ACBG  "
	,"MOVG  "
	,"CMPG  "
	,"MNEGG "
	,"TSTG  "
	,"EMODG "
	,"POLYG "
	,"CVTGH "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"ADDH2 "
	,"ADDH3 "
	,"SUBH2 "
	,"SUBH3 "
	,"MULH2 "
	,"MULH3 "
	,"DIVH2 "
	,"DIVH3 "
	,"CVTHB "
	,"CVTHW "
	,"CVTHL "
	,"CVTRHL"
	,"CVTBH "
	,"CVTWH "
	,"CVTLH "
	,"ACBH  "
	,"MOVH  "
	,"CMPH  "
	,"MNEGH "
	,"TSTH  "
	,"EMODH "
	,"POLYH "
	,"CVTHG "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"CLRO  "
	,"MOVO  "
	,"MOVAO "
	,"PUSHAO"
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"CVTFH "
	,"CVTFG "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"*N/A* "
	,"CVTHF "
	,"CVTHD "
};

#endif
