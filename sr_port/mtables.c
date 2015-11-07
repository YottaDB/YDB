/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include <rtnhdr.h>
#include "mv_stent.h"
#include "release_name.h"
#include "gdsroot.h"		/* needed for tp.h & gv_trigger.h */
#include "gdsbt.h"		/* needed for tp.h & gv_trigger.h */
#include "gtm_facility.h"	/* needed for tp.h & gv_trigger.h */
#include "fileinfo.h"		/* needed for tp.h & gv_trigger.h */
#include "gdsfhead.h"		/* needed for tp.h & gv_trigger.h */
#include "filestruct.h"		/* needed for tp.h & gv_trigger.h */
#include "gdscc.h"		/* needed for tp.h & gv_trigger.h */
#include "gdskill.h"		/* needed for tp.h & gv_trigger.h */
#include "jnl.h"		/* needed for tp.h & gv_trigger.h */
#include "buddy_list.h"		/* needed for tp.h & gv_trigger.h */
#include "hashtab_int4.h"	/* needed for tp.h & gv_trigger.h */
#include "tp.h"
#include "gtmimagename.h"
#include "arit.h"
#include "gtm_conv.h"
#include "gtm_caseconv.h"
#ifdef GTM_TRIGGER
# include "trigger.h"
# include "gv_trigger.h"
#endif
#include "mtables.h"

