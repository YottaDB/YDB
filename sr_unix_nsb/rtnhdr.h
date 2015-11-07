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
#ifndef RTNHDR_H_INCLUDED
#define RTNHDR_H_INCLUDED

/* rtnhdr.h - routine header */

/* There are several references to this structure from assembly language; these include:
 *
 * 	From VAX VMS:	G_MSF.MAX,
 * 			GTM$FGNCALL.MAR, FGNCAL_RTN.MAR
 *
 * 	From Alpha VMS:	G_MSF.MAX,
 * 			GTM$FGNCAL.M64, FGNCAL_RTN.M64
 *
 * 	From Unix:	g_msf.si
 *
 * Any changes to the routine header must be reflected in those files as well.
 *
 * Warning: the lists above may not be complete.
 */

/* rhead_struct is the routine header; it occurs at the beginning of the
 * object code part of each module.
 *
 * The routine header is initialized when a module is first linked into
 * an executable.  If a new version of that module is subsequently ZLINK'ed
 * into a running image, some of the fields will be updated to describe
 * the new version of the module so that existing references from other
 * modules to earlier versions of this module will be re-directed to the
 * current version.
 */

/* Macro to determine if given address is inside code segment. Note that even though
 *    the PTEXT_END_ADR macro is the address of end_of_code + 1, we still want a <= check
 *    here because in many cases, the address being tested is the RETURN address from a
 *    call that was done as the last instruction in the code segment. Sometimes this call
 *    is to an error or it could be the implicit quit. On HPUX, the delay slot for the
 *    implicit quit call at the end of the module can also cause the problem. Without
 *    the "=" check also being there, the test will fail when it should succeed.
 */
#define ADDR_IN_CODE(caddr, rtnhdr) (PTEXT_ADR((rtnhdr)) <= (caddr) && (caddr) <= PTEXT_END_ADR((rtnhdr)))

/* Types that are different across the versions */
#define LABENT_LNR_OFFSET lab_ln_ptr

/* Variable table entry */
typedef mname_entry var_tabent; /* the actual variable name is stored in the literal text pool */

/* Line number table entry */
typedef int4 lnr_tabent;

typedef struct
{
	mident		lab_name;		/* The name of the label */
	int4 		lab_ln_ptr;		/* Offset of the lnrtab entry from the routine header */
	boolean_t	has_parms;		/* Flag to indicate whether the callee has a formallist */
} lab_tabent;

/* Label table entry proxy for run-time linking */
typedef struct
{
	int4 		lab_ln_ptr;		/* Offset of the lnrtab entry from the routine header */
	boolean_t	has_parms;		/* Flag to indicate whether the callee has a formallist */
} lab_tabent_proxy;

typedef struct	rhead_struct
{
	char		jsb[RHEAD_JSB_SIZE];
	mstr		src_full_name;		/* (updated) full source name of current module version */
	mident		routine_name;
	int4		vartab_off;		/* (updated) offset to variable table of current module version */
	int4		vartab_len;		/* (updated) length of variable table of current module version */
	int4		labtab_off;
	int4		labtab_len;
	int4		lnrtab_off;
	int4		lnrtab_len;
	int4		ptext_off;		/* (updated) offset to start of instructions for current module version */
	int4		checksum;
	uint4		compiler_qlf;		/* bit flags of compiler qualifiers used (see cmd_qlf.h) */
	int4		old_rhead_off;
	int4		current_rhead_off;	/* (updated) offset to routine header of current module version */
	int4		temp_mvals;		/* (updated) temp_mvals value of current module version */
	int4		temp_size;		/* (updated) temp_size value of current module version */
#	ifdef GTM_TRIGGER
	void_ptr_t	trigr_handle;		/* Type is void to avoid needing gv_trigger.h to define gv_trigger_t addr */
#	endif
} rhdtyp;

/* Routine table entry */
typedef struct
{
	mident		rt_name;	/* The name of the routine (in the literal text pool) */
	rhdtyp		*rt_adr;	/* Pointer to its routine header */
} rtn_tabent;

