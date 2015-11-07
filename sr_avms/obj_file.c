/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <rms.h>

#include "compiler.h"
#include <rtnhdr.h>
#include "obj_gen.h"
#include "pdscdef.h"
#include "objlangdefs.h"
#include "cmd_qlf.h"
#include "mdq.h"
#include "stringpool.h"
#include "axp_registers.h"
#include "axp_gtm_registers.h"
#include "axp.h"
#include "proc_desc.h"
#include "mmemory.h"
#include "obj_file.h"

struct sym_table
{
	struct sym_table	*next;
	int4			linkage_offset;
	int4			psect;
	uint4			value;
	unsigned short		name_len;
	unsigned char		name[1];
};

struct linkage_entry
{
	struct linkage_entry	*next;
	struct sym_table	*symbol;
};

/* values used in the RMS calls and header records */
#define INITIAL_ALLOC		2
#define DEFAULT_EXTEN_QUAN	10
/* N.B., a useful value for the Alpha is 8192 */
#define MAX_REC_SIZE		OBJ_EMIT_BUF_SIZE
#define MAX_REC_NUM		1024
#define VERSION_NUM		"V1.0"
#define VERSION_NUM_SIZE	SIZEOF(VERSION_NUM) - 1
#define MAX_IMMED		(MAX_REC_SIZE - 2 * SIZEOF(short) - 2 * SIZEOF(short) - SIZEOF(int4))
#define NO_IMMED		0
#define MIN_LINK_PSECT_SIZE	56

LITREF char		gtm_release_name[];
LITREF int4		gtm_release_name_len;

GBLREF command_qualifier cmd_qlf;
GBLREF mident		routine_name;
GBLREF mident		module_name;
GBLREF char		rev_time_buf[];
GBLREF boolean_t	run_time;
GBLREF uint4		code_size, lits_size;
GBLREF unsigned char	*runtime_base;

GBLDEF int4		psect_use_tab[GTM_LASTPSECT];	/* bytes of each psect this module */
GBLREF char		object_file_name[256];
GBLREF short		object_name_len;
GBLREF struct FAB	obj_fab;			/* file access block for the object file */

static short int	current_psect;
static int4		linkage_size;
static struct RAB	obj_rab;  			/* record access block for the object file */
static char		sym_buff[OBJ_SYM_BUF_SIZE];	/* buffer for global symbol defs */
static short int	sym_buff_used;			/* number of chars in sym_buff */
static char		emit_buff[OBJ_EMIT_BUF_SIZE];	/* buffer for emit output (must match size of buffer used by incr_link */
static short int	emit_buff_used;			/* number of chars in emit_buff */
static short int	emit_last_immed;		/* index of last STO_IMM */
static short int	linker_stack_depth;		/* linker stack depth */

/* Values that appear in the psect definitions.  */
#define CODE_PSECT_ALIGN	4 /* octaword aligned */
#define LIT_PSECT_ALIGN		3 /* quadword aligned */
#define RNAMB_PSECT_ALIGN	2 /* longword aligned */
#define LINK_PSECT_ALIGN	4 /* octaword aligned */
#define CODE_PSECT_FLAGS	EGPS$M_PIC|EGPS$M_REL|EGPS$M_SHR|EGPS$M_EXE|EGPS$M_GBL|EGPS$M_RD
#define LIT_PSECT_FLAGS		EGPS$M_PIC|EGPS$M_REL|EGPS$M_SHR|EGPS$M_GBL|EGPS$M_RD
#define RNAMB_PSECT_FLAGS	EGPS$M_PIC|EGPS$M_REL|EGPS$M_GBL|EGPS$M_RD|EGPS$M_WRT
#define LINK_PSECT_FLAGS	EGPS$M_REL|EGPS$M_RD|EGPS$M_WRT
#define CODE_PSECT_NAME		"GTM$CODE"
#define LIT_PSECT_NAME		"GTM$LITERALS"
#define LINK_PSECT_NAME		"GTM$LINK"
#define OBJ_STRUCTURE_LEVEL	2
#define OBJ_ARCH_1		0
#define OBJ_ARCH_2		0

/* Values that appear in procedure descriptors.  These values MUST match values in gtm$main in gtm_main.m64.  */
/* Note: both RSA_END and STACK_SIZE must be multiples of 16; this may introduce padding at the beginning of each region.  */
#define RSA_END			ROUND_UP((1 + 1 + 9)*8, 16)	/* r27 + A(condition handler) + 9 saved registers */
#define RSA_OFFSET		(RSA_END - 9*8)			/* beginning of register save area */
#define STACK_SIZE		ROUND_UP(RSA_END + (1+6)*8, 16)	/* add room for r25 (arg info), r16-r21 (arguments) */

static MSTR_CONST(gtm_dyn_ch_name,"GTM$DYN_CH");
static MSTR_CONST(main_call_name,"GTM$MAIN");

/* Linkage pair index values.  In order to take advantage of linker-time procedure call optimization, we
   need one linkage pair for each external procedure called.  The linkage pair consists of two pointers,
   each with its own index (the index of the second pointer is 1 plus the index of the first: the first is
   the address of the called procedure's entry point and the second is the address of its procedure
   descriptor.
*/
#define LP_MAIN		1

error_def(ERR_OBJFILERR);

typedef struct _Addr_ref
{
	struct _Addr_ref *	next;
	int			psect;
	int			offset;
	int			index;
} Addr_ref;

static Addr_ref	*addr_list = (void *) 0;
static Addr_ref	*addr_list_end = (void *) 0;
static int	addr_index;

void emit_linkages(void);
void emit_lp_rel( int reltype, int lpx, int psect1, int off1, int repl_ins, int psect2, int off2, int namelen,
			char *name);
void emit_sta_pq(int psect, int offset);
void emit_stc_lp_psb(int lpx, int len, char *name);
void emit_sto_gbl(int len, char	*name);
void emit_sto_lw(void);
void set_psect( int psect, int offset);
void output_symbol(void);
void buff_emit( char *buff, short size);
char *emit_psc( char *buf, int align, int flags, int alloc, int length, char *name);
void emit_sto_off(void);
void flush_addrs(void);

/*
 *	create_object_file
 *
 *	Args:	routine header for object module
 *
 *	Description:	Create and open object file.
 *			Write module and language processor header records.
 *			Write global symbol directory (GSD) psect definition records.
 *			Write routine procedure descriptor to beginning of linkage psect.
 *			Write linkage pair and address records to linkage section for external procedures.
 *			Generate and write to code psect procedure prologue portion of routine header.
 *			Write rest of routine header to code psect.
 *
 */