LITDEF char ctypetab[NUM_CHARS] =
{
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

#define tokdef(n, b, u, t) {n, b, u, t}

LITDEF toktabtype tokentable[] =
{
	tokdef("NULL", 0, 0, 0),
	tokdef("TK_ERROR", 0, 0, 0),
	tokdef("TK_EOL", 0, 0, 0),
	tokdef("TK_EOR", 0, 0, 0),
	tokdef("TK_SPACE", 0, 0, 0),
	tokdef("TK_ATSIGN", 0, 0, 0),
	tokdef("TK_IDENT", 0, 0, 0),
	tokdef("TK_NUMLIT", 0, 0, 0),
	tokdef("TK_INTLIT", 0, 0, 0),
	tokdef("TK_CIRCUMFLEX", 0, 0, 0),
	tokdef("TK_COMMA", 0, 0, 0),
	tokdef("TK_LPAREN", 0, 0, 0),
	tokdef("TK_RPAREN", 0, 0, 0),
	tokdef("TK_PLUS", OC_ADD, OC_FORCENUM, OCT_MVAL),
	tokdef("TK_MINUS", OC_SUB, OC_NEG, OCT_MVAL),
	tokdef("TK_ASTERISK", OC_MUL, 0, OCT_MVAL),
	tokdef("TK_SLASH", OC_DIV, 0, OCT_MVAL),
	tokdef("TK_BACKSLASH", OC_IDIV, 0, OCT_MVAL),
	tokdef("TK_UNDERSCORE", OC_CAT, 0, OCT_MVAL),
	tokdef("TK_HASH", OC_MOD, 0, OCT_MVAL),
	tokdef("TK_APOSTROPHE", 0, OC_COM, OCT_BOOL),
	tokdef("TK_COLON", 0, 0, 0),
	tokdef("TK_QUOTE", 0, 0, 0),
	tokdef("TK_EQUAL", OC_EQU, 0, OCT_MVAL),
	tokdef("TK_GREATER", OC_GT, 0, OCT_MVAL),
	tokdef("TK_LESS", OC_LT, 0, OCT_MVAL),
	tokdef("TK_LBRACKET", OC_CONTAIN, 0, OCT_MVAL),
	tokdef("TK_RBRACKET", OC_FOLLOW, 0, OCT_MVAL),
	tokdef("TK_QUESTION", OC_PATTERN, 0, OCT_MVAL),
	tokdef("TK_AMPERSAND", OC_AND, 0, OCT_BOOL),
	tokdef("TK_EXCLAIMATION", OC_OR, 0, OCT_BOOL),
	tokdef("TK_NEQUAL", OC_NEQU, 0, OCT_MVAL),
	tokdef("TK_NGREATER", OC_NGT, 0, OCT_MVAL),
	tokdef("TK_NLESS", OC_NLT, 0, OCT_MVAL),
	tokdef("TK_NLBRACKET", OC_NCONTAIN, 0, OCT_MVAL),
	tokdef("TK_NRBRACKET", OC_NFOLLOW, 0, OCT_MVAL),
	tokdef("TK_NQUESTION", OC_NPATTERN, 0, OCT_MVAL),
	tokdef("TK_NAMPERSAND", OC_NAND, 0, OCT_BOOL),
	tokdef("TK_NEXCLAIMATION", OC_NOR, 0, OCT_BOOL),
	tokdef("TK_PERCENT", 0, 0, 0),
	tokdef("TK_UPPER", 0, 0, 0),
	tokdef("TK_LOWER", 0, 0, 0),
	tokdef("TK_PERIOD", 0, 0, 0),
	tokdef("TK_DIGIT", 0, 0, 0),
	tokdef("TK_DOLLAR", 0, 0, 0),
	tokdef("TK_SEMICOLON", 0, 0, 0),
	tokdef("TK_STRLIT", 0, 0, 0),
	tokdef("TK_VBAR", 0, 0, 0),
	tokdef("TK_EXPONENT", OC_EXP, 0, OCT_MVAL),
	tokdef("TK_SORTS_AFTER", OC_SORTS_AFTER, 0, OCT_MVAL),
	tokdef("TK_NSORTS_AFTER", OC_NSORTS_AFTER, 0, OCT_MVAL),
	tokdef("TK_ATHASH", 0, 0, 0)
};

GBLREF mv_stent *mv_chain;	/* Needed for MV_SIZE macro */
LITDEF unsigned char mvs_size[] =
{
	MV_SIZE(mvs_msav),
	MV_SIZE(mvs_mval),
	MV_SIZE(mvs_stab),
	MV_SIZE(mvs_iarr),
	MV_SIZE(mvs_ntab),
	MV_SIZE(mvs_zintcmd),
	MV_SIZE(mvs_pval),
	MV_SIZE(mvs_stck),
	MV_SIZE(mvs_nval),
	MV_SIZE(mvs_tval),
	MV_SIZE(mvs_tp_holder),
	MV_SIZE(mvs_zintr),
	MV_SIZE(mvs_zintdev),
	MV_SIZE(mvs_stck),
	MV_SIZE(mvs_lvval),
	MV_SIZE(mvs_trigr),
	MV_SIZE(mvs_rstrtpc),
	MV_SIZE(mvs_storig),
	MV_SIZE(mvs_mrgzwrsv)
};

/* All mv_stent types that need to be preserved are indicated by the mvs_save[] array.
 * MVST_STCK_SP (which is the same as the MVST_STCK type everywhere else) is handled specially here.
 * The MVST_STCK_SP entry is created by mdb_condition_handler to stack the "stackwarn" global variable.
 * This one needs to be preserved since our encountering this type in flush_jmp.c indicates that we are currently
 * in the error-handler of a STACKCRIT error which in turn has "GOTO ..." in the $ZTRAP/$ETRAP that is
 * causing us to mutate the current frame we are executing in with the contents pointed to by the GOTO.
 * In that case, we do not want to restore "stackwarn" to its previous value since we are still handling
 * the STACKCRIT error. So we set MVST_STCK_SP type as needing to be preserved.
 */
LITDEF boolean_t mvs_save[] =
{
	TRUE,	/* MVST_MSAV */
	FALSE,	/* MVST_MVAL */
	TRUE,	/* MVST_STAB */
	FALSE,	/* MVST_IARR */
	TRUE,	/* MVST_NTAB */
	TRUE,	/* MVST_ZINTCMD */
	TRUE,	/* MVST_PVAL */
	FALSE,	/* MVST_STCK */
	TRUE,	/* MVST_NVAL */
	TRUE,	/* MVST_TVAL */
	TRUE,	/* MVST_TPHOLD */
	TRUE,	/* MVST_ZINTR */
	TRUE,	/* MVST_ZINTDEV */
	TRUE,	/* MVST_STCK_SP */
	TRUE,	/* MVST_LVAL */
	FALSE,	/* MVST_TRIGR */
	FALSE,	/* MVST_RSTRTPC */
	TRUE,	/* MVST_STORIG */
	FALSE	/* MVST_MRGZWRSV */
};

static readonly unsigned char localpool[7] = {'1', '1', '1', '0', '1', '0', '0'};
LITDEF mval literal_null	= DEFINE_MVAL_LITERAL(MV_STR | MV_NM | MV_INT | MV_NUM_APPROX | MV_UTF_LEN, 0, 0, 0, 0, 0, 0);
LITDEF mval literal_zero	= DEFINE_MVAL_LITERAL(MV_STR | MV_NM | MV_INT, 0, 0, 1, (char *)&localpool[3], 0,   0);
LITDEF mval literal_one 	= DEFINE_MVAL_LITERAL(MV_STR | MV_NM | MV_INT, 0, 0, 1, (char *)&localpool[0], 0,   1 * MV_BIAS);
LITDEF mval literal_ten 	= DEFINE_MVAL_LITERAL(MV_STR | MV_NM | MV_INT, 0, 0, 2, (char *)&localpool[2], 0,  10 * MV_BIAS);
LITDEF mval literal_eleven	= DEFINE_MVAL_LITERAL(MV_STR | MV_NM | MV_INT, 0, 0, 2, (char *)&localpool[0], 0,  11 * MV_BIAS);
LITDEF mval literal_oneohoh	= DEFINE_MVAL_LITERAL(MV_STR | MV_NM | MV_INT, 0, 0, 3, (char *)&localpool[4], 0, 100 * MV_BIAS);
LITDEF mval literal_oneohone	= DEFINE_MVAL_LITERAL(MV_STR | MV_NM | MV_INT, 0, 0, 3, (char *)&localpool[2], 0, 101 * MV_BIAS);
LITDEF mval literal_oneten	= DEFINE_MVAL_LITERAL(MV_STR | MV_NM | MV_INT, 0, 0, 3, (char *)&localpool[1], 0, 110 * MV_BIAS);
LITDEF mval literal_oneeleven	= DEFINE_MVAL_LITERAL(MV_STR | MV_NM | MV_INT, 0, 0, 3, (char *)&localpool[0], 0, 111 * MV_BIAS);

/* --------------------------------------------------------------------------------------------------------------------------
 * All string mvals defined in this module using LITDEF need to have MV_NUM_APPROX bit set. This is because these mval
 * literals will most likely go into a read-only data segment of the executable and if ever they get passed into mval2subsc
 * (e.g. &literal_hashlabel is passed in gvtr_get_hasht_gblsubs using COPY_SUBS_TO_GVCURRKEY macro), it would otherwise
 * try to set the MV_NUM_APPROX bit and that could cause a SIG-11 since these mvals are in the read-only data segment.
 * --------------------------------------------------------------------------------------------------------------------------
 */

/* Create mval to hold batch type TSTART. "BA" or "BATCH" mean the same.
 * We define the shorter version here to try reduce the time taken for comparison.
 */
LITDEF mval literal_batch       = DEFINE_MVAL_LITERAL(MV_STR | MV_NUM_APPROX, 0, 0, TP_BATCH_SHRT, (char *)TP_BATCH_ID, 0, 0);

#ifdef GTM_TRIGGER
LITDEF mval literal_hasht	= DEFINE_MVAL_LITERAL(MV_STR | MV_NUM_APPROX, 0, 0, HASHT_GBLNAME_LEN    , (char *)HASHT_GBLNAME    , 0, 0);	/* BYPASSOK */
LITDEF mval literal_hashlabel	= DEFINE_MVAL_LITERAL(MV_STR | MV_NUM_APPROX, 0, 0, LITERAL_HASHLABEL_LEN, (char *)LITERAL_HASHLABEL, 0, 0);	/* BYPASSOK */
LITDEF mval literal_hashcycle	= DEFINE_MVAL_LITERAL(MV_STR | MV_NUM_APPROX, 0, 0, LITERAL_HASHCYCLE_LEN, (char *)LITERAL_HASHCYCLE, 0, 0);	/* BYPASSOK */
LITDEF mval literal_hashcount	= DEFINE_MVAL_LITERAL(MV_STR | MV_NUM_APPROX, 0, 0, LITERAL_HASHCOUNT_LEN, (char *)LITERAL_HASHCOUNT, 0, 0);	/* BYPASSOK */
LITDEF mval literal_cmd		= DEFINE_MVAL_LITERAL(MV_STR | MV_NUM_APPROX, 0, 0, LITERAL_CMD_LEN      , (char *)LITERAL_CMD      , 0, 0);	/* BYPASSOK */
LITDEF mval literal_gvsubs	= DEFINE_MVAL_LITERAL(MV_STR | MV_NUM_APPROX, 0, 0, LITERAL_GVSUBS_LEN   , (char *)LITERAL_GVSUBS   , 0, 0);	/* BYPASSOK */
LITDEF mval literal_options	= DEFINE_MVAL_LITERAL(MV_STR | MV_NUM_APPROX, 0, 0, LITERAL_OPTIONS_LEN  , (char *)LITERAL_OPTIONS  , 0, 0);	/* BYPASSOK */
LITDEF mval literal_delim	= DEFINE_MVAL_LITERAL(MV_STR | MV_NUM_APPROX, 0, 0, LITERAL_DELIM_LEN    , (char *)LITERAL_DELIM    , 0, 0);	/* BYPASSOK */
LITDEF mval literal_zdelim	= DEFINE_MVAL_LITERAL(MV_STR | MV_NUM_APPROX, 0, 0, LITERAL_ZDELIM_LEN   , (char *)LITERAL_ZDELIM   , 0, 0);	/* BYPASSOK */
LITDEF mval literal_pieces	= DEFINE_MVAL_LITERAL(MV_STR | MV_NUM_APPROX, 0, 0, LITERAL_PIECES_LEN   , (char *)LITERAL_PIECES   , 0, 0);	/* BYPASSOK */
LITDEF mval literal_trigname	= DEFINE_MVAL_LITERAL(MV_STR | MV_NUM_APPROX, 0, 0, LITERAL_TRIGNAME_LEN , (char *)LITERAL_TRIGNAME , 0, 0);	/* BYPASSOK */
LITDEF mval literal_xecute	= DEFINE_MVAL_LITERAL(MV_STR | MV_NUM_APPROX, 0, 0, LITERAL_XECUTE_LEN   , (char *)LITERAL_XECUTE   , 0, 0);	/* BYPASSOK */
LITDEF mval literal_chset	= DEFINE_MVAL_LITERAL(MV_STR | MV_NUM_APPROX, 0, 0, LITERAL_CHSET_LEN    , (char *)LITERAL_CHSET    , 0, 0);	/* BYPASSOK */

LITDEF mval gvtr_cmd_mval[GVTR_CMDTYPES] = {
/* Define GVTR_CMD_SET, GVTR_CMD_KILL etc. */
#	define	GV_TRIG_CMD_ENTRY(cmdmval, cmdlit, cmdmask)			\
		DEFINE_MVAL_LITERAL(MV_STR, 0, 0, STR_LIT_LEN(cmdlit), (char *)cmdlit, 0, 0),
#	include "gv_trig_cmd_table.h"
#	undef GV_TRIG_CMD_ENTRY
};

LITDEF	int4	gvtr_cmd_mask[GVTR_CMDTYPES] = {
/* Define GVTR_MASK_SET, GVTR_MASK_KILL etc. */
#	define	GV_TRIG_CMD_ENTRY(cmdmval, cmdlit, cmdmask)	cmdmask,
#	include "gv_trig_cmd_table.h"
#	undef GV_TRIG_CMD_ENTRY
};

/* The initialization order of this array matches enum trig_subs_t defined in triggers.h */
#define TRIGGER_SUBDEF(SUBNAME) LITERAL_##SUBNAME
LITDEF char *trigger_subs[] = {
#include "trigger_subs_def.h"
#undef TRIGGER_SUBDEF
};

#endif

LITDEF	gtmImageName	gtmImageNames[n_image_types] =
{
#define IMAGE_TABLE_ENTRY(A,B)	{LIT_AND_LEN(B)},
#include "gtmimagetable.h"
#undef IMAGE_TABLE_ENTRY
};

LITDEF mname_entry 	null_mname_entry =
{
	{UNIX_ONLY_COMMA(0) 0, NULL},
	0,
	FALSE
};

LITDEF mval *fndata_table[2][2] =
{
	{&literal_zero, &literal_one},
	{&literal_ten, &literal_eleven}
};

LITDEF mval *fnzdata_table[2][2] =
{
	{&literal_oneohoh, &literal_oneohone},
	{&literal_oneten, &literal_oneeleven}
};

LITDEF mval *fnzqgblmod_table[2] =
{
	&literal_zero, &literal_one
};

LITDEF char gtm_release_name[]   = GTM_RELEASE_NAME;
LITDEF int4 gtm_release_name_len = SIZEOF(GTM_RELEASE_NAME) - 1;
LITDEF char gtm_product[]        = GTM_PRODUCT;
LITDEF int4 gtm_product_len      = SIZEOF(GTM_PRODUCT) - 1;
LITDEF char gtm_version[]        = GTM_VERSION;
LITDEF int4 gtm_version_len      = SIZEOF(GTM_VERSION) - 1;

/* Indexed by enum db_ver in gdsdbver.h. Note that a db_ver value can be -1 but only in
 * the internal context of incremental/stream backup so the value should never appear where
 * this table is being indexed (suggest asserts for index being > 0 at usage points).
 */
LITDEF char *gtm_dbversion_table[] =
{
	"V4",
	"V6"
};

LITDEF int4 ten_pwr[NUM_DEC_DG_1L+1] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};