/* Although the names change from _ptr to _off is politically correct, (they ARE offsets, not pointers),
 * there is a lot of old code, espcially platform dependent code, that still deals with _ptr that we
 * do not wish to change at this time. Provide some translations for those entries to the proper ones.
 */
#define vartab_ptr	vartab_off
#define labtab_ptr	labtab_off
#define lnrtab_ptr	lnrtab_off
#define ptext_ptr  	ptext_off
#define old_rhead_ptr	old_rhead_off
#define current_rhead_ptr current_rhead_off

/* Macros for accessing routine header fields in a portable way */
#define VARTAB_ADR(rtnhdr) ((var_tabent *)((char *)(rtnhdr) + (rtnhdr)->vartab_off))
#define LABTAB_ADR(rtnhdr) ((lab_tabent *)((char *)(rtnhdr) + (rtnhdr)->labtab_off))
#define LNRTAB_ADR(rtnhdr) ((lnr_tabent *)((char *)(rtnhdr) + (rtnhdr)->lnrtab_off))
#define LITERAL_ADR(rtnhdr) ((unsigned char *)(rtnhdr)->literal_ptr)
#define LINKAGE_ADR(rtnhdr) ((caddr_t)NULL)
#define PTEXT_ADR(rtnhdr) ((unsigned char *)((char *)(rtnhdr) + (rtnhdr)->ptext_off))
#define PTEXT_END_ADR(rtnhdr) ((unsigned char *)((char *)(rtnhdr) + (rtnhdr)->vartab_off))
#define CURRENT_RHEAD_ADR(rtnhdr) ((rhdtyp *)((char *)(rtnhdr) + (rtnhdr)->current_rhead_off))
#define OLD_RHEAD_ADR(rtnhdr) ((rhdtyp *)((char *)(rtnhdr) + (rtnhdr)->old_rhead_off))
#define LINE_NUMBER_ADDR(rtnhdr, lnr_tabent_ptr) ((unsigned char *)((char *)(rtnhdr) + *(lnr_tabent_ptr)))
#define LABENT_LNR_ENTRY(rtnhdr, lab_tabent_ptr) ((lnr_tabent *)((char *)(rtnhdr) + (lab_tabent_ptr)->lab_ln_ptr))
#define LABEL_ADDR(rtnhdr, lab_tabent_ptr)(CODE_BASE_ADDR(rtnhdr) + *(LABENT_LNR_ENTRY(rtnhdr, lab_tabent_ptr)))
#define CODE_BASE_ADDR(rtnhdr) ((unsigned char *)(rtnhdr))
#define CODE_OFFSET(rtnhdr, addr) ((char *)(addr) - (char *)(rtnhdr))

#define DYNAMIC_LITERALS_ENABLED(rtnhdr) FALSE
#define RW_REL_START_ADR(rtnhdr) ((char *)LITERAL_ADR(rtnhdr))

/* Flag values for get_src_line call */
#define VERIFY		TRUE
#define NOVERIFY	FALSE

int get_src_line(mval *routine, mval *label, int offset, mstr **srcret, boolean_t verifytrig);
unsigned char *find_line_start(unsigned char *in_addr, rhdtyp *routine);
int4 *find_line_addr(rhdtyp *routine, mstr *label, int4 offset, mident **lent_name);
rhdtyp *find_rtn_hdr(mstr *name);
bool zlput_rname(rhdtyp *hdr);
rhdtyp *make_dmode(void);
void comp_lits(rhdtyp *rhead);
rhdtyp  *op_rhdaddr(mval *name, rhdtyp *rhd);
lnr_tabent *op_labaddr(rhdtyp *routine, mval *label, int4 offset);
void urx_resolve(rhdtyp *rtn, lab_tabent *lbl_tab, lab_tabent *lbl_top);
char *rtnlaboff2entryref(char *entryref_buff, mident *rtn, mident *lab, int offset);

#endif /* RTNHDR_H_INCLUDED */