void	create_object_file(rhdtyp *rhead)
{
	char	obj_name_buff[SIZEOF(mident_fixed) + SIZEOF(".OBJ") - 1];
	struct NAM	nam;

	int	stat, mname_len;
	register char	*fast;
	mstr	maincall;
	char	psect_def_rec[MAX_REC_SIZE];	/* psect definition records */

	/* All object files have a PSECT named "GTM$R" followed by the first 8 characters of the name of the
	   routine.  This PSECT contains the routines table entry (rtn_tables) for the routine.

	   Because the linker sorts PSECT's with the same significant attributes alphabetically in the same
	   image section, this naming convention causes the linker to build the initial routines table for
	   GT.M native object files that are included in the initial (static) link.

	   Two dummy PSECT's named "GTM$R" and "GTM$RZZZZZZZZZZ" are defined in GTM_MAIN.M64 and will be sorted
	   by the linker to delimit the beginning and end, respectively, of the initial routines table.  In
	   order for this to work properly, the GTM_RNAMESAAAAB PSECT generated here must have the same
	   significant linker attributes as those in GTM_MAIN.M64 and their alignment requirements must be
	   identical to those of entries of the routines name table.
	*/
	/* GTM_RNAMESAAAAB PSECT name prefix */
	char	rnambuf[RNAMB_PREF_LEN + SIZEOF(mident_fixed) + 1];

	/* First quadword of procedure descriptor contains flags and offset fields. */
	short	pdbuf_head[4] =
	{
		PDSC_FLAGS,
		RSA_OFFSET,		/* register save area offset in stack frame */
		0,			/* includes FRET field */
		0			/* offset of call signature */
	};

	/* Last 2 quadwords of procedure descriptor before handler information. */
	int4	pdbuf_tail[4] =
	{
		STACK_SIZE,					/* size of stack frame */
		0,						/* includes ENTRY_LENGTH field */

			  (1 << ALPHA_REG_FP)			/* IREG_MASK (N.B. don't set bit for ALPHA_REG_RA) */
			| (1 << ALPHA_REG_S0)			/* r2 */
			| (1 << ALPHA_REG_S1)                   /* r3 */
			| (1 << ALPHA_REG_S2)                   /* r4 */
			| (1 << ALPHA_REG_S3)                   /* r5 */
			| (1 << ALPHA_REG_S4)                   /* r6 */
			| (1 << ALPHA_REG_S5)                   /* r7 */
			| (1 << GTM_REG_FRAME_VAR_PTR)		/* r8 */
			| (1 << GTM_REG_FRAME_TMP_PTR)		/* r9 */
			| (1 << GTM_REG_DOLLAR_TRUTH)		/* r10 */
			| (1 << GTM_REG_XFER_TABLE)		/* r11 */
			| (1 << GTM_REG_FRAME_POINTER)		/* r12 */
			| (1 << GTM_REG_PV)			/* r13 */
			| (1 << GTM_REG_LITERAL_BASE),		/* r14 */

		0						/* FREG_MASK */
	};

	int	length;
	int4	code_buf[MAX_REC_SIZE / 4];
	int4	*codep;
	int	gtm_main_call, gtm_main_lp;

	assert(!run_time);
	/* create the object file */
	obj_fab = cc$rms_fab;		/* initialize to default FAB values supplied by <rms.h> */
	obj_fab.fab$l_dna = obj_name_buff;
	mname_len = module_name.len;
	assert(mname_len <= MAX_MIDENT_LEN);
	if (!mname_len)
	{
		MEMCPY_LIT(obj_name_buff, "MDEFAULT.OBJ");
		obj_fab.fab$b_dns = SIZEOF("MDEFAULT.OBJ") - 1;
	}
	else
	{
		memcpy(obj_name_buff, module_name.addr, mname_len);
		MEMCPY_LIT(&obj_name_buff[mname_len], ".OBJ");
		obj_fab.fab$b_dns = mname_len + SIZEOF(".OBJ") - 1;
	}
	obj_fab.fab$l_fop = FAB$M_CBT | FAB$M_TEF | FAB$M_MXV;
	obj_fab.fab$l_alq = INITIAL_ALLOC;
	obj_fab.fab$w_deq = DEFAULT_EXTEN_QUAN;
	obj_fab.fab$b_fac = FAB$M_PUT | FAB$M_UPD | FAB$M_GET;
	obj_fab.fab$w_mrs = MAX_REC_SIZE;
	obj_fab.fab$l_mrn = MAX_REC_NUM;
	obj_fab.fab$b_rfm = FAB$C_VAR;
	obj_fab.fab$b_org = FAB$C_SEQ;
	obj_fab.fab$l_nam = &nam;
	nam = cc$rms_nam;		/* initialize to default NAM values supplied by <rms.h> */
	nam.nam$l_esa = object_file_name;
	nam.nam$b_ess = 255;

	if (MV_DEFINED(&cmd_qlf.object_file))
	{
		obj_fab.fab$b_fns = cmd_qlf.object_file.str.len;
		obj_fab.fab$l_fna = cmd_qlf.object_file.str.addr;
	}
	stat = sys$create(&obj_fab);
	obj_fab.fab$l_nam = 0;
	object_name_len = nam.nam$b_esl;
	object_file_name[object_name_len] = 0;
	switch (stat)
	{
	case RMS$_NORMAL:
	case RMS$_CREATED:
	case RMS$_SUPERSEDE:
	case RMS$_FILEPURGED:
		break;
	default:
		rts_error (VARLSTCNT(6) ERR_OBJFILERR, 2, object_name_len, object_file_name, stat, obj_fab.fab$l_stv);
	}

	obj_rab = cc$rms_rab;		/* initialize to default RAB values supplied by <rms.h> */
	obj_rab.rab$l_fab = &obj_fab;

	stat = sys$connect(&obj_rab);
	if(stat!=RMS$_NORMAL)
		rts_error (VARLSTCNT(6) ERR_OBJFILERR, 2, object_name_len, object_file_name, stat, obj_fab.fab$l_stv);

	linker_stack_depth = 0;

	/* output the Main Module Header Record */
	fast = emit_buff;
	*((short *) fast)++ = EOBJ$C_EMH;
	*((short *) fast)++ = 0;	/* record length */
	*((short *) fast)++ = EMH$C_MHD;
	*((short *) fast)++ = OBJ_STRUCTURE_LEVEL;
	*((int4 *) fast)++ = OBJ_ARCH_1;
	*((int4 *) fast)++ = OBJ_ARCH_2;
	*((int4 *) fast)++ = MAX_REC_SIZE;
	*((char *) fast)++ = mname_len;
	memcpy(fast, module_name.addr, mname_len);
	fast += mname_len;
	*((char *) fast)++ = VERSION_NUM_SIZE;
	memcpy(fast,VERSION_NUM,VERSION_NUM_SIZE);
	fast += VERSION_NUM_SIZE;
	memcpy(fast,rev_time_buf,17);
	fast += 17;
	memcpy(fast,"                 ",17);
	fast += 17;
	length = fast - emit_buff;
	((short *) emit_buff)[1] = length;
	obj_rab.rab$l_rbf = emit_buff;
	obj_rab.rab$w_rsz = length;
	stat = sys$put(&obj_rab);
	if(stat != RMS$_NORMAL)
		rts_error (VARLSTCNT(6) ERR_OBJFILERR, 2, object_name_len, object_file_name, stat, obj_fab.fab$l_stv);

	/* output the Language Name Header Record */
	fast = emit_buff;
	*((short *) fast)++ = EOBJ$C_EMH;
	*((short *) fast)++ = 6 + gtm_release_name_len;
	*((short *) fast)++ = EMH$C_LNM;
	memcpy(fast,&gtm_release_name[0],gtm_release_name_len);
	fast += gtm_release_name_len;
	length = fast - emit_buff;
	obj_rab.rab$w_rsz = length;
	stat = sys$put(&obj_rab);
	if(stat != RMS$_NORMAL)
		rts_error (VARLSTCNT(6) ERR_OBJFILERR, 2, object_name_len, object_file_name, stat, obj_fab.fab$l_stv);

	/* define psect's */
	length = routine_name.len;
	MEMCPY_LIT(&rnambuf[0], RNAMB_PREF);
	memcpy(&rnambuf[RNAMB_PREF_LEN], routine_name.addr, length);
	if ('%' == rnambuf[RNAMB_PREF_LEN])
		rnambuf[RNAMB_PREF_LEN] = '.';
	length += RNAMB_PREF_LEN;
	if (length > EGPS$S_NAME) /* Truncate the PSECT name if it exceeeds the maximum allowed length (31) */
		length = EGPS$S_NAME;
	if (routine_name.len >= RNAME_SORTED_LEN && !memcmp(routine_name.addr, RNAME_ALL_Z, RNAME_SORTED_LEN))
	{ /* If the first 26 chars of the routine name are all lower case z's (currently routine names cannot be
	     in lower case, but if supported in future), we should not generate the psect name containing
	     the routine name as it could conflict with the dummy anchor gtm$rrzzzzzzzzzzzzzzzzzzzzzzzzzz defined
	     in gtm_main.m64. If such a conflict occurs, linker may not guarantee that the dummy anchor to be the
	     last entry in the routine table. So, if the routine name starts with 26 z's, we generate the psect
	     name changing the last (31st) character to '$' so that all such routines will be concatenated
	     between the anchors.  However, they need to be sorted at runtime startup based on the complete
	     routine name (see rtn_tbl_sort.c) */
		assert(length == EGPS$S_NAME);
		rnambuf[length - 1] = '$';
	}
	((short *) psect_def_rec)[0] = EOBJ$C_EGSD;
	fast = psect_def_rec+8;
	fast = emit_psc(fast, CODE_PSECT_ALIGN,  CODE_PSECT_FLAGS,  code_size,    LEN_AND_LIT(CODE_PSECT_NAME));
	fast = emit_psc(fast, LIT_PSECT_ALIGN,   LIT_PSECT_FLAGS,   lits_size,    LEN_AND_LIT(LIT_PSECT_NAME));
	fast = emit_psc(fast, RNAMB_PSECT_ALIGN, RNAMB_PSECT_FLAGS, SIZEOF(rtn_tabent), length, rnambuf);
	fast = emit_psc(fast, LINK_PSECT_ALIGN,  LINK_PSECT_FLAGS,  linkage_size, LEN_AND_LIT(LINK_PSECT_NAME));
	length = fast - psect_def_rec;
	((short *) psect_def_rec)[1] = length;

	obj_rab.rab$l_rbf = psect_def_rec;
	obj_rab.rab$w_rsz = length;
	stat = sys$put(&obj_rab);
	if (stat != RMS$_NORMAL)
		rts_error (VARLSTCNT(6) ERR_OBJFILERR, 2, object_name_len, object_file_name, stat, obj_fab.fab$l_stv);

	/* set the appropriate buffer counts */
	((short *) sym_buff)[0] = EOBJ$C_EGSD;
	((short *) sym_buff)[1] = 0;
	sym_buff_used = 8;
	((short *) emit_buff)[0] = EOBJ$C_ETIR;
	((short *) emit_buff)[1] = 0;
	emit_buff_used = 4;
	emit_last_immed = NO_IMMED;
	memset(psect_use_tab, 0, SIZEOF(psect_use_tab));

	/* Build GTM_LINKAGE ("$LINKAGE") psect. */
	set_psect(GTM_LINKAGE, 0);

	/* Generate a procedure descriptor pointing to prologue code (beginning of GTM_CODE psect). */
	/* N.B. This procedure descriptor must be at offset 0 in $LINKAGE; that's the way it's defined in obj_code.  */
	emit_immed(pdbuf_head, SIZEOF(pdbuf_head));
	emit_sta_pq(GTM_CODE, 0);
	emit_sto_off();
	emit_immed(pdbuf_tail, SIZEOF(pdbuf_tail));

	(void)define_symbol(GTM_ANOTHER_MODULE, &gtm_dyn_ch_name, 0);
	emit_sto_gbl(gtm_dyn_ch_name.len, gtm_dyn_ch_name.addr);

	/* Emit a linkage pair for to GTM$MAIN.  */
	set_psect(GTM_LINKAGE, ROUND_UP(psect_use_tab[GTM_LINKAGE],8));	/* align to quadword boundary */
	gtm_main_lp = psect_use_tab[GTM_LINKAGE];			/* remember for later */
	(void)define_symbol(GTM_ANOTHER_MODULE, &main_call_name, 0);
	emit_stc_lp_psb(LP_MAIN, main_call_name.len, main_call_name.addr);

	assert (psect_use_tab[GTM_LINKAGE] == MIN_LINK_PSECT_SIZE);
	emit_linkages ();

	addr_index = 48;

	set_psect(GTM_CODE, 0);

	/* Emit the routine header, including procedure prologue (initialization call to GTM$MAIN).  */

	/* Emit initialization call to GTM$MAIN.  */
	/* (This is the routine header jsb field.) */
	codep = code_buf;

	gtm_main_call = (char *) codep - (char *) code_buf;
	*codep++ = ALPHA_MEM(ALPHA_INS_LDQ,  ALPHA_REG_R0,   ALPHA_REG_PV,     gtm_main_lp);
	*codep++ = ALPHA_MEM(ALPHA_INS_LDQ,  ALPHA_REG_R1,   ALPHA_REG_PV,     gtm_main_lp+8);
	*codep++ = ALPHA_JMP(ALPHA_INS_JSR,  ALPHA_REG_R0,   ALPHA_REG_R0);	/* call GTM$MAIN */

	assert ((char *)codep - (char *)code_buf == RHEAD_JSB_SIZE);
	emit_immed(code_buf, (char *) codep - (char *) code_buf);

	/* Emit conditional stores for instruction optimization of call to GTM$MAIN: */
	emit_lp_rel(ETIR$C_STC_NOP_GBL, LP_MAIN,
		    GTM_CODE, gtm_main_call,
		    ALPHA_OPR(ALPHA_INS_BIS, ALPHA_REG_ZERO, ALPHA_REG_ZERO, ALPHA_REG_ZERO),
		    GTM_CODE, gtm_main_call + 12,
		    main_call_name.len, main_call_name.addr);
	emit_lp_rel(ETIR$C_STC_LDA_GBL, LP_MAIN + 1,
		    GTM_CODE, gtm_main_call + 4,
		    ALPHA_MEM(ALPHA_INS_LDA, ALPHA_REG_R1,   ALPHA_REG_PV,   0),
		    GTM_LINKAGE, 0,
		    main_call_name.len, main_call_name.addr);
	emit_lp_rel(ETIR$C_STC_BOH_GBL, LP_MAIN,
		    GTM_CODE, gtm_main_call + 8,
		    ALPHA_BRA(ALPHA_INS_BSR, ALPHA_REG_R0,   0),
		    GTM_CODE, gtm_main_call + 12,
		    main_call_name.len, main_call_name.addr);
	/* Now emit the rest of the common part of the routine header (from src_full_name through temp_size).  */

	/* src_full_name (mstr) */
	emit_immed(&rhead->src_full_name.len, SIZEOF(rhead->src_full_name.len));
	emit_pidr((int4)rhead->src_full_name.addr, GTM_LITERALS);
	/* routine_name (mident) */
	emit_immed(&rhead->routine_name.len, SIZEOF(rhead->routine_name.len));
	emit_pidr((int4)rhead->routine_name.addr, GTM_LITERALS);
	/* From vartab_off to the end of rhead.  */
	emit_immed((char *)&rhead->vartab_off, ((char *)&rhead->linkage_ptr - (char *)&rhead->vartab_off));

	emit_pidr(0, GTM_LINKAGE); /* Fill in linkage_ptr with the address of the beginning of the linkage Psect. */
	emit_pidr(0, GTM_LITERALS); /* Fill in literal_ptr with the address of the beginning of the literal Psect. */
}


