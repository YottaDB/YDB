/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
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
#include "opcode.h"
#include "toktyp.h"
#include "advancewindow.h"
#include "mlabel2xtern.h"
#include "mrout2xtern.h"
#include "gtmimagename.h"
#include <rtnhdr.h>
#include "stack_frame.h"

STATICFNDCL oprtype insert_extref(mident *rtnname);
STATICFNDCL triple *insert_extref_fast(opctype op1, opctype op2, mident *rtnname, mident *labname);

#define CONVERT_MUTIL_NAME_PERCENT_TO_UNDERSCORE(RTNNAME)	\
{								\
	if ((RTNNAME)->addr[0] == '%')				\
		(RTNNAME)->addr[0] = '_';			\
}
#define IS_SAME_RTN(NAME1, NAME2) (MIDENT_EQ(NAME1, NAME2))

#define INSERT_EXTREF_NOMORPH_AND_RETURN(RTNNAME)		\
{								\
	oprtype 	routine, rte1;				\
	triple		*ref;					\
								\
	rte1 = put_str((RTNNAME)->addr, (RTNNAME)->len);	\
	CONVERT_MUTIL_NAME_PERCENT_TO_UNDERSCORE((RTNNAME));	\
	routine = put_cdlt((RTNNAME));				\
	ref = newtriple(OC_RHDADDR);				\
	ref->operand[0] = rte1;					\
	ref->operand[1] = routine;				\
	routine = put_tref(ref);				\
	return routine;						\
}

GBLREF stack_frame	*frame_pointer;
GBLREF mident		routine_name;

error_def(ERR_LABELEXPECTED);
error_def(ERR_RTNNAME);

/* Compiler entry point to parse an entry ref generating the necessary triples for just the actual call (does not handle
 * routine parameters).
 *
 * Parameters:
 *
 *   op1   	 - Opcode to use if this is a local call (call within current routine).
 *   op2   	 - Opcode to use if this is, or needs to be treated as, an external call. This latter is for auto-relink so if a
 *	     	   new version of a routine exists, we effectively treat it as an external call since the call goes to a different
 *	     	   flavor of the routine.
 *   commargcode - What type of command this is for (code from indir_* enum in indir.h)
 *   can_commarg - Indicates whether or not routine is allowed to call commarg to deal with indirects. Currently the only
 *		   routine to set this to false is m_zgoto.c since it already deals with indirects its own way.
 *   labref	 - Only TRUE for calls frome exfunc. When TRUE, label offsets are not allowed.
 *   textname	 - Only TRUE for ZGOTO related calls where the routine/label names are passed as text instead of resolved to
 *		   linkage table entries.
 */