LITDEF unsigned char lower_to_upper_table[] =
{
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
	61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,
	91, 92, 93, 94, 95, 96, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
	89, 90,
	123, 124, 125, 126, 127, 128, 129,
	130, 131, 132, 133, 134, 135, 136, 137, 138, 139,
	140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
	150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
	160, 161, 162, 163, 164, 165, 166, 167, 168, 169,
	170, 171, 172, 173, 174, 175, 176, 177, 178, 179,
	180, 181, 182, 183, 184, 185, 186, 187, 188, 189,
	190, 191, 192, 193, 194, 195, 196, 197, 198, 199,
	200, 201, 202, 203, 204, 205, 206, 207, 208, 209,
	210, 211, 212, 213, 214, 215, 216, 217, 218, 219,
	220, 221, 222, 223, 224, 225, 226, 227, 228, 229,
	230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
	240, 241, 242, 243, 244, 245, 246, 247, 248, 249,
	250, 251, 252, 253, 254, 255
};

#ifdef KEEP_zOS_EBCDIC
LITDEF unsigned char ebcdic_lower_to_upper_table[] =
{
/*	EBCDIC	*/
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
	61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,
	91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110,
/*									129, 130*/
	111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 193, 194,
/*131, 132, 133, 134, 135, 136, 137	*/
	195, 196, 197, 198, 199, 200, 201, 138, 139, 140,
	141, 142, 143, 144, 209, 210, 211, 212, 213, 214,
	215, 216, 217, 154, 155, 156, 157, 158, 159, 160,
	161, 226, 227, 228, 229, 230, 231, 232, 233,
	170, 171, 172, 173, 174, 175, 176, 177, 178, 179,
	180, 181, 182, 183, 184, 185, 186, 187, 188, 189,
	190, 191, 192, 193, 194, 195, 196, 197, 198, 199,
	200, 201, 202, 203, 204, 205, 206, 207, 208, 209,
	210, 211, 212, 213, 214, 215, 216, 217, 218, 219,
	220, 221, 222, 223, 224, 225, 226, 227, 228, 229,
	230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
	240, 241, 242, 243, 244, 245, 246, 247, 248, 249,
	250, 251, 252, 253, 254, 255
};
#endif