/*
 *	close_object_file
 *
 *	Description:	Write routine table entry psect.
 *			Write global symbol specifications records for defined global symbols.
 *			Generate and write end of module record.
 *			Close file.
 *
 */

void	close_object_file(rhdtyp *rhead)
{
	int		stat;
	register char	*fast;

	flush_addrs();

	/* emit the routine table entry (rtn_tabent) */
	set_psect(GTM_RNAMESAAAAB, 0);
	emit_immed(&rhead->routine_name.len, SIZEOF(rhead->routine_name.len));
	emit_pidr((int4)rhead->routine_name.addr, GTM_LITERALS);

	emit_pidr(0, GTM_CODE);

	/* flush emit buffer */
	if (emit_buff_used > 4)
	{
		((short *) emit_buff)[0] = EOBJ$C_ETIR;
		((short *) emit_buff)[1] = emit_buff_used;
		obj_rab.rab$l_rbf = emit_buff;
		obj_rab.rab$w_rsz = emit_buff_used;
		stat = sys$put(&obj_rab);
		if (stat != RMS$_NORMAL)
			rts_error (VARLSTCNT(6) ERR_OBJFILERR, 2, object_name_len, object_file_name, stat, obj_fab.fab$l_stv);
	}
	output_symbol();

	/* output the End of Module Record */
	fast = emit_buff;
	*((short *) fast)++ = EOBJ$C_EEOM;
	*((short *) fast)++ = 24;
	*((int4 *) fast)++ = 2;			/* total linkage indices (= linkage pairs / 2) */
	*((short *) fast)++ = EEOM$C_SUCCESS;	/* should be completion code in range [0,3] [lidral] */
	*((unsigned char *) fast)++ = 1 << EEOM$V_WKTFR;	/* EEOM$V_WKTFR = 1 => weak transfer address */
	*((unsigned char *) fast)++ = 0;	/* alignment byte (must be zero) */
	*((int4 *) fast)++ = GTM_LINKAGE;	/* program section index of program section containing transfer address */
	*((int4 *) fast)++ = 0;			/* offset of transfer address within program section */
	*((int4 *) fast)++ = 0;
	obj_rab.rab$l_rbf = emit_buff;
	obj_rab.rab$w_rsz = 24;
	stat = sys$put(&obj_rab);
	if (stat != RMS$_NORMAL)
		rts_error (VARLSTCNT(6) ERR_OBJFILERR, 2, object_name_len, object_file_name, stat, obj_fab.fab$l_stv);

	/* close up */
	stat = sys$disconnect(&obj_rab);
	if (stat != RMS$_NORMAL)
		rts_error (VARLSTCNT(6) ERR_OBJFILERR, 2, object_name_len, object_file_name, stat, obj_fab.fab$l_stv);

	assert (linker_stack_depth == 0);
}


