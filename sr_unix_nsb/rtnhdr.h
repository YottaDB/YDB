/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __RTNHDR_H__
#define __RTNHDR_H__

/* rtnhdr.h - routine header */
#include "cache.h"

/* There are several references to this structure from assembly language; these include:

	From VAX VMS:	G_MSF.MAX,
			GTM$FGNCALL.MAR, FGNCAL_RTN.MAR

	From Alpha VMS:	G_MSF.MAX,
			GTM$FGNCAL.M64, FGNCAL_RTN.M64

	From Unix:	g_msf.si

   Any changes to the routine header must be reflected in those files as well.

   Warning: the lists above may not be complete.
*/

/*	rhead_struct is the routine header; it occurs at the beginning of the
	object code part of each module.

	The routine header is initialized when a module is first linked into
	an executable.  If a new version of that module is subsequently ZLINK'ed
	into a running image, some of the fields will be updated to describe
	the new version of the module so that existing references from other
	modules to earlier versions of this module will be re-directed to the
	current version.
*/

typedef struct	rhead_struct
{
	char		jsb[RHEAD_JSB_SIZE];
	mstr		src_full_name;		/* (updated) full source name of current module version */
	mident		routine_name;
	int4		vartab_off;		/* (updated) offset to variable table of current module version */
	short int	vartab_len;		/* (updated) length of variable table of current module version */
	int4		labtab_off;
	short int	labtab_len;
	int4		lnrtab_off;
	short int	lnrtab_len;
	int4		ptext_off;		/* (updated) offset to start of instructions for current module version */
	int4		checksum;
	bool		label_only;		/* was routine compiled for label only entry? */
	unsigned char	zlinked;		/* was routine zlinked? */
	char		filler[2];		/* reserved for future use */
	int4		old_rhead_off;
	int4		current_rhead_off;	/* (updated) offset to routine header of current module version */
	short int	temp_mvals;		/* (updated) temp_mvals value of current module version */
	unsigned short	temp_size;		/* (updated) temp_size value of current module version */
#if defined(__alpha) || defined(__MVS__) || defined(__s390__)
	int4		*linkage_ptr;		/* (updated) address of linkage Psect of current module version */
	unsigned char	*literal_ptr;		/* (updated) address of literal Psect of current module version */
#endif
} rhdtyp;

/* Although the names change from _ptr to _off is politically correct, (they ARE offsets, not pointers),
   there is a lot of old code, espcially platform dependent code, that still deals with _ptr that we
   do not wish to change at this time. Provide some translations for those entries to the proper ones.
*/
#define vartab_ptr	vartab_off
#define labtab_ptr	labtab_off
#define lnrtab_ptr	lnrtab_off
#define ptext_ptr  	ptext_off
#define old_rhead_ptr	old_rhead_off
#define current_rhead_ptr current_rhead_off

/* Macros for accessing routine header fields in a portable way */
#define VARTAB_ADR(rtnhdr) ((VAR_TABENT *)((char *)(rtnhdr) + (rtnhdr)->vartab_off))
#define LABTAB_ADR(rtnhdr) ((LAB_TABENT *)((char *)(rtnhdr) + (rtnhdr)->labtab_off))
#define LNRTAB_ADR(rtnhdr) ((LNR_TABENT *)((char *)(rtnhdr) + (rtnhdr)->lnrtab_off))
#define LITERAL_ADR(rtnhdr) ((unsigned char *)(rtnhdr)->literal_ptr)
#define LINKAGE_ADR(rtnhdr) ((caddr_t)(rtnhdr)->linkage_ptr)
#define PTEXT_ADR(rtnhdr) ((unsigned char *)((char *)(rtnhdr) + (rtnhdr)->ptext_off))
#define PTEXT_END_ADR(rtnhdr) ((unsigned char *)((char *)(rtnhdr) + (rtnhdr)->vartab_off))
#define CURRENT_RHEAD_ADR(rtnhdr) ((rhdtyp *)((char *)(rtnhdr) + (rtnhdr)->current_rhead_off))
#define OLD_RHEAD_ADR(rtnhdr) ((rhdtyp *)((char *)(rtnhdr) + (rtnhdr)->old_rhead_off))
#define LINE_NUMBER_ADDR(rtnhdr, lnr_tabent_ptr) ((unsigned char *)((char *)(rtnhdr) + *(lnr_tabent_ptr)))
#define LABENT_LNR_ENTRY(rtnhdr, lab_tabent_ptr) ((LNR_TABENT *)((char *)(rtnhdr) + (lab_tabent_ptr)->lab_ln_ptr))
#define LABEL_ADDR(rtnhdr, lab_tabent_ptr)(CODE_BASE_ADDR(rtnhdr) + *(LABENT_LNR_ENTRY(rtnhdr, lab_tabent_ptr)))
#define CODE_BASE_ADDR(rtnhdr) ((unsigned char *)(rtnhdr))
#define CODE_OFFSET(rtnhdr, addr) ((char *)(addr) - (char *)(rtnhdr))

/* Macro to determine if given address is inside code segment. Note that even though
   the PTEXT_END_ADR macro is the address of end_of_code + 1, we still want a <= check
   here because in many cases, the address being tested is the RETURN address from a
   call that was done as the last instruction in the code segment. Sometimes this call
   is to an error or it could be the implicit quit. On HPUX, the delay slot for the
   implicit quit call at the end of the module can also cause the problem. Without
   the "=" check also being there, the test will fail when it should succeed.
*/
#define ADDR_IN_CODE(caddr, rtnhdr) (PTEXT_ADR((rtnhdr)) <= (caddr) && (caddr) <= PTEXT_END_ADR((rtnhdr)))

/* Types that are different across the versions */
#define LAB_TABENT lbl_tables
#define LNR_TABENT int4
#define RTN_TABENT rtn_tables
#define VAR_TABENT vent
#define LABENT_LNR_OFFSET lab_ln_ptr
#define RTNENT_RT_ADR rt_ptr

typedef struct ihead_struct
{
	cache_entry	*indce;
	int4		vartab_off;
	int4		vartab_len;
	int4		temp_mvals;
	int4		temp_size;
	int4		fixup_vals_off;
	int4		fixup_vals_num;
} ihdtyp;

void indir_lits(ihdtyp *ihead);

typedef mident	vent;

/* use of lent deprecated .. same as lbl_tables below */
typedef struct
{
	mident	lname;
	int4	laddr;
} lent;

typedef struct
{
	mident	rt_name;
	rhdtyp	*rt_ptr;
} rtn_tables;

typedef struct
{
	mident lab_name;
	int4 lab_ln_ptr;
} lbl_tables;

int get_src_line(mval *routine, mval *label, int offset, mstr **srcret);
unsigned char *find_line_start(unsigned char *in_addr, rhdtyp *routine);
int4 *find_line_addr(rhdtyp *routine, mstr *label, short int offset);
rhdtyp *find_rtn_hdr(mstr *name);
bool zlput_rname(rhdtyp *hdr);
rhdtyp *make_dmode(void);
void comp_lits(rhdtyp *rhead);
rhdtyp  *op_rhdaddr(mval *name, rhdtyp *rhd);
LNR_TABENT *op_labaddr(rhdtyp *routine, mval *label, int4 offset);
VMS_ONLY(void urx_resolve(rhdtyp *rtn, LAB_TABENT *lbl_tab, LAB_TABENT *lbl_top);)
UNIX_ONLY(void urx_resolve(rhdtyp *rtn, lent *lbl_tab, lent *lbl_top);)
char *rtnlaboff2entryref(char *entryref_buff, mstr *rtn, mstr *lab, int offset);

#endif
