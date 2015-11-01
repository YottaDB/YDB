/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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
	int4		vartab_ptr;		/* (updated) offset to variable table of current module version */
	short int	vartab_len;		/* (updated) length of variable table of current module version */
	int4		labtab_ptr;
	short int	labtab_len;
	int4		lnrtab_ptr;
	short int	lnrtab_len;
	int4		ptext_ptr;		/* (updated) offset to start of instructions for current module version */
	int4		checksum;
	bool		label_only;		/* was routine compiled for label only entry? */
	unsigned char	zlinked;		/* was routine zlinked? */
	char		filler[2];		/* reserved for future use */
	int4		old_rhead_ptr;
	int4		current_rhead_ptr;	/* (updated) offset to routine header of current module version */
	short int	temp_mvals;		/* (updated) temp_mvals value of current module version */
	unsigned short	temp_size;		/* (updated) temp_size value of current module version */
#if defined(__alpha) || defined(__MVS__)
	int4		*linkage_ptr;		/* (updated) address of linkage Psect of current module version */
	int4		*literal_ptr;		/* (updated) address of literal Psect of current module version */
#endif
} rhdtyp;

typedef struct ihead_struct {
	int4		vartab_ptr;
	short int	vartab_len;
	short int	temp_mvals;
	unsigned short	temp_size;
	int4		fixup_vals_ptr;
	cache_entry	*indce;
	short int	fixup_vals_num;
} ihdtyp;

void indir_lits(ihdtyp *ihead);

typedef mident vent;

typedef struct {
	mident lname;
	int4 laddr;
} lent;

typedef struct
{
	mident rt_name;
	unsigned char *rt_ptr;
}rtn_tables;

typedef struct
{
	mident lab_name;
	int4 lab_ln_ptr;
} lbl_tables;

typedef struct source_line_struct
{
	mstr text_form,ls_text;
	unsigned short lablen,bodystart;
} src_lin_dsc;

int get_src_line(mval *routine, mval *label, int offset, src_lin_dsc **srcret);
unsigned char *find_line_start(unsigned char *in_addr, rhdtyp *routine);
int4 *find_line_addr(rhdtyp *routine, mstr *label, short int offset);
rhdtyp *find_rtn_hdr(mstr *name);
bool zlput_rname(rhdtyp *hdr);
rhdtyp *make_dmode(void);
void comp_lits(rhdtyp *rhead);
rhdtyp  *op_rhdaddr(mval *name, rhdtyp *rhd);
int4 *op_labaddr(rhdtyp *routine, mval *label, int4 offset);
void urx_resolve( rhdtyp *rtn, lent *lbl_tab, lent *lbl_top );

#endif