/*
 *	drop_object_file
 *
 *	Description:	Delete object file.
 */

void drop_object_file(void)
{
	obj_fab.fab$l_fop |= FAB$M_DLT;
	sys$close(&obj_fab);
	obj_fab = cc$rms_fab;
}


/*
 *	define_symbol
 *
 *	Args:	psect index, symbol name, symbol value.
 *
 *	Description:	Buffers a definition of a global symbol with the given name and value in the given psect.
 *			psect == GTM_ANOTHER_MODULE  =>  external (not defined in this module) symbol
 *
 *	N.B. the return value for define_symbol is only used in this module; all other modules refer to it as type void.
 */


static struct sym_table		*symbols;

struct sym_table* define_symbol(int4 psect, mstr *name, int4 value)
{
	int4			cmp;
	struct sym_table	**sym, *newsym;

	sym = &symbols;
	while(*sym != 0)
	{
		if ((cmp = memvcmp(name->addr, name->len, &((*sym)->name[0]), (*sym)->name_len)) <= 0)
			break;
		sym = &((*sym)->next);
	}

	if (cmp != 0  ||  *sym == 0)
	{
		/* Add new symbol in alphabetic order.  */
		newsym = (struct sym_table *) mcalloc(SIZEOF(struct sym_table) + name->len - 1);
		newsym->name_len = name->len;
		memcpy(&newsym->name[0], name->addr, name->len);
		newsym->linkage_offset = 0;	/* don't assign linkage psect offset until used */
		newsym->psect = psect;
		newsym->value = value;
		newsym->next = *sym;
		*sym = newsym;
	}
	else if (psect == GTM_CODE)		/* if psect is GTM_CODE, this is a def, not a ref */
	{
		(*sym)->psect = psect;
		(*sym)->value = value;
	}
	return *sym;
}


