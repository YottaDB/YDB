/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_TRIGGER_H
#define GTM_TRIGGER_H

#include "gtm_trigger_trc.h"

#define TRIGGER_NAME_RESERVED_SPACE	2	/* Reserved space in trigger name to make name unique if needbe */

typedef enum
{	/* Trigger rethrow types */
	trigr_rethrow_nil,
	trigr_rethrow_zg1,			/* Rethrow level-only ZGOTO (op_zg1) */
	trigr_rethrow_zgoto,			/* Rethrow level/entryref ZGOTO (op_zgoto) */
	trigr_rethrow_goerrorfrm		/* Rethrow goerrorframe() unwind */
} trigr_rethrow_t;

/* Second parm to gtm_trigger() containing non-static type items for a given trigger */
typedef struct
{
	mval		*ztoldval_new;		/* mval to set into $ZTOLDVAL.
						 * Points to stringpool at "gtm_trigger" entry - is NOT updated by the function */
	mval		*ztvalue_new;		/* mval for current value to set into $ZTVALUE.
						 * Points to stringpool at "gtm_trigger" entry.
						 */
	const mval	*ztdata_new;		/* $Data status of trigger value being Set/Killed - points to a constant mval
						 * in mtables.c */
	const mval	*ztriggerop_new;	/* Opcode type invoking the trigger - points to a constant mval in mtables.c */
	mval		*ztupdate_new;		/* mval to set into $ZTUPDATE.
						 * Points to stringpool at "gtm_trigger" entry - is NOT updated by the function */
	mval		**lvvalarray;		/* Value array for lvs named in "lvnamearray" member of gv_trigger_t structure.
						 * Values are derived from subscripts of matching key at runtime.
						 * Note that this points to an array of "mval *" each of which in turn point to
						 * mvals in the M-stack whose strings point to the stringpool.
						 * For performance reasons, we keep this an array of trigdsc->numsubs entries
						 * (not trigdsc->numlvsubs entries). So "gtm_trigger" has to look at
						 * trigdsc->lvindexarray and figure out the actual indices which correspond to
						 * local variables of interest to that trigdsc and obtain the corresponding
						 * indices from this array to get the actual local variable values.
						 * Points to stringpool at "gtm_trigger" entry - is NOT updated by the function */
	boolean_t	ztvalue_changed;	/* Set to TRUE if $ZTVALUE was changed inside the trigger. "ztvalue_new" member
						 * is updated to point to the new value (in the stringpool) in this case.
						 */
	/* All above "mval *" fields that point to the stringpool have their corresponding mvals pushed onto the M-stack
	 * so they are safe from stp_gcol already. No special care needs to be taken by the function "gtm_trigger" for this.
	 */
} gtm_trigger_parms;

int gtm_trigger(gv_trigger_t *trigdsc, gtm_trigger_parms *trigprm);
void gtm_trigger_fini(boolean_t forced_unwind);
void gtm_trigger_cleanup(gv_trigger_t *trigdsc);
int gtm_trigger_complink(gv_trigger_t *trigdsc, boolean_t dolink);

#endif
