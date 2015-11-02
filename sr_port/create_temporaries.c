/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_ctype.h"

#include "compiler.h"
#include "mdq.h"
#include "opcode.h"
#include "indir_enum.h"
#include "nametabtyp.h"
#include "toktyp.h"
#include "funsvn.h"
#include "mmemory.h"
#include "advancewindow.h"
#include "namelook.h"
#include "cmd.h"
#include "svnames.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"
#include "glvn_pool.h"
#ifdef UNICODE_SUPPORTED
#include "gtm_utf8.h"
#endif

/* This function adds triples to the execution chain to store the values of subscripts (in glvns in compound SETs)
 * in temporaries (using OC_STOTEMP opcode). This function is only invoked when
 * 	a) The SET is a compound SET (i.e. there are multiple targets specified on the left side of the SET command).
 *	b) Subscripts are specified in glvns which are targets of the SET.
 *	e.g. set a=0,(a,array(a))=1
 * The expected result of the above command as per the M-standard is that array(0) (not array(1)) gets set to 1.
 * That is, the value of the subscript "a" should be evaluated at the start of the compound SET before any sets happen
 * and should be used in any subscripts that refer to the name "a".
 * In the above example, since it is a compound SET and "a" is used in a subscript, we need to store the value of "a"
 *	before the start of the compound SET (i.e.a=0) in a temporary and use that as the subscript for "array".
 * If in the above example the compound set was instead specified as set a=1,array(a)=1, the value of 1 gets substituted
 *	when used in "array(a)".
 * This is where the compound set acts differently from a sequence of multiple sets. This is per the M-standard.
 * In the above example, the subscript used was also a target within the compound SET. It is possible that the
 *	subscript is not also an individual target within the same compound SET. Even in that case, this function
 *	will be called to store the subscript in temporaries (as we dont know at compile time if a particular
 *	subscript is also used as a target within a compound SET).
 */
void create_temporaries(triple *sub, opctype put_oc)
{
	oprtype	*sb1;
	triple	*s0, *s1;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TREF(temp_subs));
	assert(NULL != sub);
	sb1 = &sub->operand[1];
	if ((OC_GVNAME == put_oc) || (OC_PUTINDX == put_oc) || (OC_SRCHINDX == put_oc))
	{
		sub = sb1->oprval.tref;		/* global name */
		assert(OC_PARAMETER == sub->opcode);
		sb1 = &sub->operand[1];
	} else if (OC_GVEXTNAM == put_oc)
	{
		sub = sb1->oprval.tref;		/* first env */
		assert(OC_PARAMETER == sub->opcode);
		sb1 = &sub->operand[0];
		assert(TRIP_REF == sb1->oprclass);
		s0 = sb1->oprval.tref;
		if ((OC_GETINDX == s0->opcode) || (OC_VAR == s0->opcode))
		{
			s1 = maketriple(OC_STOTEMP);
			s1->operand[0] = *sb1;
			*sb1 = put_tref(s1);
			s0 = s0->exorder.fl;
			dqins(s0->exorder.bl, exorder, s1);
		}
		sb1 = &sub->operand[1];
		sub = sb1->oprval.tref;		/* second env */
		assert(OC_PARAMETER == sub->opcode);
		sb1 = &sub->operand[0];
		assert(TRIP_REF == sb1->oprclass);
		s0 = sb1->oprval.tref;
		if ((OC_GETINDX == s0->opcode) || (OC_VAR == s0->opcode))
		{
			s1 = maketriple(OC_STOTEMP);
			s1->operand[0] = *sb1;
			*sb1 = put_tref(s1);
			s0 = s0->exorder.fl;
			dqins(s0->exorder.bl, exorder, s1);
		}
		sb1 = &sub->operand[1];
		sub = sb1->oprval.tref;		/* global name */
		assert(OC_PARAMETER == sub->opcode);
		sb1 = &sub->operand[1];
	}
	while (sb1->oprclass)
	{
		assert(TRIP_REF == sb1->oprclass);
		sub = sb1->oprval.tref;
		assert(OC_PARAMETER == sub->opcode);
		sb1 = &sub->operand[0];
		assert(TRIP_REF == sb1->oprclass);
		s0 = sb1->oprval.tref;
		if ((OC_GETINDX == s0->opcode) || (OC_VAR == s0->opcode))
		{
			s1 = maketriple(OC_STOTEMP);
			s1->operand[0] = *sb1;
			*sb1 = put_tref(s1);
			s0 = s0->exorder.fl;
			dqins(s0->exorder.bl, exorder, s1);
		}
		sb1 = &sub->operand[1];
	}
}