/*
 *	emit_immed
 *
 *	Args:  buffer of executable code, and byte count to be output.
 *
 *	Description:  Issues TIR records to store the given number of bytes
 *		from the buffer directly into the image at the current image
 *		location.
 */

GBLREF spdesc stringpool;

error_def(ERR_STRINGOFLOW);


#define CRITICAL 5

void	emit_immed(char *source, uint4 size)
{
	char	buff[MAX_IMMED + 8];	/* MAX_IMMED + 2 * SIZEOF(short) + SIZEOF(int4) */
	int	amount, r, s;

	if (run_time)
	{
		if (stringpool.free + size > stringpool.top)
			rts_error(VARLSTCNT(1) ERR_STRINGOFLOW);
		memcpy(stringpool.free, source, size);
		stringpool.free += size;
	}
	else
	{
		/*  Check whether the last piece in the emit_buff is a STO_IMM, and calculate how much of the source
		    can be concatenated with it.  Do not make the last immediate longer than the maximum allowed, do
		    not overflow the buffer, and do not bother if we can only add 4 bytes or less.
		*/
		if (emit_last_immed == NO_IMMED)
		{
			r = 0;
		}
		else
		{
			r = MAX_IMMED - *((int4 *) (emit_buff + emit_last_immed + 2 * SIZEOF(short)));
		}

		s = OBJ_EMIT_BUF_SIZE - emit_buff_used;

		if(r >= CRITICAL  &&  s >= CRITICAL)
		{
			r = (r < s) ? r : s;
			amount = (size < r) ? size : r;
			memcpy(emit_buff + emit_buff_used, source, amount);

			/* emit_last_immed is the offset of the start of the last ETIR$C_STO_IMM command;
                           increment the command size and data count fields.                               */
			*((short *) (emit_buff + emit_last_immed +   SIZEOF(short))) += amount;	/* command size */
			*((int4 *)  (emit_buff + emit_last_immed + 2 * SIZEOF(short))) += amount;	/* data size */
			size -= amount;
			source += amount;
			psect_use_tab[current_psect] += amount;
			emit_buff_used += amount;
		}

		while(size>0)
		{
			amount = (size <= MAX_IMMED) ? size : MAX_IMMED;
			*(short *)(&buff[ETIR$W_RECTYP]) = ETIR$C_STO_IMM;
			*(short *)(&buff[ETIR$W_SIZE]) = amount + 2 * SIZEOF(short) + SIZEOF(int4);	/* command size */
			*(int4 *) (&buff[ETIR$W_SIZE + SIZEOF(short)]) = amount;			/* data size */
			memcpy(buff + 2 * SIZEOF(short) + SIZEOF(int4), source, amount);
			emit_last_immed = emit_buff_used;
			buff_emit(buff, amount + 2 * SIZEOF(short) + SIZEOF(int4));
			size -= amount;
			source += amount;
			psect_use_tab[current_psect] += amount;
		}
	}
}


/*	emit_linkages
 *
 *	Description:	Write symbol addresses to linkage psect.
 */

static struct linkage_entry	*linkage_first, *linkage_last;

void	emit_linkages(void)
{
	struct linkage_entry	*linkagep;

	for (linkagep = linkage_first;  linkagep != 0;  linkagep = linkagep->next)
	{
		assert (psect_use_tab[GTM_LINKAGE] == linkagep->symbol->linkage_offset);
		emit_sto_gbl (linkagep->symbol->name_len, linkagep->symbol->name);
	}
	assert (psect_use_tab[GTM_LINKAGE] == linkage_size);
}


/*
 *	emit_literals
 *
 *	Description:	Write each value in the literal chain to the literal psect.
 */

GBLREF mliteral		literal_chain;
GBLREF char		source_file_name[];
GBLREF unsigned short	source_name_len;

void	emit_literals(void)
{
	uint4		offset, padsize;
	mliteral	*p;

	assert (psect_use_tab[GTM_LITERALS] == 0);
	set_psect(GTM_LITERALS, 0);
	offset = stringpool.free - stringpool.base;
	emit_immed(stringpool.base, offset);
	padsize = PADLEN(offset, NATIVE_WSIZE); /* comp_lits aligns the start of source path on NATIVE_WSIZE boundary.*/
	if (padsize)
	{
		emit_immed(PADCHARS, padsize);
		offset += padsize;
	}
	emit_immed(source_file_name, source_name_len);
	offset += source_name_len;
	padsize = PADLEN(offset, NATIVE_WSIZE); /* comp_lits aligns the start of routine_name on NATIVE_WSIZE boundary.*/
	if (padsize)
	{
		emit_immed(PADCHARS, padsize);
		offset += padsize;
	}
	emit_immed(routine_name.addr, routine_name.len);
	offset += routine_name.len;
	padsize = PADLEN(offset, NATIVE_WSIZE); /* comp_lits aligns the start of literal area on NATIVE_WSIZE boundary.*/
	if (padsize)
	{
		emit_immed(PADCHARS, padsize);
		offset += padsize;
	}

	dqloop(&literal_chain, que, p)
	{
		assert (p->rt_addr >= 0);
		MV_FORCE_NUMD(&p->v);
		if (p->v.str.len != 0)
		{
			emit_immed(&p->v, (int)&p->v.str.addr - (int)&p->v);
			emit_pidr(p->v.str.addr - (char *)stringpool.base, GTM_LITERALS);
			emit_immed(&p->v.m, SIZEOF(p->v.m));	/* assumes no fill between end of p->v.str.addr and start of p->m */
		}
		else
		{
			p->v.str.addr = 0;
			emit_immed(&p->v, SIZEOF(p->v));
		}
		offset += SIZEOF(p->v);
	}
	assert(offset == lits_size);
}