LITDEF unsigned char upper_to_lower_table[] =
{
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
	61, 62, 63, 64, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116,
	117, 118, 119, 120, 121, 122,
	91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116,
	117, 118, 119, 120, 121, 122,
	123, 124, 125, 126, 127, 128, 129,
	130, 131, 132, 133, 134, 135, 136, 137, 138, 139,
	140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
	150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
	160, 161, 162, 163, 164, 165, 166, 167, 168, 169,
	170, 171, 172, 173, 174, 175, 176, 177, 178, 179,
	180, 181, 182, 183, 184, 185, 186, 187, 188, 189,
	190, 191, 192, 193, 194, 195, 196, 197, 198, 199,
	200, 201, 202, 203, 204, 205, 206, 207, 208, 209,
	210, 211, 212, 213, 214, 215, 216, 217, 218, 219,
	220, 221, 222, 223, 224, 225, 226, 227, 228, 229,
	230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
	240, 241, 242, 243, 244, 245, 246, 247, 248, 249,
	250, 251, 252, 253, 254, 255
};

/* Following primes are very close to 2 ** x.  These numbers should give best distribution for our hash function.
 * Instead of software limiting in hash table sizes, we have a long prime table.
 * This commented out table is there for any future reference.
 * int ht_sizes[] = {
 *      13, 37, 53, 97, 193, 389, 769, 1543, 3079, 6151, 12289,  24593, 49157, 98317, 196613, 393241, 786433, 1572869,
 *      3145739, 6291469, 12582917, 25165843, 50331653, 100663319,  201326611, 402653189, 805306457, 1610612741, 0};
 */