triple *entryref(opctype op1, opctype op2, mint commargcode, boolean_t can_commarg, boolean_t labref, boolean_t textname)
{
	oprtype 	offset, label, routine, rte1;
	char		rtn_text[SIZEOF(mident_fixed)], lab_text[SIZEOF(mident_fixed)];
	mident		rtnname, labname;
	mstr 		rtn_str, lbl_str;
	triple 		*ref, *next, *rettrip;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* There is no current case where both can_commarg and textname parameters are TRUE. If they start to exist, the code in
	 * this routine needs to be revisited for proper operation as the textname conditions were assumed not to happen if
	 * can_commarg was FALSE (which it is in the one known use of textname TRUE - in m_zgoto). Assert this now.
	 */
	assert(!(can_commarg && textname));
	/* Initialize our routine and label identifier midents */
	rtnname.len = labname.len = 0;
	rtnname.addr = &rtn_text[0];
	labname.addr = &lab_text[0];
	/* Discover what sort of entryref we have */
	switch (TREF(window_token))
	{
 		case TK_INTLIT:
			int_label();
			/* caution: fall through */
		case TK_IDENT:
			memcpy(labname.addr, (TREF(window_ident)).addr, (TREF(window_ident)).len);
			labname.len = (TREF(window_ident)).len;
			advancewindow();
			if ((TK_PLUS != TREF(window_token)) && (TK_CIRCUMFLEX != TREF(window_token)) && !IS_MCODE_RUNNING
			    && can_commarg)
			{	/* Only label specified - implies current routine */
				rettrip = newtriple(op1);
				rettrip->operand[0] =  put_mlab(&labname);
				return rettrip;
			}
			label.oprclass = NO_REF;
			break;
		case TK_ATSIGN:
			if(!indirection(&label))
				return NULL;
			if ((TK_PLUS != TREF(window_token)) && (TK_CIRCUMFLEX != TREF(window_token))
			    && (TK_COLON != TREF(window_token)) && can_commarg)
			{	/* Have a single indirect arg like @ARG - not @LBL^[@]rtn or @LBL+1, etc. */
				rettrip = ref = maketriple(OC_COMMARG);
				ref->operand[0] = label;
				ref->operand[1] = put_ilit(commargcode);
				ins_triple(ref);
				return rettrip;
			}
			labname.len = 0;
			break;
		case TK_PLUS:
			stx_error(ERR_LABELEXPECTED);
			return NULL;
		default:
			labname.len = 0;
			label.oprclass = NO_REF;
			break;
	}
	if (!labref && (TK_PLUS == TREF(window_token)))
	{	/* Have line offset allowed and specified */
		advancewindow();
		if (EXPR_FAIL == expr(&offset, MUMPS_INT))
			return NULL;
	} else
		offset.oprclass = NO_REF;
	if (TK_CIRCUMFLEX == TREF(window_token))
	{	/* Have a routine name specified */
		advancewindow();
		switch (TREF(window_token))
		{
			case TK_IDENT:
				MROUT2XTERN((TREF(window_ident)).addr, rtnname.addr, (TREF(window_ident)).len);
				rtnname.len = (TREF(window_ident)).len;
				advancewindow();
				if (!IS_MCODE_RUNNING)
				{	/* Triples for normal compiled code. */
					if (!textname)
					{	/* Triples for DO, GOTO, extrinsic functions ($$). Resolve routine and label names
						 * to addresses for most calls.
						 */
						if ((NO_REF == label.oprclass) && (NO_REF == offset.oprclass))
						{	/* Do LABEL^RTN comes here (LABEL is *not* indirect *and* no offset) */
							rettrip = insert_extref_fast(op1, op2, &rtnname, &labname);
							return rettrip;
						} else
							/* Label or offset was indirect so can't use linkage table to address
							 * the pieces - and no autorelink either
							 */
							routine = insert_extref(&rtnname);
					} else
					{	/* Triples for ZGOTO. Pass routine and label names as text literals */
						if ((NO_REF == label.oprclass) && (NO_REF == offset.oprclass))
						{	/* Both label and routine supplied but no offset */
							rettrip = maketriple(op2);
							rettrip->operand[0] = put_str(rtnname.addr, rtnname.len);
							ref = newtriple(OC_PARAMETER);
							ref->operand[0] = put_str(labname.addr, labname.len);
							ref->operand[1] = put_ilit(0);
							rettrip->operand[1] = put_tref(ref);
							ins_triple(rettrip);
							return rettrip;
						} else
							/* Routine only (no label - may have offset) */
							routine = put_str(rtnname.addr, rtnname.len);
					}
				} else
				{	/* Triples for indirect code (at indirect compile time) */
					routine = put_str(rtnname.addr, rtnname.len);
					if (!textname)
					{	/* If not returning text name, convert text name to routine header address */
						ref = newtriple(OC_RHDADDR1);
						ref->operand[0] = routine;
						routine = put_tref(ref);
					}
				}
				break;
			case TK_ATSIGN:
				if (!indirection(&routine))
					return NULL;
				if (!textname)
				{	/* If not returning text name, convert text name to routine header address */
					ref = newtriple(OC_RHDADDR1);
					ref->operand[0] = routine;
					routine = put_tref(ref);
				}
				break;
			default:
				stx_error(ERR_RTNNAME);
				return NULL;
		}
	} else
	{
		if ((NO_REF == label.oprclass) && (0 == labname.len))
		{
			stx_error(ERR_LABELEXPECTED);
			return NULL;
		}
		if (!textname)
			routine = put_tref(newtriple(OC_CURRHD));
		else
		{	/* If we need a name, the mechanism to retrieve it differs between normal and indirect compilation.
			 * For normal compile, use routine name set when started compile. Routine name can vary. For textname=TRUE
			 * callers (zgoto) fetch name from frame_pointer at runtime.
			 */
			routine = put_str("", 0);
		}
	}
	if (NO_REF == offset.oprclass)
		/* No offset supplied - supply default */
		offset = put_ilit(0);
	if (NO_REF == label.oprclass)
		/* No label indirect - value resides in labname so make a proper parameter out of it */
		label = put_str(labname.addr, labname.len);
	ref = textname ? newtriple(OC_PARAMETER) : newtriple(OC_LABADDR);
	ref->operand[0] = label;
	next = newtriple(OC_PARAMETER);
	ref->operand[1] = put_tref(next);
	next->operand[0] = offset;
	if (!textname)
		next->operand[1] = routine;	/* Not needed if giving text names */
	rettrip = next = newtriple(op2);
	next->operand[0] = routine;
	next->operand[1] = put_tref(ref);
	return rettrip;
}