/*
 *	find_linkage
 *
 *	Arg:	the name of a global symbol.
 *
 *	Description:	Return the offset into the linkage psect for the address of a global symbol.
 */

int4	find_linkage(mstr* name)
{
	struct linkage_entry	*newlnk;
	struct sym_table	*sym;

	sym = define_symbol(GTM_LITERALS, name, 0);

	if (sym->linkage_offset == 0)
	{
		/* Add new linkage psect entry at end of list.  */
		sym->linkage_offset = linkage_size;

		newlnk = (struct linkage_entry *) mcalloc(SIZEOF(struct linkage_entry));
		newlnk->symbol = sym;
		newlnk->next = 0;
		if (linkage_first == 0)
			linkage_first = newlnk;
		if (linkage_last != 0)
			linkage_last->next = newlnk;
		linkage_last = newlnk;

		linkage_size += 2 * SIZEOF(int4);
	}

	return sym->linkage_offset;
}


/*
 *	flush_addrs
 *
 *	Description:	For every entry in the address pointer list, generate corresponding linkage psect entry.
 */

void flush_addrs(void)
{
	Addr_ref *addrp;

	if (!addr_list) return;
	set_psect(GTM_LINKAGE, addr_list->index);
	for (addrp = addr_list;  addrp != (void *) 0;  addrp = addrp->next)
	{
		emit_sta_pq(addrp->psect, addrp->offset);
		emit_sto_off();
	}
}


/*
 *	literal_offset
 *
 *	Description:	Return offset to literal from context register.
 *
 *	Argument:	Offset of literal in literal psect.
 */

int4	literal_offset (UINTPTR_T offset)
{
        return (int4)((run_time ? (offset - (UINTPTR_T)runtime_base) : offset));
}


/*
 *	obj_init
 *
 *	Description:	Initialize symbol list, linkage psect list, linkage_size.
 */

void	obj_init(void)
{
	symbols = 0;

	linkage_first = linkage_last = 0;
	linkage_size = MIN_LINK_PSECT_SIZE;	/* minimum size of linkage psect, assuming no references from generated code  */

	return;
}


/*
 *	buff_emit
 *
 *	Args:  buffer pointer, number of bytes to emit
 *
 *	Description:  Does buffered i/o of TIR records.  Assumes that it will
 *		never be given more than OBJ_EMIT_BUF_SIZE-1 bytes to output;
 *		such overlength records should only arise from emit_immed, which
 *		handles the situation by itself.
 */

void	buff_emit(
char	*buff,
short	size)
{

	int	stat;

	if (OBJ_EMIT_BUF_SIZE < (emit_buff_used + size))
	{
		*(short *)(&emit_buff[EOBJ$W_RECTYP]) = EOBJ$C_ETIR;
		*(short *)(&emit_buff[EOBJ$W_SIZE]) = emit_buff_used;
		obj_rab.rab$l_rbf = emit_buff;
		obj_rab.rab$w_rsz = emit_buff_used;
		stat = sys$put(&obj_rab);
		if (stat != RMS$_NORMAL)
			lib$stop(stat);
		emit_buff_used = 2 * SIZEOF(short);
		emit_last_immed = NO_IMMED;
	}
	assert (SIZEOF(emit_buff) >= (emit_buff_used + size));
	memcpy(emit_buff + emit_buff_used, buff, size);
	emit_buff_used += size;
}


/*
 *	emit_lp_rel		Emit Linkage Pair Relocation (via instruction-related store conditional command for linkage)
 *
 *	Args:  TIR store conditional command, linkage index,
 *		psect, offset at which to replace instruction,
 *		replacement instruction,
 *		psect, offset from which to calculate displacement in determining whether to replace instruction,
 *		length of global symbol name, global symbol name
 *
 *	Description:  Issues specified TIR store conditional command.
 *		      These TIR commands instruct the linker to replace the instruction at (psect1 + offset1)
 *			with the specified replacement instruction if the displacement from (psect2 + offset2) to
 *			the procedure descriptor or procedure entry associated with the specified global name
 *			is less than some threshhold value (i.e. will fit into some appropriately-sized bit field).
 *		      These commands are typically used to allow the linker to optimize procedure calls, not by
 *			reducing the number of instructions, but by substituting either faster instructions (e.g.,
 *			NOP in place of LDQ) or by eliminating stalls caused by one instruction needing to wait for
 *			the previous instruction to finish.
 */

void	emit_lp_rel(
int 	reltype,		/* TIR store conditional command for linkage */
int	lpx,			/* linkage index of global symbol */
int	psect1,			/* psect (program section) in which to place replacement instruction */
int	off1,			/* byte offset in psect1 at which to place replacement instruction */
int	repl_ins,		/* replacement instruction */
int	psect2,			/* psect containing base address from which displacement is to be calculated */
int	off2,			/* byte offset in psect2 from which displacement is to be calculated */
int	namelen,		/* length of global symbol name */
char	*name)			/* global symbol name */
{
	int		len;
	register char	*fast;
	char		buf[MAX_REC_SIZE];

	fast = buf;

	*((short *) fast)++ = reltype;
	*((short *) fast)++ = 0;	/* record length */
	*((int4 *) fast)++ = lpx;
	*((int4 *) fast)++ = psect1;
	*((int4 *) fast)++ = off1;
	*((int4 *) fast)++ = 0;
	*((int4 *) fast)++ = repl_ins;
	*((int4 *) fast)++ = psect2;
	*((int4 *) fast)++ = off2;
	*((int4 *) fast)++ = 0;
	*fast++ = namelen;
	memcpy(fast, name, namelen);
	fast += namelen;
	len = ROUND_UP(fast - buf, 8);	/* round up to next quadword boundary */
	*((short *) (buf + 2)) = len;

	buff_emit(buf, len);
	/* No increment of psect_use_tab, because these commands must refer to previously written image locations.  */
	emit_last_immed = NO_IMMED;
}