LITDEF int ht_sizes[] =
{
	13, 37, 53, 97, 193, 389, 769, 1543, 3079, 6151, 12289,  24593, 49157,
	/* Above doubles the table size. But below has slower progression */
	62501, 102503, 154981, 218459, 290047, 366077, 442861, 517151,
	603907, 705247, 823541, 961729, 1123079, 1311473, 1531499, 1788443,
	2088497, 2438881, 2848057, 3325901, 3883903, 4535483, 5296409, 6185021,
	7222661, 8434427, 9849503, 11502019, 13431661, 15685133, 18316643, 21389671,
	24978257, 29168903, 34062629, 39777391, 46450931, 54244103, 0
};

#ifdef UNIX
/* Primarily used by gtm_trigger_complink() */
LITDEF char 	alphanumeric_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
					'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
					'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
					't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
					'8', '9', '\0'};
LITDEF int	alphanumeric_table_len = (SIZEOF(alphanumeric_table) - 1);

LITDEF mstr chset_names[CHSET_MAX_IDX_ALL] =
{ /* Supported character set (CHSET) codes for the 3-argument form of $ZCONVERT.
   *  Note: Update the *_CHSET_LEN macros below if new CHSETs are added.
   */
	{1, 1, "M"},	/* "M" should be the first CHSET (0th index of "chset_names" array). verify_chset() callers rely on this.
			 * $ZCONVERT doesn't support M, but I/O does */
	{5, 5, "UTF-8"},
	{6, 6, "UTF-16"},
	{8, 8, "UTF-16LE"},
	{8, 8, "UTF-16BE"},
	{5, 5, "ASCII"},
	{6, 6, "EBCDIC"},
	{6, 6, "BINARY"}
};
/* This array holds the ICU converter handles corresponding to the respective
 * CHSET name in the table chset_names[]
 */
