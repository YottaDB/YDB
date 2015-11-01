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

#ifndef __STACK_FRAME_H__
#define __STACK_FRAME_H__

/* stack_frame.h - GT.M MUMPS stack frame */

/* There are several references to this structure from assembly language; these include:

	From VMS:	G_MSF.MAX
	From Vax:       stack_frame_copy.mar
	From OS390:	stack_frame.h (it's own version)
	From Unix:	g_msf.si

   Any changes to the stack frame must be reflected in those files as well.

   Warning: the lists above may not be complete.
*/

typedef struct stack_frame_struct	/* contents of the GT.M MUMPS stack frame */
{
	struct rhead_struct *rvector;	/* routine header */
	mval		**l_symtab;	/* local symbol table */
	unsigned char	*mpc;		/* mumps program counter */
	unsigned char	*ctxt;		/* context pointer (base register for use when there's no PC-relative address mode) */
#ifdef __alpha
	int4		*literal_ptr;	/* pointer to base of literals */
#endif
	unsigned char	*temps_ptr;	/* pointer to base of temps */
	char		*vartab_ptr;	/* variable table may be in rvector or on stack */
	short		vartab_len;	/* variable table length */
	short           temp_mvals;     /* temp mval count if this frame for an indirect rtn (ihdtyp) */
	struct stack_frame_struct *old_frame_pointer;
	/* note the alignment of these next two fields is critical to correct operation of
	   opp_ret on the VMS platforms. Any changes here need to be reflected there.
	*/
	unsigned char	type;
	unsigned char	flags;
} stack_frame;

/* Stack frame types */
#define SFT_COUNT	(1 << 0)	/* frame counts (real code) or doesn't (transcendental code) */
#define SFT_DM		(1 << 1)	/* direct mode */
#define SFT_REP_OP	(1 << 2)	/* frame to replace opcode (zbreak has already occured) */
#define SFT_ZBRK_ACT	(1 << 3)	/* action frame for zbreak */
#define SFT_DEV_ACT	(1 << 4)	/* action frame for device error handler */
#define SFT_ZTRAP	(1 << 5)	/* ztrap frame */
#define SFT_EXTFUN	(1 << 6)	/* extfun frame */
#define SFT_ZSTEP_ACT	(1 << 7)	/* action frame for a zstep */

/* Flags for flag byte */
#define SFF_INDCE	(1 << 0)	/* This frame is executing an indirect cache entry */
#define SFF_ZTRAP_ERR 	(1 << 1)	/* error occured during $ZTRAP compilation */
#define SFF_DEV_ACT_ERR	(1 << 2)	/* compilation error occured in device exception handler */
#define SFF_INDCE_OFF   	~(SFF_INDCE)		/* Mask to turn off SFF_INDCE */
#define SFF_ZTRAP_ERR_OFF	~(SFF_ZTRAP_ERR)	/* Mask to turn off SFF_ZTRAP_ERR */
#define SFF_DEV_ACT_ERR_OFF	~(SFF_DEV_ACT_ERR)	/* Mask to turn off SFF_DEV_ACT_ERR */

void new_stack_frame(rhdtyp *rtn_base, unsigned char *context, unsigned char *transfer_addr);
void new_stack_frame_sp(rhdtyp *rtn_base, unsigned char *context, unsigned char *transfer_addr);
int4 symbinit(void);
unsigned char *get_symb_line (unsigned char *out, unsigned char **b_line,
	unsigned char **ctxt);
unsigned char *symb_line(unsigned char *in_addr, unsigned char *out, unsigned char **b_line,
	rhdtyp *routine);
void copy_stack_frame(void);
void copy_stack_frame_sp(void);
void exfun_frame(void);
void exfun_frame_sp(void);
void exfun_frame_push_dummy_frame(void);
void base_frame(rhdtyp *base_address);
void pseudo_ret(void);
void call_dm(void);
int dm_start(void);
stack_frame *op_get_msf (void);
int op_zbreak(stack_frame *fp);

#endif