/*
 *	emit_pidr
 *
 *	Args:  offset from a psect base, that psect index
 *
 *	Description:  Issues TIR records to create an image activation-time adjusted reference.
 *		That is, at activation time, the value in the current image location will be
 *		incremented by the runtime image base address.  Assumes the value is the offset
 *		from the base of the given psect of something to which one wants to refer.
 *
 *		(Originally named after the VAX linker object language command TIR$C_STO_PIDR, for
 *		Position Independent Data Reference; the Alpha implements this with two TIR commands.)
 */

void	emit_pidr(int4 offset, unsigned char psect)
{
	emit_sta_pq(psect, offset);
	emit_sto_lw();
}


/*
 *	emit_psc
 *
 *	Args:	buffer to contain program section (PSECT) definition global symbol directory subrecord,
 *		virtual address boundary at which to align program section,
 *		bit flags indicating attributes of PSECT,
 *		number of bytes in PSECT,
 *		length in bytes of the name of this PSECT, and
 *		the name of this PSECT.
 *
 *	Description:	Puts global symbol directory PSECT definition subrecord into buffer.
 *			Returns pointer to next available byte in buffer.
 */

char	*emit_psc(
char 	*buf,
int	align,
int	flags,
int	alloc,
int	length,
char	*name)
{
	char	*fast;

	fast = buf;
	*((short *) fast)++ = EGSD$C_PSC;
	*((short *) fast)++ = 0;
	*((short *) fast)++ = align;
	*((short *) fast)++ = flags;
	*((int4 *) fast)++ = alloc;
	*((char *) fast)++ = length;
	memcpy(fast, name, length);
	fast += length;
	length = ROUND_UP(fast - buf, 8);
	fast = buf + length;
	((short *) buf)[1] = length;
	return fast;
}


/*
 *	emit_sta_pq		Stack Psect Base Plus Byte Offset
 *
 *	Args:  psect index, byte offset
 *
 *	Description:  Issues TIR command to add program section base and byte offset then push result onto stack.
 */

void	emit_sta_pq (
int	psect,				/* psect (program section) index */
int	offset)				/* byte offset into psect */
{
	char		buf[MAX_REC_SIZE];
	register char	*fast;

	fast = buf;

	*((short *) fast)++ = ETIR$C_STA_PQ;
	*((short *) fast)++ = 2 * SIZEOF(short) + SIZEOF(int4) + 2 * SIZEOF(int4);
	*((int4 *) fast)++ = psect;
	*((int4 *) fast)++ = offset;
	*((int4 *) fast)++ = 0;		/* upper half of offset qw */
	linker_stack_depth++;

	buff_emit(buf, fast - buf);
	emit_last_immed = NO_IMMED;
}


/*
 *	emit_stc_lp_psb		Store Conditional Linkage Pair plus Signature
 *
 *	Args:  linkage index, length of procedure name, procedure name
 *
 *	Description:  Issues TIR command to declare conditional linkage and signature information for the named procedure.
 *		      N.B. this TIR command reserves two indices, lpx and (lpx+1).
 */

void	emit_stc_lp_psb(
int	lpx,				/* linkage pair index */
int	len,				/* length of name */
char	*name)				/* procedure name */
{
	register char	*fast;
	char		buf[MAX_REC_SIZE];

	fast = buf;

	*((short *) fast)++ = ETIR$C_STC_LP_PSB;
	*((short *) fast)++ = 0;	/* record length */
	*((int4 *) fast)++ = lpx;
	*fast++ = len;
	memcpy(fast, name, len);
	fast += len;
	*((char *) fast)++ = 1;		/* signature length */
	*((char *) fast)++ = 0;		/* signature information */
	len = ROUND_UP(fast - buf, 8);	/* round up to next quadword boundary */
	*((short *) (buf + 2)) = len;

	buff_emit(buf, len);
	psect_use_tab[current_psect] += 4 * SIZEOF(int4);	/* two quadwords */
	emit_last_immed = NO_IMMED;
}


/*
 *	emit_sto_gbl		Store Global
 *
 *	Args:  length of global symbol name, global symbol name
 *
 *	Description:  Issues TIR command to store a global symbol reference.
 */

void	emit_sto_gbl(
int	len,			/* length of global symbol name */
char	*name)			/* global symbol name */
{
	register char	*fast;
	char		buf[MAX_REC_SIZE];

	fast = buf;

	*((short *) fast)++ = ETIR$C_STO_GBL;
	*((short *) fast)++ = 0;	/* record length */
	*fast++ = len;
	memcpy(fast, name, len);
	fast += len;
	len = ROUND_UP(fast - buf, 8);	/* round up to next quadword boundary */
	*((short *) (buf + 2)) = len;

	buff_emit(buf, len);
	psect_use_tab[current_psect] += 2 * SIZEOF(int4);
	emit_last_immed = NO_IMMED;
}


/*
 *	emit_sto_lw		Store Longword
 *
 *	Args:  (none)
 *
 *	Description:  Issues TIR command to pop quadword from stack and write low longword to image; value is always treated
 *			as an address and relocated if image is relocatable or fixed up if psect contributed by shareable image.
 */

void	emit_sto_lw(void)
{
	char		buf[MAX_REC_SIZE];
	register char	*fast;

	fast = buf;

	*((short *) fast)++ = ETIR$C_STO_LW;
	*((short *) fast)++ = 2 * SIZEOF(short);
	linker_stack_depth--;

	buff_emit(buf, fast - buf);
	psect_use_tab[current_psect] += SIZEOF(int4);
	emit_last_immed = NO_IMMED;
}


/*
 *	emit_sto_off		Store Offset to Psect
 *
 *	Args:  (none)
 *
 *	Description:  Issues TIR command to pop quadword from stack and write to image; value is always treated as an address
 *			and relocated if image is relocatable or fixed up if psect contributed by shareable image.
 */

void	emit_sto_off(void)
{
	char		buf[MAX_REC_SIZE];
	register char	*fast;

	fast = buf;

	*((short *) fast)++ = ETIR$C_STO_OFF;
	*((short *) fast)++ = 2 * SIZEOF(short);
	linker_stack_depth--;

	buff_emit(buf, fast - buf);
	psect_use_tab[current_psect] += 2 * SIZEOF(int4);
	emit_last_immed = NO_IMMED;
}


/*
 *	output_symbol
 *
 *	Description:	Generate and write global symbol directory (GSD) symbol records for every
 *				global symbol defined or referenced in this module.
 */

#define GSD_DATA_TYPE 0
#define GSD_PROC_TYPE 0