GBLDEF	UConverter	*chset_desc[CHSET_MAX_IDX];
GBLDEF casemap_t casemaps[MAX_CASE_IDX] =
{ /* Supported case mappings and their disposal conversion routines for both $ZCHSET modes.
   * Note: since UTF-8 disposal functions for "U" and "L" are ICU "function pointers" rather
   * rather than their direct addresses, they are initialized in gtm_utf8_init() instead
   */
	{"U", &lower_to_upper, NULL},
	{"L", &upper_to_lower, NULL},
	{"T", NULL,            NULL}
};
#endif

#ifdef UNIX
/* Used as the value for "regular" key (i.e., the one with no hidden subscripts) of a spanning node. */
LITDEF mstr	nsb_dummy = {0, 1, "\0"};
/*LITDEF mstr	nsb_dummy = {0, LEN_AND_LIT("dummy")};*/
#endif

#ifdef DEBUG
/* These instructions follow the definitions made
 * in vxi.h and were generated from them. Where
 * skips were made in the definitions, the string
 * "*N/A*  " is placed in the table.
 */
LITDEF char vxi_opcode[][6] =
{
	"HALT  ",
	"NOP   ",
	"REI   ",
	"BPT   ",
	"RET   ",
	"RSB   ",
	"LDPCTX",
	"SVPCTX",
	"CVTPS ",
	"CVTSP ",
	"INDEX ",
	"CRC   ",
	"PROBER",
	"PROBEW",
	"INSQUE",
	"REMQUE",
	"BSBB  ",
	"BRB   ",
	"BNEQ  ",
	"BEQL  ",
	"BGTR  ",
	"BLEQ  ",
	"JSB   ",
	"JMP   ",
	"BGEQ  ",
	"BLSS  ",
	"BGTRU ",
	"BLEQU ",
	"BVC   ",
	"BVS   ",
	"BGEQU ",
	"BLSSU ",
	"ADDP4 ",
	"ADDP6 ",
	"SUBP4 ",
	"SUBP6 ",
	"CVTPT ",
	"MULP  ",
	"CVTTP ",
	"DIVP  ",
	"MOVC3 ",
	"CMPC3 ",
	"SCANC ",
	"SPANC ",
	"MOVC5 ",
	"CMPC5 ",
	"MOVTC ",
	"MOVTUC",
	"BSBW  ",
	"BRW   ",
	"CVTWL ",
	"CVTWB ",
	"MOVP  ",
	"CMPP3 ",
	"CVTPL ",
	"CMPP4 ",
	"EDITPC",
	"MATCHC",
	"LOCC  ",
	"SKPC  ",
	"MOVZWL",
	"ACBW  ",
	"MOVAW ",
	"PUSHAW",
	"ADDF2 ",
	"ADDF3 ",
	"SUBF2 ",
	"SUBF3 ",
	"MULF2 ",
	"MULF3 ",
	"DIVF2 ",
	"DIVF3 ",
	"CVTFB ",
	"CVTFW ",
	"CVTFL ",
	"CVTRFL",
	"CVTBF ",
	"CVTWF ",
	"CVTLF ",
	"ACBF  ",
	"MOVF  ",
	"CMPF  ",
	"MNEGF ",
	"TSTF  ",
	"EMODF ",
	"POLYF ",
	"CVTFD ",
	"*N/A* ",
	"ADAWI ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"INSQHI",
	"INSQTI",
	"REMQHI",
	"REMQTI",
	"ADDD2 ",
	"ADDD3 ",
	"SUBD2 ",
	"SUBD3 ",
	"MULD2 ",
	"MULD3 ",
	"DIVD2 ",
	"DIVD3 ",
	"CVTDB ",
	"CVTDW ",
	"CVTDL ",
	"CVTRDL",
	"CVTBD ",
	"CVTWD ",
	"CVTLD ",
	"ACBD  ",
	"MOVD  ",
	"CMPD  ",
	"MNEGD ",
	"TSTD  ",
	"EMODD ",
	"POLYD ",
	"CVTDF ",
	"*N/A* ",
	"ASHL  ",
	"ASHQ  ",
	"EMUL  ",
	"EDIV  ",
	"CLRQ  ",
	"MOVQ  ",
	"MOVAQ ",
	"PUSHAQ",
	"ADDB2 ",
	"ADDB3 ",
	"SUBB2 ",
	"SUBB3 ",
	"MULB2 ",
	"MULB3 ",
	"DIVB2 ",
	"DIVB3 ",
	"BISB2 ",
	"BISB3 ",
	"BICB2 ",
	"BICB3 ",
	"XORB2 ",
	"XORB3 ",
	"MNEGB ",
	"CASEB ",
	"MOVB  ",
	"CMPB  ",
	"MCOMB ",
	"BITB  ",
	"CLRB  ",
	"TSTB  ",
	"INCB  ",
	"DECB  ",
	"CVTBL ",
	"CVTBW ",
	"MOVZBL",
	"MOVZBW",
	"ROTL  ",
	"ACBB  ",
	"MOVAB ",
	"PUSHAB",
	"ADDW2 ",
	"ADDW3 ",
	"SUBW2 ",
	"SUBW3 ",
	"MULW2 ",
	"MULW3 ",
	"DIVW2 ",
	"DIVW3 ",
	"BISW2 ",
	"BISW3 ",
	"BICW2 ",
	"BICW3 ",
	"XORW2 ",
	"XORW3 ",
	"MNEGW ",
	"CASEW ",
	"MOVW  ",
	"CMPW  ",
	"MCOMW ",
	"BITW  ",
	"CLRW  ",
	"TSTW  ",
	"INCW  ",
	"DECW  ",
	"BISPSW",
	"BICPSW",
	"POPR  ",
	"PUSHR ",
	"CHMK  ",
	"CHME  ",
	"CHMS  ",
	"CHMU  ",
	"ADDL2 ",
	"ADDL3 ",
	"SUBL2 ",
	"SUBL3 ",
	"MULL2 ",
	"MULL3 ",
	"DIVL2 ",
	"DIVL3 ",
	"BISL2 ",
	"BISL3 ",
	"BICL2 ",
	"BICL3 ",
	"XORL2 ",
	"XORL3 ",
	"MNEGL ",
	"CASEL ",
	"MOVL  ",
	"CMPL  ",
	"MCOML ",
	"BITL  ",
	"CLRL  ",
	"TSTL  ",
	"INCL  ",
	"DECL  ",
	"ADWC  ",
	"SBWC  ",
	"MTPR  ",
	"MFPR  ",
	"MOVPSL",
	"PUSHL ",
	"MOVAL ",
	"PUSHAL",
	"BBS   ",
	"BBC   ",
	"BBSS  ",
	"BBCS  ",
	"BBSC  ",
	"BBCC  ",
	"BBSSI ",
	"BBCCI ",
	"BLBS  ",
	"BLBC  ",
	"FFS   ",
	"FFC   ",
	"CMPV  ",
	"CMPZV ",
	"EXTV  ",
	"EXTZV ",
	"INSV  ",
	"ACBL  ",
	"AOBLSS",
	"AOBLEQ",
	"SOBGEQ",
	"SOBGTR",
	"CVTLB ",
	"CVTLW ",
	"ASHP  ",
	"CVTLP ",
	"CALLG ",
	"CALLS ",
	"XFC   ",
	"ESCD  ",
	"ESCE  ",
	"ESCF  ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"CVTDH ",
	"CVTGF ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"ADDG2 ",
	"ADDG3 ",
	"SUBG2 ",
	"SUBG3 ",
	"MULG2 ",
	"MULG3 ",
	"DIVG2 ",
	"DIVG3 ",
	"CVTGB ",
	"CVTGW ",
	"CVTGL ",
	"CVTRGL",
	"CVTBG ",
	"CVTWG ",
	"CVTLG ",
	"ACBG  ",
	"MOVG  ",
	"CMPG  ",
	"MNEGG ",
	"TSTG  ",
	"EMODG ",
	"POLYG ",
	"CVTGH ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"ADDH2 ",
	"ADDH3 ",
	"SUBH2 ",
	"SUBH3 ",
	"MULH2 ",
	"MULH3 ",
	"DIVH2 ",
	"DIVH3 ",
	"CVTHB ",
	"CVTHW ",
	"CVTHL ",
	"CVTRHL",
	"CVTBH ",
	"CVTWH ",
	"CVTLH ",
	"ACBH  ",
	"MOVH  ",
	"CMPH  ",
	"MNEGH ",
	"TSTH  ",
	"EMODH ",
	"POLYH ",
	"CVTHG ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"CLRO  ",
	"MOVO  ",
	"MOVAO ",
	"PUSHAO",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"CVTFH ",
	"CVTFG ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"*N/A* ",
	"CVTHF ",
	"CVTHD "
};

/* Routine invoked in debug mode by init_gtm() on UNIX to verify certain assumptions about some of the
 * tables in this routine. Routine must be resident in this module to do these checks since dimensions
 * are not known in other routines using a LITREF.
 */
void mtables_chk(void)
{
	assert(SIZEOF(mvs_size) == (MVST_LAST + 1));
	assert(SIZEOF(mvs_save) == (SIZEOF(boolean_t) * (MVST_LAST + 1)));
}
#endif