#ifdef USHBIN_SUPPORTED
/* Routine used in a USHBIN build (UNIX Shared BInary) to generate the triples to reference (call, extrinsic or goto)
 * an external routine. This version calls additional opcodes to dynamically autorelink routines if the routine has been
 * changed since it was last linked if autorelinking is enabled. This requires that all such M-calls with a routine specified
 * as part of the call (i.e. DO label^routine instead of just DO label) be invoked as external routines so this routine
 * creates an external reference to the given routine/label. Note indirects do not come through here.
 *
 * Parameters:
 *
 *   op1     - Opcode to use for local routine (only passed into this flavor for compatibility with the NON-USHBIN version
 *             that follows below. This routine does not use it.
 *   op2     - Opcode to use for external call.
 *   rtnname - Text name of routine being referenced.
 *   labname - Text label being referenced in given routine (may be NULL string)
 *
 * Note this routine sets up the complete reference (label and routine) so returning from entryref() is expected after calling
 * this routine.
 */
STATICFNDEF triple *insert_extref_fast(opctype op1, opctype op2, mident *rtnname, mident *labname)
{
	triple 		*intermed1, *intermed2, *rettrip, *next, *ref;
	mstr		lbl_str;

	intermed1 = maketriple(OC_RHD_EXT);
	ref = intermed1;
	ref->operand[0] = put_str(rtnname->addr, rtnname->len);	/* arg 1 */
	next = newtriple(OC_PARAMETER);
	ref->operand[1] = put_tref(next);
	ref = next;
	ref->operand[0] = put_str(labname->addr, labname->len);	/* arg 2 */
	next = newtriple(OC_PARAMETER);
	ref->operand[1] = put_tref(next);
	ref = next;
	ref->operand[0] = put_cdlt(rtnname);			/* arg 3 */
	next = newtriple(OC_PARAMETER);
	ref->operand[1] = put_tref(next);
	ref = next;
	CONVERT_MUTIL_NAME_PERCENT_TO_UNDERSCORE(rtnname);
	mlabel2xtern(&lbl_str, rtnname, labname);
	ref->operand[0] = put_cdlt(&lbl_str);			/* arg 4 */
	ins_triple(intermed1);
	/* op_rhd_ext needs to return two pointers: one to a rtn hdr, and one to
	 * a line number entry (label). op_rhd_ext directly returns the rtn hdr
	 * and sets lab_proxy, which op_lab_ext fetches and feeds into the opcode
	 * doing the reference (DO, extrinsics, and ZBREAK).
	 */
	intermed2 = newtriple(OC_LAB_EXT); /* extracts, returns global variable */
	rettrip = newtriple(op2);
	rettrip->operand[0] = put_tref(intermed1);
	rettrip->operand[1] = put_tref(intermed2);
	return rettrip;
}

/* This routine is called to generate a triple for a given routine reference when the label is indirect and the routine
 * isn't. Since an indirect is involved, this form generates individual triples for resolving the routine and label
 * addresses with this routine generating only the routine resolution triples. Since this is for a USHBIN build, all
 * references where a routine name is provided generate an external call type call so auto-relinking can occur if it
 * is enabled.
 */
STATICFNDEF oprtype insert_extref(mident *rtnname)
{	/* Generate routine reference triple */
	INSERT_EXTREF_NOMORPH_AND_RETURN(rtnname);
}

#else
/* Routine used in a NON_USHBIN build to generate triples for a label reference (call, function invocation or goto) a routine.
 * At this point, it could be an internal or external routine. Note indirects do not come here.
 *
 * Parameters:
 *
 *   same as described in the USHBIN version of this routine above except both op1 and op2 are used.
 */
STATICFNDEF triple *insert_extref_fast(opctype op1, opctype op2, mident *rtnname, mident *labname)
{
	triple 		*rettrip;
	mstr		lbl_str;

	/* Do LABEL^RTN comes here (LABEL is *not* indirect) */
	if (IS_SAME_RTN(rtnname, &routine_name))
	{       /* If same routine as current routine, we can morph the call into an internal call that merrily
		 * references only the label.
		 */
		rettrip = newtriple(op1);
		rettrip->operand[0] = put_mlab(labname);
	} else
	{       /* Create an external reference to LABEL^RTN */
		rettrip = maketriple(op2);
		CONVERT_MUTIL_NAME_PERCENT_TO_UNDERSCORE(rtnname);
		rettrip->operand[0] = put_cdlt(rtnname);
		mlabel2xtern(&lbl_str, rtnname, labname);
		rettrip->operand[1] = put_cdlt(&lbl_str);
		ins_triple(rettrip);
	}
	return rettrip;
}

/* The NON_USHBIN flavor of thisroutine is similar to the USHBIN flavor except if the routine being called is the
 * same as the current routine, we morph it into a local (same-routine) type call (no auto-relinking).
 */
STATICFNDEF oprtype insert_extref(mident *rtnname)
{
	if (!IS_SAME_RTN(rtnname, &routine_name))
	{	/* Generate external routine reference triple */
		INSERT_EXTREF_NOMORPH_AND_RETURN(rtnname);
	}
	else	/* Generate internal routine reference triple */
		return put_tref(newtriple(OC_CURRHD));
}
#endif
