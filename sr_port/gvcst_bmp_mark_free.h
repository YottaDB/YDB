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

#ifndef GVCST_BMP_MARK_FREE_INCLUDED
#define GVCST_BMP_MARK_FREE_INCLUDED

trans_num gvcst_bmp_mark_free(kill_set *ks);

#define	GVCST_BMP_MARK_FREE(ks, ret_tn, cur_inctn_opcode, new_inctn_opcode, inctn_opcode, cs_addrs)			\
{	/* inctn_opcode is set already by callers (TP/non-TP/reorg) and is not expected to change. save it		\
	 * before modifying it. actually, the following save and reset of inctn_opcode (done before and after the	\
	 * call to gvcst_bmp_mark_free()) needs to be done only if JNL_ENABLED(cs_addrs), but since it is not		\
	 * easy to re-execute the save and reset of inctn_opcode in case t_end() detects a cdb_sc_jnlstatemod		\
	 * retry code, we choose the easier approach of doing the save and reset unconditionally even though this 	\
	 * approach has an overhead of doing a few assignments even though inctn_opcode might not be used in t_end 	\
	 * (in case JNL_ENABLED is not TRUE at t_end() time).								\
	 */														\
	assert(inctn_opcode == cur_inctn_opcode);									\
	inctn_opcode = new_inctn_opcode;										\
	ret_tn = gvcst_bmp_mark_free(ks);										\
	inctn_opcode = cur_inctn_opcode;	/* restore inctn_opcode */						\
}
#endif /* GVCST_BMP_MARK_FREE_INCLUDED */
