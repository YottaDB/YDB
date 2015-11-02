/****************************************************************
 *								*
 *	Copyright 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/


#ifndef GVCST_LBM_CHECK_H
#define GVCST_LBM_CHECK_H

boolean_t gvcst_blk_is_allocated(uchar_ptr_t lbmap, int lm_offset);
boolean_t gvcst_blk_ever_allocated(uchar_ptr_t lbmap, int lm_offset);

#endif