void	output_symbol(void)
{
	int			stat, len, reclen;
	register char		*fast;
	struct sym_table	*sym;

	sym = symbols;
	while (sym)
	{
		len = sym->name_len;

		/* Compute length of record exclusive of symbol name field.  */
		reclen = 2 * SIZEOF(short) + 2 * SIZEOF(char);	/* common record header definition information */
		switch (sym->psect)
		{
		case GTM_ANOTHER_MODULE:	/* reference to psect from some other module */
			reclen += SIZEOF(short);
			break;

		case GTM_CODE:			/* symbol definition */
		case GTM_LINKAGE:		/* symbol definition */
			reclen += SIZEOF(short) + 6 * SIZEOF(int4);
			break;

		default:			/* presumably a reference to symbol defined in this module */
			reclen += SIZEOF(short);
		}
		reclen += 1;			/* space for length of name following */

		/* Determine whether buffer has enough room; if not, flush it first. */
		if (OBJ_SYM_BUF_SIZE < ROUND_UP(sym_buff_used + reclen + len,8))
		{
			((short *) sym_buff)[0] = EOBJ$C_EGSD;
			((short *) sym_buff)[1] = sym_buff_used;
			obj_rab.rab$l_rbf = sym_buff;
			obj_rab.rab$w_rsz = sym_buff_used;
			stat = sys$put(&obj_rab);
			if (stat != RMS$_NORMAL)
				rts_error (VARLSTCNT(6) ERR_OBJFILERR, 2, object_name_len, object_file_name, stat,
					obj_fab.fab$l_stv);
			sym_buff_used = 8;
			emit_last_immed = NO_IMMED;
		}

		/* Add next symbol to buffer.  */
		fast = sym_buff + sym_buff_used;
		*((short *) fast)++ = EGSD$C_SYM;

		*((short *) fast)++ = 0;	/* EGSY$W_SIZE (record length, after padding) */

		/* Global symbol data type -- currently ignored by linker.
		   WARNING: these values need to be defined properly when the linker starts processing them.  [lidral] */
		if (sym->psect == GTM_ANOTHER_MODULE)		/* code for "not defined in this module" */
			*((char *) fast)++ = GSD_PROC_TYPE;
		else
			*((char *) fast)++ = GSD_DATA_TYPE;

		*((char *) fast)++ = '\0';	/* alignment byte (must be zero) */

		switch (sym->psect)
		{
		case GTM_ANOTHER_MODULE:	/* psect from some other module */
			*((short *) fast)++ = 0;			/* ESRF$W_FLAGS = not defined here, not a weak reference */
			break;	/* no more fields in a reference */

		case GTM_CODE:
			*((short *) fast)++ = EGSY$M_DEF | EGSY$M_REL;	/* ESDF$W_FLAGS */
			*((int4 *) fast)++ = sym->value;		/* ESDF$L_VALUE */
			*((int4 *) fast)++ = 0;
			*((int4 *) fast)++ = 0;				/* ESDF$L_CODE_ADDRESS */
			*((int4 *) fast)++ = 0;
			*((int4 *) fast)++ = 0;				/* ESDF$L_CA_PSINDX */
			*((int4 *) fast)++ = sym->psect;		/* ESDF$L_PSINDX */
			break;

		case GTM_LINKAGE:
			*((short *) fast)++ = EGSY$M_DEF | EGSY$M_REL | EGSY$M_NORM;	/* ESDF$W_FLAGS */
			*((int4 *) fast)++ = sym->value;		/* ESDF$L_VALUE */
			*((int4 *) fast)++ = 0;
			*((int4 *) fast)++ = 0;				/* ESDF$L_CODE_ADDRESS = offset in GTM_CODE */
			*((int4 *) fast)++ = 0;
			*((int4 *) fast)++ = GTM_CODE;			/* ESDF$L_CA_PSINDX = psect containing code address */
			*((int4 *) fast)++ = sym->psect;		/* ESDF$L_PSINDX = psect containing procedure descriptor */
			break;

		default:	/* presumably a reference to a symbol defined in this module */
			*((short *) fast)++ = EGSY$M_REL;		/* ESRF$W_FLAGS */
			break; /* no more fields in a reference */
		}

		*((char *) fast)++ = len;				/* ESDF$B_NAMLNG/ESRF$B_NAMLNG */

		memcpy(fast, &sym->name[0], len);

		fast += len;
		len = fast - (sym_buff + sym_buff_used);
		len = ROUND_UP(len, 8);					/* round up to quadword boundary */
		*((short *) (sym_buff + sym_buff_used + 2)) = len;	/* fill in EGSD$C_SYM record length (EGSY$W_SIZE) field */

		sym_buff_used += len;
		sym = sym->next;
	}
	/* flush symbol buffer */
	if (sym_buff_used > 8)
	{
		((short *) sym_buff)[0] = EOBJ$C_EGSD;
		((short *) sym_buff)[1] = sym_buff_used;
		obj_rab.rab$l_rbf = sym_buff;
		obj_rab.rab$w_rsz = sym_buff_used;
		stat = sys$put(&obj_rab);
		if (stat != RMS$_NORMAL)
			rts_error (VARLSTCNT(6) ERR_OBJFILERR, 2, object_name_len, object_file_name, stat, obj_fab.fab$l_stv);
		sym_buff_used = 8;
		emit_last_immed = NO_IMMED;
	}
}


/*
 *	set_psect
 *
 *	Args:  psect index, byte offset
 *
 *	Description:  Issues TIR commands to set image location to the specified psect's base plus the given byte offset.
 *		      Resets current_psect.
 */

void	set_psect(
int	psect,			/* psect (program section) index */
int	offset)			/* byte offset into psect */
{
	register char	*fast;
	char		set_psect_rec[MAX_REC_SIZE]; 		/* TIR records to set the psect */

	assert (offset >= psect_use_tab[psect]);	/* not really necessary, but our code works this way */

	if (current_psect != psect  ||  psect_use_tab[current_psect] != offset)
	{
		emit_sta_pq(psect, offset);

		fast = set_psect_rec;
		*((short *) fast)++ = ETIR$C_CTL_SETRB;	/* pop stack to set relocation base */
		*((short *) fast)++ = 2 * SIZEOF(short);
		linker_stack_depth--;

		buff_emit(set_psect_rec, fast - set_psect_rec);

		current_psect = psect;
		psect_use_tab[psect] = offset;
		emit_last_immed = NO_IMMED;
	}
}
